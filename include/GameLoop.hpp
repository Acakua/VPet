#pragma once

// GameLoop.hpp — Vòng lặp chính (nhịp tim) của VPet
// Port logic từ: MainLogic.cs (FunctionSpend, EventTimer_Elapsed)
//
// Đây là "bộ não" tính toán sinh tồn của Pet, điều phối các chỉ số
// đói, khát, thể lực, cảm xúc theo thời gian.

#include <stdint.h>
#include "GameSave.hpp"
#include "AnimationStateMachine.hpp"
#include "GraphInfo.hpp"

namespace VPet {

    // ========================================================================
    // GameLoop — Bộ điều phối chính
    // ========================================================================
    struct GameLoop {
        GameSave* save;
        AnimationStateMachine* anim;
        Work* currentWork;          // Công việc hiện tại đang chạy (hoàn thành sau)
        
        // Timer
        uint32_t lastInteractionTime; // Tính bằng millis() hoặc unix timestamp
        uint32_t lastEventTime;       // Lần cuối chạy EventTimer
        
        // Configs
        uint32_t eventIntervalMs;     // Chu kỳ EventTimer (C# gốc = 15000ms)

        GameLoop(GameSave* _save = nullptr, AnimationStateMachine* _anim = nullptr)
            : save(_save), anim(_anim)
            , lastInteractionTime(0), lastEventTime(0)
            , eventIntervalMs(15000)
        {
        }

        // ================================================================
        // start() — Khởi động vòng lặp
        // ================================================================
        void start();

        // ================================================================
        // tick() — Gọi mỗi vòng Arduino loop()
        // Kiểm tra EventTimer, nếu đủ thời gian thì chạy logic
        // ================================================================
        void tick();

        // ================================================================
        // interact() — Gọi khi người dùng tương tác với Pet (xoa đầu, chạm)
        // ================================================================
        void interact();

    private:
        // ================================================================
        // eventTimerElapsed() — Xử lý mỗi 15s (hoặc theo eventIntervalMs)
        // Port từ MainLogic.cs::EventTimer_Elapsed() dòng 470-533
        // ================================================================
        void eventTimerElapsed();

        // ================================================================
        // functionSpend(timePass) — Tính toán sinh tồn CỐT LÕI
        // Port từ MainLogic.cs::FunctionSpend() dòng 234-426
        // ================================================================
        void functionSpend(double timePass);

        // ================================================================
        // playSwitchAnimat(before, after) — Phát hoạt ảnh chuyển trạng thái
        // Port từ MainLogic.cs::PlaySwitchAnimat() dòng 432-453
        // ================================================================
        void playSwitchAnimat(ModeType before, ModeType after);
    };

} // namespace VPet
