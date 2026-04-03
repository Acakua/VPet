/**
 * main.cpp — Điểm khởi đầu của VPet-ESP32
 * Tích hợp toàn bộ Phân hệ 1, 2, 3 và phần driver Phân hệ 4.
 */

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <SD.h>       // Cần cắm SD trước khi boot
#include <TFT_eSPI.h> // Màn hình ST7796
#include <lvgl.h>

// Include tất cả file đã port ở các Phase trước
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
// I2C Touch (FT6336U)
#define TOUCH_SDA 4
#define TOUCH_SCL 5
#define I2C_ADDR_FT6336U 0x38

// SD Card (SPI Interface)
#define SD_CS   47
#define SD_MOSI 48
#define SD_MISO 45
#define SD_SCK  21

// Khởi tạo các Hardware Object
TFT_eSPI tft = TFT_eSPI(); 

// Kích thước chuẩn từ User_Setup.h
static const uint16_t screenWidth  = TFT_WIDTH;
static const uint16_t screenHeight = TFT_HEIGHT;

// Buffers chuyển Frame cho LVGL
static lv_disp_draw_buf_t draw_buf;
static lv_color_t* buf1;
static lv_color_t* buf2;

// Các con trỏ tổng quan quản lý Pet
using namespace VPet;

GameSave* gameSave = nullptr;
GameLoop* gameLoop = nullptr;
AnimationPlayer* animPlayer = nullptr;
GraphCore* graphCore = nullptr;
AnimationStateMachine* animFSM = nullptr;
StatusBar* statusBar = nullptr;
WorkTimerDisplay* workTimer = nullptr;
MessageBubble* msgBubble = nullptr;

// ==========================================
// CALLBACK CHO TỪNG LỚP LVGL DRIVER
// ==========================================

// Flush điểm ảnh ra TFT
void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

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
    if (Wire.endTransmission() != 0) {
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
        if (touches > 0 && touches <= 2) {
            uint16_t x = ((p1_xh & 0x0F) << 8) | p1_xl;
            uint16_t y = ((p1_yh & 0x0F) << 8) | p1_yl;

            data->state = LV_INDEV_STATE_PR;
            data->point.x = x;
            data->point.y = y;
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
    delay(1500);

    Serial.println("--- VPet ESP32 Boot Sequence Initiated ---");

    // 1. Phân hệ 4A: Khởi tạo màn hình
    tft.begin();
    tft.setRotation(0); 
    tft.fillScreen(TFT_BLACK);
    Serial.println("> TFT Initialized (TFT_eSPI).");
    
    // 2. Phân hệ 4B: Khởi tạo chân I2C cho cảm ứng (FT6336U)
    Wire.begin(TOUCH_SDA, TOUCH_SCL);
    Serial.println("> I2C Touch Driver Ready.");
    
    // 3. Phân hệ 4C: Khởi tạo SD Card
    SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    if(!SD.begin(SD_CS, SPI, 20000000)) {
        Serial.println(">> ERROR: SD Card Mount Failed!");
    } else {
        Serial.println("> SD Card Driver Ready.");
    }

    // Khởi tạo Graphic System (LVGL)
    lv_init();

    // 4. Cấp phát Memory cho Framebuffer (PSRAM)
    uint32_t bufSize = screenWidth * 60; // 60 line ~ khoảng 38KB mỗi buffer
    buf1 = (lv_color_t*)heap_caps_malloc(bufSize * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    buf2 = (lv_color_t*)heap_caps_malloc(bufSize * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, bufSize);

    // Kích hoạt Graphic Rendering Pipe
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    disp_drv.full_refresh = 0;
    lv_disp_drv_register(&disp_drv);

    // Kích hoạt Touch System Input
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);

    // Rẽ nhánh RTOS cho lv_tick độc lập
    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &lv_tick_task,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "lv_tick",
        .skip_unhandled_events = true
    };
    esp_timer_handle_t periodic_timer;
    esp_timer_create(&periodic_timer_args, &periodic_timer);
    esp_timer_start_periodic(periodic_timer, 5000); // Trigger mổi 5000 us (5ms)

    // ==============================================
    // KHỞI TẠO LOGIC VPET VÀ COMPONENT
    // ==============================================
    gameSave = new GameSave();
    
    // Tải dữ liệu lưu trước đó thẻ SD (Phân hệ 4D)
    if (!gameSave->load("/savedata.bin")) {
        Serial.println("> No save file found, creating a new pet...");
        gameSave->initNewGame("V-Pet");
        gameSave->money = 1500.50; // Mock Start Money
    } else {
        Serial.println("> Save file loaded successfully!");
    }
    
    graphCore = new GraphCore();
    animPlayer = new AnimationPlayer(); 
    animFSM = new AnimationStateMachine();
    
    // Liên kết các module
    animFSM->graph = graphCore;
    animFSM->save = gameSave;
    animFSM->player = animPlayer;
    
    gameLoop = new GameLoop(gameSave, animFSM);

    // Khởi tạo các Component giao diện lên lớp LVGL Active màn hình
    lv_obj_t* scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x282c30), LV_PART_MAIN);

    statusBar = new StatusBar(scr);
    workTimer = new WorkTimerDisplay(scr, gameLoop);
    msgBubble = new MessageBubble(scr);

    // Test thử UI ban đầu
    statusBar->show();
    msgBubble->show("VPet-System", "Khởi động thành công! Module đang chờ load dữ liệu.", 5000);

    Serial.println("--- VPet ESP32 All System Init Done ---");
}

void loop() {
    static uint32_t lastSaveTime = millis();

    // 1. Core tick điều hoà việc Render UI và event click
    lv_timer_handler();
    
    // 2. Timers sinh tồn của Pet (Cộng trừ đói khát / 15s check)
    if (gameLoop) {
        gameLoop->tick();
    }

    // 3. Xử lý giải nén Frame ảnh và load từ SD liên tục
    if (animPlayer) {
        animPlayer->tick();
    }

    // 4. Update số liệu hiển thị trên thanh Toolbars
    if (statusBar) {
        statusBar->update(gameSave);
    }
    
    // 5. Cập nhật các UI Tick-driven
    if (workTimer) workTimer->tick();
    if (msgBubble) msgBubble->tick();

    // 6. Auto-Save mỗi 5 phút (300,000 ms)
    if (millis() - lastSaveTime >= 300000) {
        if (gameSave && gameSave->save("/savedata.bin")) {
            Serial.println("Auto-saved game state to SD Card.");
            if (msgBubble) msgBubble->show("System", "Đã tự động lưu thành công!", 3000);
        }
        lastSaveTime = millis();
    }

    // Prevent Watchdog timeout & Cho phép RTOS breath
    delay(5);
}
