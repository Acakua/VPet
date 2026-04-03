// AnimationPlayer.cpp — Implementation bộ phát hoạt ảnh từ SD Card
// Port logic từ: PNGAnimation.cs + Picture.cs + IGraph.cs
//
// File này chứa toàn bộ logic phần cứng: SD Card đọc file, PSRAM cấp phát,
// LVGL cập nhật hình ảnh.

#include "AnimationPlayer.hpp"
#include <Arduino.h>
#include <SD.h>
#include <esp_heap_caps.h>

namespace VPet {

    // ====================================================================
    // load() — Mở thư mục animation trên SD, parse danh sách frame
    //
    // Port logic từ: PNGAnimation.cs dòng 182-186
    //   C#: var noExtFileName = Path.GetFileNameWithoutExtension(paths[i].Name);
    //       int time = int.Parse(noExtFileName.Substring(noExtFileName.LastIndexOf('_') + 1));
    //
    // Trên ESP32: Mở thư mục SD → liệt kê file .bin → parse duration từ tên
    // Tên file: 000_150.bin → frame 0, duration 150ms
    //
    // Trả về: số frame đã load (0 = thất bại)
    // ====================================================================
    uint16_t AnimationPlayer::load(const char* path) {
        if (!path) return 0;

        strncpy(basePath, path, sizeof(basePath) - 1);
        basePath[sizeof(basePath) - 1] = '\0';
        frameCount = 0;

        File dir = SD.open(path);
        if (!dir || !dir.isDirectory()) {
            if (dir) dir.close();
            return 0;
        }

        // Đọc tất cả file .bin trong thư mục
        // Lưu tạm để sort theo index
        struct TempFrame {
            uint16_t index;
            uint16_t duration;
        } tempFrames[MAX_FRAMES];
        uint16_t tempCount = 0;

        File entry = dir.openNextFile();
        while (entry && tempCount < MAX_FRAMES) {
            if (!entry.isDirectory()) {
                const char* name = entry.name();
                // Kiểm tra đuôi .bin
                size_t len = strlen(name);
                if (len > 4 && strcmp(name + len - 4, ".bin") == 0) {
                    // Parse: 000_150.bin → index=0, duration=150
                    // Tìm dấu '_' cuối cùng trước .bin
                    uint16_t idx = 0, dur = 100;
                    _parseFrameFilename(name, &idx, &dur);
                    tempFrames[tempCount].index = idx;
                    tempFrames[tempCount].duration = dur;
                    tempCount++;
                }
            }
            entry.close();
            entry = dir.openNextFile();
        }
        dir.close();

        if (tempCount == 0) return 0;

        // Sort theo index (insertion sort — nhỏ, đơn giản)
        for (uint16_t i = 1; i < tempCount; i++) {
            TempFrame key = tempFrames[i];
            int16_t j = i - 1;
            while (j >= 0 && tempFrames[j].index > key.index) {
                tempFrames[j + 1] = tempFrames[j];
                j--;
            }
            tempFrames[j + 1] = key;
        }

        // Chuyển vào frames[]
        for (uint16_t i = 0; i < tempCount; i++) {
            frames[i] = FrameInfo(tempFrames[i].duration, tempFrames[i].index);
        }
        frameCount = tempCount;

        return frameCount;
    }

