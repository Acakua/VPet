#pragma once

// AnimationStateMachine.hpp — Máy trạng thái hoạt ảnh
// Port logic từ: MainDisplay.cs (687 dòng) — logic rải rác, tập trung lại
//
// Trong C#: Logic chuỗi A→B→C nằm rải rác qua nhiều hàm Display*()
//           với callback chains (lambda nesting). Ví dụ:
//   DisplayTouchHead():
//     Display(A_Start, (name) =>
//       Display(name, B_Loop, (name) =>
//         DisplayCEndtoNomal(name)))
//
// Trên ESP32: Tập trung vào 1 struct duy nhất với state machine rõ ràng.
//             Mỗi tick() kiểm tra trạng thái + quyết định animation tiếp theo.
//
// LOGIC CHUYỂN TRẠNG THÁI A→B→C (Port từ MainDisplay.cs):
//   IDLE          → trigger (TouchHead/Idel/Sleep...)
//   PLAY_A_START  → animation xong → chuyển B_LOOP 
//   PLAY_B_LOOP   → loop N lần (randomized) hoặc bị interrupt → chuyển C_END
//   PLAY_C_END    → animation xong → chuyển IDLE (DisplayToNomal)
//
// INTERRUPT LOGIC (Port từ MainDisplay.cs dòng 138-157):
//   Đang ở B_Loop + nhận touch mới cùng loại → SetContinue() (kéo dài B)
//   Đang ở B_Loop + nhận touch loại khác → Stop → C_End → animation mới

#include <stdint.h>
#include <string.h>
#include "GraphInfo.hpp"
#include "GraphCore.hpp"
#include "AnimationPlayer.hpp"
#include "GameSave.hpp"
#include "Utils.hpp"

namespace VPet {

    // ========================================================================
    // AnimState — Trạng thái hiện tại của máy hoạt ảnh
    // Không tồn tại riêng trong C# — logic này nằm implicit trong callback
    // ========================================================================
    enum class AnimState : uint8_t {
        Idle = 0,           // Không có animation đặc biệt (Default/thở)
        Playing_A_Start,    // Đang phát giai đoạn bắt đầu
        Playing_B_Loop,     // Đang phát giai đoạn lặp
        Playing_C_End,      // Đang phát giai đoạn kết thúc
        Playing_Single      // Đang phát animation đơn (không chia A/B/C)
    };

    // ========================================================================
    // WorkingState — Trạng thái hoạt động tổng thể
    // Port từ: MainLogic.cs (enum WorkingState)
    // ========================================================================
    enum class WorkingState : uint8_t {
        Normal = 0,         // 正常 — Bình thường (Default animation)
        Work,               // 工作中 — Đang làm việc
        Sleep,              // 睡觉中 — Đang ngủ
        // Travel — Bỏ qua trên ESP32
    };

    // ========================================================================
    // Callback khi animation chain hoàn thành
    // ========================================================================
    typedef void (*AnimChainCallback)(void* context);

    // ========================================================================
    // AnimationStateMachine — Bộ điều phối chuỗi hoạt ảnh
    //
    // Tập trung toàn bộ logic từ MainDisplay.cs:
    //   DisplayTouchHead()         → dòng 128-158
    //   DisplayTouchBody()         → dòng 166-196
    //   DisplaySleep()             → dòng 314-325
    //   DisplayBLoopingToNomal()   → dòng 302-308
    //   DisplayBLoopingForce()     → dòng 329-332
    //   DisplayCEndtoNomal()       → dòng 410-413
    //   DisplayToNomal()           → dòng 28-46
    //   DisplayDefault()           → dòng 67-71
    //   DisplayToIdel()            → dòng 260-294
    //   DisplayToIdel_StateONE()   → dòng 200-210
    //   DisplayIdel_StateTWO()     → dòng 234-254
    //   DisplayStopForce()         → dòng 93-97
    // ========================================================================
    struct AnimationStateMachine {
        // === Tham chiếu đến các thành phần khác ===
        GraphCore* graph;           // Từ điển hoạt ảnh
        GameSave* save;             // Dữ liệu game
        AnimationPlayer* player;    // Bộ phát animation

