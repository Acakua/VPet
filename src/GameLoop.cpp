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

        // Khởi tạo 10 công việc mặc định (Port từ vup.lps)
        // 1. Văn bản (Work)
        strncpy(works[0].name, "Copywriting", 31);
        strncpy(works[0].graph, "workone", 31);
        works[0].type = WorkType::Work;
        works[0].moneyBase = 8;
        works[0].strengthFood = 3.5;
        works[0].strengthDrink = 2.5;
        works[0].feeling = 1;
        works[0].timeMinutes = 60;
        works[0].finishBonus = 0.1;

        // 2. Dọn màn hình (Work)
        strncpy(works[1].name, "Screen Clean", 31);
        strncpy(works[1].graph, "workclean", 31);
        works[1].type = WorkType::Work;
        works[1].moneyBase = 16;
        works[1].strengthFood = 5;
        works[1].strengthDrink = 5;
        works[1].feeling = 2.5;
        works[1].timeMinutes = 90;
        works[1].levelLimit = 10;
        works[1].finishBonus = 0.2;

        // 3. Livestream (Work)
        strncpy(works[2].name, "Live Stream", 31);
        strncpy(works[2].graph, "worktwo", 31);
        works[2].type = WorkType::Work;
        works[2].moneyBase = 28;
        works[2].strengthFood = 5;
        works[2].strengthDrink = 10;
        works[2].feeling = 4;
        works[2].timeMinutes = 180;
        works[2].levelLimit = 20;
        works[2].finishBonus = 0.25;

        // 4. Học tập (Study)
        strncpy(works[3].name, "Learning", 31);
        strncpy(works[3].graph, "study", 31);
        works[3].type = WorkType::Study;
        works[3].moneyBase = 80;
        works[3].strengthFood = 2;
        works[3].strengthDrink = 2;
        works[3].feeling = 3;
        works[3].timeMinutes = 45;
        works[3].finishBonus = 0.2;

        // 5. Nghiên cứu (Study)
        strncpy(works[4].name, "Research", 31);
        strncpy(works[4].graph, "studytwo", 31);
        works[4].type = WorkType::Study;
        works[4].moneyBase = 120;
        works[4].strengthFood = 2.5;
        works[4].strengthDrink = 3.5;
        works[4].feeling = 4;
        works[4].timeMinutes = 75;
        works[4].levelLimit = 15;
        works[4].finishBonus = 0.4;

        // 6. Chơi game (Play)
        strncpy(works[5].name, "Gaming", 31);
        strncpy(works[5].graph, "playone", 31);
        works[5].type = WorkType::Play;
        works[5].moneyBase = 18;
        works[5].strengthFood = 1;
        works[5].strengthDrink = 1.5;
        works[5].feeling = -1;
        works[5].timeMinutes = 30;
        works[5].finishBonus = 0.2;

        // 7. Sửa lỗi (Play)
        strncpy(works[6].name, "Bug Fix", 31);
        strncpy(works[6].graph, "removeobject", 31);
        works[6].type = WorkType::Play;
        works[6].moneyBase = 18;
        works[6].strengthFood = 0.5;
        works[6].strengthDrink = 0.5;
        works[6].feeling = -0.5;
        works[6].timeMinutes = 60;
        works[6].levelLimit = 6;
        works[6].finishBonus = 0.25;

        // 8. Nhảy dây (Play)
        strncpy(works[7].name, "Jump Rope", 31);
        strncpy(works[7].graph, "ropeskipping", 31);
        works[7].type = WorkType::Play;
        works[7].moneyBase = 10;
        works[7].strengthFood = 1;
        works[7].strengthDrink = 1;
        works[7].feeling = 0.5;
        works[7].timeMinutes = 10;
        works[7].levelLimit = 12;
        works[7].finishBonus = 0.2;

        // 9. Thư pháp (Study)
        strncpy(works[8].name, "Calligraphy", 31);
        strncpy(works[8].graph, "calligraphy", 31);
        works[8].type = WorkType::Study;
        works[8].strengthFood = 1;
        works[8].strengthDrink = 1;
        works[8].feeling = 1;
        works[8].timeMinutes = 20;
        works[8].levelLimit = 8;
        works[8].finishBonus = 0.2;

        // 10. Vẽ tranh (Study)
        strncpy(works[9].name, "Painting", 31);
        strncpy(works[9].graph, "studypaint", 31);
        works[9].type = WorkType::Study;
        works[9].strengthFood = 2.2;
        works[9].strengthDrink = 1.2;
        works[9].feeling = 0.8;
        works[9].timeMinutes = 120;
        works[9].levelLimit = 25;
        works[9].finishBonus = 0.25;
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
    // eventTimerElapsed() — Xử lý mỗi nhịp thở (Debug: Tăng tốc tiêu thụ)
    // ================================================================
    void GameLoop::eventTimerElapsed() {
        // Debug: Tăng timePass từ 0.05 lên 0.5 để thấy chỉ số tụt nhanh
        functionSpend(0.5);

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

        switch (state) {
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
                if (!nowWork) break;

                double needfood = timePass * nowWork->strengthFood;
                double needdrink = timePass * nowWork->strengthDrink;
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

                double addmoney = timePass * nowWork->moneyBase * (2.0 * efficiency - 0.5);
                if (addmoney < 0) addmoney = 0;

                if (nowWork->type == WorkType::Work) {
                    save->money += addmoney;
                } else {
                    save->exp += addmoney;
                }
                
                if (nowWork->type == WorkType::Play) {
                    lastInteractionTime = millis();
                    save->feelingChange(-nowWork->feeling * timePass);
                } else {
                    save->feelingChange(-freedrop * (0.5 + nowWork->feeling / 2.0));
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

        if (save->modeType == ModeType::Ill && state == WorkingState::Work) {
            VPET_LOG_W("LOOP", "Pet is Ill, stopping work.");
            stopWork();
        }

        VPET_LOG_V("PET", "Stats: HLT %.1f, STR %.1f, FD %.1f, DK %.1f, FLG %.1f, MD %s", 
                  save->getHealth(), save->getStrength(), save->getStrengthFood(), 
                  save->getStrengthDrink(), save->getFeeling(), _modeTypeName(save->modeType));
        
        VPET_TIME_END(SurvivalLogic, "LOOP");
    }

    // ================================================================
    // startWork(index)
    // ================================================================
    bool GameLoop::startWork(int index) {
        if (index < 0 || index >= WORK_COUNT) return false;
        Work* w = &works[index];

        if (save->modeType == ModeType::Ill) {
            VPET_LOG_W("LOOP", "Cannot work while Ill.");
            return false;
        }

        /* Tạm thời bỏ qua kiểm tra level để debug 
        if (save->getLevel() < w->levelLimit) {
            VPET_LOG_W("LOOP", "Level too low: %d < %d", save->getLevel(), w->levelLimit);
            return false;
        }
        */

        nowWork = w;
        state = WorkingState::Work;
        anim->displayWork(w->graph);
        
        VPET_LOG_I("LOOP", "Started work: %s (Graph: %s)", w->name, w->graph);
        return true;
    }

    // ================================================================
    // stopWork()
    // ================================================================
    void GameLoop::stopWork() {
        if (state != WorkingState::Work) return;
        
        VPET_LOG_I("LOOP", "Stopping work: %s", nowWork ? nowWork->name : "None");
        nowWork = nullptr;
        state = WorkingState::Normal;
        anim->displayToNormal();
    }

} // namespace VPet
