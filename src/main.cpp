#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
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
// I2C Touch (FT6336U)
#define TOUCH_SDA 17
#define TOUCH_SCL 18
#define I2C_ADDR_FT6336U 0x38

// SD Card (SPI Interface - Chung với TFT)
#define SD_CS   14
#define SD_MOSI 6
#define SD_MISO 5
#define SD_SCK  7

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

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    // Tính năng PushColors tối ưu của TFT_eSPI sẽ đẩy DMA (nếu bật DMA)
    tft.pushColors((uint16_t *)&color_p->full, w * h, true);
    tft.endWrite();

    lv_disp_flush_ready(disp_drv);
}

// Đọc toạ độ cảm ứng điểm chạm I2C
void my_touchpad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data) {
    // TẠM THỜI TẮT: I2C Touch chưa được khởi tạo
    data->state = LV_INDEV_STATE_REL;
    return;

    /* --- Code gốc (sẽ bật lại khi I2C Touch hoạt động) ---
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
            VPET_LOG_V("TOUCH", "Pressed at X:%u, Y:%u", x, y);
        } else {
            if (data->state == LV_INDEV_STATE_PR) {
                VPET_LOG_V("TOUCH", "Released");
            }
            data->state = LV_INDEV_STATE_REL;
        }
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
    */
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
    delay(2000); 

    VPET_LOG_I("SYS", "--- VPet ESP32 Boot Sequence Initiated ---");
    VPET_MEM_DUMP("SYS");

    // 1. Phân hệ 4A: Khởi tạo màn hình
    VPET_LOG_I("SYS", "Initializing TFT...");
    
    // Cấu hình LEDC PWM cho đèn nền (GPIO 21) - API mới cho Arduino v3.x
    ledcAttach(21, 5000, 8);    // Gắn chân 21 vào PWM, tần số 5kHz, độ phân giải 8-bit
    ledcWrite(21, 50);          // Độ sáng 20% (50/255)
    
    tft.begin();
    tft.setRotation(0); 
    tft.fillScreen(TFT_BLACK);
    VPET_LOG_I("SYS", "TFT Initialized (TFT_eSPI).");
    
    // 2. Phân hệ 4B: Cảm ứng (FT6336U) — TẠM THỜI TẮT
    // Wire.begin() đang gây crash trên ESP32-S3 khi chân CTP chưa nối đúng.
    // Sẽ bật lại sau khi xác nhận Display + SD hoạt động ổn định.
    VPET_LOG_W("SYS", "I2C Touch SKIPPED (debug mode).");
    yield();
    
    // 3. Phân hệ 4C: Khởi tạo SD Card (Shared SPI Bus)
    VPET_LOG_I("SYS", "Initializing SD Card...");
    yield();
    // Khởi tạo Arduino SPI (3 chân, KHÔNG truyền SS để SD lib tự quản lý CS)
    SPI.begin(SD_SCK, SD_MISO, SD_MOSI);
    yield();
    if(!SD.begin(SD_CS, SPI, 4000000)) {
        VPET_LOG_E("SYS", "SD Card Mount Failed!");
    } else {
        VPET_LOG_I("SYS", "SD Card Ready.");
    }
    yield();

    // Khởi tạo Graphic System (LVGL)
    VPET_LOG_I("SYS", "Initializing LVGL...");
    lv_init();
    lv_log_register_print_cb(my_log_cb);

    // 4. Cấp phát Memory cho Framebuffer (Internal RAM — PSRAM tạm tắt)
    VPET_LOG_I("SYS", "Allocating Framebuffers in Internal RAM...");
    uint32_t bufSize = screenWidth * 20; // 20 lines = ~12.5KB (vừa đủ trong Internal RAM)
    buf1 = (lv_color_t*)malloc(bufSize * sizeof(lv_color_t));
    buf2 = NULL; // Single buffer mode để tiết kiệm RAM
    
    if (!buf1) {
        VPET_LOG_E("SYS", "Framebuffer allocation FAILED! Halting.");
        while(1) { delay(1000); }
    }
    VPET_LOG_I("SYS", "Framebuffer OK (%u bytes).", bufSize * sizeof(lv_color_t));
    yield();
    
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
    // MINIMAL TEST: Chỉ vẽ một nhãn chữ lên màn hình
    // Toàn bộ VPet Logic (GameSave, GraphCore, AnimPlayer...) TẠM TẮT
    // ==============================================
    lv_obj_t* scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x003366), LV_PART_MAIN);
    
    lv_obj_t* label = lv_label_create(scr);
    lv_label_set_text(label, "VPet ESP32\nDisplay OK!");
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_center(label);

    VPET_LOG_I("SYS", "--- MINIMAL BOOT COMPLETE ---");
    VPET_MEM_DUMP("SYS");
}

void loop() {
    lv_timer_handler();
    delay(5);
}
