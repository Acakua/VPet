#pragma once

// Utils.hpp — Các hàm tiện ích cho VPet ESP32
// Port từ: VPet-Simulator.Core/Handle/Function.cs
//
// BỎ QUA (WPF/Windows-only):
//   - HEXToColor(), ColorToHEX()    → WPF Color
//   - ResourcesBrush(), BrushType   → WPF Application Resources
//   - LPSConvertToLower             → LinePutScript serializer

#include <Arduino.h>
#include <stdint.h>

namespace VPet {

    // ========================================================================
    // Rnd — Hàm số ngẫu nhiên
    // Port từ: Function.cs dòng 25 (static Random Rnd = new Random())
    //
    // C#:   Function.Rnd.Next(n)    → số nguyên ngẫu nhiên [0, n)
    // ESP32: random(n)              → tương đương (Arduino built-in)
    //        esp_random()           → nguồn entropy phần cứng (32-bit)
    // ========================================================================
    namespace Utils {

        // [PORTED_TO_ESP32] - Utils.hpp::randomInt() - 2026-04-03
        // Tương đương: Function.Rnd.Next(max)
        inline int32_t randomInt(int32_t max) {
            if (max <= 0) return 0;
            return (int32_t)(esp_random() % (uint32_t)max);
        }

        inline int32_t randomInt(int32_t min, int32_t max) {
            if (max <= min) return min;
            return min + (int32_t)(esp_random() % (uint32_t)(max - min));
        }

        // Tương đương: Function.Rnd.NextDouble() -> [0.0, 1.0)
        inline double randomDouble() {
            return (double)esp_random() / (double)UINT32_MAX;
        }

        inline double randomDouble(double max) {
            return randomDouble() * max;
        }

        inline double randomDouble(double min, double max) {
            return min + randomDouble() * (max - min);
        }

        // ====================================================================
        // MemoryUsage — Bộ nhớ Heap đang dùng (bytes)
        // Port từ: Function.cs dòng 88-91
        // C#: Process.GetCurrentProcess().WorkingSet64
        // ESP32: ESP.getHeapSize() - ESP.getFreeHeap()
        // ====================================================================
        // [PORTED_TO_ESP32] - Utils.hpp::memoryUsage() - 2026-04-03
        inline uint32_t memoryUsage() {
            return ESP.getHeapSize() - ESP.getFreeHeap();
        }

        // ====================================================================
        // MemoryAvailable — Bộ nhớ PSRAM còn trống (bytes)
        // Port từ: Function.cs dòng 92-112
        // C#: GC.GetGCMemoryInfo().TotalAvailableMemoryBytes
        // ESP32: ESP.getFreePsram() — PSRAM là bộ nhớ lớn dùng cho ảnh
        // ====================================================================
        // [PORTED_TO_ESP32] - Utils.hpp::memoryAvailable() - 2026-04-03
        inline uint32_t memoryAvailable() {
            return ESP.getFreePsram();
        }

        // Bộ nhớ Heap nội bộ còn trống (dùng cho stack, biến nhỏ)
        inline uint32_t heapFree() {
            return ESP.getFreeHeap();
        }

        // ====================================================================
        // ComCheck — Đếm số dấu câu trong chuỗi = số câu ước tính
        // Port từ: Function.cs dòng 121-124
        // C#: text.Replace("\r","").Replace("\n\n","\n").Count(com.Contains)
        // Dùng để ước tính thời gian hiển thị hội thoại (dài hơn = chờ lâu hơn)
        // ====================================================================
        // [PORTED_TO_ESP32] - Utils.hpp::comCheck() - 2026-04-03
        inline int comCheck(const char* text) {
            if (!text) return 0;
            // Các ký tự dấu câu tương đương Function.cs dòng 116
            const char punctuation[] = {
                '\xef', '\xbc', '\x8c',  // '，' UTF-8: 0xEF BC 8C
                '\xe3', '\x80', '\x82',  // '。' UTF-8: 0xE3 80 82
                '\xef', '\xbc', '\x81',  // '！' UTF-8: 0xEF BC 81
                '\xef', '\xbc', '\x9f',  // '？' UTF-8: 0xEF BC 9F
                '\xef', '\xbc', '\x9b',  // '；' UTF-8: 0xEF BC 9B
                '\0'
            };
            int count = 0;
            for (const char* p = text; *p != '\0'; p++) {
                char c = *p;
                // ASCII dấu câu đơn giản — khớp với C# list (dòng 116)
                if (c == '.' || c == ',' || c == '!' || c == '?'
                    || c == ';' || c == ':' || c == '\n') {
                    count++;
                }
            }
            return count;
        }

    } // namespace Utils

} // namespace VPet
