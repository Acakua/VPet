#pragma once

// MessageBubble.hpp — Bong bóng thoại hiển thị văn bản (Port từ MessageBar.xaml.cs)
// Hiển thị text popup và tự động ẩn.

#include <stdint.h>
#include <string.h>

// Forward declaration của hệ thống LVGL
typedef struct _lv_obj_t lv_obj_t;

namespace VPet {

    class MessageBubble {
    public:
        lv_obj_t* container;
        lv_obj_t* lblName;
        lv_obj_t* lblMessage;

        uint32_t showStartTime;
        uint32_t durationMs;

        MessageBubble(lv_obj_t* parent);
        ~MessageBubble();

        // Hiển thị thông báo trong thời gian `durationMs`
        void show(const char* name, const char* text, uint32_t durationMs = 3000);

        // Tick cập nhật (Gọi mỗi frame trên main loop để check ẩn đi)
        void tick();

        // Ẩn bóng thoại ngay lập tức
        void hide();

        bool isVisible() const;
    };

} // namespace VPet
