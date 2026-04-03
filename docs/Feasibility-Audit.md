# BÁO CÁO ĐÁNH GIÁ TÍNH KHẢ THI
## Dự án: Chuyển đổi VPet-Simulator sang ESP32-S3

*Ngày đánh giá: 2026-04-02*
*Phiên bản Porting-Map được đánh giá: v1*

---

## 1. TỔNG QUAN ĐÁNH GIÁ

| Hạng mục | Điểm | Ghi chú |
|:---|:---:|:---|
| **Tính đầy đủ của Porting-Map** | 6/10 | Còn nhiều lỗ hổng nghiêm trọng (xem mục 2) |
| **Khả năng thành công của phần cứng** | 8/10 | Phần cứng đủ mạnh, nhưng băng thông SPI là nút thắt cổ chai |
| **Khả năng thành công của phần mềm** | 5/10 | Phân hệ hoạt ảnh cực kỳ phức tạp, cần thiết kế lại từ đầu |
| **Đánh giá tổng thể** | **CÓ THỂ THÀNH CÔNG** nhưng cần sửa đổi kế hoạch đáng kể |

---

## 2. CÁC LỖ HỔNG NGHIÊM TRỌNG TRONG PORTING-MAP HIỆN TẠI

### 🔴 Lỗ hổng 1: Thiếu hoàn toàn hệ thống Enum `GraphType` (22 loại hoạt ảnh)
**Vấn đề:** Porting-Map chỉ liệt kê vài phương thức hiển thị (`DisplayTouchHead`, `DisplaySleep`...) nhưng **không hề nhắc đến** bảng phân loại 22 loại hoạt ảnh cốt lõi được định nghĩa trong `GraphInfo.cs` dòng 153-248.

Danh sách **đầy đủ** các loại hoạt ảnh mà ESP32 phải hỗ trợ:
| # | GraphType | Ý nghĩa | Cần cho ESP32? |
|:--|:---|:---|:---:|
| 1 | `Common` | Hoạt ảnh chung (MOD gọi) | ❌ Bỏ qua |
| 2 | `Raised_Dynamic` | Bị nhấc lên (động) | ✅ Bắt buộc |
| 3 | `Raised_Static` | Bị nhấc lên (tĩnh) | ✅ Bắt buộc |
| 4 | `Move` | Di chuyển trên màn hình | ⚠️ Tùy chọn (màn 4 inch nhỏ) |
| 5 | `Default` | Thở / Đứng yên | ✅ Bắt buộc |
| 6 | `Touch_Head` | Vuốt ve đầu | ✅ Bắt buộc |
| 7 | `Touch_Body` | Chạm vào thân | ✅ Bắt buộc |
| 8 | `Idel` | Rảnh rỗi (ngẫu nhiên) | ✅ Bắt buộc |
| 9 | `Sleep` | Ngủ | ✅ Bắt buộc |
| 10 | `Say` | Nói chuyện | ✅ Bắt buộc |
| 11 | `StateONE` | Trạng thái chờ 1 | ✅ Bắt buộc |
| 12 | `StateTWO` | Trạng thái chờ 2 | ✅ Bắt buộc |
| 13 | `StartUP` | Khởi động | ✅ Bắt buộc |
| 14 | `Shutdown` | Tắt máy | ⚠️ Tùy chọn |
| 15 | `Work` | Làm việc / Học | ✅ Bắt buộc |
| 16 | `Switch_Up` | Chuyển trạng thái lên | ✅ Bắt buộc |
| 17 | `Switch_Down` | Chuyển trạng thái xuống | ✅ Bắt buộc |
| 18 | `Switch_Thirsty` | Biểu hiện khát | ✅ Bắt buộc |
| 19 | `Switch_Hunger` | Biểu hiện đói | ✅ Bắt buộc |
| 20-23 | `SideHide_*` | Trốn ở mép màn hình | ❌ Bỏ qua (không có mép) |

**Hệ quả:** Nếu không port đủ 15+ loại hoạt ảnh bắt buộc, con Pet sẽ chỉ biết đứng yên — mất hết "linh hồn".

### 🔴 Lỗ hổng 2: Thiếu hệ thống `AnimatType` (Chuỗi A→B→C)
**Vấn đề:** Mỗi hoạt ảnh trong VPet không phải chỉ là một tệp ảnh. Nó là **chuỗi 3 bước**:
- `A_Start` → Hoạt ảnh bắt đầu (ví dụ: Pet cúi đầu xuống chuẩn bị ngủ)
- `B_Loop` → Hoạt ảnh lặp lại (ví dụ: Pet ngáy)
- `C_End` → Hoạt ảnh kết thúc (ví dụ: Pet ngẩng đầu lên tỉnh dậy)

