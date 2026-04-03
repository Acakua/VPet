// MessageBubble.cpp
// Port logic từ: Display/MessageBar.xaml.cs

#include "MessageBubble.hpp"
#include <Arduino.h>

#include <lvgl.h>

namespace VPet {

    MessageBubble::MessageBubble(lv_obj_t* parent) 
        : showStartTime(0), durationMs(0) {
        
        container = lv_obj_create(parent);
        lv_obj_set_size(container, 250, 100);
        lv_obj_align(container, LV_ALIGN_TOP_MID, 0, 10); // Hiển thị trên đầu Pet

        lblName = lv_label_create(container);
        lv_label_set_long_mode(lblName, LV_LABEL_LONG_WRAP);
        lv_label_set_text(lblName, "Name");
        
        lblMessage = lv_label_create(container);
        lv_label_set_long_mode(lblMessage, LV_LABEL_LONG_WRAP);
        lv_label_set_text(lblMessage, "...");

        hide();
    }

    MessageBubble::~MessageBubble() {
        if (container) lv_obj_del(container);
    }

    void MessageBubble::show(const char* name, const char* text, uint32_t duration) {
        if (!name || !text) return;

        lv_label_set_text(lblName, name);
        lv_label_set_text(lblMessage, text);

        showStartTime = millis();
        durationMs = duration;

        // Ước tính thời gian hiển thị tương đương C# Logic: timeleft = Function.ComCheck(text) * 10 + 20;
        // Ở đây đơn giản tính theo độ dài chuỗi
        if (durationMs == 0) {
            durationMs = strlen(text) * 150 + 2000; 
        }

        lv_obj_clear_flag(container, LV_OBJ_FLAG_HIDDEN);
    }

    void MessageBubble::tick() {
        if (!isVisible()) return;

        // Tự động tắt sau time
        if (millis() - showStartTime >= durationMs) {
            hide();
        }
    }

    void MessageBubble::hide() {
        lv_obj_add_flag(container, LV_OBJ_FLAG_HIDDEN);
    }

    bool MessageBubble::isVisible() const {
        return !lv_obj_has_flag(container, LV_OBJ_FLAG_HIDDEN);
    }

} // namespace VPet