    // ====================================================================
    // start() — Cấp phát PSRAM, đọc frame đầu, bắt đầu phát
    //
    // Port từ: PNGAnimation::Run() dòng 274-328 (logic khởi tạo)
    //   C#: nowid = 0; Control = new TaskControl(EndAction);
    //       Task.Run(() => Animations[0].Run(img, NEWControl));
    //
    // ESP32: Cấp phát 2 buffer PSRAM → đọc frame 0 → state=Playing
    // ====================================================================
    bool AnimationPlayer::start(bool loop, AnimEndCallback onEnd, void* ctx) {
        if (frameCount == 0) return false;

        // Dừng animation hiện tại nếu đang chạy
        if (state == PlayerState::Playing) {
            stop(false);
        }

        isLoop = loop;
        control = TaskControl(onEnd, ctx);
        currentFrame = 0;
        bufferSwapped = false;
        nextFrameReady = false;

        // Cấp phát PSRAM cho double buffering
        // heap_caps_malloc trả về nullptr nếu không đủ PSRAM
        if (!bufferA) {
            bufferA = (uint8_t*)heap_caps_malloc(FRAME_BYTES, MALLOC_CAP_SPIRAM);
            if (!bufferA) return false;
        }
        if (!bufferB) {
            bufferB = (uint8_t*)heap_caps_malloc(FRAME_BYTES, MALLOC_CAP_SPIRAM);
            if (!bufferB) {
                heap_caps_free(bufferA);
                bufferA = nullptr;
                return false;
            }
        }

        // Đọc frame 0 vào bufferA
        if (!_readFrame(0, bufferA)) {
            stop(false);
            return false;
        }

        // Preload frame 1 vào bufferB (nếu có)
        if (frameCount > 1) {
            nextFrameReady = _readFrame(1, bufferB);
        }

        state = PlayerState::Playing;
        lastFrameTime = millis();
        return true;
    }

    // ====================================================================
    // tick() — Non-blocking frame advance
    //
    // Port logic từ: PNGAnimation.cs::Animation::Run() dòng 230-268
    //
    // C# gốc (blocking):
    //   This.Dispatcher.Invoke(() => This.Margin = ...) // hiển thị
    //   Thread.Sleep(Time)                               // chờ
    //   switch (Control.Type) { ... }                    // check control
    //
    // ESP32 (non-blocking):
    //   if (millis() - lastFrameTime < duration) return false  // chưa đến lúc
    //   swap buffer, advance frame, check control              // đến lúc rồi
    //
    // GIỮ NGUYÊN logic control từ C# dòng 237-268:
    //   Stop         → EndAction, return
    //   Stoped       → return
    //   Status_Quo   → hết frame? loop: reset / else: Stoped+EndAction
    //   Continue     → hết frame? reset, type=Status_Quo
    // ====================================================================
    bool AnimationPlayer::tick() {
        if (state != PlayerState::Playing) return false;
        if (frameCount == 0) return false;

        uint32_t now = millis();
        uint32_t elapsed = now - lastFrameTime;

        // Chưa đến lúc đổi frame
        if (elapsed < frames[currentFrame].duration) {
            return false;
        }

        // === Kiểm tra TaskControl (khớp C# dòng 237-268) ===
        switch (control.type) {
            case TaskControl::ControlType::Stop:
                // C# dòng 239-241: Control.EndAction?.Invoke(); return;
                control.invokeEndAction();
                state = PlayerState::Idle;
                return false;

            case TaskControl::ControlType::Status_Stoped:
                // C# dòng 242-243: return;
                state = PlayerState::Idle;
                return false;

            case TaskControl::ControlType::Status_Quo:
            case TaskControl::ControlType::Continue:
            {
                uint16_t nextFrame = currentFrame + 1;

                // C# dòng 246: if (++parent.nowid >= parent.Animations.Count)
                if (nextFrame >= frameCount) {
                    if (isLoop) {
                        // C# dòng 248-252: parent.nowid = 0; Task.Run(Animations[0].Run)
                        nextFrame = 0;
                    } else if (control.type == TaskControl::ControlType::Continue) {
                        // C# dòng 254-258: type = Status_Quo; nowid = 0
                        control.type = TaskControl::ControlType::Status_Quo;
                        nextFrame = 0;
                    } else {
                        // C# dòng 259-264: type = Stoped; EndAction?.Invoke()
                        control.type = TaskControl::ControlType::Status_Stoped;
                        control.invokeEndAction();
                        state = PlayerState::Idle;
                        return false;
                    }
                }

                // === Swap buffer & advance ===
                if (nextFrameReady) {
                    bufferSwapped = !bufferSwapped;
                }

                currentFrame = nextFrame;
                lastFrameTime = now;

                // Preload frame tiếp theo
                uint16_t preloadIdx = (currentFrame + 1) % frameCount;
                if (preloadIdx != currentFrame || isLoop) {
                    nextFrameReady = _readFrame(preloadIdx, preloadBuffer());
                }

                return true; // Frame đã đổi — cần cập nhật LVGL
            }
        }
        return false;
    }

