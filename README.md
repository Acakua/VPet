# VPet-ESP32S3: C#/WPF to ESP32-S3 Vitual Pet Port

A high-performance port of the popular **VPet-Simulator** (originally C#/WPF) to the **ESP32-S3** microcontroller, featuring a beautiful **LVGL-based** user interface and high-quality 2D animations.

> [!IMPORTANT]
> This project is currently in early development (Milestone: MVP 1). Core logic and basic animations are being ported.

## 🚀 Key Features

- **High-Quality Animations**: 320x320 RGB565 frames streamed directly from a MicroSD card using a custom **Double Buffering PSRAM Engine**.
- **Survival Logic**: Full port of the core survival mechanics (Hunger, Thirst, Health, Feeling, Experience) from the original C# codebase.
- **Interactive Touch**: Supports capacitive touch for "Petting" and "Lifting" interactions.
- **Hardware Optimized**: Specifically designed for ESP32-S3 N16R8 with 8MB PSRAM to achieve 24-30 FPS.

## 🛠️ Hardware Requirements

| Component | specification |
| :--- | :--- |
| **MCU** | ESP32-S3 (N16R8) - 16MB Flash, 8MB PSRAM |
| **Display** | MSP4031 4.0" IPS TFT (480x320, ST7796S Driver) |
| **Touch** | Capacitive Touch (FT6336U) |
| **Storage** | MicroSD Card (1GB+ for animations) |

## 🔌 Hardware Wiring

For **ESP32-S3 DevKitC-1 (N16R8)** and **ST7796S** display with integrated SD slot:

| Component | Nhãn trên Màn hình | GPIO Pin (S3) | Ghi chú |
| :--- | :--- | :--- | :--- |
| **Nguồn** | **VCC** | **5V / VBUS** | Ưu tiên nguồn 5V rời |
| | **GND** | **GND** | Phải nối chung GND |
| **SPI (Chung)** | **SCK** | **7** | Chia sẻ bus HSPI |
| | **SDI (MOSI)** | **6** | Chia sẻ bus HSPI |
| | **SDO (MISO)** | **5** | Chia sẻ bus HSPI |
| **Display** | **LCD_CS** | **15** | |
| | **LCD_RS** | **4** | Lựa chọn an toàn |
| | **LCD_RST** | **2** | Lựa chọn an toàn |
| **LED** | | **21** | Đèn nền (Backlight) |
| **Thẻ nhớ** | **SD_CS** | **14** | |
| **Cảm ứng** | **CTP_SDA** | **1** | SDA (Tránh PSRAM/HSPI) |
| | **CTP_SCL** | **9** | SCL (Tránh PSRAM/HSPI) |
| | **CTP_RST** | **10** | Reset cảm ứng |

> [!CAUTION]
> DO NOT use GPIOs 33-37 and **47-48** for peripherals on the **N16R8 (OPI PSRAM)** variant, as they are reserved for the high-speed PSRAM interface and differential clock.

## 📦 Technical Stack

- **Framework**: PlatformIO / Arduino-ESP32
- **Graphics**: [LVGL 8.3/9.0](https://lvgl.io/)
- **Display Driver**: [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI)
- **Data Pipeline**: Python scripts for offline image processing (**Lanczos** resize & RGB565 conversion).

## 📂 Project Structure

- `src/`: Core logic and UI implementations.
- `include/`: Header files and configurations (`lv_conf.h`, `User_Setup.h`).
- `docs/`: Technical documentation and porting roadmaps.
- `sdcard_data/`: Pre-processed binary frame data for the pet.
- `pipeline.py`: Python script for asset conversion.

## 🏗️ Getting Started

1. **Build**: Open with PlatformIO and run `pio run`.
2. **Flash**: Connect your ESP32-S3 and run `pio run --target upload`.
3. **Headless Testing**: If you don't have the screen/SD yet, the project supports **Headless Mode** via Serial Monitor (115200 baud).

## 🗺️ Porting Progress

Refer to [docs/Porting-Map.md](./docs/Porting-Map.md) for a detailed breakdown of already-ported components and upcoming milestones.

## 🐛 Bug & Fix Tracking

| Bug | Symptom | Status | Root Cause & Resolution |
| :--- | :--- | :--- | :--- |
| **SPI Bus Contention** | `Card Failed! cmd: 0x11` after TFT init. | **Fixed** | **Root Cause**: Conflict between default SPI (SPI2) and HSPI (SPI3) on pins 5,6,7. **Fix**: Synchronized all peripherals to `vpet_spi` (HSPI bus). |
| **Guru Meditation** | `StoreProhibited` crash on boot. | **Fixed** | **Fix**: Use `espressif32@6.6.0` and maintain `#define USE_HSPI_PORT` in `User_Setup.h`. |
| **Startup Delay** | 45s wait during SD scan. | **Fixed** | **Fix**: Disabled recursive `VPET_LOG_I` calls to eliminate Serial bottleneck. |

---
**Original Project**: [VPet-Simulator](https://github.com/LorisYounger/VPet) by LorisYounger.  
**Ported by**: DeepMind Advanced Agentic Coding Team & Contributors.
