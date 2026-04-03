#pragma once

// StatusBar.hpp — Thanh trạng thái hiển thị thông số (Port từ ToolBar.xaml.cs)
// Hiển thị và quản lý các LVGL Widget cho thông số sinh tồn của Pet

#include "GameSave.hpp"
#include <stdint.h>

// Forward declaration của hệ thống LVGL (tránh include lvgl.h ở header gốc nếu chưa cần thiết)
typedef struct _lv_obj_t lv_obj_t;

namespace VPet {

    class StatusBar {
    public:
        lv_obj_t* container;    // Container chính chứa toàn bộ StatusBar
        lv_obj_t* menuContainer; // Container chứa Menu Buttons

        // Các nhãn (Labels) hiển thị text
        lv_obj_t* lblLevel;
        lv_obj_t* lblMoney;
        lv_obj_t* lblExpText;
        lv_obj_t* lblStatusDelta; // Hiển thị thay đổi theo thời gian (ví dụ: -0.5/t)

        // Các thanh tiến trình (Progress Bars)
        lv_obj_t* barExp;
        lv_obj_t* barFood;      // Hunger
        lv_obj_t* barDrink;     // Thirst
        lv_obj_t* barFeeling;   // Spirit
        lv_obj_t* barStrength;  // Tương đương PgbStrength

        // Các nút bấm Menu cơ bản
        lv_obj_t* btnWork;
        lv_obj_t* btnSleep;
        lv_obj_t* btnInteract;

        // Constructor khởi tạo giao diện trên 1 parent (thường là lv_scr_act())
        StatusBar(lv_obj_t* parent);
        ~StatusBar();

        // ----------------------------------------------------
        // update() — Tương đương M_TimeUIHandle trong C#
        // Cập nhật giá trị các thanh progress theo GameSave
        // ----------------------------------------------------
        void update(const GameSave* save);

        // Hiển thị / Ẩn thanh trạng thái
        void show();
        void hide();
        bool isVisible() const;
    };

} // namespace VPet