        // === Trạng thái ===
        AnimState animState;
        WorkingState workState;

        // === Animation hiện tại ===
        char currentGraphName[32];  // Tên animation đang phát
        GraphType currentGraphType; // Loại animation đang phát

        // === Loop control ===
        // Port từ: MainDisplay.cs dòng 256 (int looptimes)
        //          MainDisplay.cs dòng 24  (int CountNomal)
        int16_t loopTimes;          // Số lần đã loop B_Loop
        int16_t countNormal;        // Bộ đếm chuyển về Normal
        int16_t maxLoopLength;      // Ngưỡng loop (từ GetDuration)
        bool forceLoop;             // true = loop vô hạn (Sleep force)

        AnimationStateMachine()
            : graph(nullptr), save(nullptr), player(nullptr)
            , animState(AnimState::Idle)
            , workState(WorkingState::Normal)
            , currentGraphType(GraphType::Default)
            , loopTimes(0), countNormal(0), maxLoopLength(10)
            , forceLoop(false)
        {
            currentGraphName[0] = '\0';
        }

        // === CÁC HÀM DISPLAY — PORT TỪ MainDisplay.cs ===

        // ================================================================
        // displayToNormal() — Quay về animation mặc định theo WorkingState
        // Port từ: MainDisplay.cs::DisplayToNomal() dòng 28-46
        //
        // C# gốc:
        //   switch (State) {
        //     case Nomal: DisplayNomal();         // → DisplayDefault()
        //     case Sleep: DisplaySleep(true);      // → Sleep A→B(force loop)
        //     case Work:  NowWork.Display(this);   // → Work animation
        //   }
        // ================================================================
        void displayToNormal() {
            switch (workState) {
                case WorkingState::Sleep:
                    displaySleep(true);
                    break;
                case WorkingState::Work:
                    // TODO: Implement work animation
                    displayDefault();
                    break;
                default:
                    displayDefault();
                    break;
            }
        }

        // ================================================================
        // displayDefault() — Phát animation Default (thở/đứng yên)
        // Port từ: MainDisplay.cs::DisplayDefault() dòng 67-71
        //   C#: CountNomal++; Display(Default, Single, DisplayNomal);
        // ================================================================
        void displayDefault() {
            countNormal++;
            _playAnimation(GraphType::Default, AnimatType::Single);
            animState = AnimState::Playing_Single;
        }

        // ================================================================
        // displayTouchHead() — Xoa đầu: A_Start → B_Loop → C_End
        // Port từ: MainDisplay.cs::DisplayTouchHead() dòng 128-158
        //
        // Logic C# gốc:
        //   1. Trừ Strength-2, cộng Feeling+1
        //   2. Nếu đang ở B_Loop cùng loại → SetContinue (kéo dài)
        //   3. Nếu không → phát A_Start, callback → B_Loop → C_End → Normal
        // ================================================================
        void displayTouchHead() {
            countNormal = 0;

            // Tính toán chỉ số (Port dòng 131-136)
            if (save && save->getStrength() >= 10 && save->getFeeling() < GameSave::FEELING_MAX) {
                save->strengthChange(-2);
                save->feelingChange(1);
                save->calculateMode();
            }

            // Interrupt check (Port dòng 138-152)
            if (currentGraphType == GraphType::Touch_Head
                && animState == AnimState::Playing_B_Loop) {
                // Đang B_Loop Touch_Head → kéo dài
                player->control.setContinue();
                return;
            }

            // Phát chuỗi mới: A_Start → B_Loop → C_End
            _startChain(GraphType::Touch_Head);
        }

