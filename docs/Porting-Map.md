# BẢN ĐỒ CHUYỂN ĐỔI CHI TIẾT VPET (C# → ESP32-S3 C++)
*Phiên bản 2.0 — Cập nhật sau đánh giá khả thi (Feasibility-Audit.md)*

> [!IMPORTANT]
> Tài liệu này là **nguồn sự thật duy nhất (Single Source of Truth)** cho toàn bộ dự án chuyển đổi.
> Mỗi ô `[ ]` là một đơn vị công việc. Khi hoàn thành, đánh dấu `[x]` ở đây VÀ thêm `// [PORTED_TO_ESP32]` vào file C# gốc.

## Thông tin dự án
- **Ảnh gốc:** 5,643 file PNG, kích thước 1000×1000, tổng 755 MB
- **Ảnh sau convert:** 320×320 RGB565, mỗi ảnh ≈ 200KB, tổng ≈ 1.1 GB
- **FPS mục tiêu thực tế:** 24-30 FPS (do giới hạn SPI 80MHz ≈ 6-7MB/s)
- **PSRAM khả dụng cho animation:** ~6MB (đủ buffer ~30 frame)

---

## 🔧 PHÂN HỆ 0: TIỀN XỬ LÝ DỮ LIỆU (Data Pipeline)
*Chạy trên PC, hoàn toàn độc lập với phần cứng ESP32. Nên làm TRƯỚC TIÊN.*

### Script Python: Xử lý ảnh
- [x] `pipeline.py`: Duyệt đệ quy thư mục `VPet-Simulator.Windows/mod/0000_core/pet/vup`, resize từ 1000×1000 → 320×320 bằng thuật toán Lanczos
- [x] `pipeline.py`: Chuyển đổi PNG → file `.bin` (RGB565 little-endian, 2 byte/pixel, không header)
- [x] Kiểm tra dung lượng tổng sau convert, đảm bảo vừa thẻ nhớ MicroSD (Size file .bin chuẩn 204.8KB)

### Script Python: Xử lý metadata hoạt ảnh
- [x] `pipeline.py`: Phân tích tên file PNG chứa FPS thay vì info.lps (chạy chung trong pipeline) xuất ra file `manifest.json / idx` cho mỗi animation
  - Thông tin cần trích: loại graph (`pnganimation`/`picture`/`foodanimation`), có lặp không (`loop`), tên, mode, animat type
  - **Nguồn C#:** `PetLoader.cs` dòng 81-110 — logic đọc `info.lps`
- [x] `pipeline.py`: Tổng hợp thành `animations.idx` chứa `[GraphType][AnimatType][ModeType] → thư mục + số frame + thời gian mỗi frame`

### Script Python: Xử lý cấu hình Pet
- [x] Dùng JSON Config (pet_config.json) chứa các Box tọa độ tĩnh hoặc tự định nghĩa trên Touch Driver.

---

## 🧠 PHÂN HỆ 1: HANDLE — Logic Lõi & Dữ liệu Sinh tồn
*Bộ não của Pet. Hoàn toàn độc lập với phần cứng hiển thị.*

### 1A. Tệp `Handle/IGameSave.cs` → `include/GameSave.hpp`
Định nghĩa giao diện (interface) và enum trạng thái của Pet.

**Enum `ModeType` (4 trạng thái):**
- [x] `Happy` — Vui vẻ *(Đã port)*
- [x] `Nomal` — Bình thường *(Đã port)*
- [x] `PoorCondition` — Sức khỏe kém *(Đã port)*
- [x] `Ill` — Ốm/Bệnh *(Đã port)*

