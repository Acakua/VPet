// GameLoop.cpp — Implement vòng lặp chính (nhịp tim)
// Port logic từ: MainLogic.cs (FunctionSpend, EventTimer_Elapsed)

#include "GameLoop.hpp"
#include <Arduino.h>
#include <math.h>
#include "Utils.hpp"

namespace VPet {

    const char* _modeTypeName(ModeType m) {
        switch (m) {
            case ModeType::Happy: return "Happy";
            case ModeType::Normal: return "Normal";
            case ModeType::PoorCondition: return "PoorCondition";
            case ModeType::Ill: return "Ill";
            default: return "Unknown";
        }
    }

    // ================================================================
    // start()
    // ================================================================
    void GameLoop::start() {
        VPET_LOG_I("LOOP", "GameLoop started. Interval: %u ms", eventIntervalMs);
        lastInteractionTime = millis();
        lastEventTime = millis();
    }

    // ================================================================
    // interact() — Người dùng tương tác (chạm)
    // ================================================================
    void GameLoop::interact() {
        VPET_LOG_D("LOOP", "Interaction detected, resetting idle timers.");
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
            VPET_LOG_D("LOOP", "Event timer pulse (15s)");
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
            
            if (anim->currentGraphType == GraphType::Work) {
                rndDisplay = 2 * rndDisplay + 20;
            }

            int rnd = Utils::randomInt(rndDisplay);
            VPET_LOG_V("LOOP", "Idle check - CountNormal: %d, Max: %d, Dice: %d", anim->countNormal, rndDisplay, rnd);

            switch (rnd) {
                case 0:
                case 1:
                case 2:
                    VPET_LOG_V("LOOP", "Dice rolled Move (not implemented)");
                    break;
                case 3:
                case 4:
                case 5:
                    VPET_LOG_I("LOOP", "Dice rolled -> displayIdel()");
                    anim->displayIdel();
                    break;
                case 6:
                    VPET_LOG_I("LOOP", "Dice rolled -> displayStateONE()");
                    anim->displayStateONE();
                    break;
                case 7:
                    VPET_LOG_I("LOOP", "Dice rolled -> displaySleep(false)");
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
        VPET_LOG_I("LOOP", "Mode change detected: %s -> %s", _modeTypeName(before), _modeTypeName(after));

        if (anim->currentGraphType != GraphType::Default && 
            anim->currentGraphType != GraphType::Switch_Down && 
            anim->currentGraphType != GraphType::Switch_Up) {
            VPET_LOG_V("LOOP", "Skip switch animation (busy with %u)", (uint8_t)anim->currentGraphType);
            return;
        }

        if (before == after) {
            anim->displayToNormal();
            return;
        }

        if (before < after) {
            // Xấu đi
            VPET_LOG_I("LOOP", "Trigger Switch_Down animation");
            anim->displayToNormal();
        } else {
            // Tốt lên
            VPET_LOG_I("LOOP", "Trigger Switch_Up animation");
            anim->displayToNormal();
        }
    }

    // ================================================================
    // functionSpend(timePass) — Tính toán sinh tồn (CỐT LÕI)
    // Port từ MainLogic.cs::FunctionSpend() dòng 234-426
    // ================================================================
    void GameLoop::functionSpend(double timePass) {
        VPET_TIME_START(SurvivalLogic);

        save->cleanChange();

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
                save->strengthChange(timePass * 2.0);
                save->strengthChangeFood(timePass);
                if (save->getStrengthFood() <= sm25) {
                    save->strengthChangeFood(timePass); // X2
                } else if (save->getStrengthFood() >= sm75) {
                    save->setHealth(save->getHealth() + timePass * 2.0);
                }

                save->strengthChangeDrink(timePass);
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
            VPET_LOG_W("LOOP", "Pet is Ill, stopping work.");
            anim->workState = WorkingState::Normal;
        }

        VPET_LOG_V("PET", "Stats: HLT %.1f, STR %.1f, FD %.1f, DK %.1f, FLG %.1f, MD %s", 
                  save->getHealth(), save->getStrength(), save->getStrengthFood(), 
                  save->getStrengthDrink(), save->getFeeling(), _modeTypeName(save->modeType));
        
        VPET_TIME_END(SurvivalLogic, "LOOP");
    }

} // namespace VPet
