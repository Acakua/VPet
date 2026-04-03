// FoodAnimation.cpp
// Port logic từ: FoodAnimation.cs

#include "FoodAnimation.hpp"
#include <Arduino.h>
#include "GraphCore.hpp"

namespace VPet {

    // ================================================================
    // start() — Khởi tạo chuỗi hoạt ảnh
    // Port từ FoodAnimation.cs::Run() dòng 245-282
    // ================================================================
    void FoodAnimationPlayer::start(FoodAnimation* anim, GraphCore* graphCore) {
        if (!anim || !graphCore) return;

        if (control.isPlaying()) {
            // Stop logic...
            stop();
        }

        currentAnim = anim;
        currentFrameIdx = 0;
        control.reset();

        // 1. Tải Front & Back graph resources
        const AnimationEntry* fl = graphCore->findGraph(anim->frontLayName, anim->info.animat, anim->info.modeType);
        const AnimationEntry* bl = graphCore->findGraph(anim->backLayName, anim->info.animat, anim->info.modeType);

        if (!frontPlayer) {
            frontPlayer = new AnimationPlayer();
        }
        if (!backPlayer) {
            backPlayer = new AnimationPlayer();
        }

        if (fl) {
            frontPlayer->load(fl->path);
            frontPlayer->start(false); // Play single/A_Start/etc logic based on control
        }
        if (bl) {
            backPlayer->load(bl->path);
            backPlayer->start(false);
        }

        // 2. Tải frame đồ ăn đầu tiên
        if (anim->frames.size() > 0) {
            currentFoodState = anim->frames[0];
            frameStartTime = millis();
        }
    }

    // ================================================================
    // tick() — Cập nhật logic (gọi mỗi vòng loop)
    // C# dòng 148-213: parent.Animations[parent.nowid].Run(This, Control) (với Thread.Sleep)
    // ESP32: Dùng `millis()` để non-blocking
    // ================================================================
    bool FoodAnimationPlayer::tick() {
        if (!currentAnim || currentAnim->frames.empty()) return false;

        bool updated = false;

        // Tick front/back players
        if (frontPlayer) {
            if (frontPlayer->tick()) updated = true;
        }
        if (backPlayer) {
            if (backPlayer->tick()) updated = true;
        }

        // Logic điều khiển đồ ăn (State Machine của FoodFrame)
        uint32_t now = millis();
        if (now - frameStartTime >= currentAnim->frames[currentFrameIdx].timeMs) {
            // Hết frame
            frameStartTime = now; // Hoặc = now cho frame kế tiếp

            switch (control.type) {
                case TaskControl::ControlType::Stop:
                    if (control.endAction) {
                        control.endAction(control.endActionContext);
                    }
                    stop();
                    return true;

                case TaskControl::ControlType::Status_Stoped:
                    return updated;

                case TaskControl::ControlType::Status_Quo:
                case TaskControl::ControlType::Continue:
                    currentFrameIdx++;
                    if (currentFrameIdx >= currentAnim->frames.size()) {
                        if (currentAnim->isLoop) {
                            currentFrameIdx = 0; // Loop lại food
                        } else if (control.type == TaskControl::ControlType::Continue) {
                            control.type = TaskControl::ControlType::Status_Quo;
                            currentFrameIdx = 0;
                        } else {
                            // C_End reached
                            control.type = TaskControl::ControlType::Status_Stoped;
                            currentFoodState.isVisible = false;
                            
                            if (control.endAction) {
                                control.endAction(control.endActionContext);
                            }
                            return true;
                        }
                    }
                    // Chuyển frame thành công
                    currentFoodState = currentAnim->frames[currentFrameIdx];
                    updated = true;
                    break;
            }
        }

        return updated;
    }

    // ================================================================
    // stop()
    // ================================================================
    void FoodAnimationPlayer::stop() {
        control.type = TaskControl::ControlType::Status_Stoped;
        control.setPlaying(false);
        
        if (frontPlayer) frontPlayer->stop();
        if (backPlayer) backPlayer->stop();
    }

} // namespace VPet