**Thuộc tính cần port:**
- [x] `Name` (string) — Tên Pet *(→ GameSave.hpp char name[32])*
- [x] `HostName` (string) — Tên chủ nhân *(→ GameSave.hpp char hostName[32])*
- [x] `Money` (double) — Tiền tệ
- [x] `Exp` (double) — Kinh nghiệm
- [x] `ExpBonus` (double) — Hệ số cộng thêm Exp *(→ getExpBonus())*
- [x] `Level` (int, computed) — Cấp độ = `sqrt(Exp) / 10 + 1` *(→ getLevel())*
- [x] `Strength` (double, 0-100) — Thể lực *(→ setStrength() với clamp)*
- [x] `StrengthMax` (double) — Thể lực tối đa (cố định 100) *(→ STRENGTH_MAX)*
- [x] `StrengthFood` (double, 0-100) — Độ no *(→ setStrengthFood() trừ Health khi <=0)*
- [x] `StrengthDrink` (double, 0-100) — Độ khát *(→ setStrengthDrink() trừ Health khi <=0)*
- [x] `Feeling` (double, 0-100) — Cảm xúc *(→ setFeeling() trừ Health+Likability khi <=0)*
- [x] `FeelingMax` (double) — Cảm xúc tối đa *(→ FEELING_MAX)*
- [x] `Health` (double, 0-100) — Sức khỏe (ẩn) *(→ setHealth() với clamp)*
- [x] `Likability` (double) — Thân thiết (ẩn) *(→ setLikability() overflow vào Health)*
- [x] `LikabilityMax` (double, computed) — Thân thiết tối đa *(→ getLikabilityMax())*
- [x] `StoreStrength` / `StoreStrengthFood` / `StoreStrengthDrink` — Bộ đệm hồi phục từ từ
- [x] `ChangeStrength` / `ChangeStrengthFood` / `ChangeStrengthDrink` / `ChangeFeeling` — Bộ đệm biến động

### 1B. Tệp `Handle/GameSave.cs` → `src/GameSave.cpp`
Triển khai (Implementation) logic tính toán các chỉ số.

**Phương thức đã port:**
- [x] `CalMode()` → `GameSave::calculateMode()` — Xác định trạng thái Vui/Bệnh dựa trên Health, Feeling, Likability
- [x] `StrengthChange(value)` → `GameSave::strengthChange()` — 2026-04-02
- [x] `StrengthChangeFood(value)` → `GameSave::strengthChangeFood()` — 2026-04-02
- [x] `StrengthChangeDrink(value)` → `GameSave::strengthChangeDrink()` — 2026-04-02
- [x] `FeelingChange(value)` → `GameSave::feelingChange()` — 2026-04-02
- [x] `CleanChange()` → `GameSave::cleanChange()` — 2026-04-02
- [x] `StoreTake()` → `GameSave::storeTake()` — 2026-04-02
- [x] `EatFood(IFood food)` → `GameSave::eatFood()` — 2026-04-02
- [x] `LevelUpNeed()` → `GameSave::levelUpNeed()` — 2026-04-02

**Phương thức chưa port:**
- [ ] `ToLine()` / `Load()` — Lưu/Đọc game state → Chuyển sang EEPROM hoặc file trên SD Card *(để Milestone cuối)*

### 1C. Tệp `Handle/IFood.cs` → `include/GameSave.hpp` (struct Food)
Cấu trúc dữ liệu thức ăn.

- [x] `struct Food` với các trường: `exp`, `strength`, `strengthFood`, `strengthDrink`, `feeling`, `health`, `likability` *(→ struct Food trong GameSave.hpp)*
- [ ] Danh sách thức ăn mặc định (hardcode hoặc đọc từ file trên SD Card)

### 1D. Tệp `Handle/GameCore.cs` → `include/GameCore.hpp`
Bộ chứa (Container) kết nối tất cả thành phần.

- [x] Class `GameCore` chứa: `ESP32Controller*`, `GraphCore*`, `GameSave*`, `TouchArea[]` *(→ GameCore.hpp)* — 2026-04-03
- [x] Class `TouchArea`: Kiểm tra hitbox cảm ứng `touch(px, py)` *(→ GameCore.hpp)* — 2026-04-03
  - **Logic C#:** `return inx >= 0 && inx <= Size.Width && iny >= 0 && iny <= Size.Height;` — ✅ Giữ nguyên
- [x] `GameCore::processTouchAt(px, py, isLongPress)` — Dispatch touch tới tất cả vùng *(→ GameCore.hpp)* — 2026-04-03

### 1E. Tệp `Handle/IController.cs` → `include/GameCore.hpp` (struct ESP32Controller)
Bộ điều phối chính — đã gộp vào GameCore.hpp thay vì file riêng.

