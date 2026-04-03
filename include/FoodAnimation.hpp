#pragma once

// FoodAnimation.hpp — Hoạt ảnh ăn uống (3 lớp)
// Port logic từ: FoodAnimation.cs (dòng 1-287)
// Hỗ trợ hiển thị 3 lớp: Front (thú cưng trước), Food (ảnh thức ăn), Back (thú cưng sau)
// Animation của đồ ăn được định nghĩa theo chuỗi khung hình.

#include <stdint.h>
#include <vector>
#include "GraphInfo.hpp"
#include "AnimationPlayer.hpp"

namespace VPet {

    class GraphCore;

    // ========================================================================
    // FoodFrame — 1 Khung hình di chuyển của đồ ăn (Tương đương Animation class trong C#)
    // ========================================================================
    struct FoodFrame {
        int timeMs;         // Thời gian duy trì (ms)
        float marginX;      // Tọa độ X
        float marginY;      // Tọa độ Y
        float width;        // Kích thước (width = height)
        float rotate;       // Góc xoay
        float opacity;      // Độ trong suốt (0.0 - 1.0)
        bool isVisible;     // Hiển thị hay ẩn
    };

    // ========================================================================
    // FoodAnimation — Chuỗi hoạt ảnh ăn uống
    // ========================================================================
    class FoodAnimation {
    public:
        char frontLayName[64];
        char backLayName[64];
        std::vector<FoodFrame> frames;
        
        GraphInfo info;
        bool isLoop;
        bool isReady;

        FoodAnimation(const char* front, const char* back, bool loop)
            : isLoop(loop), isReady(false) {
            strncpy(frontLayName, front, 63);
            frontLayName[63] = '\0';
            strncpy(backLayName, back, 63);
            backLayName[63] = '\0';
        }

        void addFrame(const FoodFrame& frame) {
            frames.push_back(frame);
            isReady = true;
        }
    };

    // ========================================================================
    // FoodAnimationPlayer — Trình điều khiển hoạt ảnh ăn uống
    // Gồm 2 AnimationPlayer (Front/Back) và logic điều khiển ảnh Food
    // ========================================================================
    class FoodAnimationPlayer {
    public:
        AnimationPlayer* frontPlayer;
        AnimationPlayer* backPlayer;
        FoodAnimation* currentAnim;
        
        TaskControl control;
        
        int currentFrameIdx;
        uint32_t frameStartTime;
        
        // Trạng thái ảnh Food hiện tại để UI (LVGL) render
        FoodFrame currentFoodState;

        FoodAnimationPlayer()
            : frontPlayer(nullptr), backPlayer(nullptr), currentAnim(nullptr)
            , currentFrameIdx(0), frameStartTime(0) {
        }

        ~FoodAnimationPlayer() {
            if (frontPlayer) { delete frontPlayer; frontPlayer = nullptr; }
            if (backPlayer) { delete backPlayer; backPlayer = nullptr; }
        }

        // ================================================================
        // start() — Tương đương Run() trong C# (dòng 245-282)
        // ================================================================
        void start(FoodAnimation* anim, GraphCore* graphCore);

        // ================================================================
        // tick() — Cập nhật non-blocking mỗi vòng lặp
        // ================================================================
        bool tick();

        // ================================================================
        // stop()
        // ================================================================
        void stop();
    };

} // namespace VPet
