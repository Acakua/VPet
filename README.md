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
| **Display** | 4.0" IPS TFT (480x320, ST7796S Driver) |
| **Touch** | Capacitive Touch (FT6336U) |
| **Storage** | MicroSD Card (1GB+ for animations) |

## 🔌 Hardware Wiring

For **ESP32-S3 DevKitC-1 (N16R8)** and **ST7796S** display with integrated SD slot:

| Component | Module Pin | GPIO Pin (S3) | Note |
| :--- | :--- | :--- | :--- |
| **Power** | VCC | **5V** | **Recommended 5V** for internal LDO |
| | GND | GND | |
| **Shared SPI Bus** | **SCK / SCLK** | **7** | Shared by TFT & SD |
| | **SDI / MOSI** | **6** | Shared by TFT & SD |
| | **SDO / MISO** | **5** | Shared by TFT & SD |
| **Display** | **TFT_CS** | **10** | TFT Chip Select |
| | DC / RS | 11 | Data/Command |
| | RESET / RES | 12 | Hardware Reset |
| | LED / BL | 13 | Backlight Control |
| **Storage** | **SD_CS** | **14** | SD Card Chip Select |
| **Touch (I2C)** | **T_SDA** | **17** | I2C Data |
| | **T_SCL** | **18** | I2C Clock |

> [!CAUTION]
> DO NOT use GPIOs 33-37 for peripherals, as they are reserved for the **8MB OPI PSRAM** on the N16R8 variant.

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

---

**Original Project**: [VPet-Simulator](https://github.com/LorisYounger/VPet) by LorisYounger.
**Ported by**: DeepMind Advanced Agentic Coding Team & Contributors.
