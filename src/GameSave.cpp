#include "GameSave.hpp"
#include <cstring>
#include <cmath>
#include <algorithm>

namespace VPet {

    // ========================================================================
    // Constructor — Tương đương GameSave() C# (dòng 310-322)
    // ========================================================================
    GameSave::GameSave()
        : money(0), exp(0), modeType(ModeType::Normal),
          _strength(0), _strengthFood(0), _strengthDrink(0),
          _feeling(0), _health(0), _likability(0),
          storeStrength(0), storeStrengthFood(0), storeStrengthDrink(0),
          _changeStrength(0), _changeStrengthFood(0),
          _changeStrengthDrink(0), _changeFeeling(0)
    {
        name[0] = '\0';
        hostName[0] = '\0';
    }

    // ========================================================================
    // initNewGame — Tương đương GameSave(string name) C# (dòng 294-306)
    // ========================================================================
    void GameSave::initNewGame(const char* petName) {
        strncpy(name, petName, sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';
        hostName[0] = '\0';

        money = 100;
        exp = 0;
        _strength = 100;
        _strengthFood = 100;
        _strengthDrink = 100;
        _feeling = 60;
        _health = 100;
        _likability = 0;

        storeStrength = 0;
        storeStrengthFood = 0;
        storeStrengthDrink = 0;
        _changeStrength = 0;
        _changeStrengthFood = 0;
        _changeStrengthDrink = 0;
        _changeFeeling = 0;

        modeType = calculateMode();
    }

    // ========================================================================
    // Cấp độ — Port từ GameSave.cs dòng 32, 37
    // ========================================================================

    // Level => Exp < 0 ? 1 : (int)(Math.Sqrt(Exp) / 10) + 1;
    int GameSave::getLevel() const {
        return exp < 0 ? 1 : (int)(sqrt(exp) / 10.0) + 1;
    }

    // LevelUpNeed() => (int)(Math.Pow((Level) * 10, 2));
    int GameSave::levelUpNeed() const {
        return (int)(pow(getLevel() * 10.0, 2.0));
    }

    // ExpBonus => 1;
    double GameSave::getExpBonus() const {
        return 1.0;
    }

    // ========================================================================
    // Getters — Đọc giá trị các chỉ số
    // ========================================================================

    double GameSave::getStrength() const      { return _strength; }
    double GameSave::getStrengthFood() const  { return _strengthFood; }
    double GameSave::getStrengthDrink() const { return _strengthDrink; }
    double GameSave::getFeeling() const       { return _feeling; }
    double GameSave::getHealth() const        { return _health; }
    double GameSave::getLikability() const    { return _likability; }

    double GameSave::getStrengthMax() const   { return STRENGTH_MAX; }
    double GameSave::getFeelingMax() const    { return FEELING_MAX; }

    // LikabilityMax => 90 + Level * 10;  (GameSave.cs dòng 252)
    double GameSave::getLikabilityMax() const {
        return 90.0 + getLevel() * 10.0;
    }

    // ========================================================================
    // Setters — Port chính xác logic clamp từ C# property setters
    // ========================================================================

    // C# dòng 41: Strength { set => strength = Math.Min(StrengthMax, Math.Max(0, value)); }
    void GameSave::setStrength(double value) {
        _strength = std::min(STRENGTH_MAX, std::max(0.0, value));
    }

    // C# dòng 64-77: StrengthFood setter — khi <= 0: trừ Health
    void GameSave::setStrengthFood(double value) {
        value = std::min(100.0, value);
        if (value <= 0) {
            setHealth(_health + value);  // Health += value (value là số âm → trừ máu)
            _strengthFood = 0;
        } else {
            _strengthFood = value;
        }
    }

    // C# dòng 97-110: StrengthDrink setter — khi <= 0: trừ Health
    void GameSave::setStrengthDrink(double value) {
        value = std::min(100.0, value);
        if (value <= 0) {
            setHealth(_health + value);
            _strengthDrink = 0;
        } else {
            _strengthDrink = value;
        }
    }

    // C# dòng 131-146: Feeling setter — khi <= 0: trừ Health/2 và Likability/2
    void GameSave::setFeeling(double value) {
        value = std::min(100.0, value);
        if (value <= 0) {
            setHealth(_health + value / 2.0);
            setLikability(_likability + value / 2.0);
            _feeling = 0;
        } else {
            _feeling = value;
        }
    }

    // C# dòng 162: Health { set => health = Math.Min(100, Math.Max(0, value)); }
    void GameSave::setHealth(double value) {
        _health = std::min(100.0, std::max(0.0, value));
    }

    // C# dòng 169-183: Likability setter — overflow vào Health
    void GameSave::setLikability(double value) {
        double maxVal = getLikabilityMax();
        value = std::max(0.0, value);
        if (value > maxVal) {
            _likability = maxVal;
            setHealth(_health + (value - maxVal));  // Overflow → tăng Health
        } else {
            _likability = value;
        }
    }

    // ========================================================================
    // Change Methods — Port từ GameSave.cs dòng 56-157
    // ========================================================================

    // C# dòng 56-60:
    // void StrengthChange(double value) { ChangeStrength += value; Strength += value; }
    void GameSave::strengthChange(double value) {
        _changeStrength += value;
        setStrength(_strength + value);
    }

    // C# dòng 85-89:
    // void StrengthChangeFood(double value) { ChangeStrengthFood += value; StrengthFood += value; }
    void GameSave::strengthChangeFood(double value) {
        _changeStrengthFood += value;
        setStrengthFood(_strengthFood + value);
    }

    // C# dòng 123-127:
    // void StrengthChangeDrink(double value) { ChangeStrengthDrink += value; StrengthDrink += value; }
    void GameSave::strengthChangeDrink(double value) {
        _changeStrengthDrink += value;
        setStrengthDrink(_strengthDrink + value);
    }

    // C# dòng 154-158:
    // void FeelingChange(double value) { ChangeFeeling += value; Feeling += value; }
    void GameSave::feelingChange(double value) {
        _changeFeeling += value;
        setFeeling(_feeling + value);
    }

    // Change getters
    double GameSave::getChangeStrength() const      { return _changeStrength; }
    double GameSave::getChangeStrengthFood() const  { return _changeStrengthFood; }
    double GameSave::getChangeStrengthDrink() const { return _changeStrengthDrink; }
    double GameSave::getChangeFeeling() const       { return _changeFeeling; }

    // ========================================================================
    // CleanChange — Port từ GameSave.cs dòng 191-197
    // Chia đôi tất cả các buffer biến động
    // ========================================================================
    void GameSave::cleanChange() {
        _changeStrength     /= 2.0;
        _changeFeeling      /= 2.0;
        _changeStrengthDrink /= 2.0;
        _changeStrengthFood  /= 2.0;
    }

    // ========================================================================
    // StoreTake — Port từ GameSave.cs dòng 201-225
    // Lấy 1/10 giá trị Store, nếu dư quá nhỏ (<1) thì xóa luôn
    // ========================================================================
    void GameSave::storeTake() {
        const int t = 10;

        // Strength store
        double s = storeStrength / t;
        storeStrength -= s;
        if (fabs(storeStrength) < 1.0) {
            storeStrength = 0;
        } else {
            strengthChange(s);
        }

        // Drink store
        s = storeStrengthDrink / t;
        storeStrengthDrink -= s;
        if (fabs(storeStrengthDrink) < 1.0) {
            storeStrengthDrink = 0;
        } else {
            strengthChangeDrink(s);
        }

        // Food store
        s = storeStrengthFood / t;
        storeStrengthFood -= s;
        if (fabs(storeStrengthFood) < 1.0) {
            storeStrengthFood = 0;
        } else {
            strengthChangeFood(s);
        }
    }

    // ========================================================================
    // EatFood — Port từ GameSave.cs dòng 230-245
    // 50% hiệu lực tức thì + 50% vào Store buffer (hồi phục từ từ)
    // ========================================================================
    void GameSave::eatFood(const Food& food) {
        exp += food.exp;

        double tmp = food.strength / 2.0;
        strengthChange(tmp);
        storeStrength += tmp;

        tmp = food.strengthFood / 2.0;
        strengthChangeFood(tmp);
        storeStrengthFood += tmp;

        tmp = food.strengthDrink / 2.0;
        strengthChangeDrink(tmp);
        storeStrengthDrink += tmp;

        feelingChange(food.feeling);
        setHealth(_health + food.health);
        setLikability(_likability + food.likability);
    }

    // ========================================================================
    // CalculateMode — Port từ GameSave.cs dòng 262-290
    // Tính trạng thái: Happy / Normal / PoorCondition / Ill
    // ========================================================================
    ModeType GameSave::calculateMode() {
        int realhel = 60
            - (_feeling >= 80 ? 12 : 0)
            - (_likability >= 80 ? 12 : (_likability >= 40 ? 6 : 0));

        // Kiểm tra sức khỏe thấp → Ill hoặc PoorCondition
        if (_health <= realhel) {
            if (_health <= realhel / 2.0) {
                return ModeType::Ill;
            } else {
                return ModeType::PoorCondition;
            }
        }

        // Kiểm tra cảm xúc → Happy hoặc PoorCondition hoặc Normal
        double realfel = 0.90
            - (_likability >= 80 ? 0.20 : (_likability >= 40 ? 0.10 : 0));
        double felps = _feeling / FEELING_MAX;

        if (felps >= realfel) {
            return ModeType::Happy;
        } else if (felps <= realfel / 2.0) {
            return ModeType::PoorCondition;
        }

        return ModeType::Normal;
    }

    // ========================================================================
    // Serialize / Deserialize tới File (Phân hệ 4D)
    // ========================================================================
    bool GameSave::save(const char* filepath) {
        File f = SD.open(filepath, FILE_WRITE);
        if (!f) return false;
        
        // Ghi tuần tự hoá toàn bộ Byte của struct GameSave (Binary)
        size_t written = f.write((uint8_t*)this, sizeof(GameSave));
        f.close();
        return written == sizeof(GameSave);
    }

    bool GameSave::load(const char* filepath) {
        if (!SD.exists(filepath)) return false;

        File f = SD.open(filepath, FILE_READ);
        if (!f) return false;

        if (f.size() == sizeof(GameSave)) {
            f.read((uint8_t*)this, sizeof(GameSave));
            f.close();
            return true;
        }
        f.close();
        return false;
    }

} // namespace VPet
