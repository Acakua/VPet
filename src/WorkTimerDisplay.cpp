// WorkTimerDisplay.cpp
// Port logic từ: Display/WorkTimer.xaml.cs

#include "WorkTimerDisplay.hpp"
#include <Arduino.h>
#include <stdio.h>

// Fallback dummy LVGL definitions (để code C++ compile được khi chưa có lvgl)
#include <lvgl.h>

namespace VPet {

    // Dummy button click handler for LVGL
    static void btn_stop_click_cb(void* e) {
        // This will be properly implemented with LVGL events later.
        // lv_event_t * e
    }

    WorkTimerDisplay::WorkTimerDisplay(lv_obj_t* parent, GameLoop* gameLoop)
        : m_gameLoop(gameLoop), m_currentWork(nullptr), m_startTimeMs(0)
        , m_getCount(0), m_displayType(0), onFinishCallback(nullptr), onFinishContext(nullptr) {
        
        container = lv_obj_create(parent);
        lv_obj_set_size(container, 180, 100);
        lv_obj_align(container, LV_ALIGN_TOP_MID, 0, 10);
        
        // Premium Glassmorphism Style
        lv_obj_set_style_bg_color(container, lv_color_hex(0x000000), 0);
        lv_obj_set_style_bg_opa(container, 180, 0);
        lv_obj_set_style_border_width(container, 1, 0);
        lv_obj_set_style_border_color(container, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_border_opa(container, 50, 0);
        lv_obj_set_style_radius(container, 12, 0);
        lv_obj_set_style_shadow_width(container, 20, 0);
        lv_obj_set_style_shadow_opa(container, 80, 0);
        lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);

        lblTitle = lv_label_create(container);
        lv_label_set_text(lblTitle, "Working...");
        lv_obj_set_style_text_color(lblTitle, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(lblTitle, &lv_font_montserrat_14, 0);
        lv_obj_align(lblTitle, LV_ALIGN_TOP_MID, 0, 0);

        lblStats = lv_label_create(container);
        lv_obj_set_style_text_color(lblStats, lv_color_hex(0xFFCC00), 0);
        lv_obj_set_style_text_font(lblStats, &lv_font_montserrat_12, 0);
        lv_obj_align(lblStats, LV_ALIGN_CENTER, 0, 0);

        barProgress = lv_bar_create(container);
        lv_obj_set_size(barProgress, 160, 6);
        lv_obj_align(barProgress, LV_ALIGN_BOTTOM_MID, 0, -30);
        lv_obj_set_style_bg_color(barProgress, lv_color_hex(0x444444), LV_PART_MAIN);
        lv_obj_set_style_bg_color(barProgress, lv_color_hex(0x00FF00), LV_PART_INDICATOR);

        btnStop = lv_btn_create(container);
        lv_obj_set_size(btnStop, 60, 25);
        lv_obj_align(btnStop, LV_ALIGN_BOTTOM_MID, 0, 5);
        lv_obj_set_style_bg_color(btnStop, lv_color_hex(0xCC0000), 0);
        lv_obj_t* btnLbl = lv_label_create(btnStop);
        lv_label_set_text(btnLbl, "Stop");
        lv_obj_set_style_text_font(btnLbl, &lv_font_montserrat_10, 0);
        lv_obj_center(btnLbl);

        //lv_obj_add_event_cb(btnStop, btn_stop_click_cb, LV_EVENT_CLICKED, this);

        hide();
    }

    WorkTimerDisplay::~WorkTimerDisplay() {
        if (container) lv_obj_del(container);
    }

    void WorkTimerDisplay::start(const Work* work) {
        if (!work) return;

        show();
        m_currentWork = work;
        m_startTimeMs = millis();
        m_getCount = 0;
        
        lv_bar_set_range(barProgress, 0, work->timeMinutes * 60); // giây
        lv_bar_set_value(barProgress, 0, LV_ANIM_OFF);
        
        char buf[64];
        snprintf(buf, sizeof(buf), "Working: %s", work->name);
        lv_label_set_text(lblTitle, buf);

        updateUI(0);
    }

    void WorkTimerDisplay::stop(FinishWorkInfo::Reason reason) {
        if (!m_currentWork) return;

        double spendTimeMins = (millis() - m_startTimeMs) / 60000.0;
        
        FinishWorkInfo fwi(m_currentWork, m_getCount, spendTimeMins, reason);
        if (onFinishCallback) {
            onFinishCallback(fwi, onFinishContext);
        }

        m_currentWork = nullptr;
        hide();
    }

    void WorkTimerDisplay::tick() {
        if (!isVisible() || !m_currentWork) return;

        uint32_t elapsedMs = millis() - m_startTimeMs;
        double totalMinutes = elapsedMs / 60000.0;

        // Nếu thời gian vượt quá thời gian làm việc
        if (totalMinutes > m_currentWork->timeMinutes) {
            stop(FinishWorkInfo::TimeFinish);
            return;
        }

        updateUI(totalMinutes);
    }

    void WorkTimerDisplay::formatTimeSpan(uint32_t seconds, char* buf, int bufSize) {
        if (seconds < 90) {
            snprintf(buf, bufSize, "%d s", seconds);
        } else if (seconds < 5400) { // 90 phút
            snprintf(buf, bufSize, "%.1f m", (double)seconds / 60.0);
        } else {
            snprintf(buf, bufSize, "%.1f h", (double)seconds / 3600.0);
        }
    }

    void WorkTimerDisplay::updateUI(double totalMinutesElapsed) {
        uint32_t elapsedSeconds = (uint32_t)(totalMinutesElapsed * 60);
        uint32_t remainingSeconds = (uint32_t)(m_currentWork->timeMinutes * 60) - elapsedSeconds;

        lv_bar_set_value(barProgress, elapsedSeconds, LV_ANIM_OFF);

        char buf[64] = {0};
        switch (m_displayType) {
            case 0: // Elapsed time
                formatTimeSpan(elapsedSeconds, buf, sizeof(buf));
                break;
            case 1: // Remaining time
                formatTimeSpan(remainingSeconds, buf, sizeof(buf));
                break;
            case 2: // Earned
                snprintf(buf, sizeof(buf), "%.0f %s", m_getCount, 
                        m_currentWork->type == WorkType::Work ? "Money" : "EXP");
                break;
        }
        
        lv_label_set_text(lblStats, buf);
    }

    void WorkTimerDisplay::switchDisplayType() {
        m_displayType = (m_displayType + 1) % 3;
    }

    void WorkTimerDisplay::show() {
        lv_obj_clear_flag(container, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(container); // Đảm bảo nổi lên trên Pet
    }

    void WorkTimerDisplay::hide() {
        lv_obj_add_flag(container, LV_OBJ_FLAG_HIDDEN);
    }

    bool WorkTimerDisplay::isVisible() const {
        return !lv_obj_has_flag(container, LV_OBJ_FLAG_HIDDEN);
    }

} // namespace VPet