- [x] `ZoomRatio` → Cố định = `320.0f / 1000.0f` *(→ ESP32Controller::ZOOM_RATIO constexpr)* — 2026-04-03
- [x] `PressLength` → Mặc định 500ms *(→ ESP32Controller::pressLength)* — 2026-04-03
- [x] `InteractionCycle` → Mặc định 20 *(→ ESP32Controller::interactionCycle)* — 2026-04-03
- [x] `EnableFunction` → Bật/tắt sinh tồn *(→ ESP32Controller::enableFunction)* — 2026-04-03
- [x] Các hàm `GetWindowsDistance*` → Ghi rõ KHÔNG CẦN trong comment *(→ GameCore.hpp)* — 2026-04-03

### 1F. Tệp `Handle/SayInfo.cs` → `include/GameCore.hpp` (struct SayInfo)
Hệ thống hội thoại — đã đơn giản hóa và gộp vào GameCore.hpp.

- [x] Class `SayInfo` (đơn giản): `text[128]` + `graphName[32]` + `force` *(→ GameCore.hpp)* — 2026-04-03
- [x] Bỏ qua `SayInfoWithStream` — ✅ Đã ghi rõ trong comment
- [ ] Danh sách câu nói ngẫu nhiên lưu trên SD Card (file `sayings.txt`) *(để Milestone cuối)*

### 1G. Tệp `Handle/Function.cs` → `include/Utils.hpp`
Các hàm tiện ích — đã gộp vào Utils.hpp.

- [x] `Function.Rnd` → `Utils::randomInt(max)` dùng `esp_random()` *(→ Utils.hpp)* — 2026-04-03
- [x] `MemoryUsage()` → `Utils::memoryUsage()` dùng `ESP.getHeapSize()-getFreeHeap()` — 2026-04-03
- [x] `MemoryAvailable()` → `Utils::memoryAvailable()` dùng `ESP.getFreePsram()` — 2026-04-03
- [x] `ComCheck(string)` → `Utils::comCheck(const char*)` đếm dấu câu — 2026-04-03
- [x] Bỏ qua: `HEXToColor`, `ColorToHEX`, `ResourcesBrush`, `BrushType` *(WPF-only)* — ✅

---


## 🎞️ PHÂN HỆ 2: GRAPH — Hệ thống Hoạt ảnh
*Khu vực phức tạp nhất. Cần thiết kế lại kiến trúc hoàn toàn.*

### 2A. Tệp `Graph/GraphInfo.cs` → `include/GraphInfo.hpp` *(MỚI)*

**Enum `GraphType` — 22 loại hoạt ảnh (BẮT BUỘC port đầy đủ):**
| # | Tên | Ý nghĩa | Port? | Ghi chú |
|:--|:---|:---|:---:|:---|
| 0 | `Common` | Hoạt ảnh chung (MOD) | ❌ | Bỏ qua |
| 1 | `Raised_Dynamic` | Bị nhấc (động) | ✅ | |
| 2 | `Raised_Static` | Bị nhấc (tĩnh, A→B→C) | ✅ | |
| 3 | `Move` | Di chuyển | ⚠️ | Tùy chọn, màn 4 inch nhỏ |
| 4 | `Default` | Thở / Đứng yên | ✅ | **Ưu tiên cao nhất** |
| 5 | `Touch_Head` | Vuốt ve đầu (A→B→C) | ✅ | |
| 6 | `Touch_Body` | Chạm thân (A→B→C) | ✅ | |
| 7 | `Idel` | Rảnh rỗi / Ngẫu nhiên (A→B→C) | ✅ | |
| 8 | `Sleep` | Ngủ (A→B→C) | ✅ | |
| 9 | `Say` | Nói chuyện (A→B→C) | ✅ | |
| 10 | `StateONE` | Chờ ngẫu nhiên 1 (A→B→C) | ✅ | |
| 11 | `StateTWO` | Chờ ngẫu nhiên 2 (A→B→C) | ✅ | |
| 12 | `StartUP` | Khởi động | ✅ | Chạy 1 lần khi bật |
| 13 | `Shutdown` | Tắt máy | ⚠️ | Tùy chọn |
| 14 | `Work` | Làm việc / Học (A→B→C) | ✅ | |
| 15 | `Switch_Up` | Chuyển trạng thái ↑ | ✅ | Khi Pet vui lên |
| 16 | `Switch_Down` | Chuyển trạng thái ↓ | ✅ | Khi Pet buồn đi |
| 17 | `Switch_Thirsty` | Biểu hiện khát | ✅ | |
| 18 | `Switch_Hunger` | Biểu hiện đói | ✅ | |
| 19-22 | `SideHide_*` | Trốn ở mép màn hình | ❌ | Không phù hợp ESP32 |

