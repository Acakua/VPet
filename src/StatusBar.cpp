// StatusBar.cpp
// Port logic từ: Display/ToolBar.xaml.cs

#include "StatusBar.hpp"
#include <stdio.h> // for snprintf

#include <lvgl.h>

namespace VPet {

    static void toggle_btn_cb(lv_event_t* e) {
        StatusBar* bar = (StatusBar*)lv_event_get_user_data(e);
        bar->toggle();
    }

    StatusBar::StatusBar(lv_obj_t* parent, lv_obj_t* petImg) {
        collapsed = false;
        petImage = petImg;

        // 1. Khởi tạo container chính (SideBar 160x320 - Bên Phải)
        container = lv_obj_create(parent);
        lv_obj_set_size(container, 160, 320); 
        lv_obj_align(container, LV_ALIGN_TOP_RIGHT, 0, 0);
        
        // Thiết kế Flex Layout xếp dọc
        lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
        lv_obj_set_style_pad_all(container, 10, 0);
        lv_obj_set_style_pad_row(container, 12, 0);
        lv_obj_set_style_bg_color(container, lv_color_hex(0x000000), 0);
        lv_obj_set_style_bg_opa(container, LV_OPA_60, 0);
        lv_obj_set_style_border_width(container, 0, 0);
        lv_obj_set_style_radius(container, 0, 0);

        // 2. Nút bấm Thu/Phóng (Toggle Button - Nằm bên trái SideBar)
        btnToggle = lv_btn_create(parent);
        lv_obj_set_size(btnToggle, 30, 40);
        lv_obj_align_to(btnToggle, container, LV_ALIGN_OUT_LEFT_TOP, 0, 10);
        lv_obj_set_style_bg_color(btnToggle, lv_color_hex(0x000000), 0);
        lv_obj_set_style_bg_opa(btnToggle, LV_OPA_80, 0);
        lv_obj_set_style_radius(btnToggle, 8, 0);
        
        lv_obj_t* btn_lbl = lv_label_create(btnToggle);
        lv_label_set_text(btn_lbl, LV_SYMBOL_RIGHT); // Mặc định mở menu hướng sang phải
        lv_obj_center(btn_lbl);

        lv_obj_add_event_cb(btnToggle, toggle_btn_cb, LV_EVENT_CLICKED, this);

        // 3. Khởi tạo các thành phần (Dùng Flex)
        lblLevel = lv_label_create(container);
        lv_label_set_text(lblLevel, "Lv 1");
        lv_obj_set_style_text_font(lblLevel, &lv_font_montserrat_14, 0);

        lblMoney = lv_label_create(container);
        lv_label_set_text(lblMoney, "$ 0.00");
        lv_obj_set_style_text_color(lblMoney, lv_color_hex(0xFFD633), 0);

        barFood = lv_bar_create(container);
        lv_obj_set_size(barFood, 130, 8);
        lv_obj_set_style_bg_color(barFood, lv_color_hex(0xFF4D4D), LV_PART_INDICATOR);

        barDrink = lv_bar_create(container);
        lv_obj_set_size(barDrink, 130, 8);
        lv_obj_set_style_bg_color(barDrink, lv_color_hex(0x3399FF), LV_PART_INDICATOR);

        barFeeling = lv_bar_create(container);
        lv_obj_set_size(barFeeling, 130, 8);
        lv_obj_set_style_bg_color(barFeeling, lv_color_hex(0xFF66B2), LV_PART_INDICATOR);

        barStrength = lv_bar_create(container);
        lv_obj_set_size(barStrength, 130, 8);
        lv_obj_set_style_bg_color(barStrength, lv_color_hex(0x70DB70), LV_PART_INDICATOR);

        lblExpText = lv_label_create(container);
        lv_obj_set_style_text_font(lblExpText, &lv_font_montserrat_14, 0);
        
        barExp = lv_bar_create(container);
        lv_obj_set_size(barExp, 130, 4);
        lv_obj_set_style_bg_color(barExp, lv_color_hex(0xFFA500), LV_PART_INDICATOR);

        menuContainer = lv_obj_create(parent);
        lv_obj_add_flag(menuContainer, LV_OBJ_FLAG_HIDDEN);

        hide();
    }

    StatusBar::~StatusBar() {
        if (container) lv_obj_del(container);
        if (btnToggle) lv_obj_del(btnToggle);
        if (menuContainer) lv_obj_del(menuContainer);
    }

    void StatusBar::toggle() {
        collapsed = !collapsed;
        
        if (collapsed) {
            lv_obj_set_x(container, 160); // Trượt sang phải để ẩn
            lv_obj_align_to(btnToggle, container, LV_ALIGN_OUT_LEFT_TOP, -160, 10);
            lv_label_set_text(lv_obj_get_child(btnToggle, 0), LV_SYMBOL_LEFT);
            if (petImage) lv_obj_set_x(petImage, 80); // Căn giữa
        } else {
            lv_obj_set_x(container, 0); // Trở về vị trí cũ
            lv_obj_align_to(btnToggle, container, LV_ALIGN_OUT_LEFT_TOP, 0, 10);
            lv_label_set_text(lv_obj_get_child(btnToggle, 0), LV_SYMBOL_RIGHT);
            if (petImage) lv_obj_set_x(petImage, 0); // Sát lề trái
        }
    }

    void StatusBar::update(const GameSave* save) {
        if (!save || !isVisible()) return;

        char buf[64];
        snprintf(buf, sizeof(buf), "Lv %d", save->getLevel());
        lv_label_set_text(lblLevel, buf);

        snprintf(buf, sizeof(buf), "$ %.2f", save->money);
        lv_label_set_text(lblMoney, buf);

        double maxExp = save->levelUpNeed();
        lv_bar_set_range(barExp, 0, (int)maxExp);
        lv_bar_set_value(barExp, (int)save->exp, LV_ANIM_OFF);
        snprintf(buf, sizeof(buf), "Exp %.1f/%.0f", save->exp, maxExp);
        lv_label_set_text(lblExpText, buf);

        lv_bar_set_range(barStrength, 0, (int)save->getStrengthMax());
        lv_bar_set_value(barStrength, (int)save->getStrength(), LV_ANIM_OFF);

        lv_bar_set_range(barFeeling, 0, (int)save->getFeelingMax());
        lv_bar_set_value(barFeeling, (int)save->getFeeling(), LV_ANIM_OFF);

        lv_bar_set_range(barFood, 0, (int)save->getStrengthMax());
        lv_bar_set_value(barFood, (int)save->getStrengthFood(), LV_ANIM_OFF);

        lv_bar_set_range(barDrink, 0, (int)save->getStrengthMax());
        lv_bar_set_value(barDrink, (int)save->getStrengthDrink(), LV_ANIM_OFF);
    }

    void StatusBar::show() {
        lv_obj_clear_flag(container, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(btnToggle, LV_OBJ_FLAG_HIDDEN);
    }

    void StatusBar::hide() {
        lv_obj_add_flag(container, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(btnToggle, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(menuContainer, LV_OBJ_FLAG_HIDDEN);
    }

    bool StatusBar::isVisible() const {
        return !lv_obj_has_flag(container, LV_OBJ_FLAG_HIDDEN);
    }

} // namespace VPet
