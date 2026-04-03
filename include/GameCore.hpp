#pragma once

#include <stdint.h>
#include <string.h>
#include "GameSave.hpp"
#include "GraphInfo.hpp"

namespace VPet {

    // ========================================================================
    // TouchArea — Vùng cảm ứng trên màn hình
    // Port từ: GameCore.cs dòng 32-75
    //
    // Logic gốc C#: point.X - Locate.X nằm trong [0, Width]
    //               point.Y - Locate.Y nằm trong [0, Height]
    //
    // Trên ESP32: Tọa độ từ FT6336U đã nhân ZoomRatio (320/1000) rồi
    //             nên chúng ta dùng tọa độ trực tiếp.
    // ========================================================================

    // Callback khi chạm vào vùng — trả về true nếu xử lý thành công
    typedef bool (*TouchActionCallback)(void* context);

    struct TouchArea {
        int16_t x;              // 位置X — Tọa độ góc trên-trái
        int16_t y;              // 位置Y  
        int16_t width;          // 宽度 — Chiều rộng vùng
        int16_t height;         // 高度 — Chiều cao vùng
        bool isPress;           // 是否长按 — false = tap, true = long-press
        TouchActionCallback action; // 回调函数 — Hàm xử lý khi chạm
        void* actionContext;    // Dữ liệu ngữ cảnh cho callback

        TouchArea()
            : x(0), y(0), width(0), height(0)
            , isPress(false), action(nullptr), actionContext(nullptr)
        {}

        TouchArea(int16_t _x, int16_t _y, int16_t _w, int16_t _h,
                  TouchActionCallback _action, void* _ctx = nullptr, bool _isPress = false)
            : x(_x), y(_y), width(_w), height(_h)
            , isPress(_isPress), action(_action), actionContext(_ctx)
        {}

        // Port từ: GameCore.cs dòng 69-73
        // return inx >= 0 && inx <= Size.Width && iny >= 0 && iny <= Size.Height
        bool touch(int16_t px, int16_t py) const {
            int16_t inx = px - x;
            int16_t iny = py - y;
            return (inx >= 0 && inx <= width && iny >= 0 && iny <= height);
        }
    };

    // ========================================================================
    // ESP32Controller — Bộ điều khiển phần cứng ESP32
    // Port từ: IController.cs (interface, 77 dòng)
    //
    // KHÁC BIỆT CHÍNH SO VỚI C# WINDOWS:
    // - ZoomRatio:    C# = do người dùng điều chỉnh kích thước cửa sổ
    //                 ESP32 = cố định 320.0 / 1000.0 = 0.32
    // - MoveWindows:  C# = di chuyển cửa sổ trên desktop
    //                 ESP32 = KHÔNG CẦN (Pet luôn ở giữa màn 4 inch)
    // - GetWindowsDistance*: C# = khoảng cách cửa sổ đến mép desktop
    //                        ESP32 = KHÔNG CẦN (màn cố định)
    // - ShowPanel:    C# = mở dialog WPF
    //                 ESP32 = vẽ panel LVGL (sẽ implement ở Phân hệ 3)
    // ========================================================================
    struct ESP32Controller {
        // === Cấu hình cố định ===
        static constexpr float ZOOM_RATIO = 320.0f / 1000.0f;  // 缩放比例 — Tỷ lệ thu nhỏ
        static constexpr int16_t SCREEN_WIDTH = 320;
        static constexpr int16_t SCREEN_HEIGHT = 480;

        // === Cấu hình có thể thay đổi ===
        int pressLength;        // 按多久视为长按 — Ngưỡng long-press (ms)
        int interactionCycle;   // 互动周期 — Chu kỳ Pet tự phát sinh hành vi ngẫu nhiên
        bool enableFunction;    // 启用计算 — Bật/tắt tính toán sinh tồn (FunctionSpend)

        ESP32Controller()
            : pressLength(500)      // Mặc định 500ms
            , interactionCycle(20)  // Mặc định 20 (giống C#)
            , enableFunction(true)
        {}

