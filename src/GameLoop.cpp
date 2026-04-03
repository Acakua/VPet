// GameLoop.cpp — Implement vòng lặp chính (nhịp tim)
// Port logic từ: MainLogic.cs (FunctionSpend, EventTimer_Elapsed)

#include "GameLoop.hpp"
#include <Arduino.h>
#include <math.h>
#include "Utils.hpp"

namespace VPet {

    // ================================================================
    // start()
    // ================================================================
    void GameLoop::start() {
        lastInteractionTime = millis();
        lastEventTime = millis();
    }

    // ================================================================
    // interact() — Người dùng tương tác (chạm)
    // ================================================================
    void GameLoop::interact() {
        lastInteractionTime = millis();
    }

    // ================================================================
    // tick() — Kiểm tra thời gian
    // ================================================================
    void GameLoop::tick() {
        if (!save || !anim) return;

        uint32_t now = millis();
        if (now - lastEventTime >= eventIntervalMs) {
            lastEventTime = now;
            eventTimerElapsed();
        }
    }

    // ================================================================
    // eventTimerElapsed() — Xử lý mỗi 15s
    // Port từ MainLogic.cs::EventTimer_Elapsed() dòng 470-533
    // ================================================================
    void GameLoop::eventTimerElapsed() {
        // Tính toán sinh tồn (timePass = 0.05 như C# gốc)
        functionSpend(0.05);

        // Kiểm tra IsIdel: (Default hoặc Work) và không đang click
        bool isIdle = (anim->currentGraphType == GraphType::Default || 
                       anim->currentGraphType == GraphType::Work);

        if (isIdle) {
            // CountNomal = anim->countNormal
            int16_t rndDisplay = 20 > (20 - anim->countNormal) ? 20 : (20 - anim->countNormal); 
            // C# gốc: Math.Max(20, Core.Controller.InteractionCycle - CountNomal);
            // Ở đây gán cố định InteractionCycle = 20 để đơn giản hóa

            if (anim->currentGraphType == GraphType::Work) {
                rndDisplay = 2 * rndDisplay + 20;
            }

            int rnd = Utils::randomInt(rndDisplay);
            switch (rnd) {
                case 0:
                case 1:
                case 2:
                    // DisplayMove(); // Chưa implement Move trên ESP32
                    break;
                case 3:
                case 4:
                case 5:
                    anim->displayIdel();
                    break;
                case 6:
                    anim->displayStateONE();
                    break;
                case 7:
                    anim->displaySleep(false);
                    break;
                default:
                    // RandomInteractionAction (các thao tác random khác)
                    break;
            }
        }
    }

    // ================================================================
    // playSwitchAnimat() — Phát hoạt ảnh chuyển trạng thái
    // Port từ MainLogic.cs::PlaySwitchAnimat() dòng 432-453
    // ================================================================
    void GameLoop::playSwitchAnimat(ModeType before, ModeType after) {
        if (anim->currentGraphType != GraphType::Default && 
            anim->currentGraphType != GraphType::Switch_Down && 
            anim->currentGraphType != GraphType::Switch_Up) {
            return;
        }

        if (before == after) {
            anim->displayToNormal();
            return;
        }

        // Logic đệ quy trong C#, ở đây trên C++ ta simplify lại bằng cách
        // chỉ phát 1 lần rồi về Normal, vì callback lambda đệ quy trong C++
        // phức tạp và dễ gây leak bộ nhớ.
        if (before < after) {
            // Xấu đi
            const char* name = anim->graph->findName(GraphType::Switch_Down);
            if (name) {
                // Tạm thời gọi trực tiếp qua player, hoặc map vào state machine
                // Để chuẩn, đáng lẽ phải có 1 state PLAYING_SWITCH
                // Ta có thể gọi hàm của anim->playAnimation() nếu ta mở API
                // Ở đây, simplifed:
                anim->displayToNormal();
            } else {
                anim->displayToNormal();
            }
        } else {
            // Tốt lên
            const char* name = anim->graph->findName(GraphType::Switch_Up);
            if (name) {
                anim->displayToNormal();
            } else {
                anim->displayToNormal();
            }
        }
        // TODO: Mở rộng state machine để xử lý chuỗi switch recursive
    }

