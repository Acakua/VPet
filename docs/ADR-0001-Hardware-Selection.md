# ADR 0001: Lựa chọn Phần cứng và Nền tảng cho dự án VPet

## Trạng thái
Đã chấp nhận (2026-04-02)

## Bối cảnh
Người dùng muốn chuyển đổi (port) ứng dụng VPet-Simulator từ máy tính (C#/WPF) sang một thiết bị phần cứng độc lập có kích thước "bằng bàn tay" và hình ảnh "sắc nét như màn hình PC".

## Quyết định
Chúng ta thống nhất sử dụng các thành phần phần cứng và phần mềm sau:

### 1. Vi điều khiển chính (Main Controller)
- **ESP32-S3 (Phiên bản N16R8)**: 
    - 16MB Flash, 8MB PSRAM (Octal SPI).
    - CPU Dual-core 240MHz.
    - **Lý do chọn**: Cần bộ nhớ PSRAM lớn để xử lý các phim hoạt hình 2D độ phân giải cao và lưu trữ các khung hình đã giải mã. Chip S3 có khả năng tăng tốc phần cứng cho các thao tác giao diện người dùng (UI).

### 2. Màn hình Hiển thị (Display)
- **MSP4031 (4.0 inch IPS TFT)**:
    - Độ phân giải: **480x320**.
    - Chip Driver: **ST7796S**.
    - Giao diện kết nối: **SPI**.
    - **Lý do chọn**: Kích thước 4.0 inch đủ lớn để cầm tay, độ phân giải 480x320 cho mật độ điểm ảnh (PPI) tốt đảm bảo độ sắc nét cao (khoảng 144 PPI), và công nghệ IPS cho góc nhìn rộng hơn màn TFT thông thường.

### 3. Tương tác Người dùng (User Interaction)
- **Cảm ứng Điện dung (FT6336U)**:
    - **Lý do chọn**: Quan trọng cho các cơ chế như "Xoa đầu" (Petting) và "Xách" (Lifting) của VPet. Cảm ứng điện dung mang lại trải nghiệm mượt mà, cao cấp hơn cảm ứng điện trở.

### 4. Nền tảng Phần mềm (Software Stack)
- **PlatformIO / C++**: Để quản lý dự án chuyên nghiệp.
- **LVGL (v8.x/v9.x)**: Thư viện đồ họa chính cho UI và hoạt ảnh của Pet.
- **TFT_eSPI**: Thư viện driver cấp thấp để điều khiển màn hình.
- **Python**: Dùng để xử lý dữ liệu ảnh Offline (Nén và thu nhỏ ảnh về kích thước **320x320** để phù hợp với chiều cao màn hình).

### 5. Lưu trữ (Storage)
- **Thẻ nhớ MicroSD**: Dùng để chứa hàng ngàn khung hình ảnh của Pet đã được xử lý.
- **Lý do chọn**: Bộ nhớ Flash trong chip (16MB) không đủ sức chứa toàn bộ thư viện ảnh chất lượng cao của dự án VPet gốc.

## Hệ quả
- **Tỷ lệ hiển thị**: Nhân vật sẽ được thu nhỏ (resize) về kích thước **320x320 pixels** để giữ nguyên tỷ lệ hình vuông gốc, giúp không bị biến dạng khi đưa lên màn hình 480x320.
- **Bố trí giao diện (UI)**: Với nhân vật 320x320 nằm giữa màn hình, chúng ta sẽ có dư 160 pixels ở hai bên (80px mỗi bên) để hiển thị thanh trạng thái (Đói, Vui, Cấp độ) và các Menu điều khiển.
- **Tiêu thụ năng lượng**: Tiêu thụ năng lượng sẽ cao do màn hình lớn; cần nguồn điện ổn định hoặc pin Li-ion dung lượng lớn.
- **Trình phát hoạt ảnh**: Cần viết một trình phát hoạt ảnh (Animation Player) tùy chỉnh bằng C++ sử dụng LVGL và bộ đệm PSRAM để đạt được tốc độ khung hình (FPS) mượt mà.
