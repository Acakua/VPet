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

// Biến cho FPS counter
static uint32_t frame_count = 0;
lv_obj_t* lblFPS = nullptr;

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

// Flush điểm ảnh ra TFT dùng DMA
void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    // Chờ DMA trước đó hoàn tất (đảm bảo hardware sẵn sàng)
    tft.dmaWait();

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    // Lưu ý: Đã dùng #define LV_COLOR_16_SWAP 1 và TFT_BGR_ORDER
    tft.pushImageDMA(area->x1, area->y1, w, h, (uint16_t*)color_p);
    tft.endWrite();

    frame_count++;
    lv_disp_flush_ready(disp_drv);
}

// Callback cập nhật FPS mỗi giây
static void update_fps_cb(lv_timer_t * timer) {
    if (lblFPS) {
        lv_label_set_text_fmt(lblFPS, "FPS: %u", frame_count * 2); // Nhân 2 vì chu kỳ 500ms
        frame_count = 0;
    }
}

// Đọc toạ độ cảm ứng điểm chạm I2C
void my_touchpad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data) {
    Wire.beginTransmission(I2C_ADDR_FT6336U);
    Wire.write(0x02); // TD_STATUS Register
    uint8_t err = Wire.endTransmission();
    if (err != 0) {
        static uint32_t lastErr = 0;
        if (millis() - lastErr > 5000) {
            VPET_LOG_W("TOUCH", "I2C Communication Error: %d", err);
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

        touches = touches & 0x0F;
        if (touches > 0 && touches <= 2) {
            // Tọa độ Raw từ sensor (Thường là 0-320 cho X, 0-480 cho Y)
            uint16_t raw_x = ((p1_xh & 0x0F) << 8) | p1_xl;
            uint16_t raw_y = ((p1_yh & 0x0F) << 8) | p1_yl;

            // CHUYỂN ĐỔI TỌA ĐỘ CHO CHẾ ĐỘ NGANG (Rotation 1)
            // Sensor Portrait (320x480) -> Screen Landscape (480x320)
            uint16_t screen_x = raw_y;           // X màn hình = Y cảm ứng
            uint16_t screen_y = 320 - raw_x;     // Y màn hình = Đảo ngược X cảm ứng

            // Giới hạn vùng an toàn
            if (screen_x >= screenWidth)  screen_x = screenWidth - 1;
            if (screen_y >= screenHeight) screen_y = screenHeight - 1;

            data->state = LV_INDEV_STATE_PR;
            data->point.x = screen_x;
            data->point.y = screen_y;
            
            // Log debug tọa độ (chỉ in mỗi 100ms khi nhấn để tránh lụt log)
            static uint32_t lastTouchLog = 0;
            if (millis() - lastTouchLog > 100) {
                VPET_LOG_D("TOUCH", "Raw[%u,%u] -> Screen[%u,%u]", raw_x, raw_y, screen_x, screen_y);
                lastTouchLog = millis();
            }

            // Dispatch tới AnimationStateMachine (Vùng tương tác Pet)
            if (animFSM) {
                uint16_t pet_boundary = (statusBar && !statusBar->isCollapsed()) ? 300 : 440;
                if (screen_x < pet_boundary) { 
                    if (screen_y < 160) {
                        animFSM->displayTouchHead();
                        if (gameLoop) gameLoop->interact();
                    } else {
                        animFSM->displayTouchBody();
                        if (gameLoop) gameLoop->interact();
                    }
                }
            }
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
    Wire.setClock(400000); // Tăng lên 400kHz (Fast Mode) để giảm lag touch
    VPET_LOG_I("SYS", "I2C Touch initialized (SDA=%d, SCL=%d, RST=%d)", TOUCH_SDA, TOUCH_SCL, TOUCH_RST);

    // 2. KHỞI TẠO THẺ SD (Reset bus và thử lại ở 16MHz)
    vpet_spi.end(); 
    delay(100);
    vpet_spi.begin(SD_SCK, SD_MISO, SD_MOSI);
    delay(100);

    VPET_LOG_I("SYS", "Initializing SD Card at 20MHz...");
    if (!SD.begin(SD_CS, vpet_spi, 20000000)) {
        VPET_LOG_E("SYS", "SD Card Mount Failed!");
    } else {
        VPET_LOG_I("SYS", "SD Card OK.");
    }
    yield();

    // 4. NẠP DỮ LIỆU GAME (Scan SD khi bus còn sạch)

    VPET_LOG_I("SYS", "Initialization Complete.");
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
    VPET_LOG_I("SYS", "Initializing TFT on HSPI with DMA...");
    tft.begin();
    tft.initDMA(); 
    tft.invertDisplay(false); // Sửa lỗi âm bản cho màn IPS
    tft.setRotation(1); 
    
    // Cấu hình Đèn nền (GPIO 21)
    ledcSetup(0, 5000, 8); 
    ledcAttachPin(21, 0); 
    ledcWrite(0, 255); // Độ sáng tối đa

    tft.fillScreen(TFT_BLACK); 
    VPET_LOG_I("SYS", "TFT Initialized.");

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
    // Tối ưu buffer về mức 16k (16384 bytes) để ổn định bus SPI
    uint32_t bufSize = 16384; 
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
            
            if (statusBar) {
                statusBar->refresh_tasks();
            }
            
            VPET_LOG_I("SYS", "GameLoop started in DEBUG MODE (2s interval).");
        }
    }

    // 8. TẠO FPS COUNTER (Phải đặt sau khi toàn bộ UI và LVGL đã sẵn sàng)
    lblFPS = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_color(lblFPS, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_color(lblFPS, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(lblFPS, 150, 0);
    lv_obj_align(lblFPS, LV_ALIGN_TOP_LEFT, 5, 5);
    lv_timer_create(update_fps_cb, 500, NULL);

    VPET_LOG_I("SYS", "Initialization Complete.");
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
    
    delay(1); // Giảm xuống 1ms để vòng lặp quay nhanh nhất có thể
}
