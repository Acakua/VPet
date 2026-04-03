#pragma once

// AnimationPlayer.hpp — Bộ phát hoạt ảnh từ SD Card
// Port logic từ: PNGAnimation.cs + Picture.cs + IGraph.cs
//
// KIẾN TRÚC HOÀN TOÀN MỚI CHO ESP32:
//   C#: Tất cả frame nạp vào RAM (sprite sheet caching)
//   ESP32: Đọc từng frame từ SD Card (double buffering trên PSRAM)
//
// Flow:
//   SD Card → [Read Buffer A/B in PSRAM] → [lv_img widget] → SPI DMA → Màn hình
//
// KHÔNG port:
//   - Sprite sheet caching (startup() hàm dòng 110-196)
//   - SKBitmap/BitmapImage (WPF/SkiaSharp rendering)
//   - Dispatcher.Invoke, CommUIElements (WPF thread management)
//   - Parallel.For (multi-threading image decode)

#include <stdint.h>
#include <string.h>
#include "GraphInfo.hpp"

namespace VPet {

    // ========================================================================
    // TaskControl — Bộ điều khiển phát animation
    // Port từ: IGraph.cs dòng 87-139 (class TaskControl)
    //
    // Logic C# gốc (GIỮ NGUYÊN 100%):
    //   Status_Quo   = duy trì hiện trạng, khi hết animation → gọi EndAction
    //   Stop         = dừng ngay, gọi EndAction
    //   Continue     = phát lại 1 lần nữa khi hết, rồi chuyển về Status_Quo
    //   Status_Stoped = đã dừng hoàn toàn, không làm gì nữa
    // ========================================================================
    
    // Callback khi animation kết thúc
    typedef void (*AnimEndCallback)(void* context);

    struct TaskControl {
        enum class ControlType : uint8_t {
            Status_Quo = 0,     // 维持现状 — Duy trì (chạy bình thường)
            Stop,               // 停止 — Yêu cầu dừng
            Continue,           // 继续 — Phát lại thêm 1 lần rồi về Status_Quo
            Status_Stoped       // 已停止 — Đã dừng xong
        };

        ControlType type;
        AnimEndCallback endAction;
        void* endActionContext;

        TaskControl(AnimEndCallback cb = nullptr, void* ctx = nullptr)
            : type(ControlType::Status_Quo)
            , endAction(cb), endActionContext(ctx)
        {}

        void reset() {
            type = ControlType::Status_Quo;
            endAction = nullptr;
            endActionContext = nullptr;
        }

        // Port từ: IGraph.cs dòng 92
        // C#: PlayState => Type != ControlType.Status_Stoped && Type != ControlType.Stop
        bool isPlaying() const {
            return type != ControlType::Status_Quo || type == ControlType::Continue;
        }

        void setPlaying(bool value) {
            if (!value) type = ControlType::Stop;
            else if (type == ControlType::Status_Stoped || type == ControlType::Stop)
                type = ControlType::Status_Quo;
        }

        // Port từ: IGraph.cs dòng 96
        void setContinue() { type = ControlType::Continue; }

        // Port từ: IGraph.cs dòng 100
        void stop(AnimEndCallback cb = nullptr, void* ctx = nullptr) {
            endAction = cb;
            endActionContext = ctx;
            type = ControlType::Stop;
        }

        // Gọi EndAction nếu có
        void invokeEndAction() {
            if (endAction) {
                endAction(endActionContext);
            }
        }
    };

    // ========================================================================
    // FrameInfo — Thông tin 1 frame animation
    // Port từ: PNGAnimation.cs dòng 201-224 (class Animation)
    //
    // C# gốc:
    //   int Time;        // Thời gian hiển thị frame (ms)
    //   int MarginWIX;   // Offset X trên sprite sheet (WPF) → KHÔNG CẦN
    //
    // ESP32:
    //   duration     = thời gian hiển thị (ms)
    //   fileIndex    = index trong thư mục (để tạo tên file: 000.bin, 001.bin)
    // ========================================================================
    struct FrameInfo {
        uint16_t duration;      // 帧时间(ms) — Thời gian hiển thị
        uint16_t fileIndex;     // 文件索引 — Index file trên SD

        FrameInfo() : duration(100), fileIndex(0) {}
        FrameInfo(uint16_t dur, uint16_t idx) : duration(dur), fileIndex(idx) {}
    };

    // ========================================================================
    // AnimationPlayer — Bộ phát hoạt ảnh chính
    //
    // THIẾT KẾ MỚI CHO ESP32 (không tồn tại trong C#):
    //
    // Double Buffering:
    //   bufferA / bufferB trên PSRAM
    //   Trong khi bufferA đang hiển thị → đọc frame tiếp từ SD vào bufferB
    //   Khi đến thời gian chuyển frame → swap A ↔ B
    //
    // Cách dùng:
    //   player.load("/pet/default/nomal/loop")  → Mở thư mục, parse frame info
    //   player.start(true)                       → Bắt đầu phát, isLoop=true
    //   [trong loop()] player.tick()             → Gọi mỗi vòng để update
    //   player.stop()                            → Dừng phát, free PSRAM
    //
    // Frame format trên SD Card (.bin = raw RGB565):
    //   Tên file: 000_150.bin (frame 0, hiển thị 150ms)
    //   Kích thước: 320 × 320 × 2 = 204,800 bytes mỗi frame
    //   (320×320 là kích thước Pet trên màn 320×480)
    // ========================================================================

    static constexpr uint16_t MAX_FRAMES = 64;           // Tối đa frame/animation
    static constexpr uint16_t FRAME_WIDTH  = 320;        // Chiều rộng frame (pixels)
    static constexpr uint16_t FRAME_HEIGHT = 320;        // Chiều cao frame (pixels)
    static constexpr uint32_t FRAME_BYTES  = FRAME_WIDTH * FRAME_HEIGHT * 2; // RGB565 = 2 bytes/pixel

