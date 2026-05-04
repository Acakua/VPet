#pragma once

// StatusBar.hpp — Thanh trạng thái hiển thị thông số (Port từ ToolBar.xaml.cs)
// Hiển thị và quản lý các LVGL Widget cho thông số sinh tồn của Pet

#include "GameSave.hpp"
#include <stdint.h>

#include <lvgl.h>

namespace VPet {

    class StatusBar {
    public:
        lv_obj_t* container;    // Container chính chứa toàn bộ StatusBar
        lv_obj_t* menuContainer; // Container chứa Menu Buttons

        // Các nhãn (Labels) hiển thị text
        lv_obj_t* lblLevel;
        lv_obj_t* lblMoney;
        lv_obj_t* lblExpText;
        
        // Nhãn thông số (Value Labels - Mới)
        lv_obj_t* lblStrengthVal;
        lv_obj_t* lblFoodVal;
        lv_obj_t* lblDrinkVal;
        lv_obj_t* lblFeelingVal;

        // Các thanh tiến trình (Progress Bars)
        lv_obj_t* barExp;
        lv_obj_t* barFood;      // Hunger
        lv_obj_t* barDrink;     // Thirst
        lv_obj_t* barFeeling;   // Spirit
        lv_obj_t* barStrength;  // Health/Strength

        // Styles (Mới)
        lv_style_t styleGlass;
        lv_style_t styleBarBg;
        lv_style_t styleBarIndic;

        // Các nút bấm Menu chức năng
        lv_obj_t* btnWork;      // Kiếm tiền
        lv_obj_t* btnStudy;     // Kiếm EXP
        lv_obj_t* btnPlay;      // Giải trí
        lv_obj_t* btnSleep;     // Ngủ

        // Menu chọn công việc chi tiết
        lv_obj_t* workMenu; 
        void create_work_menu(lv_obj_t* parent);

        // Constructor khởi tạo giao diện trên 1 parent
        StatusBar(lv_obj_t* parent, lv_obj_t* petImage);
        ~StatusBar();

        // Cập nhật giá trị
        void update(const GameSave* save);

        // Hiển thị / Ẩn thanh trạng thái
        void show();
        void hide();
        bool isVisible() const;

        // Toggle Expand/Collapse SideBar
        void toggle();
        bool isCollapsed() const { return collapsed; }

        // Nạp lại danh sách công việc
        void refresh_tasks();

    private:
        bool collapsed = false;
        lv_obj_t* btnToggle;     // Nút bấm thu phóng SideBar
        lv_obj_t* btnLabel;      // Icon/Text trên nút Toggle
        lv_obj_t* petImage;      // Con trỏ tới widget Pet để dịch chuyển tọa độ
        
        // Helper để tạo một cụm chỉ số (Stat Card)
        lv_obj_t* create_stat_card(lv_obj_t* parent, const char* symbol, const char* name, lv_color_t color, lv_obj_t** outBar, lv_obj_t** outVal);
    };

} // namespace VPet