Porting-Map hiện tại chỉ nói "Port hàm `DisplaySleep`" nhưng **không đề cập** rằng phải xây dựng cả một **State Machine (Máy trạng thái) trong C++** để điều phối chuỗi A→B→C này.

### 🔴 Lỗ hổng 3: Thiếu file `IController.cs` — Bộ điều phối chính
**Vấn đề:** File `IController.cs` định nghĩa giao diện điều khiển chính của Pet (di chuyển, kiểm tra vị trí, tỷ lệ zoom, chu kỳ tương tác). Porting-Map bỏ sót hoàn toàn file này.

Trên ESP32, chúng ta cần viết lại `IController` thành một class cụ thể quản lý:
- `ZoomRatio` → Tỷ lệ co giãn Pet (cố định = 320/1000 trên ESP32)
- `PressLength` → Bao nhiêu ms thì tính là "nhấn giữ" trên cảm ứng
- `InteractionCycle` → Chu kỳ tương tác (quyết định tần suất Pet tự làm hành vi ngẫu nhiên)

### 🔴 Lỗ hổng 4: Thiếu file `SayInfo.cs` — Hệ thống hội thoại
**Vấn đề:** File `SayInfo.cs` với lớp `SayInfoWithStream` xử lý **stream text** (hiển thị chữ từ từ như đang gõ). Đây là tính năng rất quan trọng để Pet "trò chuyện" với người dùng. Porting-Map bỏ sót.

### 🔴 Lỗ hổng 5: Thiếu hệ thống `GraphHelper.cs` — Định nghĩa `Work` và `Move`
**Vấn đề:** File `GraphHelper.cs` (22KB) chứa các class cốt lõi:
- `Work`: Định nghĩa công việc (Work/Study/Play) với các thông số tiêu hao thức ăn/nước uống và tiền lương.
- `Move`: Định nghĩa các kiểu di chuyển của Pet.

Porting-Map chưa hề đề cập đến việc port 2 class này.

### 🟡 Lỗ hổng 6: Chưa có kế hoạch xử lý `info.lps` (Metadata hoạt ảnh)
**Vấn đề:** Trong `PetLoader.cs`, mỗi thư mục hoạt ảnh chứa một file `info.lps` mô tả siêu dữ liệu (loại hoạt ảnh, tốc độ khung hình, có lặp không...). ESP32 cần một **bộ phân tích cú pháp (parser) cho file LPS** hoặc phải chuyển đổi sang JSON/Binary trước khi nạp vào thẻ nhớ.

---

## 3. PHÂN TÍCH RỦI RO KỸ THUẬT CHÍNH

### ⚡ Rủi ro 1: Băng thông SPI không đủ cho 60FPS (RỦI RO CAO)

**Dữ liệu thực tế:**
- Một frame RGB565 kích thước 320×320 = `320 × 320 × 2 bytes = 204,800 bytes ≈ 200KB`
- SPI chạy ở 80MHz ≈ 10MB/s lý thuyết (thực tế ~6-7MB/s)
- Để đạt 30FPS: cần `200KB × 30 = 6MB/s` → **vừa khít giới hạn SPI**
- Để đạt 60FPS: cần `12MB/s` → **KHÔNG KHẢ THI** trên giao diện SPI

**Kết luận:** Mục tiêu thực tế là **24-30 FPS**, không phải 60 FPS như tài liệu ADR nêu.

**Giải pháp giảm thiểu:**
- Sử dụng **Partial Refresh** (chỉ vẽ lại phần thay đổi, không vẽ lại toàn bộ frame)
- Dùng DMA transfer để CPU rảnh tay xử lý logic trong khi SPI đang truyền dữ liệu

### ⚡ Rủi ro 2: Dung lượng ảnh trên thẻ nhớ (RỦI RO TRUNG BÌNH)

**Dữ liệu thực tế đo được từ codebase:**
- Tổng: **5,643 file PNG**, kích thước gốc **1000×1000 pixels**, tổng dung lượng **755 MB**
- Sau khi resize về 320×320 và chuyển sang RGB565 raw: mỗi ảnh ≈ 200KB
- Tổng dung lượng RGB565: `5643 × 200KB ≈ 1.1 GB`

**Kết luận:** Cần thẻ nhớ **tối thiểu 2GB**. Có thể giảm bằng cách nén RLE hoặc chỉ port một phần hoạt ảnh.

### ⚡ Rủi ro 3: Bộ nhớ PSRAM chỉ đủ cho 2-3 frame cùng lúc (RỦI RO CAO)