    // ====================================================================
    // stop() — Dừng phát, giải phóng PSRAM
    // Port từ: IGraph.cs::Stop() dòng 51-58
    //   C#: if (StopEndAction) Control.EndAction = null;
    //       Control.Type = ControlType.Stop;
    // ESP32: + free bufferA/B PSRAM
    // ====================================================================
    void AnimationPlayer::stop(bool invokeEnd) {
        if (invokeEnd) {
            control.invokeEndAction();
        }
        control.type = TaskControl::ControlType::Status_Stoped;
        state = PlayerState::Idle;
        currentFrame = 0;

        // Giải phóng PSRAM
        if (bufferA) {
            heap_caps_free(bufferA);
            bufferA = nullptr;
        }
        if (bufferB) {
            heap_caps_free(bufferB);
            bufferB = nullptr;
        }
        nextFrameReady = false;
    }

    // ====================================================================
    // _readFrame() — Đọc 1 frame binary từ SD Card vào buffer
    //
    // KHÔNG CÓ TRONG C# GỐC (PC đọc toàn bộ vào RAM).
    // Đây là phần viết mới cho ESP32 SD Card streaming.
    //
    // Tên file: <basePath>/<frameIndex dạng 3 chữ số>.bin
    //   Ví dụ: /pet/default/nomal/loop/000.bin
    //
    // File format: Raw RGB565 Little-Endian, 320×320×2 = 204,800 bytes
    // ====================================================================
    bool AnimationPlayer::_readFrame(uint16_t frameIdx, uint8_t* buffer) {
        if (!buffer || frameIdx >= frameCount) return false;

        // Tạo đường dẫn file: basePath/000.bin
        char filePath[160];
        snprintf(filePath, sizeof(filePath), "%s/%03u.bin", basePath, frames[frameIdx].fileIndex);

        File f = SD.open(filePath, FILE_READ);
        if (!f) return false;

        size_t bytesRead = f.read(buffer, FRAME_BYTES);
        f.close();

        return (bytesRead == FRAME_BYTES);
    }

    // ====================================================================
    // _parseFrameFilename() — Parse tên file để lấy index và duration
    //
    // Port logic từ: PNGAnimation.cs dòng 184-186
    //   C#: var noExt = Path.GetFileNameWithoutExtension(name);
    //       int time = int.Parse(noExt.Substring(noExt.LastIndexOf('_') + 1));
    //
    // Input:  "000_150.bin"
    // Output: *outIndex = 0, *outDuration = 150
    // ====================================================================
    void AnimationPlayer::_parseFrameFilename(const char* filename,
                                               uint16_t* outIndex,
                                               uint16_t* outDuration) {
        // Default values
        *outIndex = 0;
        *outDuration = 100;

        if (!filename) return;

        // Parse: "000_150.bin"
        // Tìm tất cả phần trước ".bin"
        char name[64];
        strncpy(name, filename, 63);
        name[63] = '\0';

        // Bỏ extension
        char* dot = strrchr(name, '.');
        if (dot) *dot = '\0';

        // Tìm dấu '_' cuối cùng
        char* underscore = strrchr(name, '_');
        if (underscore) {
            // Phần sau '_' là duration
            *outDuration = (uint16_t)atoi(underscore + 1);
            *underscore = '\0';
            // Phần trước '_' là index
            *outIndex = (uint16_t)atoi(name);
        } else {
            // Không có '_' → dùng toàn bộ tên làm index
            *outIndex = (uint16_t)atoi(name);
        }
    }

} // namespace VPet
