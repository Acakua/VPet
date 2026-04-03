#pragma once

// ==========================================
// VPet ESP32-S3 Hardware Configuration
// ==========================================

// 1. Display Configuration (MSP4031 - ST7796S)
// Resolution: 480 x 320 (Landscape)
#define DISPLAY_WIDTH  480
#define DISPLAY_HEIGHT 320

// Mapped GPIOs for standard ESP32-S3 SPI
#define TFT_MOSI 11
#define TFT_MISO 13
#define TFT_SCLK 12
#define TFT_CS   10
#define TFT_DC   9
#define TFT_RST  14
#define TFT_BL   48  // Backlight control pin

// 2. Touch Screen Configuration (FT6336U - I2C)
#define TOUCH_SDA  17
#define TOUCH_SCL  18
#define TOUCH_INT  16
#define TOUCH_RST  15

// 3. SD Card (SPI)
#define SD_CS    5

// 4. VPet Application Config
#define VPET_PIXEL_SCALE 320 // Size of pet sprite (320x320)
