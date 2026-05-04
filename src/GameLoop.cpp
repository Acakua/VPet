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
        // ====================================================================
        // Khởi tạo danh sách công việc & học tập (Port từ vup.lps dự án gốc)
        // Lưu ý: Tên graph phải khớp chính xác với thư mục trên SD Card
        // ====================================================================
        
        // 0. Dọn dẹp (Work)
        strncpy(works[0].name, "Dọn dẹp", 31);
        strncpy(works[0].graph, "WorkClean", 31);
        works[0].type = WorkType::Work;
        works[0].moneyBase = 20;
        works[0].strengthFood = 3;
        works[0].strengthDrink = 2;
        works[0].feeling = 1;
        works[0].timeMinutes = 30;
        works[0].levelLimit = 0;

        // 1. Lao động (Work)
        strncpy(works[1].name, "Lao động", 31);
        strncpy(works[1].graph, "WorkONE", 31);
        works[1].type = WorkType::Work;
        works[1].moneyBase = 40;
        works[1].strengthFood = 5;
        works[1].strengthDrink = 5;
        works[1].feeling = -2;
        works[1].timeMinutes = 60;
        works[1].levelLimit = 0;

        // 2. Lập trình (Work)
        strncpy(works[2].name, "Lập trình", 31);
        strncpy(works[2].graph, "WorkTWO", 31);
        works[2].type = WorkType::Work;
        works[2].moneyBase = 100;
        works[2].strengthFood = 8;
        works[2].strengthDrink = 10;
        works[2].feeling = -5;
        works[2].timeMinutes = 120;
        works[2].levelLimit = 0;

        // 3. Tự học (Study)
        strncpy(works[3].name, "Tự học", 31);
        strncpy(works[3].graph, "Study", 31);
        works[3].type = WorkType::Study;
        works[3].moneyBase = 0;
        works[3].strengthFood = 2;
        works[3].strengthDrink = 2;
        works[3].feeling = 2;
        works[3].timeMinutes = 45;
        works[3].levelLimit = 0;

        // 4. Nghiên cứu (Study)
        strncpy(works[4].name, "Nghiên cứu", 31);
        strncpy(works[4].graph, "StudyTWO", 31);
        works[4].type = WorkType::Study;
        works[4].moneyBase = 0;
        works[4].strengthFood = 4;
        works[4].strengthDrink = 4;
        works[4].feeling = 4;
        works[4].timeMinutes = 90;
        works[4].levelLimit = 0;

        // 5. Vẽ tranh (Study)
        strncpy(works[5].name, "Vẽ tranh", 31);
        strncpy(works[5].graph, "StudyPaint", 31);
        works[5].type = WorkType::Study;
        works[5].moneyBase = 0;
        works[5].strengthFood = 3;
        works[5].strengthDrink = 2;
        works[5].feeling = 5;
        works[5].timeMinutes = 60;
        works[5].levelLimit = 0;

        // 6. Chơi Game (Play)
        strncpy(works[6].name, "Chơi Game", 31);
        strncpy(works[6].graph, "PlayONE", 31);
        works[6].type = WorkType::Play;
        works[6].moneyBase = 0;
        works[6].strengthFood = 1;
        works[6].strengthDrink = 1;
        works[6].feeling = 10;
        works[6].timeMinutes = 30;
        works[6].levelLimit = 0;

        // 7. Nhảy dây (Play)
        strncpy(works[7].name, "Nhảy dây", 31);
        strncpy(works[7].graph, "RopeSkipping", 31);
        works[7].type = WorkType::Play;
        works[7].moneyBase = 0;
        works[7].strengthFood = 10;
        works[7].strengthDrink = 10;
        works[7].feeling = 5;
        works[7].timeMinutes = 15;
        works[7].levelLimit = 0;

        // 8. Sửa lỗi (Play)
        strncpy(works[8].name, "Sửa lỗi", 31);
        strncpy(works[8].graph, "RemoveObject", 31);
        works[8].type = WorkType::Play;
        works[8].moneyBase = 0;
        works[8].strengthFood = 2;
        works[8].strengthDrink = 2;
        works[8].feeling = 2;
        works[8].timeMinutes = 40;
        works[8].levelLimit = 0;

        // 9. Thư pháp (Study)
        strncpy(works[9].name, "Thư pháp", 31);
        strncpy(works[9].graph, "Calligraphy", 31);
        works[9].type = WorkType::Study;
        works[9].moneyBase = 0;
        works[9].strengthFood = 1;
        works[9].strengthDrink = 1;
        works[9].feeling = 3;
        works[9].timeMinutes = 60;
        works[9].levelLimit = 0;
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
