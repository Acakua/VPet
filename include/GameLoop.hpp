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
    // WorkingState — Trạng thái làm việc (Port từ MainLogic.cs dòng 635)
    // ========================================================================
    // WorkingState đã được định nghĩa trong AnimationStateMachine.hpp
    // enum class WorkingState { ... };

    // ========================================================================
    // GameLoop — Bộ điều phối chính
    // ========================================================================
    struct GameLoop {
        GameSave* save;
        AnimationStateMachine* anim;
        
        Work* nowWork;              // Công việc hiện tại (Port từ MainLogic.cs NowWork)
        WorkingState state;         // Trạng thái hiện tại (Port từ MainLogic.cs State)
        
        static const int WORK_COUNT = 10;
        Work works[WORK_COUNT];     // Danh sách 10 công việc mặc định
        
        // Timer
        uint32_t lastInteractionTime; // Tính bằng millis()
        uint32_t lastEventTime;       // Lần cuối chạy EventTimer
        
        // Configs
        uint32_t eventIntervalMs;     // Chu kỳ EventTimer (C# gốc = 15000ms)

        GameLoop(GameSave* _save = nullptr, AnimationStateMachine* _anim = nullptr)
            : save(_save), anim(_anim)
            , nowWork(nullptr), state(WorkingState::Normal)
            , lastInteractionTime(0), lastEventTime(0)
            , eventIntervalMs(15000)
        {
        }

        // ================================================================
        // start() — Khởi động vòng lặp và nạp dữ liệu công việc
        // ================================================================
        void start();

        // ================================================================
        // tick() — Gọi mỗi vòng Arduino loop()
        // ================================================================
        void tick();

        // ================================================================
        // interact() — Gọi khi người dùng tương tác với Pet
        // ================================================================
        void interact();

        // ================================================================
        // startWork(index) — Bắt đầu một công việc từ danh sách
        // ================================================================
        bool startWork(int index);

        // ================================================================
        // stopWork() — Dừng công việc hiện tại
        // ================================================================
        void stopWork();

    private:
        // ================================================================
        // eventTimerElapsed() — Xử lý mỗi 15s
        // ================================================================
        void eventTimerElapsed();

        // ================================================================
        // functionSpend(timePass) — Tính toán sinh tồn CỐT LÕI
        // ================================================================
        void functionSpend(double timePass);

        // ================================================================
        // playSwitchAnimat(before, after) — Phát hoạt ảnh chuyển trạng thái
        // ================================================================
        void playSwitchAnimat(ModeType before, ModeType after);
    };

} // namespace VPet
