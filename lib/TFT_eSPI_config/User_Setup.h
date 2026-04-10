#ifndef USER_SETUP_H
#define USER_SETUP_H

#warning "VPet User_Setup.h (SPI 27MHz) Loaded successfully!"

// Màn hình MSP4031 (ST7796S)
#define ST7796_DRIVER
#define TFT_WIDTH  320
#define TFT_HEIGHT 480

// Pinout mặc định (Có thể thay đổi khi hàn mạch thật)
#define TFT_MISO 5

#ifdef TFT_MOSI
  #undef TFT_MOSI
#endif
#define TFT_MOSI 6

#ifdef TFT_SCLK
  #undef TFT_SCLK
#endif
#define TFT_SCLK 7

#ifdef TFT_CS
  #undef TFT_CS
#endif
#define TFT_CS   15

#ifdef TFT_DC
  #undef TFT_DC
#endif
#define TFT_DC   4

#ifdef TFT_RST
  #undef TFT_RST
#endif
#define TFT_RST  2

#ifdef TFT_BL
  #undef TFT_BL
#endif
#define TFT_BL   21 // Backlight

// Enable SPI DMA
#define USE_HSPI_PORT
#define SPI_FREQUENCY  27000000
#define SPI_READ_FREQUENCY  20000000
#define SPI_TOUCH_FREQUENCY  2500000

// Font cho TFT (LVGL dùng font riêng cứng nên các font này không thực sự quan trọng nhưng cần để build ko lỗi build flag)
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4

#endif