    // ================================================================
    // functionSpend(timePass) — Tính toán sinh tồn (CỐT LÕI)
    // Port từ MainLogic.cs::FunctionSpend() dòng 234-426
    // ================================================================
    void GameLoop::functionSpend(double timePass) {
        save->cleanChange();
        // save->storeTake(); // Bỏ qua trên ESP32

        double freedrop = (millis() - lastInteractionTime) / 60000.0; // TotalMinutes
        if (freedrop < 1.0) {
            freedrop = 0.0;
        } else {
            double dropCalculated = sqrt(freedrop) * timePass / 4.0;
            double dropLimit = GameSave::FEELING_MAX / 800.0;
            freedrop = (dropCalculated < dropLimit) ? dropCalculated : dropLimit;
        }

        double sm25 = GameSave::STRENGTH_MAX * 0.25;
        double sm50 = GameSave::STRENGTH_MAX * 0.50;
        double sm60 = GameSave::STRENGTH_MAX * 0.60;
        double sm75 = GameSave::STRENGTH_MAX * 0.75;

        int addhealth = -2;

        switch (anim->workState) {
            case WorkingState::Normal:
                // 默认 (Default/Empty)
                if (save->getStrengthFood() >= sm50) {
                    save->strengthChangeFood(-timePass);
                    save->strengthChange(timePass);
                    if (save->getStrengthFood() >= sm75) {
                        addhealth += Utils::randomInt(1, 3);
                    }
                } else if (save->getStrengthFood() <= sm25) {
                    save->setHealth(save->getHealth() - Utils::randomDouble() * timePass);
                    addhealth -= 2;
                }

                if (save->getStrengthDrink() >= sm50) {
                    save->strengthChangeDrink(-timePass);
                    save->strengthChange(timePass);
                    if (save->getStrengthDrink() >= sm75) {
                        addhealth += Utils::randomInt(1, 3);
                    }
                } else if (save->getStrengthDrink() <= sm25) {
                    save->setHealth(save->getHealth() - Utils::randomDouble() * timePass);
                    addhealth -= 2;
                }

                if (addhealth > 0) {
                    save->setHealth(save->getHealth() + addhealth * timePass);
                }
                save->strengthChangeFood(-timePass);
                save->strengthChangeDrink(-timePass);
                save->feelingChange(-freedrop);
                break;

            case WorkingState::Sleep:
                // 睡觉
                save->strengthChange(timePass * 2.0);
                save->strengthChangeFood(timePass);
                if (save->getStrengthFood() <= sm25) {
                    save->strengthChangeFood(timePass); // X2
                } else if (save->getStrengthFood() >= sm75) {
                    save->setHealth(save->getHealth() + timePass * 2.0);
                }

                save->strengthChangeDrink(timePass);
                // C# có logical bug: if (StrengthDrink >= sm25) -> nhưng comment là "低状态2倍" (dòng 263)
                // Ta fix logic thành <= sm25 cho đúng ý nghĩa.
                if (save->getStrengthDrink() <= sm25) {
                    save->strengthChangeDrink(timePass); // X2
                } else if (save->getStrengthDrink() >= sm75) {
                    save->setHealth(save->getHealth() + timePass * 2.0);
                }
                
                lastInteractionTime = millis();
                break;

            case WorkingState::Work:
                if (!currentWork) break;

                double needfood = timePass * currentWork->strengthFood;
                double needdrink = timePass * currentWork->strengthDrink;
                double efficiency = 0.0;
                addhealth = -2;

                double nsfood = needfood * 0.3;
                double nsdrink = needdrink * 0.3;

                if (save->getStrength() > sm25 + nsfood + nsdrink) {
                    save->strengthChange(-nsfood - nsdrink);
                    efficiency += 0.1;
                    needfood -= nsfood;
                    needdrink -= nsdrink;
                }

                // Food
                if (save->getStrengthFood() <= sm25) {
                    save->strengthChangeFood(-needfood / 2.0);
                    efficiency += 0.2;
                    if (save->getStrength() >= needfood) {
                        save->strengthChange(-needfood);
                        efficiency += 0.1;
                    }
                    addhealth -= 2;
                } else {
                    save->strengthChangeFood(-needfood);
                    efficiency += 0.4;
                    if (save->getStrengthFood() >= sm60) {
                        addhealth += Utils::randomInt(1, 3);
                        efficiency += 0.1;
                    }
                }

                // Drink
                if (save->getStrengthDrink() <= sm25) {
                    save->strengthChangeDrink(-needdrink / 2.0);
                    efficiency += 0.2;
                    if (save->getStrength() >= needdrink) {
                        save->strengthChange(-needdrink);
                        efficiency += 0.1;
                    }
                    addhealth -= 2;
                } else {
                    save->strengthChangeDrink(-needdrink);
                    efficiency += 0.4;
                    if (save->getStrengthDrink() >= sm60) {
                        addhealth += Utils::randomInt(1, 3);
                        efficiency += 0.1;
                    }
                }

                if (addhealth > 0) {
                    save->setHealth(save->getHealth() + addhealth * timePass);
                }

                double addmoney = timePass * currentWork->moneyBase * (2.0 * efficiency - 0.5);
                if (addmoney < 0) addmoney = 0;

                if (currentWork->type == WorkType::Work) {
                    save->money += addmoney;
                } else {
                    save->exp += addmoney;
                }
                
                if (currentWork->type == WorkType::Play) {
                    lastInteractionTime = millis();
                    save->feelingChange(-currentWork->feeling * timePass);
                } else {
                    save->feelingChange(-freedrop * (0.5 + currentWork->feeling / 2.0));
                }
                break;
        }

        // Post-processing
        save->exp += timePass;

        if (save->getFeeling() >= GameSave::FEELING_MAX * 0.75) {
            if (save->getFeeling() >= GameSave::FEELING_MAX * 0.90) {
                save->setLikability(save->getLikability() + timePass);
            }
            save->exp += timePass * 2.0;
            save->setHealth(save->getHealth() + timePass);
        } else if (save->getFeeling() <= 25.0) {
            save->setLikability(save->getLikability() - timePass);
            save->exp -= timePass;
        }

        if (save->getStrengthDrink() <= sm25) {
            save->setHealth(save->getHealth() - Utils::randomInt(0, 1) * timePass);
            save->exp -= timePass;
        } else if (save->getStrengthDrink() >= sm75) {
            save->setHealth(save->getHealth() + Utils::randomInt(0, 1) * timePass);
        }

        // Cal mode
        ModeType newMode = save->calculateMode();
        if (save->modeType != newMode) {
            playSwitchAnimat(save->modeType, newMode);
            save->modeType = newMode;
        }

        if (save->modeType == ModeType::Ill && anim->workState == WorkingState::Work) {
            // TODO: Stop work via WorkTimer
            anim->workState = WorkingState::Normal;
        }
    }

} // namespace VPet