        // === Các hàm Windows bị loại bỏ trên ESP32 ===
        // void moveWindows(double x, double y)        → KHÔNG CẦN
        // double getWindowsDistanceLeft()              → KHÔNG CẦN  
        // double getWindowsDistanceRight()             → KHÔNG CẦN
        // double getWindowsDistanceUp()                → KHÔNG CẦN
        // double getWindowsDistanceDown()              → KHÔNG CẦN
        // void resetPosition()                         → KHÔNG CẦN
        // bool checkPosition()                         → KHÔNG CẦN
        // bool rePositionActive                        → KHÔNG CẦN
    };

    // ========================================================================
    // SayInfo — Thông tin hội thoại Pet
    // Port từ: SayInfo.cs (211 dòng)
    //
    // ĐƠN GIẢN HÓA CHO ESP32:
    // - Bỏ: UIElement (WPF), stream-based SayInfo (async), Event system
    // - Giữ: text, graphName, force flag
    // - Trên ESP32: Hội thoại hiển thị bằng lv_label trên LVGL
    // ========================================================================
    struct SayInfo {
        char text[128];         // 说话内容 — Nội dung nói
        char graphName[32];     // 图像名 — Tên animation khi nói
        bool force;             // 强制显示 — Bắt buộc phát animation Say

        SayInfo() : force(false) {
            text[0] = '\0';
            graphName[0] = '\0';
        }

        SayInfo(const char* _text, const char* _graph = nullptr, bool _force = false)
            : force(_force)
        {
            strncpy(text, _text, sizeof(text) - 1);
            text[sizeof(text) - 1] = '\0';
            if (_graph) {
                strncpy(graphName, _graph, sizeof(graphName) - 1);
                graphName[sizeof(graphName) - 1] = '\0';
            } else {
                graphName[0] = '\0';
            }
        }
    };

    // ========================================================================
    // GameCore — Bộ chứa (Container) kết nối tất cả thành phần
    // Port từ: GameCore.cs dòng 10-28
    //
    // Trên C#: Chứa con trỏ đến Controller, Graph, Save, TouchEvent list
    // Trên ESP32: Tương tự nhưng dùng con trỏ thô (sở hữu bởi main.cpp)
    //
    // Các thành phần sẽ được khởi tạo tuần tự trong setup():
    //   1. controller (cấu hình phần cứng)
    //   2. save (dữ liệu game)  
    //   3. graph (sẽ implement ở Phân hệ 2B: GraphCore)
    //   4. touchEvents (sẽ setup khi có màn cảm ứng)
    // ========================================================================

    // Forward declarations cho các thành phần chưa implement
    struct GraphCore;  // Phân hệ 2B — sẽ tạo sau

    static constexpr int MAX_TOUCH_AREAS = 8;  // Giới hạn vùng cảm ứng

    struct GameCore {
        ESP32Controller* controller;    // 控制器 — Bộ điều khiển phần cứng
        GraphCore* graph;               // 图形核心 — Bộ quản lý hoạt ảnh
        GameSave* save;                 // 游戏数据 — Dữ liệu game save
        TouchArea touchEvents[MAX_TOUCH_AREAS]; // 触摸事件 — Danh sách vùng cảm ứng
        uint8_t touchEventCount;        // Số vùng cảm ứng hiện tại

        GameCore()
            : controller(nullptr)
            , graph(nullptr)
            , save(nullptr)
            , touchEventCount(0)
        {}

        // Thêm vùng cảm ứng mới
        bool addTouchArea(const TouchArea& area) {
            if (touchEventCount >= MAX_TOUCH_AREAS) return false;
            touchEvents[touchEventCount++] = area;
            return true;
        }

        // Kiểm tra tất cả vùng cảm ứng — trả về true nếu có vùng nào xử lý
        // Port logic từ: MainDisplay.cs Main_MouseDown (dispatch touch events)
        bool processTouchAt(int16_t px, int16_t py, bool isLongPress) {
            for (uint8_t i = 0; i < touchEventCount; i++) {
                if (touchEvents[i].touch(px, py)) {
                    if (touchEvents[i].isPress == isLongPress) {
                        if (touchEvents[i].action) {
                            return touchEvents[i].action(touchEvents[i].actionContext);
                        }
                    }
                }
            }
            return false;
        }
    };

} // namespace VPet
