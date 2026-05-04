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
        Travel,             // 旅游中 — Du lịch
        Empty               // Trạng thái trống/Khác
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

        // Helper to get state name for logging
        const char* _animStateName(AnimState s) {
            switch (s) {
                case AnimState::Idle: return "Idle";
                case AnimState::Playing_A_Start: return "A_Start";
                case AnimState::Playing_B_Loop: return "B_Loop";
                case AnimState::Playing_C_End: return "C_End";
                case AnimState::Playing_Single: return "Single";
                default: return "Unknown";
            }
        }

        const char* _workStateName(WorkingState s) {
            switch (s) {
                case WorkingState::Normal: return "Normal";
                case WorkingState::Work: return "Work";
                case WorkingState::Sleep: return "Sleep";
                default: return "Unknown";
            }
        }

        // === CÁC HÀM DISPLAY — PORT TỪ MainDisplay.cs ===

        void displayToNormal() {
            VPET_LOG_I("FSM", "Transitioning to normal display. WorkState: %s", _workStateName(workState));
            switch (workState) {
                case WorkingState::Sleep:
                    displaySleep(true);
                    break;
                case WorkingState::Work:
                    displayDefault();
                    break;
                default:
                    displayDefault();
                    break;
            }
        }

        void displayDefault() {
            countNormal++;
            VPET_LOG_V("FSM", "Playing default animation (CountNormal: %d)", countNormal);
            
            // Tìm tên animation Default
            const char* name = graph->findName(GraphType::Default);
            if (!name) name = "Default";

            // Thử B_Loop trước (Thở)
            const AnimationEntry* entry = graph->findGraph(name, AnimatType::B_Loop, save->calculateMode());
            if (entry) {
                _playEntry(entry);
                animState = AnimState::Playing_B_Loop;
                forceLoop = true; // Loop thở vô hạn cho đến khi có event
                return;
            }

            // Fallback sang Single
            entry = graph->findGraph(name, AnimatType::Single, save->calculateMode());
            if (entry) {
                _playEntry(entry);
                animState = AnimState::Playing_Single;
                forceLoop = false;
                return;
            }
            
            VPET_LOG_W("FSM", "Failed to find any Default animation (B_Loop or Single) for: %s", name);
        }

        void displayTouchHead() {
            countNormal = 0;
            VPET_LOG_I("FSM", "Trigger: Touch Head");

            // Tính toán chỉ số (Port dòng 131-136)
            if (save && save->getStrength() >= 10 && save->getFeeling() < GameSave::FEELING_MAX) {
                save->strengthChange(-2);
                save->feelingChange(1);
                save->calculateMode();
                VPET_LOG_V("PET", "Stats updated - Strength: %d, Feeling: %d", save->getStrength(), save->getFeeling());
            }

            // Interrupt check (Port dòng 138-152)
            if (currentGraphType == GraphType::Touch_Head
                && animState == AnimState::Playing_B_Loop) {
                VPET_LOG_I("FSM", "Continuing existing TouchHead B_Loop");
                player->control.setContinue();
                return;
            }

            // Phát chuỗi mới: A_Start → B_Loop → C_End
            _startChain(GraphType::Touch_Head);
        }

        void displayTouchBody() {
            countNormal = 0;
            VPET_LOG_I("FSM", "Trigger: Touch Body");
            if (save && save->getStrength() >= 10 && save->getFeeling() < GameSave::FEELING_MAX) {
                save->strengthChange(-2);
                save->feelingChange(1);
                save->calculateMode();
                VPET_LOG_V("PET", "Stats updated - Strength: %d, Feeling: %d", save->getStrength(), save->getFeeling());
            }
            if (currentGraphType == GraphType::Touch_Body
                && animState == AnimState::Playing_B_Loop) {
                VPET_LOG_I("FSM", "Continuing existing TouchBody B_Loop");
                player->control.setContinue();
                return;
            }
            _startChain(GraphType::Touch_Body);
        }

        void displaySleep(bool force = false) {
            VPET_LOG_I("FSM", "Trigger: Sleep (Force: %d)", force);
            loopTimes = 0;
            countNormal = 0;
            forceLoop = force;
            if (force) {
                workState = WorkingState::Sleep;
            }
            _startChain(GraphType::Sleep);
        }

        void displayWork(const char* graphName) {
            VPET_LOG_I("FSM", "Trigger: Work (%s)", graphName);
            loopTimes = 0;
            countNormal = 0;
            forceLoop = true; // Loop work animation indefinitely
            workState = WorkingState::Work;
            
            // Port logic startChain but with specific graph name
            strncpy(currentGraphName, graphName, 31);
            currentGraphName[31] = '\0';
            currentGraphType = GraphType::Work;
            maxLoopLength = graph->config.getDuration(graphName);

            const AnimationEntry* entry = graph->findGraph(
                graphName, AnimatType::A_Start, save->calculateMode());
            if (entry) {
                _playEntry(entry);
                animState = AnimState::Playing_A_Start;
            } else {
                entry = graph->findGraph(graphName, AnimatType::B_Loop, save->calculateMode());
                if (entry) {
                    _playEntry(entry);
                    animState = AnimState::Playing_B_Loop;
                } else {
                    VPET_LOG_E("FSM", "No suitable entry found for work: %s", graphName);
                    displayToNormal();
                }
            }
        }

        bool displayIdel() {
            const char* name = graph->findName(GraphType::Idel);
            if (!name) {
                VPET_LOG_V("FSM", "Idle animation not found, falling back to normal.");
                displayToNormal();
                return false;
            }

            VPET_LOG_I("FSM", "Trigger: Random Idle (%s)", name);

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

            VPET_LOG_W("FSM", "Failed to find suitable entry for Idle animation: %s", name);
            displayToNormal();
            return false;
        }

        void displayStateONE() {
            VPET_LOG_I("FSM", "Trigger: StateONE");
            loopTimes = 0;
            countNormal = 0;
            forceLoop = false;
            _startChain(GraphType::StateONE);
        }

        bool tick() {
            if (!player) return false;

            // Để player xử lý frame timing trước
            bool frameChanged = player->tick();

            // Kiểm tra player đã kết thúc animation hiện tại?
            if (player->state == PlayerState::Idle && animState != AnimState::Idle) {
                VPET_LOG_D("FSM", "Animation finished for state: %s", _animStateName(animState));
                _onAnimationFinished();
            }

            return frameChanged;
        }

    private:
        // === LOGIC CHUYỂN TRẠNG THÁI KHI ANIMATION KẾT THÚC ===

        void _onAnimationFinished() {
            switch (animState) {
                case AnimState::Playing_A_Start:
                    VPET_LOG_I("FSM", "A_Start finished -> Transitioning to B_Loop");
                    _transitionToBLoop();
                    break;

                case AnimState::Playing_B_Loop:
                    if (forceLoop) {
                        VPET_LOG_V("FSM", "B_Loop finished -> Force loop enabled, repeating B_Loop");
                        _playPhase(AnimatType::B_Loop);
                    } else {
                        loopTimes++;
                        int rnd = Utils::randomInt(loopTimes);
                        VPET_LOG_V("FSM", "B_Loop finished -> Loop count: %d/%d (Dice: %d)", loopTimes, maxLoopLength, rnd);
                        if (rnd > maxLoopLength) {
                            VPET_LOG_I("FSM", "B_Loop finished -> Dice > maxLoopLength, transitioning to C_End");
                            _transitionToCEnd();
                        } else {
                            _playPhase(AnimatType::B_Loop);
                        }
                    }
                    break;

                case AnimState::Playing_C_End:
                    VPET_LOG_I("FSM", "C_End finished -> Returning to Normal");
                    animState = AnimState::Idle;
                    displayToNormal();
                    break;

                case AnimState::Playing_Single:
                    VPET_LOG_I("FSM", "Single animation finished -> Returning to Normal");
                    animState = AnimState::Idle;
                    displayToNormal();
                    break;

                default:
                    animState = AnimState::Idle;
                    break;
            }
        }

        void _transitionToBLoop() {
            const AnimationEntry* entry = graph->findGraph(
                currentGraphName, AnimatType::B_Loop, save->calculateMode());
            if (entry) {
                VPET_LOG_V("FSM", "  Found B_Loop entry: %s", entry->path);
                _playEntry(entry);
                animState = AnimState::Playing_B_Loop;
            } else {
                VPET_LOG_W("FSM", "  B_Loop entry not found for: %s, skipping to C_End", currentGraphName);
                _transitionToCEnd();
            }
        }

        void _transitionToCEnd() {
            const AnimationEntry* entry = graph->findGraph(
                currentGraphName, AnimatType::C_End, save->calculateMode());
            if (entry) {
                VPET_LOG_V("FSM", "  Found C_End entry: %s", entry->path);
                _playEntry(entry);
                animState = AnimState::Playing_C_End;
            } else {
                VPET_LOG_W("FSM", "  C_End entry not found for: %s, returning to Idle", currentGraphName);
                animState = AnimState::Idle;
                displayToNormal();
            }
        }

        void _playPhase(AnimatType phase) {
            const AnimationEntry* entry = graph->findGraph(
                currentGraphName, phase, save->calculateMode());
            if (entry) {
                _playEntry(entry);
            }
        }

        void _startChain(GraphType type) {
            const char* name = graph->findName(type);
            if (!name) {
                VPET_LOG_W("FSM", "Failed to start chain: No name found for GraphType %u", (uint8_t)type);
                displayDefault();
                return;
            }

            VPET_LOG_I("FSM", "Starting animation chain: %s (Type: %u)", name, (uint8_t)type);
            strncpy(currentGraphName, name, 31);
            currentGraphName[31] = '\0';
            currentGraphType = type;
            maxLoopLength = graph->config.getDuration(name);

            const AnimationEntry* entry = graph->findGraph(
                name, AnimatType::A_Start, save->calculateMode());
            if (entry) {
                VPET_LOG_V("FSM", "  Chain A_Start entry: %s", entry->path);
                _playEntry(entry);
                animState = AnimState::Playing_A_Start;
            } else {
                VPET_LOG_I("FSM", "  A_Start not found, trying Single...");
                entry = graph->findGraph(name, AnimatType::Single, save->calculateMode());
                if (entry) {
                    VPET_LOG_V("FSM", "  Chain Single entry: %s", entry->path);
                    _playEntry(entry);
                    animState = AnimState::Playing_Single;
                } else {
                    VPET_LOG_E("FSM", "  No suitable entry found to start chain for: %s", name);
                    displayDefault();
                }
            }
        }

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

        void _playEntry(const AnimationEntry* entry) {
            if (!entry || !player) return;
            VPET_LOG_V("FSM", "  Executing play on entry: %s", entry->path);
            player->load(entry->path);
            bool isLoop = (entry->info.animat == AnimatType::B_Loop);
            player->start(isLoop);
        }
    };

} // namespace VPet
