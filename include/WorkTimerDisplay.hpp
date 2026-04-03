#pragma once

// WorkTimerDisplay.hpp — Bộ đếm giờ làm việc (Port từ WorkTimer.xaml.cs)
// Hiển thị UI khi Pet đang làm việc/học tập.

#include "GraphInfo.hpp"
#include <stdint.h>

// Forward declaration của hệ thống LVGL (tránh include lvgl.h ở header gốc nếu chưa cần thiết)
typedef struct _lv_obj_t lv_obj_t;

namespace VPet {

    // Forward declaration của GameLoop để tương tác trạng thái
    class GameLoop;

    // ========================================================================
    // FinishWorkInfo — Thông tin hoạt động hoàn thành (dòng 44-108 C#)
    // ========================================================================
    struct FinishWorkInfo {
        const Work* work;
        double count;          // Tiền/Exp nhận được
        double spendTimeMins;  // Thời gian đã tiêu tốn (Phút)
        
        enum Reason {
            TimeFinish,
            ManualStop,
            StateFail,
            Other
        } reason;

        FinishWorkInfo() : work(nullptr), count(0), spendTimeMins(0), reason(Other) {}
        FinishWorkInfo(const Work* w, double c, double time, Reason r)
            : work(w), count(c), spendTimeMins(time), reason(r) {}
    };

    // ========================================================================
    // WorkTimerDisplay — Giao diện Timer (LVGL widget)
    // ========================================================================
    class WorkTimerDisplay {
    public:
        lv_obj_t* container;
        lv_obj_t* lblTitle;
        lv_obj_t* lblStats;
        lv_obj_t* barProgress;
        lv_obj_t* btnStop;

        GameLoop* m_gameLoop;
        const Work* m_currentWork;

        uint32_t m_startTimeMs;
        double m_getCount;      // C# GetCount (tiền/exp tạm tính)
        int m_displayType;      // 0 = Chạy tới, 1 = Chạy lùi, 2 = Trạng thái thu nhập

        typedef void (*FinishWorkCallback)(FinishWorkInfo info, void* ctx);
        FinishWorkCallback onFinishCallback;
        void* onFinishContext;

        WorkTimerDisplay(lv_obj_t* parent, GameLoop* gameLoop);
        ~WorkTimerDisplay();

        // Bắt đầu một công việc
        void start(const Work* work);

        // Dừng công việc
        void stop(FinishWorkInfo::Reason reason = FinishWorkInfo::ManualStop);

        // Tick cập nhật (Gọi mỗi frame trên main loop)
        void tick();

        // Điều khiển hiển thị
        void switchDisplayType();
        void show();
        void hide();
        bool isVisible() const;

    private:
        void formatTimeSpan(uint32_t seconds, char* buf, int bufSize);
        void updateUI(double totalMinutesElapsed);
    };

} // namespace VPet
