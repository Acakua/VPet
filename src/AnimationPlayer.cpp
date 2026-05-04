// AnimationPlayer.cpp — Implementation bộ phát hoạt ảnh từ SD Card
// Port logic từ: PNGAnimation.cs + Picture.cs + IGraph.cs
//
// File này chứa toàn bộ logic phần cứng: SD Card đọc file, PSRAM cấp phát,
// LVGL cập nhật hình ảnh.

#include "AnimationPlayer.hpp"
#include <Arduino.h>
#include <SD.h>
#include <esp_heap_caps.h>
#include <lvgl.h>
#include "Utils.hpp"

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
        
        VPET_LOG_I("GFX", "Mapping animation folder: %s", path);
        strncpy(basePath, path, sizeof(basePath) - 1);
        basePath[sizeof(basePath) - 1] = '\0';
        frameCount = 0;

        File dir = SD.open(path);
        if (!dir || !dir.isDirectory()) {
            VPET_LOG_E("GFX", "Failed to open animation directory: %s", path);
            if (dir) dir.close();
            return 0;
        }

        // Đọc tất cả file .bin trong thư mục (Chỉ lấy metadata, không nạp ảnh)
        struct TempFrame {
            uint16_t index;
            uint16_t duration;
            bool hasSuffix;
        } tempFrames[MAX_FRAMES];
        uint16_t tempCount = 0;

        File entry = dir.openNextFile();
        while (entry && tempCount < MAX_FRAMES) {
            if (!entry.isDirectory()) {
                const char* name = entry.name();
                size_t len = strlen(name);
                if (len > 4 && strcmp(name + len - 4, ".bin") == 0) {
                    uint16_t idx = 0, dur = 100;
                    bool hasSuffix = false;
                    _parseFrameFilename(name, &idx, &dur, &hasSuffix);
                    tempFrames[tempCount].index = idx;
                    tempFrames[tempCount].duration = dur;
                    tempFrames[tempCount].hasSuffix = hasSuffix;
                    tempCount++;
                }
            }
            entry.close();
            entry = dir.openNextFile();
        }
        dir.close();

        if (tempCount == 0) {
            VPET_LOG_W("GFX", "No .bin frames found in folder: %s", path);
            return 0;
        }

        // Sort theo index
        for (uint16_t i = 1; i < tempCount; i++) {
            TempFrame key = tempFrames[i];
            int16_t j = i - 1;
            while (j >= 0 && tempFrames[j].index > key.index) {
                tempFrames[j + 1] = tempFrames[j];
                j--;
            }
            tempFrames[j + 1] = key;
        }

        // Lưu metadata vào mảng frames
        for (uint16_t i = 0; i < tempCount; i++) {
            frames[i] = FrameInfo(tempFrames[i].duration, tempFrames[i].index, tempFrames[i].hasSuffix);
        }
        frameCount = tempCount;

        VPET_LOG_I("GFX", "Animation mapped: %u frames. PSRAM will be allocated on start().", frameCount);
        return frameCount;
    }


    bool AnimationPlayer::start(bool loop, AnimEndCallback onEnd, void* ctx) {
        if (frameCount == 0) {
            VPET_LOG_W("GFX", "Cannot start: no frames mapped.");
            return false;
        }

        // Cấp phát buffer nếu chưa có (Reuse để tránh fragmentation)
        if (!bufferA) bufferA = (uint8_t*)heap_caps_malloc(FRAME_BYTES, MALLOC_CAP_SPIRAM);
        if (!bufferB) bufferB = (uint8_t*)heap_caps_malloc(FRAME_BYTES, MALLOC_CAP_SPIRAM);

        if (!bufferA || !bufferB) {
            VPET_LOG_E("GFX", "Failed to allocate Streaming Buffers!");
            return false;
        }

        VPET_LOG_V("GFX", "Starting Streaming playback. Frames: %u", frameCount);

        isLoop = loop;
        control = TaskControl(onEnd, ctx);
        currentFrame = 0;
        isBufferAShowing = true;
        isNextPreloaded = false;

        // Nạp frame 0 vào bufferA ngay lập tức (hiển thị trước)
        if (!_readFrame(0, bufferA)) {
            VPET_LOG_E("GFX", "Failed to read first frame!");
            return false;
        }

        state = PlayerState::Playing;
        lastFrameTime = millis();
        
        // Bắt đầu nạp preload frame 1 vào bufferB (chờ swap)
        if (frameCount > 1) {
            isNextPreloaded = _readFrame(1, bufferB);
        }

        return true;
    }


    bool AnimationPlayer::tick() {
        if (state != PlayerState::Playing) return false;
        if (frameCount == 0) return false;

        uint32_t now = millis();
        uint32_t elapsed = now - lastFrameTime;

        // 1. Kiểm tra đến lúc đổi frame chưa?
        if (elapsed >= frames[currentFrame].duration) {
            // Kiểm tra trạng thái dừng
            if (control.type == TaskControl::ControlType::Stop) {
                control.invokeEndAction();
                state = PlayerState::Idle;
                return false;
            }

            // Tính toán index frame tiếp theo
            uint16_t nextFrameIdx = currentFrame + 1;
            bool shouldStop = false;

            if (nextFrameIdx >= frameCount) {
                if (isLoop) {
                    nextFrameIdx = 0;
                } else if (control.type == TaskControl::ControlType::Continue) {
                    control.type = TaskControl::ControlType::Status_Quo;
                    nextFrameIdx = 0;
                } else {
                    shouldStop = true;
                }
            }

            if (shouldStop) {
                control.type = TaskControl::ControlType::Status_Stoped;
                control.invokeEndAction();
                state = PlayerState::Idle;
                return false;
            }

            // SWAP BUFFER: Chỉ swap nếu frame tiếp theo đã nạp xong vào buffer chờ
            if (isNextPreloaded) {
                isBufferAShowing = !isBufferAShowing;
                currentFrame = nextFrameIdx;
                lastFrameTime = now;
                isNextPreloaded = false; // Reset để nạp frame kế tiếp nữa
                return true; // LVGL sẽ vẽ buffer mới
            } else {
                // Nếu chưa nạp kịp từ SD, đợi thêm (Lag về mặt hình ảnh nhưng không crash)
                VPET_LOG_W("GFX", "Frame %u not preloaded in time! Display lag.", nextFrameIdx);
                // Thử nạp lại ngay lập tức (blocking read để đồng bộ lại)
                uint8_t* nextBuf = isBufferAShowing ? bufferB : bufferA;
                isNextPreloaded = _readFrame(nextFrameIdx, nextBuf);
                return false;
            }
        }

        // 2. PRELOAD: Nếu đang trong thời gian hiển thị mà chưa preload frame tiếp theo, hãy nạp nó ngay
        if (!isNextPreloaded) {
            uint16_t nextFrameIdx = currentFrame + 1;
            if (nextFrameIdx >= frameCount) {
                if (isLoop || control.type == TaskControl::ControlType::Continue) nextFrameIdx = 0;
                else return false;
            }
            
            uint8_t* nextBuf = isBufferAShowing ? bufferB : bufferA;
            VPET_LOG_V("GFX", "Preloading next frame: %u", nextFrameIdx);
            isNextPreloaded = _readFrame(nextFrameIdx, nextBuf);
        }

        return false;
    }


    void AnimationPlayer::stop(bool invokeEnd) {
        VPET_LOG_V("GFX", "Stopping player (invokeEnd: %d)", invokeEnd);
        if (invokeEnd) {
            control.invokeEndAction();
        }
        control.type = TaskControl::ControlType::Status_Stoped;
        state = PlayerState::Idle;
        // KHÔNG giải phóng cache ở đây, vì load() sẽ quản lý vòng đời bộ nhớ
    }

    void AnimationPlayer::releaseBuffers() {
        if (bufferA) {
            heap_caps_free(bufferA);
            bufferA = nullptr;
        }
        if (bufferB) {
            heap_caps_free(bufferB);
            bufferB = nullptr;
        }
        VPET_LOG_I("GFX", "Streaming buffers released.");
        frameCount = 0;
    }


    bool AnimationPlayer::_readFrame(uint16_t frameIdx, uint8_t* buffer) {
        if (!buffer || frameIdx >= frameCount) return false;

        char filePath[160];
        if (frames[frameIdx].hasDurationSuffix) {
            snprintf(filePath, sizeof(filePath), "%s/%03u_%u.bin", basePath, frames[frameIdx].fileIndex, frames[frameIdx].duration);
        } else {
            snprintf(filePath, sizeof(filePath), "%s/%03u.bin", basePath, frames[frameIdx].fileIndex);
        }

        VPET_LOG_V("SD", "Opening frame file: %s", filePath);
        VPET_TIME_START(SDRead);
        
        digitalWrite(15, HIGH); // Ép TFT_CS=15 lên cao (Disable) để nhả bus SPI
        File f = SD.open(filePath, FILE_READ);
        if (!f) {
            VPET_LOG_E("SD", "Failed to open frame file: %s", filePath);
            // Thử thêm một lần nữa cho chắc (retry)
            delay(5);
            f = SD.open(filePath, FILE_READ);
            if (!f) return false;
        }

        size_t bytesRead = 0;
        size_t totalRead = 0;
        while (totalRead < FRAME_BYTES) {
            size_t toRead = (FRAME_BYTES - totalRead > 16384) ? 16384 : (FRAME_BYTES - totalRead);
            bytesRead = f.read(buffer + totalRead, toRead);
            if (bytesRead == 0) break;
            totalRead += bytesRead;
            
            // CỰC KỲ QUAN TRỌNG: Gọi lv_timer_handler để xử lý touch/UI xen kẽ 
            // tránh việc đọc SD chặn hoàn toàn phản hồi của hệ thống.
            lv_timer_handler(); 
        }
        f.close();

        VPET_TIME_END(SDRead, "SD");

        if (totalRead != FRAME_BYTES) {
            VPET_LOG_E("SD", "Incomplete read: %u/%u bytes for %s", totalRead, FRAME_BYTES, filePath);
            return false;
        }

        // Fast Byte Swap: Chuyển Little Endian (SD) sang Big Endian (Display)
        // Việc này cực kỳ quan trọng để DMA gửi đúng thứ tự màu lên SPI
        uint16_t* p = (uint16_t*)buffer;
        size_t count = FRAME_BYTES / 2;
        for (size_t i = 0; i < count; i++) {
            p[i] = (p[i] >> 8) | (p[i] << 8); // Manual swap or __builtin_bswap16
        }

        return true;
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
                                               uint16_t* outDuration,
                                               bool* outHasSuffix) {
        // Default values
        *outIndex = 0;
        *outDuration = 100;
        if (outHasSuffix) *outHasSuffix = false;

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
            if (outHasSuffix) *outHasSuffix = true;
            *underscore = '\0';
            // Phần trước '_' là index
            *outIndex = (uint16_t)atoi(name);
        } else {
            // Không có '_' → dùng toàn bộ tên làm index
            *outIndex = (uint16_t)atoi(name);
        }
    }

} // namespace VPet
