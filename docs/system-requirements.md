# Yêu cầu Hệ thống cho VPet ESP32-S3

Tài liệu này liệt kê các yêu cầu về phần cứng, phần mềm và quy trình chuẩn bị dữ liệu hình ảnh cho dự án chuyển đổi (port) VPet-Simulator lên nền tảng ESP32-S3.

## 1. Danh sách Phần cứng
| Linh kiện | Thông số kỹ thuật | Mục đích |
| :--- | :--- | :--- |
| **Vi điều khiển** | ESP32-S3 (N16R8) | Trung tâm xử lý, 16MB Flash, 8MB PSRAM (Octal SPI) |
| **Màn hình** | MSP4031 (4.0" IPS TFT) | Hiển thị chính cho Pet và UI (480x320) |
| **Cảm ứng** | Capacitive Touch (FT6336U) | Tương tác người dùng (xoa đầu, vuốt ve, menu) |
| **Lưu trữ** | Thẻ nhớ MicroSD (1GB+) | Chứa hàng ngàn tệp khung hình ảnh hoạt họa |
| **Âm thanh (Cần thêm)** | Mạch I2S DAC (MAX98357A) + Loa 3W | Cho Pet phát tiếng kêu và thông báo |
| **Nguồn điện** | 5V 2A microUSB / USB-C | Cấp nguồn ổn định cho chip và đèn nền màn hình |

## 2. Môi trường Phần mềm (Lập trình)
- **Công cụ lập trình (IDE)**: Visual Studio Code.
- **Tiện ích mở rộng**: PlatformIO.
- **Nền tảng (Framework)**: Arduino Core cho ESP32 (v2.0.x trở lên).
- **Thư viện đồ họa (UI Library)**: LVGL (Light and Versatile Graphics Library) - Phiên bản v8.3 hoặc v9.0.
- **Driver hiển thị**: TFT_eSPI.
- **Driver cảm ứng**: FT6336U.

## 3. Chuẩn bị Dữ liệu (Assets)
- **Nguồn ảnh gốc**: `VPet-Simulator.Windows/mod/0000_core/pet/vup`.
- **Kích thước mục tiêu**: **320x320 Pixels** (Để giữ nguyên tỷ lệ hình vuông và dành không gian hai bên cho UI).
- **Định dạng tối ưu**: RGB565 / Binary (tốc độ cao).
- **Tối ưu hóa**: Phải thu nhỏ (resize) ảnh trên PC bằng các thuật toán chất lượng cao (như Lanczos) trước khi nạp vào thẻ nhớ SD.

## 4. Các giai đoạn phát triển chính
1. **Thiết kế**: Thống nhất phần cứng và sơ đồ nối dây (Đã hoàn thành).
2. **Thiết lập**: Cấu hình Driver màn hình và kiểm tra kết nối hiển thị.
3. **Dữ liệu**: Viết script Python để xử lý hàng ngàn ảnh của VPet sang định dạng nhúng.
4. **Lập trình**: Chuyển logic game và máy trạng thái của Pet sang C++.
5. **Giao diện**: Xây dựng Animation Player và hệ thống Menu bằng LVGL.
6. **Kiểm thử**: Tối ưu hóa tốc độ load ảnh và độ nhạy của cảm ứng.
