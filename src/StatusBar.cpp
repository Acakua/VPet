// StatusBar.cpp
// Port logic từ: Display/ToolBar.xaml.cs

#include "StatusBar.hpp"
#include <stdio.h> // for snprintf

#include <lvgl.h>

#include "GameLoop.hpp"
extern VPet::GameLoop* gameLoop;

namespace VPet {

    static void toggle_anim_cb(void* var, int32_t v) {
        lv_obj_set_x((lv_obj_t*)var, v);
    }

    static void work_btn_event_cb(lv_event_t* e) {
        int index = (int)(intptr_t)lv_event_get_user_data(e);
        if (gameLoop) {
            gameLoop->startWork(index);
        }
        // Đóng menu sau khi chọn
        lv_obj_t* menu = (lv_obj_t*)lv_event_get_current_target(e)->parent->parent;
        lv_obj_add_flag(menu, LV_OBJ_FLAG_HIDDEN);
    }

    static void open_work_menu_cb(lv_event_t* e) {
        lv_obj_t* menu = (lv_obj_t*)lv_event_get_user_data(e);
        lv_obj_clear_flag(menu, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(menu); // Đưa menu lên trên cùng khi mở
    }

    static void sleep_btn_cb(lv_event_t* e) {
        if (gameLoop) {
            if (gameLoop->state == WorkingState::Sleep) {
                gameLoop->state = WorkingState::Normal;
                gameLoop->anim->displayToNormal();
            } else {
                gameLoop->state = WorkingState::Sleep;
                gameLoop->anim->displaySleep(true);
            }
        }
    }

    static void toggle_btn_cb(lv_event_t* e) {
        StatusBar* bar = (StatusBar*)lv_event_get_user_data(e);
        bar->toggle();
    }

    lv_obj_t* StatusBar::create_stat_card(lv_obj_t* parent, const char* symbol, const char* name, lv_color_t color, lv_obj_t** outBar, lv_obj_t** outVal) {
        lv_obj_t* card = lv_obj_create(parent);
        lv_obj_set_size(card, lv_pct(100), 45);
        lv_obj_set_style_bg_opa(card, 0, 0);
        lv_obj_set_style_border_width(card, 0, 0);
        lv_obj_set_style_pad_all(card, 0, 0); // Xóa sạch padding để tận dụng chiều rộng
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

        // Hàng 1: Icon + Tên + Giá trị (Sử dụng Symbol chuyên nghiệp)
        lv_obj_t* lblTitle = lv_label_create(card);
        lv_label_set_text_fmt(lblTitle, "%s %s", symbol, name);
        lv_obj_align(lblTitle, LV_ALIGN_TOP_LEFT, 0, 0);
        lv_obj_set_style_text_font(lblTitle, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(lblTitle, color, 0); // Icon màu theo loại

        *outVal = lv_label_create(card);
        lv_label_set_text(*outVal, "0/0");
        lv_obj_align(*outVal, LV_ALIGN_TOP_RIGHT, 0, 0);
        lv_obj_set_style_text_font(*outVal, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(*outVal, lv_color_hex(0xBBBBBB), 0);

        // Hàng 2: Thanh Bar bo tròn
        *outBar = lv_bar_create(card);
        lv_obj_set_size(*outBar, lv_pct(100), 8);
        lv_obj_align(*outBar, LV_ALIGN_BOTTOM_MID, 0, -2);
        
        lv_obj_add_style(*outBar, &styleBarBg, LV_PART_MAIN);
        lv_obj_add_style(*outBar, &styleBarIndic, LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(*outBar, color, LV_PART_INDICATOR);

        return card;
    }

    StatusBar::StatusBar(lv_obj_t* parent, lv_obj_t* petImg) {
        collapsed = false;
        petImage = petImg;

        // --- KHỞI TẠO STYLES (Cyber-Sanctuary Glassmorphism) ---
        lv_style_init(&styleGlass);
        lv_style_set_bg_color(&styleGlass, lv_color_hex(0x0A0A0A)); 
        lv_style_set_bg_opa(&styleGlass, 150); // Giảm nhẹ độ mờ để nhìn xuyên thấu tốt hơn
        lv_style_set_border_width(&styleGlass, 1);
        lv_style_set_border_color(&styleGlass, lv_color_hex(0x888888)); 
        lv_style_set_border_opa(&styleGlass, 60);
        lv_style_set_border_side(&styleGlass, LV_BORDER_SIDE_LEFT);
        lv_style_set_radius(&styleGlass, 15); 
        
        lv_style_init(&styleBarBg);
        lv_style_set_bg_color(&styleBarBg, lv_color_hex(0x333333));
        lv_style_set_bg_opa(&styleBarBg, LV_OPA_100);
        lv_style_set_radius(&styleBarBg, 5);

        lv_style_init(&styleBarIndic);
        lv_style_set_radius(&styleBarIndic, 5);

        // 1. Container chính (SideBar)
        container = lv_obj_create(parent);
        lv_obj_set_size(container, 160, 320); 
        lv_obj_set_pos(container, 320, 0);
        lv_obj_add_style(container, &styleGlass, 0);
        lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_all(container, 12, 0);
        lv_obj_set_style_pad_row(container, 2, 0); // Khoảng cách giữa các card
        lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);

        // 2. Header: Level & Money
        lblLevel = lv_label_create(container);
        lv_obj_set_style_text_font(lblLevel, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(lblLevel, lv_color_hex(0xFFFFFF), 0);

        lblMoney = lv_label_create(container);
        lv_obj_set_style_text_font(lblMoney, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(lblMoney, lv_color_hex(0xFFD633), 0);
        lv_obj_set_style_pad_bottom(lblMoney, 10, 0);

        // 3. Stat Cards (Professional Symbols)
        create_stat_card(container, LV_SYMBOL_CHARGE, "HP", lv_color_hex(0x00FF88), &barStrength, &lblStrengthVal);
        create_stat_card(container, LV_SYMBOL_UP, "FOOD", lv_color_hex(0xFF4D4D), &barFood, &lblFoodVal);
        create_stat_card(container, LV_SYMBOL_TINT, "DRINK", lv_color_hex(0x00CCFF), &barDrink, &lblDrinkVal);
        create_stat_card(container, LV_SYMBOL_PLAY, "MOOD", lv_color_hex(0xFF007F), &barFeeling, &lblFeelingVal);

        // 4. Footer: EXP
        lblExpText = lv_label_create(container);
        lv_obj_set_style_text_font(lblExpText, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(lblExpText, lv_color_hex(0xAAAAAA), 0);
        lv_obj_set_style_pad_top(lblExpText, 10, 0);

        barExp = lv_bar_create(container);
        lv_obj_set_size(barExp, lv_pct(100), 4);
        lv_obj_add_style(barExp, &styleBarBg, LV_PART_MAIN);
        lv_obj_add_style(barExp, &styleBarIndic, LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(barExp, lv_color_hex(0xFFA500), LV_PART_INDICATOR);

        // 5. Nút bấm chức năng (Hàng 2x2)
        lv_obj_t* btnCont = lv_obj_create(container);
        lv_obj_set_size(btnCont, lv_pct(100), 100);
        lv_obj_set_style_bg_opa(btnCont, 0, 0);
        lv_obj_set_style_border_width(btnCont, 0, 0);
        lv_obj_set_flex_flow(btnCont, LV_FLEX_FLOW_ROW_WRAP);
        lv_obj_set_style_pad_all(btnCont, 0, 0);
        lv_obj_set_style_pad_gap(btnCont, 5, 0);
        lv_obj_clear_flag(btnCont, LV_OBJ_FLAG_SCROLLABLE);

        auto create_btn = [&](const char* icon, lv_color_t color) {
            lv_obj_t* btn = lv_btn_create(btnCont);
            lv_obj_set_size(btn, 62, 40); // Giảm nhẹ kích thước để chắc chắn vừa hàng 2
            lv_obj_set_style_bg_color(btn, color, 0);
            lv_obj_set_style_bg_opa(btn, 150, 0);
            lv_obj_t* lbl = lv_label_create(btn);
            lv_label_set_text(lbl, icon);
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_10, 0); // Dùng font nhỏ hơn cho button
            lv_obj_center(lbl);
            return btn;
        };

        btnWork = create_btn(LV_SYMBOL_EDIT "\nWork", lv_color_hex(0x444444));
        btnStudy = create_btn(LV_SYMBOL_DIRECTORY "\nStudy", lv_color_hex(0x444444));
        btnPlay = create_btn(LV_SYMBOL_IMAGE "\nPlay", lv_color_hex(0x444444));
        btnSleep = create_btn(LV_SYMBOL_PAUSE "\nSleep", lv_color_hex(0x444444));

        // 6. Nút Toggle (Handle - Pill Design with Icon)
        btnToggle = lv_btn_create(parent);
        lv_obj_set_size(btnToggle, 24, 80); 
        lv_obj_set_style_bg_color(btnToggle, lv_color_hex(0x222222), 0);
        lv_obj_set_style_bg_opa(btnToggle, 180, 0);
        lv_obj_set_style_radius(btnToggle, 10, 0); 
        lv_obj_set_style_border_width(btnToggle, 1, 0);
        lv_obj_set_style_border_color(btnToggle, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_border_opa(btnToggle, 80, 0);
        
        btnLabel = lv_label_create(btnToggle);
        lv_label_set_text(btnLabel, LV_SYMBOL_LEFT); 
        lv_obj_center(btnLabel);
        lv_obj_set_style_text_color(btnLabel, lv_color_hex(0xFFFFFF), 0);

        collapsed = true; 
        lv_obj_set_pos(btnToggle, 480 - 24, 120); 
        lv_obj_set_pos(container, 480, 0);

        lv_obj_add_event_cb(btnToggle, toggle_btn_cb, LV_EVENT_CLICKED, this);

        // 7. Menu chọn công việc
        create_work_menu(parent);

        // ĐẢM BẢO THỨ TỰ LAYER:
        // 1. Pet ở dưới cùng (đã tạo trước đó trong main.cpp)
        // 2. Sidebar container và Nút Toggle
        lv_obj_move_foreground(container);
        lv_obj_move_foreground(btnToggle);
        // 3. Menu chọn công việc ở trên cùng
        lv_obj_move_foreground(workMenu);

        lv_obj_add_event_cb(btnWork, open_work_menu_cb, LV_EVENT_CLICKED, workMenu); 
        lv_obj_add_event_cb(btnStudy, open_work_menu_cb, LV_EVENT_CLICKED, workMenu);
        lv_obj_add_event_cb(btnPlay, open_work_menu_cb, LV_EVENT_CLICKED, workMenu);
        lv_obj_add_event_cb(btnSleep, sleep_btn_cb, LV_EVENT_CLICKED, nullptr);

        menuContainer = lv_obj_create(parent);
        lv_obj_add_flag(menuContainer, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(menuContainer);

        hide();
    }

    StatusBar::~StatusBar() {
        if (container) lv_obj_del(container);
        if (btnToggle) lv_obj_del(btnToggle);
        if (menuContainer) lv_obj_del(menuContainer);
    }

    void StatusBar::toggle() {
        collapsed = !collapsed;
        
        // Tọa độ đích cho SideBar (Width=160, ScreenWidth=480)
        int32_t target_x = collapsed ? 480 : 320;
        int32_t btn_target_x = target_x - 24; // Sát mép kính

        // Cập nhật Icon
        lv_label_set_text(btnLabel, collapsed ? LV_SYMBOL_LEFT : LV_SYMBOL_RIGHT);

        // Hoạt ảnh trượt SideBar
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, container);
        lv_anim_set_values(&a, lv_obj_get_x(container), target_x);
        lv_anim_set_time(&a, 300);
        lv_anim_set_exec_cb(&a, toggle_anim_cb);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
        lv_anim_start(&a);

        // Hoạt ảnh trượt nút Toggle
        lv_anim_t b;
        lv_anim_init(&b);
        lv_anim_set_var(&b, btnToggle);
        lv_anim_set_values(&b, lv_obj_get_x(btnToggle), btn_target_x);
        lv_anim_set_time(&b, 300);
        lv_anim_set_exec_cb(&b, toggle_anim_cb);
        lv_anim_start(&b);

        if (!collapsed) {
            // Khi mở SideBar: Đẩy Pet sang trái để tránh bị đè (Pet center ở 160 thay vì 240)
            if (petImage) {
                lv_anim_t p;
                lv_anim_init(&p);
                lv_anim_set_var(&p, petImage);
                lv_anim_set_values(&p, lv_obj_get_x(petImage), 40); 
                lv_anim_set_time(&p, 300);
                lv_anim_set_exec_cb(&p, toggle_anim_cb);
                lv_anim_start(&p);
            }
        } else {
            // Khi thu gọn: Đưa Pet về giữa màn hình chính (Center 240)
            if (petImage) {
                lv_anim_t p;
                lv_anim_init(&p);
                lv_anim_set_var(&p, petImage);
                lv_anim_set_values(&p, lv_obj_get_x(petImage), 120); 
                lv_anim_set_time(&p, 300);
                lv_anim_set_exec_cb(&p, toggle_anim_cb);
                lv_anim_start(&p);
            }
        }
    }

    void StatusBar::update(const GameSave* save) {
        if (!save || !isVisible()) return;

        char buf[64];
        snprintf(buf, sizeof(buf), "Pet: %s | Lv %d", save->name, save->getLevel());
        lv_label_set_text(lblLevel, buf);

        snprintf(buf, sizeof(buf), "Gold: $ %.2f", save->money);
        lv_label_set_text(lblMoney, buf);

        // Update EXP
        double maxExp = save->levelUpNeed();
        lv_bar_set_range(barExp, 0, (int)maxExp);
        lv_bar_set_value(barExp, (int)save->exp, LV_ANIM_ON);
        snprintf(buf, sizeof(buf), "EXP: %.0f/%.0f", save->exp, maxExp);
        lv_label_set_text(lblExpText, buf);

        // Update Strength/HP
        lv_bar_set_range(barStrength, 0, (int)save->getStrengthMax());
        lv_bar_set_value(barStrength, (int)save->getStrength(), LV_ANIM_ON);
        snprintf(buf, sizeof(buf), "%.0f/%.0f", save->getStrength(), save->getStrengthMax());
        lv_label_set_text(lblStrengthVal, buf);

        // Update Food
        lv_bar_set_range(barFood, 0, (int)save->getStrengthMax());
        lv_bar_set_value(barFood, (int)save->getStrengthFood(), LV_ANIM_ON);
        snprintf(buf, sizeof(buf), "%.0f%%", (save->getStrengthFood() / save->getStrengthMax()) * 100);
        lv_label_set_text(lblFoodVal, buf);

        // Update Drink
        lv_bar_set_range(barDrink, 0, (int)save->getStrengthMax());
        lv_bar_set_value(barDrink, (int)save->getStrengthDrink(), LV_ANIM_ON);
        snprintf(buf, sizeof(buf), "%.0f%%", (save->getStrengthDrink() / save->getStrengthMax()) * 100);
        lv_label_set_text(lblDrinkVal, buf);

        // Update Feeling
        lv_bar_set_range(barFeeling, 0, (int)save->getFeelingMax());
        lv_bar_set_value(barFeeling, (int)save->getFeeling(), LV_ANIM_ON);
        snprintf(buf, sizeof(buf), "%.0f/%.0f", save->getFeeling(), save->getFeelingMax());
        lv_label_set_text(lblFeelingVal, buf);
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

    void StatusBar::create_work_menu(lv_obj_t* parent) {
        workMenu = lv_obj_create(parent);
        lv_obj_set_size(workMenu, 300, 280);
        lv_obj_center(workMenu);
        lv_obj_add_style(workMenu, &styleGlass, 0);
        lv_obj_set_style_bg_opa(workMenu, 240, 0); // Làm menu đậm hơn
        lv_obj_add_flag(workMenu, LV_OBJ_FLAG_HIDDEN);

        lv_obj_t* lblTitle = lv_label_create(workMenu);
        lv_label_set_text(lblTitle, "Select Task");
        lv_obj_set_style_text_font(lblTitle, &lv_font_montserrat_14, 0);
        lv_obj_align(lblTitle, LV_ALIGN_TOP_MID, 0, 5); // Đẩy xuống một chút để không bị dính viền

        lv_obj_t* list = lv_list_create(workMenu);
        lv_obj_set_size(list, lv_pct(100), lv_pct(90));
        lv_obj_align(list, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_bg_opa(list, 0, 0);
        lv_obj_set_style_border_width(list, 0, 0);

        // Thêm 10 công việc từ GameLoop
        const char* icons[] = {
            LV_SYMBOL_EDIT, LV_SYMBOL_TRASH, LV_SYMBOL_VIDEO, // Work
            LV_SYMBOL_DIRECTORY, LV_SYMBOL_SETTINGS, // Study
            LV_SYMBOL_IMAGE, LV_SYMBOL_CLOSE, LV_SYMBOL_REFRESH, // Play
            LV_SYMBOL_KEYBOARD, LV_SYMBOL_IMAGE // Study 2
        };

        if (gameLoop) {
            for (int i = 0; i < GameLoop::WORK_COUNT; i++) {
                const char* icon = (i < 10) ? icons[i] : LV_SYMBOL_LIST;
                lv_obj_t* btn = lv_list_add_btn(list, icon, gameLoop->works[i].name);
                lv_obj_add_event_cb(btn, work_btn_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
            }
        }

        // Nút đóng
        lv_obj_t* btnClose = lv_btn_create(workMenu);
        lv_obj_set_size(btnClose, 30, 30);
        lv_obj_align(btnClose, LV_ALIGN_TOP_RIGHT, 0, 0); // Đặt sát góc trong container
        lv_obj_set_style_bg_color(btnClose, lv_color_hex(0xFF4444), 0);
        lv_obj_set_style_radius(btnClose, LV_RADIUS_CIRCLE, 0);
        lv_obj_t* lblClose = lv_label_create(btnClose);
        lv_label_set_text(lblClose, LV_SYMBOL_CLOSE);
        lv_obj_center(lblClose);
        lv_obj_add_event_cb(btnClose, [](lv_event_t* e){
            lv_obj_add_flag((lv_obj_t*)lv_event_get_user_data(e), LV_OBJ_FLAG_HIDDEN);
        }, LV_EVENT_CLICKED, workMenu);
    }

    void StatusBar::refresh_tasks() {
        if (!workMenu) return;
        
        lv_obj_t* list = lv_obj_get_child(workMenu, 1); // List là con thứ 2 (sau Title)
        if (!list || !lv_obj_check_type(list, &lv_list_class)) return;

        lv_obj_clean(list);

        const char* icons[] = {
            LV_SYMBOL_EDIT, LV_SYMBOL_TRASH, LV_SYMBOL_VIDEO,
            LV_SYMBOL_DIRECTORY, LV_SYMBOL_SETTINGS,
            LV_SYMBOL_IMAGE, LV_SYMBOL_CLOSE, LV_SYMBOL_REFRESH,
            LV_SYMBOL_KEYBOARD, LV_SYMBOL_IMAGE
        };

        if (gameLoop) {
            for (int i = 0; i < GameLoop::WORK_COUNT; i++) {
                const char* icon = (i < 10) ? icons[i] : LV_SYMBOL_LIST;
                lv_obj_t* btn = lv_list_add_btn(list, icon, gameLoop->works[i].name);
                lv_obj_add_event_cb(btn, work_btn_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
            }
        }
    }

} // namespace VPet