    // Trạng thái player
    enum class PlayerState : uint8_t {
        Idle = 0,           // Chưa load hoặc đã stop
        Loading,            // Đang load frame đầu tiên
        Playing,            // Đang phát
        Paused              // Tạm dừng
    };

    struct AnimationPlayer {
        // === Metadata ===
        char basePath[128];         // Đường dẫn thư mục trên SD
        FrameInfo frames[MAX_FRAMES];
        uint16_t frameCount;
        bool isLoop;

        // === Playback state ===
        PlayerState state;
        TaskControl control;
        uint16_t currentFrame;       // Index frame hiện tại
        uint32_t lastFrameTime;      // millis() lần cuối đổi frame

        // === Double buffering (PSRAM) ===
        // Hai buffer frame trên PSRAM — swap khi đổi frame
        // Initialized/freed by start()/stop()
        uint8_t* bufferA;           // Frame đang hiển thị
        uint8_t* bufferB;           // Frame đang đọc (preload)
        bool bufferSwapped;         // true = bufferB là frame hiện tại
        bool nextFrameReady;        // true = buffer dự phòng đã đọc xong

        // === LVGL image descriptor ===
        // Trỏ tới buffer hiện tại, cập nhật khi swap
        // (sẽ được gắn vào lv_img_set_src() trong PetDisplay.cpp)
        void* lvglImgDesc;          // lv_img_dsc_t* — cast khi dùng LVGL

        AnimationPlayer()
            : frameCount(0), isLoop(false)
            , state(PlayerState::Idle)
            , currentFrame(0), lastFrameTime(0)
            , bufferA(nullptr), bufferB(nullptr)
            , bufferSwapped(false), nextFrameReady(false)
            , lvglImgDesc(nullptr)
        {
            basePath[0] = '\0';
        }

        // ====================================================================
        // load() — Mở thư mục animation, parse danh sách frame
        // Port logic từ: PNGAnimation.cs dòng 182-186
        //   C#: for (paths) { parse tên file → Animation(time, offset) }
        //   ESP32: Đọc tên file trong thư mục SD → parse duration từ tên
        //
        // Tên file format: <index>_<duration_ms>.bin
        //   Ví dụ: 000_150.bin, 001_125.bin, 002_100.bin
        // 
        // Trả về: số frame đã load, 0 nếu thất bại
        // ====================================================================
        // [PORTED_TO_ESP32] - Logic parse frame timing - 2026-04-03
        uint16_t load(const char* path);
        // Implementation sẽ ở AnimationPlayer.cpp

        // ====================================================================
        // start() — Bắt đầu phát animation
        //   1. Cấp phát 2 buffer PSRAM (bufferA, bufferB)
        //   2. Đọc frame 0 vào bufferA
        //   3. Bắt đầu đọc frame 1 vào bufferB (preload)
        //   4. Đặt state = Playing
        // ====================================================================
        bool start(bool loop, AnimEndCallback onEnd = nullptr, void* ctx = nullptr);
        // Implementation sẽ ở AnimationPlayer.cpp

        // ====================================================================
        // tick() — Gọi mỗi vòng loop() của Arduino
        // Port logic từ: PNGAnimation.cs::Animation::Run() dòng 230-268
        //
        // Logic C# gốc (mỗi frame):
        //   1. This.Dispatcher.Invoke(() => This.Margin = ...) → Hiển thị frame
        //   2. Thread.Sleep(Time)                              → Chờ đủ thời gian
        //   3. Check Control.Type:
        //      - Stop        → gọi EndAction, return
        //      - Stoped      → return
        //      - Status_Quo  → nếu hết frame:
        //                        nếu IsLoop → reset về frame 0
        //                        else       → Status_Stoped, EndAction
        //      - Continue    → nếu hết frame → reset, type=Status_Quo
        //   4. Chuyển đến frame tiếp
        //
        // Trên ESP32 (non-blocking):
        //   1. Kiểm tra millis() - lastFrameTime >= duration
        //   2. Nếu đúng → swap buffer, cập nhật LVGL, bắt đầu đọc frame tiếp
        //   3. Kiểm tra Control giống C#
        //   4. Trả về true nếu có đổi frame (LVGL cần invalidate)
        // ====================================================================
        // [PORTED_TO_ESP32] - Animation::Run() logic - 2026-04-03
        bool tick();
        // Implementation sẽ ở AnimationPlayer.cpp

        // ====================================================================
        // stop() — Dừng phát, giải phóng PSRAM
        // Port logic từ: IGraph.cs::Stop() dòng 51-58
        //   C#: Control.EndAction = null; Control.Type = Stop
        //   ESP32: + free bufferA/B PSRAM
        // ====================================================================
        void stop(bool invokeEndAction = false);
        // Implementation sẽ ở AnimationPlayer.cpp

        // === Helper ===
        // Lấy con trỏ buffer đang hiển thị
        uint8_t* currentBuffer() const {
            return bufferSwapped ? bufferB : bufferA;
        }
        // Lấy con trỏ buffer đang preload
        uint8_t* preloadBuffer() const {
            return bufferSwapped ? bufferA : bufferB;
        }

    private:
        // Đọc 1 frame từ SD Card vào buffer PSRAM
        bool _readFrame(uint16_t frameIdx, uint8_t* buffer);
        // Parse tên file để lấy index và duration
        static void _parseFrameFilename(const char* filename,
                                         uint16_t* outIndex,
                                         uint16_t* outDuration);
    };

} // namespace VPet