- [x] Khai báo enum `GraphType` trong C++ với tất cả 23 giá trị *(→ GraphInfo.hpp, bao gồm GRAPH_TYPE_COUNT)* — 2026-04-03
- [ ] Logic fallback: Nếu không tìm thấy animation cho ModeType hiện tại → tìm mode lân cận (logic dòng 114-137 trong `GraphCore.cs`)

**Enum `AnimatType` — 4 giai đoạn của mỗi hoạt ảnh:**
- [x] `Single` — Hoạt ảnh chỉ có 1 phần duy nhất *(→ GraphInfo.hpp)* — 2026-04-03
- [x] `A_Start` — Giai đoạn bắt đầu *(→ GraphInfo.hpp)* — 2026-04-03
- [x] `B_Loop` — Giai đoạn lặp *(→ GraphInfo.hpp)* — 2026-04-03
- [x] `C_End` — Giai đoạn kết thúc *(→ GraphInfo.hpp)* — 2026-04-03

### 2B. Tệp `Graph/GraphCore.cs` → `include/GraphCore.hpp`
Từ điển hoạt ảnh — Bộ não tìm kiếm animation.

- [x] Cấu trúc dữ liệu: `AnimationEntry[]` phẳng + chỉ mục `graphsName[GraphType]` *(→ GraphCore.hpp)* — 2026-04-03
  - Lý do dùng mảng phẳng thay `std::map`: không có overhead node, linear scan < 200 phần tử ≈ 10µs
- [x] `findName(GraphType)` → Tìm tên ngẫu nhiên theo loại *(→ GraphCore.hpp)* — 2026-04-03
- [x] `findGraph(name, animat, mode)` → Fallback 4 tầng *(→ GraphCore.hpp)* — 2026-04-03
  - ✅ **Khớp 100% C# dòng 101-140**: Tầng1(mode) → Tầng2(Ill=null) → Tầng3(mode+1) → Tầng4(mode-1) → Tầng5(any≠Ill)
- [x] `GraphConfig.touchHead*`, `touchBody*` → Vùng cảm ứng đầu/thân *(→ GraphCore.hpp)* — 2026-04-03
- [x] `GraphConfig.touchRaised*[4]`, `raisePoint*[4]` → Điểm nhấc 4 trạng thái *(→ GraphCore.hpp)* — 2026-04-03
- [x] `GraphConfig.works[]` → Danh sách công việc (MAX 8) *(→ GraphCore.hpp)* — 2026-04-03
- [x] `GraphConfig.getDuration(name)` → Mặc định = 10 giống C# *(→ GraphCore.hpp)* — 2026-04-03

### 2C. Tệp `Graph/PNGAnimation.cs` + `IGraph.cs` + `Picture.cs` → `include/AnimationPlayer.hpp` + `src/AnimationPlayer.cpp` *(VIẾT MỚI)*
**SD Card Streaming Engine — thiết kế hoàn toàn mới cho ESP32.**

Kiến trúc: **Double Buffering trên PSRAM**
```
SD Card → [bufferA/B in PSRAM] → [lv_img widget] → SPI DMA → Màn hình
              ^                        |
              |                        v
         Đọc frame tiếp       Hiển thị frame hiện tại
         (preload)             (swap khi đến thời gian)
```

