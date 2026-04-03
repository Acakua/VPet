#pragma once

#include <stdint.h>
#include <math.h>
#include <algorithm>
#include <SD.h>

namespace VPet {

    // ========================================================================
    // ModeType — Trạng thái Pet (Port từ IGameSave.cs dòng 141-158)
    // ========================================================================
    enum class ModeType {
        Happy,          // 高兴 — Vui vẻ
        Normal,         // 普通 — Bình thường
        PoorCondition,  // 状态不佳 — Sức khỏe kém
        Ill             // 生病 — Ốm/Bệnh
    };

    // ========================================================================
    // Food — Cấu trúc thức ăn (Port từ IFood.cs)
    // ========================================================================
    struct Food {
        int exp;                // 经验值
        double strength;        // 体力
        double strengthFood;    // 饱腹度
        double strengthDrink;   // 口渴度
        double feeling;         // 心情
        double health;          // 健康
        double likability;      // 好感度
    };

    // ========================================================================
    // GameSave — Logic sinh tồn của Pet
    // Port từ: VPet-Simulator.Core/Handle/GameSave.cs (toàn bộ 345 dòng)
    // ========================================================================
    class GameSave {
    public:
        // --- Thông tin cơ bản ---
        char name[32];          // 宠物名字 (tối đa 31 ký tự + null)
        char hostName[32];      // 主人称呼

        // --- Kinh tế & Kinh nghiệm ---
        double money;           // 金钱
        double exp;             // 经验值

        // --- Trạng thái hiện tại ---
        ModeType modeType;

        // Hằng số tính toán (C# gốc)
        static constexpr double STRENGTH_MAX = 100.0;
        static constexpr double FEELING_MAX  = 100.0;

        // --- Các phương thức công khai ---
        GameSave();
        void initNewGame(const char* petName);
        
        // Lưu trữ SD Card (Phân hệ 4D)
        bool save(const char* filepath);
        bool load(const char* filepath);

        // Cấp độ (Port từ GameSave.cs dòng 32, 37)
        int getLevel() const;
        int levelUpNeed() const;
        double getExpBonus() const;

        // Getter cho các chỉ số (đọc qua getter vì setter có logic clamp)
        double getStrength() const;
        double getStrengthFood() const;
        double getStrengthDrink() const;
        double getFeeling() const;
        double getHealth() const;
        double getLikability() const;
        double getStrengthMax() const;
        double getFeelingMax() const;
        double getLikabilityMax() const;

        // Setter có logic clamp (Port chính xác từ GameSave.cs property setters)
        void setStrength(double value);
        void setStrengthFood(double value);
        void setStrengthDrink(double value);
        void setFeeling(double value);
        void setHealth(double value);
        void setLikability(double value);

        // Thay đổi chỉ số (Port từ GameSave.cs dòng 56-157)
        void strengthChange(double value);
        void strengthChangeFood(double value);
        void strengthChangeDrink(double value);
        void feelingChange(double value);

        // Buffer thay đổi
        double getChangeStrength() const;
        double getChangeStrengthFood() const;
        double getChangeStrengthDrink() const;
        double getChangeFeeling() const;

        // Store/Buffer hồi phục (Port từ GameSave.cs dòng 191-225)
        void cleanChange();
        void storeTake();

        // Ăn uống (Port từ GameSave.cs dòng 230-245)
        void eatFood(const Food& food);

        // Tính trạng thái (Port từ GameSave.cs dòng 262-290)
        ModeType calculateMode();

        // Store buffers — public để MainLogic có thể truy cập
        double storeStrength;
        double storeStrengthFood;
        double storeStrengthDrink;

    private:
        // Giá trị lưu trữ thực (backing fields, tương tự C# protected double strength)
        double _strength;
        double _strengthFood;
        double _strengthDrink;
        double _feeling;
        double _health;
        double _likability;

        // Buffer biến động (Change tracking)
        double _changeStrength;
        double _changeStrengthFood;
        double _changeStrengthDrink;
        double _changeFeeling;
    };

} // namespace VPet
