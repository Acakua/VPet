#pragma once

#include <stdint.h>
#include <string.h>
#include "GameSave.hpp"   // ModeType (Happy/Normal/PoorCondition/Ill)
namespace VPet {

    // ========================================================================
    // GraphType — 23 loại hoạt ảnh
    // Port từ: GraphInfo.cs dòng 153-248
    // Thứ tự giá trị enum PHẢI giữ nguyên vì dùng làm index
    // ========================================================================
    enum class GraphType : uint8_t {
        Common = 0,             // 通用动画 — MOD dùng, ESP32 bỏ qua
        Raised_Dynamic,         // 被提起动态 — Bị nhấc lên (động)
        Raised_Static,          // 被提起静态 — Bị nhấc lên (A→B→C)
        Move,                   // 移动 — Di chuyển (tùy chọn trên ESP32)
        Default,                // 呼吸 — Thở / Đứng yên ★ BẮT BUỘC
        Touch_Head,             // 摸头 — Vuốt ve đầu (A→B→C)
        Touch_Body,             // 摸身体 — Chạm thân (A→B→C)
        Idel,                   // 空闲 — Rảnh rỗi / Ngẫu nhiên (A→B→C)
        Sleep,                  // 睡觉 — Ngủ (A→B→C)
        Say,                    // 说话 — Nói chuyện (A→B→C)
        StateONE,               // 待机1 — Chờ ngẫu nhiên 1 (A→B→C)
        StateTWO,               // 待机2 — Chờ ngẫu nhiên 2 (A→B→C)
        StartUP,                // 开机 — Khởi động
        Shutdown,               // 关机 — Tắt máy
        Work,                   // 工作 — Làm việc (A→B→C)
        Switch_Up,              // 向上切换 — Chuyển trạng thái tốt lên
        Switch_Down,            // 向下切换 — Chuyển trạng thái xấu đi
        Switch_Thirsty,         // 口渴 — Biểu hiện khát
        Switch_Hunger,          // 饥饿 — Biểu hiện đói
        SideHide_Left_Main,     // 躲藏左 — Trốn bên trái (Bỏ qua trên ESP32)
        SideHide_Left_Rise,     // 躲藏左显示 — Hiện ra từ trái (Bỏ qua)
        SideHide_Right_Main,    // 躲藏右 — Trốn bên phải (Bỏ qua)
        SideHide_Right_Rise,    // 躲藏右显示 — Hiện ra từ phải (Bỏ qua)
        Eat,                    // 喂食 — Ăn uống (A→B→C) ★ BỔ SUNG

        GRAPH_TYPE_COUNT        // Tổng số loại — dùng cho kích thước mảng
    };

    // ========================================================================
    // AnimatType — 4 giai đoạn của mỗi hoạt ảnh
    // Port từ: GraphInfo.cs dòng 252-270
    //
    // Quy trình phát: Single (1 lần) hoặc A_Start → B_Loop (lặp) → C_End
    // ========================================================================
    enum class AnimatType : uint8_t {
        Single = 0,     // 单次动画 — Chỉ 1 phần duy nhất
        A_Start,        // 开始动作 — Giai đoạn bắt đầu
        B_Loop,         // 循环动作 — Giai đoạn lặp
        C_End,          // 结束动作 — Giai đoạn kết thúc

        ANIMAT_TYPE_COUNT
    };

    // ========================================================================
    // WorkType — Loại công việc
    // Port từ: GraphHelper.cs dòng 84
    // ========================================================================
    enum class WorkType : uint8_t {
        Work = 0,       // 工作 — Làm việc (kiếm tiền)
        Study,          // 学习 — Học tập (kiếm exp)
        Play            // 玩耍 — Chơi đùa (reset tương tác)
    };

    // ========================================================================
    // GraphInfo — Metadata mô tả 1 animation entry
    // Port từ: GraphInfo.cs dòng 19-295
    //
    // Trên C#, GraphInfo được tạo bằng cách parse đường dẫn thư mục.
    // Trên ESP32, GraphInfo được đọc từ file manifest.json trên SD Card
    // (tạo bởi script Python parse_info_lps.py).
    // ========================================================================
    struct GraphInfo {
        char name[32];          // 动画名字 — Tên animation (user-defined)
        GraphType type;         // 类型 — Loại hoạt ảnh (enum ở trên)
        AnimatType animat;      // 动作 — Giai đoạn A/B/C/Single
        ModeType modeType;      // 状态 — Trạng thái Pet (Happy/Normal/...)

        GraphInfo()
            : type(GraphType::Common)
            , animat(AnimatType::Single)
            , modeType(ModeType::Normal)
        {
            name[0] = '\0';
        }

        GraphInfo(const char* n, GraphType t, AnimatType a, ModeType m)
            : type(t), animat(a), modeType(m)
        {
            strncpy(name, n, sizeof(name) - 1);
            name[sizeof(name) - 1] = '\0';
        }
    };

    // ========================================================================
    // WorkInfo — Định nghĩa 1 công việc
    // Port từ: GraphHelper.cs dòng 79-215 (class Work)
    //
    // Bỏ qua: SetStyle(), Display() (WPF-only)
    // Bỏ qua: BorderBrush, Background, ButtonBackground... (WPF colors)
    // Bỏ qua: Left, Top, Width (WPF layout)
    // Giữ lại: Toàn bộ logic gameplay (thu nhập, tiêu hao, giới hạn level)
    // ========================================================================
    struct Work {
        char name[32];          // 工作名称 — Tên công việc
        char graph[32];         // 使用动画名称 — Tên animation khi làm việc
        WorkType type;          // 类型 — Work/Study/Play
        double moneyBase;       // 工作盈利 — Thu nhập cơ bản
        double strengthFood;    // 食物消耗倍率 — Tốc độ tiêu hao đói
        double strengthDrink;   // 饮料消耗倍率 — Tốc độ tiêu hao khát
        double feeling;         // 心情消耗倍率 — Ảnh hưởng cảm xúc
        double finishBonus;     // 完成奖励倍率 — Bonus khi hoàn thành
        int levelLimit;         // 等级限制 — Level tối thiểu cần đạt
        int timeMinutes;               // 花费时间 — Thời gian (phút)

        Work()
            : type(WorkType::Work)
            , moneyBase(0), strengthFood(0), strengthDrink(0)
            , feeling(0), finishBonus(0)
            , levelLimit(0), timeMinutes(0)
        {
            name[0] = '\0';
            graph[0] = '\0';
        }
    };

} // namespace VPet