- [x] `TaskControl` struct — 4 trạng thái điều khiển *(port từ IGraph.cs dòng 87-139, khớp 100%)* — 2026-04-03
- [x] `FrameInfo` struct — duration + fileIndex mỗi frame *(port từ PNGAnimation.Animation)* — 2026-04-03
- [x] `AnimationPlayer::load(path)` — Mở thư mục SD, parse tên file → frame info — 2026-04-03
  - ✅ Parse `000_150.bin` → index=0, duration=150ms *(khớp C# dòng 184-186)*
- [x] `AnimationPlayer::start(isLoop)` — Cấp phát 2 buffer PSRAM, đọc frame 0 — 2026-04-03
- [x] `AnimationPlayer::tick()` — Non-blocking frame advance *(port logic từ Animation::Run dòng 230-268)* — 2026-04-03
  - ✅ **Khớp C#**: Stop→EndAction | Stoped→return | StatusQuo→loop/end | Continue→reset
- [x] `AnimationPlayer::stop()` — Giải phóng PSRAM buffer — 2026-04-03
- [x] **Double Buffering:** bufferA (hiển thị) / bufferB (preload) trên PSRAM — 2026-04-03
- [x] **TaskControl logic** (Stop/Continue/Loop) — port từ C# dòng 237-268 — 2026-04-03

### 2D. Tệp `Graph/FoodAnimation.cs` → `src/FoodAnimation.cpp`
Hoạt ảnh ăn uống — chuỗi 3 lớp (front/back/food image).

- [x] `FoodAnimation::load()` — Tải hoạt ảnh: "front" (Pet trước đồ ăn), "back" (Pet sau đồ ăn), "food" (ảnh thức ăn) — 2026-04-03
- [x] `FoodAnimation::run()` — Chạy chuỗi: Đưa đồ ăn → Nhai → Nuốt xong — 2026-04-03
  - **C# gốc:** Dùng 3 lớp Image chồng lên nhau (dòng toàn bộ file)
  - **ESP32:** Dùng `FoodAnimationPlayer` quản lý 2 `AnimationPlayer` (front/back) và logic khung hình cho food — 2026-04-03

### 2E. Tệp `Graph/Picture.cs` → *Gộp vào AnimationPlayer*
Ảnh tĩnh = AnimationPlayer với frameCount=1, isLoop theo config.

- [x] `Picture` → `AnimationPlayer` với 1 frame duy nhất — 2026-04-03
  - Khi thư mục chỉ có 1 file .bin → load() trả frameCount=1 → tick() hoạt động bình thường

### 2F. Tệp `Graph/GraphHelper.cs` → `include/Work.hpp` + `include/Move.hpp` *(MỚI)*
Định nghĩa cấu trúc dữ liệu cho Work và Move.

**Class Work (Công việc):**
- [x] `name` (string) — Tên công việc *(→ WorkInfo.name trong GraphInfo.hpp)* — 2026-04-03
- [x] `type` (enum: Work/Study/Play) — Loại *(→ enum WorkType trong GraphInfo.hpp)* — 2026-04-03
- [x] `moneyBase` (double) — Thu nhập cơ bản *(→ WorkInfo.moneyBase)* — 2026-04-03
- [x] `strengthFood` / `strengthDrink` (double) — Tốc độ tiêu hao đói/khát — 2026-04-03
- [x] `feeling` (double) — Ảnh hưởng cảm xúc — 2026-04-03
- [x] `levelLimit` (int) — Cấp độ tối thiểu cần đạt — 2026-04-03
- [x] `graph` (string) — Tên animation khi làm việc — 2026-04-03
- [x] `time` (int) — Thời gian hoàn thành (phút) — 2026-04-03
- [x] `finishBonus` (double) — Bonus khi hoàn thành — 2026-04-03

**Class Move (Di chuyển):** *(Tùy chọn cho ESP32)*
- [ ] `speed` (double) — Tốc độ di chuyển
- [ ] `type` (enum) — Loại di chuyển

### 2G. Animation State Machine → `include/AnimationStateMachine.hpp` *(THIẾT KẾ MỚI)*
*Tập trung logic rải rác từ MainDisplay.cs (687 dòng) vào 1 struct.*

- [x] State machine A→B→C *(→ AnimationStateMachine.hpp)* — 2026-04-03
  ```
  [IDLE] → trigger → [PLAY_A_START] → xong → [PLAY_B_LOOP] → (loop) → [PLAY_C_END] → xong → [IDLE]
  ```
- [x] Interrupt: Đang B_Loop + cùng type → `setContinue()` *(port dòng 138-152)* — 2026-04-03
- [x] Chuyển trạng thái: `displayToNormal()` dispatch theo WorkingState *(port dòng 28-46)* — 2026-04-03
- [x] `displayTouchHead()` / `displayTouchBody()` — Strength-2, Feeling+1 *(port dòng 128-196)* — 2026-04-03
- [x] `displaySleep(force)` — Force loop B vô hạn *(port dòng 314-325)* — 2026-04-03
- [x] `displayIdel()` — Random pick từ Idel names *(port dòng 260-294)* — 2026-04-03
- [x] `displayStateONE()` — StateONE chain *(port dòng 200-210)* — 2026-04-03
- [x] Loop counting: `Rnd.Next(++looptimes) > duration` *(port dòng 302-308)* — 2026-04-03

---

## 👆 PHÂN HỆ 3: DISPLAY — Hiển thị LVGL & Cảm ứng
*Gắn kết tất cả lại: LVGL vẽ lên màn hình, FT6336U nhận ngón tay.*

### 3A. Tệp `Display/MainDisplay.cs` → `src/PetDisplay.cpp`
Các hàm điều khiển hiển thị — ánh xạ từ WPF sang LVGL. (Gộp vào `AnimationStateMachine.hpp` ở 2G)

**Hiển thị cơ bản:**
- [x] `Display(GraphType, AnimatType, callback)` — Hàm gốc: Tìm animation → Phát → Gọi callback khi xong
- [x] `DisplayToNomal()` → Quay về animation `Default` (thở/đứng yên)
- [x] `DisplayDefault()` → Phát animation mặc định theo mode hiện tại

**Hiển thị tương tác:**
- [x] `DisplayTouchHead()` → Khi xoa đầu: phát `Touch_Head` A→B→C
- [x] `DisplayTouchBody()` → Khi chạm thân: phát `Touch_Body` A→B→C
- [x] `DisplayRaising()` → Khi nhấc Pet (long-press trên cảm ứng)
- [x] `DisplayRaised()` → Pet đang treo lơ lửng

**Hiển thị trạng thái:**
- [x] `DisplaySleep()` → Phát animation `Sleep` A→B→C
- [x] `DisplayIdel()` → Phát animation `Idel` ngẫu nhiên
- [x] `DisplayToIdel_StateONE()` → Phát animation `StateONE` A→B→C
- [x] `DisplayIdel_StateTWO()` → Phát animation `StateTWO` A→B→C

**Logic loop:**
- [x] `DisplayBLoopingToNomal(duration)` → Lặp B_Loop trong `duration` giây rồi chuyển sang C_End → Normal
- [x] `DisplayBLoopingForce(name)` → Lặp B_Loop vô hạn cho đến khi có sự kiện mới
- [x] `DisplayCEndtoNomal()` → Phát C_End rồi quay về Normal
- [x] `DisplayStopForce()` → Dừng mọi thứ ngay lập tức

**Di chuyển:** *(Tùy chọn)*
- [x] `DisplayMove()` → Pet di chuyển trên màn hình (nếu implement)
- [x] `DisplayToMove()` → Khởi tạo di chuyển

### 3B. Tệp `Display/MainLogic.cs` → `src/GameLoop.cpp` *(CỐT LÕI)*
Vòng lặp chính (Game Loop) — "nhịp tim" của toàn bộ hệ thống.

**`FunctionSpend(TimePass)` — Hàm tính toán sinh tồn (200 dòng logic):**
Phải port **chính xác** vì đây là "bộ não vận hành" của Pet.

- [x] **Trạng thái Normal (dòng 349-381):**
  - Nếu `StrengthFood >= 50%`: trừ food, cộng strength (chuyển hóa)
  - Nếu `StrengthFood <= 25%`: trừ Health, trừ thêm health bonus
  - Nếu `StrengthDrink >= 50%`: tương tự food
  - Nếu `StrengthDrink <= 25%`: trừ Health
  - Luôn trừ food và drink theo thời gian
  - Trừ Feeling theo thời gian rảnh (`freedrop`)

- [x] **Trạng thái Sleep (dòng 252-270):**
  - Hồi phục strength ×2, food ×1, drink ×1
  - Nếu food/drink thấp: hồi phục gấp đôi
  - Nếu food/drink cao: cộng thêm health

- [x] **Trạng thái Work (dòng 271-348):**
  - Tính efficiency dựa trên mức food/drink còn lại
  - Nếu strength > 25%: dùng strength để giảm tiêu hao food/drink
  - Tính addmoney = `TimePass * Work.MoneyBase * (2*efficiency - 0.5)`
  - Nếu Work → cộng Money; nếu Study → cộng Exp
  - Nếu Play → reset LastInteractionTime

- [x] **Bonus cuối (dòng 388-412):**
  - Luôn cộng Exp += TimePass
  - Nếu Feeling >= 90%: cộng Likability
  - Nếu Feeling >= 75%: cộng thêm Exp×2 và Health
  - Nếu Feeling <= 25%: trừ Likability và Exp
  - Gọi `CalMode()` để kiểm tra chuyển trạng thái

**`EventTimer_Elapsed()` — Timer chính (dòng 470-533):**
- [x] Gọi `FunctionSpend(0.05)` mỗi 15 giây
- [x] Logic hành vi ngẫu nhiên khi Pet rảnh (`IsIdel`):
  - Random 0-2: Di chuyển
  - Random 3-5: Idle animation
  - Random 6: StateONE
  - Random 7: Sleep
  - Random 8-10: Hành vi tùy chỉnh

**`PlaySwitchAnimat(before, after)` — Chuyển đổi animation khi mode thay đổi (dòng 432-453):**
- [x] Nếu mode tăng (xấu đi): phát `Switch_Down` lần lượt qua từng bậc
- [x] Nếu mode giảm (tốt lên): phát `Switch_Up` lần lượt qua từng bậc
- [x] Đệ quy cho đến khi `before == after`

**Khác:**
- [x] `Say(text)` / `SayRnd(text)` → Hiển thị text dưới dạng `lv_label` gần Pet
- [x] `LabelDisplayShow(text, time)` → Popup thông báo tạm thời ("+10 Exp", "Đói quá!")
- [x] `StartWork(work)` → Bắt đầu công việc nếu đủ level và không bị ốm
- [x] `WorkingState` enum: `Normal`, `Work`, `Sleep`, `Travel`, `Empty`

### 3C. Tệp `Display/WorkTimer.xaml.cs` → `src/WorkTimer.cpp`
Bộ đếm giờ làm việc. (Thực tế là `src/WorkTimerDisplay.cpp/hpp`)

- [x] `Start(work)` → Bắt đầu đếm thời gian, hiển thị trạng thái trên UI — 2026-04-03
- [x] `Stop(reason)` → Dừng công việc, tính toán thu nhập cuối — 2026-04-03
- [x] `DisplayUI()` → Thay bằng `tick()` và `updateUI()` của LVGL bên cạnh Pet — 2026-04-03

### 3D. Tệp `Display/ToolBar.xaml.cs` → `src/StatusBar.cpp`
Thanh trạng thái — hiển thị ở 2 bên Pet (80px mỗi bên).

- [x] Thanh Hunger (Đói) — `PgbHunger` — 2026-04-03
- [x] Thanh Thirst (Khát) — `PgbThirsty` — 2026-04-03  
- [x] Thanh Spirit (Cảm xúc) — `PgbSpirit` — 2026-04-03
- [x] Thanh Strength (Thể lực) — `PgbStrength` — 2026-04-03
- [x] Thanh Experience (Kinh nghiệm) — `PgbExperience` — 2026-04-03
- [x] Hiển thị Level và Tên Pet — 2026-04-03
- [x] Menu: Nút Ăn, Uống, Ngủ, Làm việc (dùng `lv_btn` + `lv_label`) — 2026-04-03

### 3E. Tệp `Display/MessageBar.xaml.cs` → `src/MessageBubble.cpp`
Bong bóng chat — hiển thị khi Pet "nói chuyện".

- [x] `Show(name, text)` → Hiển thị bong bóng text trên Pet — 2026-04-03
- [x] Tự động ẩn sau N giây (dùng `lv_timer` / `millis()`) — 2026-04-03
- [x] Hiệu ứng fade-in/fade-out (dùng `lv_anim`) — 2026-04-03

---

## 📦 PHÂN HỆ 4: DRIVER PHẦN CỨNG
*Cấu hình thấp cấp — chỉ cần làm 1 lần khi có phần cứng.*

### 4A. Màn hình MSP4031 (ST7796S via SPI)
- [x] Cấu hình `User_Setup.h` cho TFT_eSPI *(Đã tạo xong)*
- [x] Test hiển thị cơ bản (fill màu, vẽ text) qua LVGL `main.cpp`
- [x] Cấu hình Buffer push DMA transfer để giải phóng CPU

### 4B. Cảm ứng FT6336U (I2C)
- [x] Cập nhật module Wire (I2C) giả lập FT6336U
- [x] Đọc tọa độ (x, y) qua register 0x02, 0x03, 0x04
- [x] Đăng ký callback `my_touchpad_read` với LVGL (`lv_indev_drv_t`)
- [x] Ánh xạ tọa độ cảm ứng → vùng TouchHead / TouchBody / RaisePoint (bằng event lvgl)

### 4C. SD Card (SPI)
- [x] Khởi tạo SD card trên bus SPI riêng với tần số >20MHz
- [x] Sẵn sàng cho AnimationPlayer đọc stream
- [x] Đã cấu hình khởi tạo thẻ thẻ trong `setup`

### 4D. Lưu trữ Game State
- [x] Khởi tạo khung `gameSave` cứng trên RAM, chờ test flash/NVS
- [x] Auto-save mỗi 5 phút (Ghi dữ liệu nhị phân xuống SD Card)
- [x] Đọc lại khi khởi động (Load SD Card)

---

## 🗓️ LỘ TRÌNH MILESTONE

### MVP 1 — "Pet Sống" (Tuần 1-2, không cần phần cứng)
- [ ] Script Python chuyển đổi ảnh hoàn chỉnh
- [ ] Port toàn bộ GameSave (Phân hệ 1A + 1B)
- [ ] Port GraphInfo enums (Phân hệ 2A)
- [ ] Viết unit test trên PC kiểm tra logic sinh tồn

### MVP 2 — "Pet Thở" (Tuần 3-4, cần phần cứng)
- [ ] Driver màn hình + cảm ứng (Phân hệ 4)
- [ ] AnimationPlayer đọc SD Card (Phân hệ 2C)
- [ ] Hiển thị 1 animation `Default` (thở) chạy loop trên màn hình thật
- [ ] Thanh trạng thái cơ bản (Phân hệ 3D)

### MVP 3 — "Pet Tương tác" (Tuần 5-6)
- [ ] State Machine A→B→C (Phân hệ 2G)
- [ ] TouchHead + TouchBody phản hồi cảm ứng
- [ ] EatFood + Thanh trạng thái cập nhật real-time
- [ ] Game Loop `EventTimer_Elapsed` + `FunctionSpend` (Phân hệ 3B)

### MVP 4 — "Pet Hoàn chỉnh" (Tuần 7+)
- [ ] Toàn bộ 15+ loại GraphType
- [ ] Work/Study/Play system
- [ ] Sleep + Idle random
- [ ] Message Bubble
- [ ] Auto-save & Load

---

> [!CAUTION]
> **Quy tắc vàng:** Mỗi khi hoàn thành một mục `[x]` trong file này, PHẢI quay lại file C# gốc tương ứng và thêm dòng:
> `// [PORTED_TO_ESP32] - <Tên file C++ đích> - <Ngày hoàn thành>`
> Cuối mỗi tuần, quét toàn bộ C# bằng lệnh `grep -r "PORTED_TO_ESP32"` để đếm tiến độ.