        // ================================================================
        // displayTouchBody() — Chạm thân: tương tự TouchHead
        // Port từ: MainDisplay.cs::DisplayTouchBody() dòng 166-196
        // Logic giống 100% TouchHead, chỉ khác GraphType
        // ================================================================
        void displayTouchBody() {
            countNormal = 0;
            if (save && save->getStrength() >= 10 && save->getFeeling() < GameSave::FEELING_MAX) {
                save->strengthChange(-2);
                save->feelingChange(1);
                save->calculateMode();
            }
            if (currentGraphType == GraphType::Touch_Body
                && animState == AnimState::Playing_B_Loop) {
                player->control.setContinue();
                return;
            }
            _startChain(GraphType::Touch_Body);
        }

        // ================================================================
        // displaySleep() — Ngủ
        // Port từ: MainDisplay.cs::DisplaySleep() dòng 314-325
        //
        // C# gốc:
        //   if (force) { State=Sleep; A_Start → BLoopingForce }
        //   else       { A_Start → BLoopingToNomal(duration) }
        // ================================================================
        void displaySleep(bool force = false) {
            loopTimes = 0;
            countNormal = 0;
            forceLoop = force;
            if (force) {
                workState = WorkingState::Sleep;
            }
            _startChain(GraphType::Sleep);
        }

        // ================================================================
        // displayIdel() — Idle animation ngẫu nhiên
        // Port từ: MainDisplay.cs::DisplayToIdel() dòng 260-294
        //
        // C#: random pick từ GraphType.Idel → A_Start → BLoopToNomal(dur)
        // ================================================================
        bool displayIdel() {
            const char* name = graph->findName(GraphType::Idel);
            if (!name) return false;

            // Tìm animation A_Start cho Idel
            const AnimationEntry* entry = graph->findGraph(name, AnimatType::A_Start, save->calculateMode());
            if (entry) {
                loopTimes = 0;
                countNormal = 0;
                forceLoop = false;
                maxLoopLength = graph->config.getDuration(name);
                strncpy(currentGraphName, name, 31);
                currentGraphName[31] = '\0';
                currentGraphType = GraphType::Idel;
                _playEntry(entry);
                animState = AnimState::Playing_A_Start;
                return true;
            }

            // Thử Single (Port dòng 279-287)
            entry = graph->findGraph(name, AnimatType::Single, save->calculateMode());
            if (entry) {
                loopTimes = 0;
                countNormal = 0;
                _playEntry(entry);
                animState = AnimState::Playing_Single;
                return true;
            }

            return false;
        }

        // ================================================================
        // displayStateONE() — Idle state 1
        // Port từ: MainDisplay.cs::DisplayToIdel_StateONE() dòng 200-210
        // ================================================================
        void displayStateONE() {
            loopTimes = 0;
            countNormal = 0;
            forceLoop = false;
            _startChain(GraphType::StateONE);
        }

        // ================================================================
        // tick() — Gọi mỗi vòng loop() — kiểm tra state machine
        //
        // Kiểm tra: player đã phát xong chưa?
        //   → Nếu xong → chuyển sang giai đoạn tiếp theo
        //   → A_Start xong → B_Loop
        //   → B_Loop xong → kiểm tra loop count → C_End hoặc loop tiếp
        //   → C_End xong → displayToNormal()
        //   → Single xong → displayToNormal()
        //
        // Trả về true nếu LVGL cần refresh
        // ================================================================
        bool tick() {
            if (!player) return false;

            // Để player xử lý frame timing trước
            bool frameChanged = player->tick();

            // Kiểm tra player đã kết thúc animation hiện tại?
            if (player->state == PlayerState::Idle && animState != AnimState::Idle) {
                _onAnimationFinished();
            }

            return frameChanged;
        }

    private:
        // === LOGIC CHUYỂN TRẠNG THÁI KHI ANIMATION KẾT THÚC ===