**Phân tích:**
- PSRAM tổng: 8MB
- LVGL cần ~1MB cho hệ thống và draw buffer
- Mỗi frame 320×320 RGB565 = 200KB
- Còn lại cho animation buffer: ~6MB → tối đa ~30 frame

**Kết luận:** Không thể preload toàn bộ animation vào PSRAM như bản PC. Phải dùng chiến lược **Streaming**: đọc 2-3 frame trước từ SD Card, hiển thị xong thì đọc tiếp. Đây là thay đổi kiến trúc lớn nhất so với bản gốc.

---

## 4. NHỮNG GÌ PORTING-MAP ĐÃ LÀM TỐT

- ✅ Xác định đúng 3 phân hệ chính (Handle, Graph, Display)
- ✅ Liệt kê đúng các phương thức cốt lõi trong `GameSave.cs`
- ✅ Nhận diện đúng rằng `PNGAnimation.cs` cần "đập đi xây lại"
- ✅ Quy trình đánh dấu `[PORTED_TO_ESP32]` rất hay và thực tiễn

---

## 5. CÁC MỤC CẦN BỔ SUNG VÀO PORTING-MAP

### Phân hệ 0 (MỚI): Tiền xử lý dữ liệu (Data Pipeline)
- [ ] Script Python: Resize 5,643 ảnh PNG từ 1000×1000 → 320×320
- [ ] Script Python: Convert PNG → RGB565 binary (.bin)
- [ ] Script Python: Parse `info.lps` → tạo file JSON/Binary index cho mỗi animation
- [ ] Ước tính & kiểm tra dung lượng thẻ nhớ SD

### Bổ sung vào Phân hệ 1 (Handle)
- [ ] Port `IController.cs` → `ESP32Controller.cpp` (ZoomRatio, PressLength, InteractionCycle)
- [ ] Port `SayInfo.cs` → `SayInfo.cpp` (Chỉ cần phiên bản đơn giản, bỏ stream)
- [ ] Port `GraphHelper.cs::Work` → `Work.hpp` (Thông số công việc)

### Bổ sung vào Phân hệ 2 (Graph)
- [ ] Port đầy đủ enum `GraphType` (22 loại) và enum `AnimatType` (4 loại) từ `GraphInfo.cs`
- [ ] Port `GraphCore.Config` → bộ đọc cấu hình `TouchHead/TouchBody/RaisePoint` từ file nhúng
- [ ] Thiết kế **Animation State Machine** (A→B→C) trên C++/LVGL
- [ ] Thiết kế **SD Card Streaming Engine** (đọc frame từ SD, giải mã, đẩy lên LVGL)
- [ ] Port `Picture.cs` → `StaticImage.cpp` (hiển thị ảnh tĩnh đơn)

### Bổ sung vào Phân hệ 3 (Display)
- [ ] Port logic `FunctionSpend()` trong `MainLogic.cs` (rất dài, ~200 dòng logic sinh tồn)
- [ ] Port `EventTimer_Elapsed()` → Game Loop chính trên ESP32
- [ ] Port `PlaySwitchAnimat()` → Logic chuyển đổi animation khi trạng thái thay đổi
- [ ] Port `WorkTimer.xaml.cs` → `WorkTimer.cpp` (Hệ thống đếm giờ làm việc)
- [ ] Port `ToolBar.xaml.cs` → UI bằng LVGL hiển thị thanh trạng thái

---

## 6. KẾT LUẬN VÀ KHUYẾN NGHỊ

### Dự án CÓ THỂ thành công nếu tuân thủ 3 điều kiện:

1. **Chấp nhận giới hạn FPS:** Mục tiêu thực tế là 24-30 FPS, không phải 60 FPS. Điều này vẫn đủ mượt cho animation của Pet.

2. **Chia nhỏ dự án thành 3 mốc (Milestone):**
   - **MVP1 (Tuần 1-2):** Pet đứng thở + Hiển thị thanh trạng thái. Chỉ cần 1 animation `Default` chạy ổn.
   - **MVP2 (Tuần 3-4):** Xoa đầu + Ăn uống + Trạng thái sinh tồn. Thêm `Touch_Head`, `EatFood`.
   - **MVP3 (Tuần 5+):** Toàn bộ hoạt ảnh, Work, Sleep, Idle random.

3. **Cập nhật Porting-Map** với tất cả các mục bổ sung ở mục 5 trước khi bắt tay vào code.

### Rủi ro lớn nhất: **SD Card Streaming Engine**
Đây là thành phần chưa hề tồn tại trong bản PC gốc (PC load tất cả vào RAM). Chúng ta phải tự thiết kế và viết từ đầu. Nếu engine này chậm → Pet giật lag. Nếu nhanh → dự án thành công.
