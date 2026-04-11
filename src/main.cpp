#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>

// Cấu hình Cảm ứng FT6336U (Chuẩn cuối cho N16R8)
#define I2C_ADDR_FT6336U 0x38
#define TOUCH_SDA 1
#define TOUCH_SCL 9
#define TOUCH_RST 10

#include <SD.h>       // Cần cắm SD trước khi boot
#include <TFT_eSPI.h> // Màn hình ST7796
#include <lvgl.h>

// Include tất cả file đã port ở các Phase trước
#include "Utils.hpp"
#include "GameSave.hpp"
#include "GameLoop.hpp"
#include "AnimationPlayer.hpp"
#include "AnimationStateMachine.hpp"
#include "StatusBar.hpp"
#include "WorkTimerDisplay.hpp"
#include "MessageBubble.hpp"

// ==========================================
// CẤU HÌNH PINOUT DỰ KIẾN (Thay đổi nếu cần)
// ==========================================
// SD Card (SPI Interface - Chung với TFT trên HSPI)
#define SD_CS   14
#define SD_MOSI 6
#define SD_MISO 5
#define SD_SCK  7
#define TFT_RST_PIN 2
#define TOUCH_RST   10

// Khởi tạo đối tượng vpet_spi riêng cho Flash/SD/TFT 
// Sử dụng bộ HSPI (SPI3_HOST) để tách biệt với bus nội bộ
SPIClass vpet_spi(HSPI);
TFT_eSPI tft = TFT_eSPI(); 

// Kích thước chuẩn sau khi xoay ngang (Landscape)
static const uint16_t screenWidth  = 480;
static const uint16_t screenHeight = 320;

// Buffers chuyển Frame cho LVGL
static lv_disp_draw_buf_t draw_buf;
static lv_color_t* buf1;
static lv_color_t* buf2;

// Các con trỏ tổng quan quản lý Pet
using namespace VPet;

static lv_img_dsc_t pet_img_dsc;
static lv_obj_t* pet_img_obj = nullptr;

GameSave* gameSave = nullptr;
GameLoop* gameLoop = nullptr;
AnimationPlayer* animPlayer = nullptr;
GraphCore* graphCore = nullptr;
AnimationStateMachine* animFSM = nullptr;
StatusBar* statusBar = nullptr;
WorkTimerDisplay* workTimer = nullptr;
MessageBubble* msgBubble = nullptr;

// ==========================================
// CALLBACK CHO TỨNG LỚP LVGL DRIVER
// ==========================================

// LVGL Log Callback
void my_log_cb(const char * buf) {
    Serial.printf("[LVGL] %s", buf);
}

// Flush điểm ảnh ra TFT
void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    static uint32_t flush_count = 0;
    if (flush_count++ % 50 == 0) {
        VPET_LOG_I("GFX", "lv_disp_flush called, area: [%d,%d] -> [%d,%d], w:%u, h:%u", 
                   area->x1, area->y1, area->x2, area->y2, w, h);
    }

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    // Tính năng PushColors tối ưu của TFT_eSPI sẽ đẩy DMA (nếu bật DMA)
    tft.pushColors((uint16_t *)&color_p->full, w * h, true);
    tft.endWrite();

    lv_disp_flush_ready(disp_drv);
}

