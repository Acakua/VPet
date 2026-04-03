// StatusBar.cpp
// Port logic từ: Display/ToolBar.xaml.cs

#include "StatusBar.hpp"
#include <stdio.h> // for snprintf

#include <lvgl.h>

namespace VPet {

    StatusBar::StatusBar(lv_obj_t* parent) {
        // 1. Khởi tạo container chính
        container = lv_obj_create(parent);
        lv_obj_set_size(container, 320, 80); // Width 320, Height 80
        lv_obj_align(container, LV_ALIGN_TOP_LEFT, 0, 0);

        // 2. Khởi tạo các Labels
        lblLevel = lv_label_create(container);
        lv_label_set_text(lblLevel, "Lv 1");

        lblMoney = lv_label_create(container);
        lv_label_set_text(lblMoney, "$ 0.00");

        lblExpText = lv_label_create(container);
        lv_label_set_text(lblExpText, "Exp: 0/100");

        // 3. Khởi tạo các Progress Bars
        barExp = lv_bar_create(container);
        lv_obj_set_size(barExp, 100, 10);
        
        barFood = lv_bar_create(container);
        lv_obj_set_size(barFood, 100, 10);

        barDrink = lv_bar_create(container);
        lv_obj_set_size(barDrink, 100, 10);

        barFeeling = lv_bar_create(container);
        lv_obj_set_size(barFeeling, 100, 10);

        barStrength = lv_bar_create(container);
        lv_obj_set_size(barStrength, 100, 10);

        // 4. Khởi tạo Menu (Tạm thời ẩn)
        menuContainer = lv_obj_create(parent);
        lv_obj_add_flag(menuContainer, LV_OBJ_FLAG_HIDDEN);

        btnWork = lv_btn_create(menuContainer);
        btnSleep = lv_btn_create(menuContainer);
        btnInteract = lv_btn_create(menuContainer);

        // Mặc định ẩn
        hide();
    }

    StatusBar::~StatusBar() {
        if (container) {
            lv_obj_del(container);
        }
        if (menuContainer) {
            lv_obj_del(menuContainer);
        }
    }

    // ================================================================
    // update() — M_TimeUIHandle từ ToolBar.xaml.cs (dòng 176 - 248)
    // Cập nhật giá trị hiển thị từ GameSave
    // ================================================================
    void StatusBar::update(const GameSave* save) {
        if (!save || !isVisible()) return;

        char buf[64];

        // 1. Cập nhật nhãn Level & Money
        snprintf(buf, sizeof(buf), "Lv %d", save->getLevel());
        lv_label_set_text(lblLevel, buf);

        snprintf(buf, sizeof(buf), "$ %.2f", save->money);
        lv_label_set_text(lblMoney, buf);

        // Exp bar
        double maxExp = save->levelUpNeed();
        lv_bar_set_range(barExp, 0, (int)maxExp);
        lv_bar_set_value(barExp, (int)save->exp, LV_ANIM_OFF);
        snprintf(buf, sizeof(buf), "Exp %.2f / %.0f", save->exp, maxExp);
        lv_label_set_text(lblExpText, buf);

        // 2. Cập nhật thanh sinh tồn
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
    }

    void StatusBar::hide() {
        lv_obj_add_flag(container, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(menuContainer, LV_OBJ_FLAG_HIDDEN);
    }

    bool StatusBar::isVisible() const {
        return !lv_obj_has_flag(container, LV_OBJ_FLAG_HIDDEN);
    }

} // namespace VPet