        void _onAnimationFinished() {
            switch (animState) {
                case AnimState::Playing_A_Start:
                    // A xong → chuyển B_Loop
                    _transitionToBLoop();
                    break;

                case AnimState::Playing_B_Loop:
                    // B xong → kiểm tra loop
                    if (forceLoop) {
                        // Port từ: DisplayBLoopingForce (dòng 329-332)
                        // Loop vô hạn cho Sleep force
                        _playPhase(AnimatType::B_Loop);
                    } else {
                        // Port từ: DisplayBLoopingToNomal (dòng 302-308)
                        // C#: if (Rnd.Next(++looptimes) > loopLength) → C_End
                        loopTimes++;
                        if (Utils::randomInt(loopTimes) > maxLoopLength) {
                            _transitionToCEnd();
                        } else {
                            _playPhase(AnimatType::B_Loop);
                        }
                    }
                    break;

                case AnimState::Playing_C_End:
                    // C xong → quay về Normal
                    // Port từ: DisplayCEndtoNomal (dòng 410-413)
                    animState = AnimState::Idle;
                    displayToNormal();
                    break;

                case AnimState::Playing_Single:
                    // Single xong → quay về Normal
                    animState = AnimState::Idle;
                    displayToNormal();
                    break;

                default:
                    break;
            }
        }

        // Chuyển sang B_Loop
        void _transitionToBLoop() {
            const AnimationEntry* entry = graph->findGraph(
                currentGraphName, AnimatType::B_Loop, save->calculateMode());
            if (entry) {
                _playEntry(entry);
                animState = AnimState::Playing_B_Loop;
            } else {
                // Không có B_Loop → skip sang C_End hoặc Normal
                _transitionToCEnd();
            }
        }

        // Chuyển sang C_End
        void _transitionToCEnd() {
            const AnimationEntry* entry = graph->findGraph(
                currentGraphName, AnimatType::C_End, save->calculateMode());
            if (entry) {
                _playEntry(entry);
                animState = AnimState::Playing_C_End;
            } else {
                // Không có C_End → trực tiếp về Normal
                animState = AnimState::Idle;
                displayToNormal();
            }
        }

        // Phát 1 giai đoạn (A/B/C) từ animation hiện tại
        void _playPhase(AnimatType phase) {
            const AnimationEntry* entry = graph->findGraph(
                currentGraphName, phase, save->calculateMode());
            if (entry) {
                _playEntry(entry);
            }
        }

        // Bắt đầu chuỗi A→B→C cho 1 GraphType
        void _startChain(GraphType type) {
            const char* name = graph->findName(type);
            if (!name) {
                displayDefault();
                return;
            }

            strncpy(currentGraphName, name, 31);
            currentGraphName[31] = '\0';
            currentGraphType = type;
            maxLoopLength = graph->config.getDuration(name);

            const AnimationEntry* entry = graph->findGraph(
                name, AnimatType::A_Start, save->calculateMode());
            if (entry) {
                _playEntry(entry);
                animState = AnimState::Playing_A_Start;
            } else {
                // Thử Single nếu không có A_Start
                entry = graph->findGraph(name, AnimatType::Single, save->calculateMode());
                if (entry) {
                    _playEntry(entry);
                    animState = AnimState::Playing_Single;
                } else {
                    displayDefault();
                }
            }
        }

        // Phát 1 animation entry trên player
        void _playAnimation(GraphType type, AnimatType animat) {
            const char* name = graph->findName(type);
            if (!name) return;
            strncpy(currentGraphName, name, 31);
            currentGraphName[31] = '\0';
            currentGraphType = type;

            const AnimationEntry* entry = graph->findGraph(
                name, animat, save->calculateMode());
            if (entry) _playEntry(entry);
        }

        // Load + start animation entry trên player
        void _playEntry(const AnimationEntry* entry) {
            if (!entry || !player) return;
            player->load(entry->path);
            bool isLoop = (entry->info.animat == AnimatType::B_Loop);
            player->start(isLoop);
        }
    };

} // namespace VPet