// Đọc toạ độ cảm ứng điểm chạm I2C
void my_touchpad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data) {
    Wire.beginTransmission(I2C_ADDR_FT6336U);
    Wire.write(0x02); // TD_STATUS Register
    uint8_t err = Wire.endTransmission();
    if (err != 0) {
        static uint32_t lastErr = 0;
        if (millis() - lastErr > 5000) {
            VPET_LOG_W("TOUCH", "I2C Communication Error: %d (Check wires!)", err);
            lastErr = millis();
        }
        data->state = LV_INDEV_STATE_REL;
        return;
    }
    
    Wire.requestFrom(I2C_ADDR_FT6336U, 5);
    if (Wire.available() >= 5) {
        uint8_t touches = Wire.read();
        uint8_t p1_xh = Wire.read();
        uint8_t p1_xl = Wire.read();
        uint8_t p1_yh = Wire.read();
        uint8_t p1_yl = Wire.read();

        // Xử lý touchpoint 1
        touches = touches & 0x0F;
        if (touches > 0 && touches <= 2) {
            uint16_t x = ((p1_xh & 0x0F) << 8) | p1_xl;
            uint16_t y = ((p1_yh & 0x0F) << 8) | p1_yl;

            // Lọc dữ liệu lỗi/ngoài màn hình
            if (x >= screenWidth || y >= screenHeight) {
                data->state = LV_INDEV_STATE_REL;
                return;
            }

            data->state = LV_INDEV_STATE_PR;
            data->point.x = x;
            data->point.y = y;
            
            // Dispatch tới AnimationStateMachine
            if (animFSM) {
                if (x < 320) { // Vùng Pet
                    if (y < 160) {
                        animFSM->displayTouchHead();
                        if (gameLoop) gameLoop->interact();
                    } else {
                        animFSM->displayTouchBody();
                        if (gameLoop) gameLoop->interact();
                    }
                }
            }
            VPET_LOG_V("TOUCH", "Pressed at X:%u, Y:%u", x, y);
        } else {
            data->state = LV_INDEV_STATE_REL;
        }
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

// RTOS Timer cấp heartbeat (Time Base) cho LVGL
void lv_tick_task(void *arg) {
    lv_tick_inc(5); // Period 5ms
}

// ==========================================
// HÀM KHỞI TẠO HỆ THỐNG
// ==========================================
void setup() {
    Serial.begin(115200);
    Serial.setDebugOutput(true);
    // Chờ Serial Monitor kết nối (tối đa 5s) để không bỏ lỡ log khởi động
    while (!Serial && millis() < 5000) {
        delay(10);
    }
    Serial.flush();
    delay(500); 
    Serial.println("\n\n###########################################");
    Serial.println("##    VPET-ESP32S3 STARTUP SEQUENCE     ##");
    Serial.println("###########################################\n");

    VPET_LOG_I("SYS", "--- VPet ESP32 Boot Sequence Initiated ---");
    VPET_MEM_DUMP("SYS");

    // 1. Manual Reset cho màn hình (GPIO 2)
    pinMode(TFT_RST_PIN, OUTPUT);
    digitalWrite(TFT_RST_PIN, LOW);
    delay(100);
    digitalWrite(TFT_RST_PIN, HIGH);
    delay(200);

    // 1b. Khóa chân CS để tránh bus bị thả nổi
    pinMode(SD_CS, OUTPUT);
    pinMode(TFT_CS, OUTPUT);
    digitalWrite(SD_CS, HIGH);
    digitalWrite(TFT_CS, HIGH);

    // 2. Khởi tạo Bus HSPI (Dùng chung cho SD và TFT)
    vpet_spi.begin(SD_SCK, SD_MISO, SD_MOSI);
    delay(100);
    yield();

    // 1. KHỞI TẠO CẢM ỨNG (I2C)
    pinMode(TOUCH_RST, OUTPUT);
    digitalWrite(TOUCH_RST, LOW);  delay(20);
    digitalWrite(TOUCH_RST, HIGH); delay(100);

    Wire.begin(TOUCH_SDA, TOUCH_SCL);
    Wire.setClock(100000); // Standard Mode để ổn định hơn
    VPET_LOG_I("SYS", "I2C Touch initialized (SDA=%d, SCL=%d, RST=%d)", TOUCH_SDA, TOUCH_SCL, TOUCH_RST);

    // 2. KHỞI TẠO THẺ SD (Dùng bus SPI nhanh)
    VPET_LOG_I("SYS", "Initializing SD Card on HSPI bus...");
    if (!SD.begin(SD_CS, vpet_spi, 16000000)) {
        VPET_LOG_E("SYS", "SD Card Mount Failed!");
    } else {
        VPET_LOG_I("SYS", "SD Card OK.");
    }
    yield();

    // 4. NẠP DỮ LIỆU GAME (Scan SD khi bus còn sạch)
    VPET_LOG_I("SYS", "Initializing GameSave...");
    gameSave = new GameSave();
    if (!gameSave->load("/savedata.bin")) {
        gameSave->initNewGame("V-Pet");
    }
    yield();

    VPET_LOG_I("SYS", "Initializing GraphCore (SD Scan)...");
    graphCore = new GraphCore();
    if (!graphCore->load("/pet")) {
        VPET_LOG_E("SYS", "Failed to load animation dictionary from SD!");
    } else {
        VPET_LOG_I("SYS", "GraphCore OK. Found %u animations.", graphCore->entryCount);
    }
    yield();

    // 5. KHỞI TẠO MÀN HÌNH (Dùng chung bus HSPI đã quét xong)
    VPET_LOG_I("SYS", "Initializing TFT on HSPI...");
    tft.begin();
    tft.setRotation(1); 
    tft.fillScreen(TFT_BLUE); 
    
    // Cấu hình LEDC PWM cho đèn nền (GPIO 21)
    ledcSetup(0, 5000, 8); 
    ledcAttachPin(21, 0); 
    ledcWrite(0, 180); 
    VPET_LOG_I("SYS", "TFT Initialized.");
    yield();

    // Đảm bảo Bus SPI trả về trạng thái nhàn rỗi
    digitalWrite(TFT_CS, HIGH);
    digitalWrite(SD_CS, HIGH);
    yield();

    ledcWrite(0, 255);      
    VPET_LOG_I("SYS", "STEP 2 COMPLETE: Hardware Ready.");

    VPET_LOG_I("SYS", "Initializing LVGL...");
    lv_init();
    lv_log_register_print_cb(my_log_cb);

    VPET_LOG_I("SYS", "Allocating Framebuffers in Internal RAM...");
    uint32_t bufSize = screenWidth * 20;
    buf1 = (lv_color_t*) heap_caps_malloc(bufSize * sizeof(lv_color_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    buf2 = (lv_color_t*) heap_caps_malloc(bufSize * sizeof(lv_color_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    
    if (buf1 && buf2) {
        lv_disp_draw_buf_init(&draw_buf, buf1, buf2, bufSize);

        static lv_disp_drv_t disp_drv;
        lv_disp_drv_init(&disp_drv);
        disp_drv.hor_res = screenWidth;
        disp_drv.ver_res = screenHeight;
        disp_drv.flush_cb = my_disp_flush;
        disp_drv.draw_buf = &draw_buf;
        lv_disp_drv_register(&disp_drv);

        static lv_indev_drv_t indev_drv;
        lv_indev_drv_init(&indev_drv);
        indev_drv.type = LV_INDEV_TYPE_POINTER;
        indev_drv.read_cb = my_touchpad_read;
        lv_indev_drv_register(&indev_drv);

        const esp_timer_create_args_t periodic_timer_args = {
            .callback = &lv_tick_task,
            .arg = NULL,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "lv_tick"
        };
        esp_timer_handle_t periodic_timer;
        esp_timer_create(&periodic_timer_args, &periodic_timer);
        esp_timer_start_periodic(periodic_timer, 5000);

        lv_obj_t* scr = lv_scr_act();
        lv_obj_set_style_bg_color(scr, lv_color_hex(0x003366), LV_PART_MAIN);
        
        lv_obj_t* label = lv_label_create(scr);
        if (graphCore && graphCore->entryCount > 0) {
            lv_label_set_text_fmt(label, "VPet ESP32\nDisplay OK!\nPet: %s\nAnims: %u", 
                               gameSave->name, graphCore->entryCount);
        } else {
            uint64_t cardSize = SD.cardSize() / (1024 * 1024);
            lv_label_set_text_fmt(label, "VPet ESP32\nSD: %llu MB\nData Load Fail!", cardSize);
        }
        lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);

        // ===================================
        // PHASE 1: Wiring AnimationPlayer
        // ===================================

        // 1. Tạo lv_img widget cho Pet (320x320 pixel, bên trái màn hình)
        pet_img_obj = lv_img_create(lv_scr_act());
        lv_obj_set_pos(pet_img_obj, 0, 0);
        lv_obj_set_size(pet_img_obj, 320, 320);

        // Cấu hình lv_img_dsc trỏ vào PSRAM buffer (sẽ swap mỗi frame)
        pet_img_dsc.header.always_zero = 0;
        pet_img_dsc.header.cf     = LV_IMG_CF_TRUE_COLOR_CHROMA_KEYED;
        pet_img_dsc.header.w      = 320;
        pet_img_dsc.header.h      = 320;
        pet_img_dsc.data_size     = 320 * 320 * 2;
        pet_img_dsc.data          = nullptr;  // Sẽ được animPlayer set

        // 2. Tạo AnimationPlayer
        animPlayer = new AnimationPlayer();
        animPlayer->lvglImgDesc = &pet_img_dsc;

        // ===================================
        // PHASE 2: Wiring AnimationStateMachine
        // ===================================
        
        // 2b. Tạo AnimationStateMachine
        animFSM = new AnimationStateMachine();
        animFSM->graph  = graphCore;
        animFSM->save   = gameSave;
        animFSM->player = animPlayer;

        // 3. Khởi động animation mặc định qua FSM
        if (graphCore && graphCore->entryCount > 0) {
            animFSM->displayDefault();
            VPET_LOG_I("SYS", "AnimationStateMachine started.");
        }

        // ===================================
        // PHASE 3: Wiring StatusBar
        // ===================================
        
        // 4. Tạo và hiển thị thanh trạng thái
        if (pet_img_obj) {
            statusBar = new StatusBar(lv_scr_act(), pet_img_obj);
            statusBar->show();
            // Trong Landscape, Pet (0,0) và Sidebar (320,0) tự khớp nhau
            VPET_LOG_I("SYS", "StatusBar initialized in Landscape mode.");
        }

        // ===================================
        // PHASE 4: Wiring GameLoop
        // ===================================

        // 5. Khởi tạo vòng lặp sinh tồn
        if (gameSave && animFSM) {
            gameLoop = new GameLoop(gameSave, animFSM);
            gameLoop->eventIntervalMs = 2000; // Debug: 2 giây một nhịp (Gốc 15s)
            gameLoop->start();
            VPET_LOG_I("SYS", "GameLoop started in DEBUG MODE (2s interval).");
        }
    }

    VPET_LOG_I("SYS", "--- STEP 3 COMPLETE ---");
    VPET_MEM_DUMP("SYS");
}

void loop() {
    lv_timer_handler();
    
    // Phase 2: AnimationStateMachine tick
    if (animFSM && animFSM->tick()) {
        // ... frame update logic ...
        if (animPlayer && animPlayer->currentBuffer()) {
            pet_img_dsc.data = animPlayer->currentBuffer();
            lv_img_set_src(pet_img_obj, &pet_img_dsc);
            lv_obj_invalidate(pet_img_obj);
            lv_obj_move_foreground(pet_img_obj);
        }
    }

    // Phase 4: GameLoop tick ( survival check every 15s )
    if (gameLoop) {
        gameLoop->tick();
    }

    static uint32_t lastUpdate = 0;
    if (statusBar && gameSave && millis() - lastUpdate > 1000) {
        statusBar->update(gameSave);
        lastUpdate = millis();
    }
    
    delay(5);
}
