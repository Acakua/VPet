#pragma once

// GraphCore.hpp — Từ điển hoạt ảnh (Animation Dictionary)
// Port từ: VPet-Simulator.Core/Graph/GraphCore.cs (381 dòng)
//
// BỎ QUA (WPF/Windows-only):
//   - Dispatcher                     → ESP32 single-thread, không cần
//   - CachePath / Cache              → Không dùng file cache trên ESP32
//   - CommUIElements (UIElement)     → LVGL thay thế
//   - Config(LpsDocument)            → Đọc từ manifest.json trên SD Card
//   - StrGetString/LocalizeCore      → Không i18n trên ESP32
//
// KIẾN TRÚC TRÊN ESP32:
//   Thay vì load ảnh vào RAM như C# (IGraph chứa BitmapImage),
//   ESP32 chỉ lưu ĐƯỜNG DẪN file trên SD Card.
//   AnimationEntry = metadata (path, frame count, timing) — không có pixel data.

#include <stdint.h>
#include <string.h>
#include <Arduino.h>
#include <SD.h>
#include "GameSave.hpp"
#include "GraphInfo.hpp"
#include "Utils.hpp"

namespace VPet {

    // ========================================================================
    // AnimationEntry — Metadata của 1 animation file trên SD Card
    // Tương đương: IGraph interface trong C# nhưng KHÔNG chứa ảnh,
    //              chỉ chứa thông tin để AnimationPlayer load từ SD.
    //
    // Cấu trúc thư mục trên SD Card (tạo bởi script Python):
    //   /pet/default/nomal/loop/     → frames: 000_150.bin, 001_150.bin, ...
    //   Tên file: <index>_<duration_ms>.bin
    // ========================================================================
    struct AnimationEntry {
        char path[128];         // Đường dẫn thư mục trên SD (ví dụ: /pet/default/nomal/loop)
        GraphInfo info;         // Metadata: type, animat, modeType, name
        uint16_t frameCount;    // Số lượng frame
        uint16_t defaultDuration; // Thời gian mỗi frame mặc định (ms)

        AnimationEntry() : frameCount(0), defaultDuration(100) {
            path[0] = '\0';
        }

        AnimationEntry(const char* _path, const GraphInfo& _info,
                       uint16_t frames, uint16_t dur)
            : info(_info), frameCount(frames), defaultDuration(dur)
        {
            strncpy(path, _path, sizeof(path) - 1);
            path[sizeof(path) - 1] = '\0';
        }
    };

    // ========================================================================
    // GraphConfig — Cấu hình từ file config.json trên SD Card
    // Port từ: GraphCore.cs dòng 214-377 (class Config)
    //
    // Trên C#: Đọc từ file LPS (LinePutScript format)
    // Trên ESP32: Đọc từ /pet/config.json (tạo bởi script Python)
    //
    // Tất cả tọa độ đã được nhân ZoomRatio (0.32) khi Python tạo config,
    // nên giá trị ở đây là tọa độ thực trên màn hình 320x480 rồi.
    // ========================================================================
    struct GraphConfig {
        // === Vùng cảm ứng Xoa đầu (Touch Head) ===
        // Port từ: Config.TouchHeadLocate, Config.TouchHeadSize
        int16_t touchHead_x, touchHead_y;      // Góc trên-trái
        int16_t touchHead_w, touchHead_h;      // Kích thước

        // === Vùng cảm ứng Chạm thân (Touch Body) ===
        // Port từ: Config.TouchBodyLocate, Config.TouchBodySize
        int16_t touchBody_x, touchBody_y;
        int16_t touchBody_w, touchBody_h;

        // === Vùng cảm ứng Nhấc Pet (Raised) — 4 trạng thái ===
        // Port từ: Config.TouchRaisedLocate[4], Config.TouchRaisedSize[4]
        int16_t touchRaised_x[4], touchRaised_y[4];
        int16_t touchRaised_w[4], touchRaised_h[4];

        // === Điểm neo khi nhấc (Raise Point) — 4 trạng thái ===
        // Port từ: Config.RaisePoint[4]
        // [0]=Happy, [1]=Normal, [2]=PoorCondition, [3]=Ill
        int16_t raisePoint_x[4], raisePoint_y[4];

        // === Thời gian phát animation (Duration dictionary) ===
        // Port từ: Config.GetDuration(name)
        // Mặc định 10 × đơn vị cơ bản (xem GetDuration)
        // Trên ESP32: Lưu trong mảng tĩnh (tối đa 8 animation custom duration)
        static constexpr uint8_t MAX_DURATION_ENTRIES = 8;
        struct DurationEntry {
            char name[32];
            int16_t value;         // Số đơn vị phát (giống C# GetDuration)
        } durations[MAX_DURATION_ENTRIES];
        uint8_t durationCount;

        // === Danh sách công việc ===
        // Port từ: Config.Works (List<Work>)
        static constexpr uint8_t MAX_WORKS = 8;
        Work works[MAX_WORKS];
        uint8_t workCount;

        GraphConfig()
            : touchHead_x(0), touchHead_y(0), touchHead_w(100), touchHead_h(100)
            , touchBody_x(0), touchBody_y(100), touchBody_w(100), touchBody_h(150)
            , durationCount(0), workCount(0)
        {
            for (int i = 0; i < 4; i++) {
                touchRaised_x[i] = 0; touchRaised_y[i] = 0;
                touchRaised_w[i] = 50; touchRaised_h[i] = 50;
                raisePoint_x[i] = 160; raisePoint_y[i] = 100;
            }
        }

        // Port từ: Config.GetDuration(name) dòng 266
        // C#: Duration.GetInt(name ?? "", 10)  → trả về 10 nếu không tìm thấy
        int16_t getDuration(const char* name) const {
            if (!name) return 10;
            for (uint8_t i = 0; i < durationCount; i++) {
                if (strncmp(durations[i].name, name, 31) == 0)
                    return durations[i].value;
            }
            return 10; // Mặc định = 10 (giống C#)
        }

        // Thêm duration entry (khi đọc từ JSON)
        bool addDuration(const char* name, int16_t value) {
            if (durationCount >= MAX_DURATION_ENTRIES) return false;
            strncpy(durations[durationCount].name, name, 31);
            durations[durationCount].value = value;
            durationCount++;
            return true;
        }
    };

    // ========================================================================
    // GraphCore — Từ điển hoạt ảnh trung tâm
    // Port từ: GraphCore.cs dòng 20-208
    //
    // Cấu trúc dữ liệu chính (tương đương C#):
    //   C#: Dictionary<GraphType, HashSet<string>> GraphsName
    //   C++: graphsName[GraphType] = mảng tên animation (tối đa N/loại)
    //
    //   C#: Dictionary<string, Dictionary<AnimatType, List<IGraph>>> GraphsList
    //   C++: entries[] = mảng phẳng tất cả AnimationEntry
    //        (tìm kiếm bằng linear scan — phù hợp với số lượng nhỏ ~200)
    //
    // Lý do dùng mảng thay vì std::map:
    //   - ESP32 PSRAM: không có overhead của node-based containers
    //   - Số animation thực tế < 200 (sau khi lọc N mode)
    //   - Linear scan 200 phần tử ≈ 10µs → chấp nhận được
    // ========================================================================

    static constexpr uint16_t MAX_ANIMATION_ENTRIES = 256;
    static constexpr uint8_t  MAX_NAMES_PER_TYPE    = 32;

    struct GraphCore {

        // Tất cả animation entries (thay cho GraphsList)
        AnimationEntry entries[MAX_ANIMATION_ENTRIES];
        uint16_t entryCount;

        // Chỉ mục tên theo GraphType (thay cho GraphsName)
        // graphsName[type][i] = tên của animation thứ i thuộc loại type
        struct NameList {
            char names[MAX_NAMES_PER_TYPE][32];
            uint8_t count;
        } graphsName[static_cast<uint8_t>(GraphType::GRAPH_TYPE_COUNT)];

        // Cấu hình
        GraphConfig config;

        GraphCore() : entryCount(0) {
            for (int i = 0; i < (int)GraphType::GRAPH_TYPE_COUNT; i++) {
                graphsName[i].count = 0;
            }
        }

        // ====================================================================
        // load() — Quét thư mục pet trên SD và tự động đăng ký hoạt ảnh
        //
        // Chiến lược quét thư mục:
        //   root/ <GraphName>/ <ModeName>/ <AnimatIndex>/ *.bin
        //   Ví dụ: /pet/Default/Nomal/1/000_150.bin
        // ====================================================================
        bool load(const char* root) {
            VPET_LOG_I("SD", "Starting SD animation scan at root: %s", root);
            VPET_TIME_START(SDScan);

            File rootDir = SD.open(root);
            if (!rootDir || !rootDir.isDirectory()) {
                VPET_LOG_E("SD", "Failed to open root directory or not a directory: %s", root);
                if (rootDir) rootDir.close();
                return false;
            }

            // 1. Quét đệ quy toàn bộ thư mục /pet
            char rootPath[160];
            snprintf(rootPath, sizeof(rootPath), "%s", root);
            _scanRecursive(rootDir, rootPath);
            
            rootDir.close();
            
            VPET_TIME_END(SDScan, "SD");
            VPET_LOG_I("SD", "Scan complete. Total animations registered: %u", entryCount);
            return entryCount > 0;
        }

        // Đệ quy tìm tất cả các thư mục chứa file .bin
        void _scanRecursive(File& dir, const char* currentPath) {
            File entry = dir.openNextFile();
            bool hasBinFiles = false;
            uint16_t binCount = 0;

            while (entry) {
                if (entry.isDirectory()) {
                    char nextPath[160];
                    snprintf(nextPath, sizeof(nextPath), "%s/%s", currentPath, entry.name());
                    File subDir = SD.open(nextPath);
                    if (subDir) {
                        _scanRecursive(subDir, nextPath);
                        subDir.close();
                    }
                } else {
                    const char* fname = entry.name();
                    if (strstr(fname, ".bin")) {
                        hasBinFiles = true;
                        binCount++;
                    }
                }
                entry.close();
                entry = dir.openNextFile();
            }

            if (hasBinFiles) {
                _registerAnimationPath(currentPath, binCount);
            }
        }

        // Đăng ký 1 đường dẫn đã xác nhận có file .bin
        void _registerAnimationPath(const char* fullPath, uint16_t frames) {
            GraphInfo info;
            _parseGraphInfo(fullPath, info);

            VPET_LOG_I("SD", "    Registered: [%s] -> Type:%u Name:%s Mode:%u Animat:%u Frames:%u", 
                       fullPath, (uint8_t)info.type, info.name, (uint8_t)info.modeType, (uint8_t)info.animat, frames);
            
            addGraph(AnimationEntry(fullPath, info, frames, 100)); // duration_ms sẽ đc parse lúc load frames
        }

        // Logic parse thông minh hỗ trợ cả 2 cấu trúc thư mục của VPet
        void _parseGraphInfo(const char* fullPath, GraphInfo& outInfo) {
            // fullPath có dạng: /pet/Default/Nomal/1 hoặc /pet/Drink/Happy/back_lay
            char pathCopy[160];
            strncpy(pathCopy, fullPath, sizeof(pathCopy) - 1);
            pathCopy[sizeof(pathCopy) - 1] = '\0';

            char* segments[10];
            int segCount = 0;

            char* token = strtok(pathCopy, "/");
            while (token != nullptr && segCount < 10) {
                if (strcmp(token, "pet") != 0) { // Bỏ qua root
                    segments[segCount++] = token;
                }
                token = strtok(nullptr, "/");
            }

            if (segCount == 0) return;

            // Segment 0 luôn là GraphType
            outInfo.type = _parseGraphType(segments[0]);
            strncpy(outInfo.name, segments[0], 31);
            outInfo.name[31] = '\0';
            outInfo.modeType = ModeType::Normal;
            outInfo.animat = AnimatType::Single;

            // Xử lý các segment còn lại
            for (int i = 1; i < segCount; i++) {
                const char* seg = segments[i];
                bool matchedNumeric = false;

                // Numeric animat (1, 2, 3, 4)
                if (strcmp(seg, "1") == 0) { outInfo.animat = AnimatType::A_Start; matchedNumeric = true; }
                else if (strcmp(seg, "2") == 0) { outInfo.animat = AnimatType::B_Loop; matchedNumeric = true; }
                else if (strcmp(seg, "3") == 0) { outInfo.animat = AnimatType::C_End; matchedNumeric = true; }
                else if (strcmp(seg, "4") == 0) { outInfo.animat = AnimatType::Single; matchedNumeric = true; }
                
                bool containsPrefix = false;
                if (!matchedNumeric) {
                    if (strncmp(seg, "A_", 2) == 0) { outInfo.animat = AnimatType::A_Start; containsPrefix = true; }
                    else if (strncmp(seg, "B_", 2) == 0) { outInfo.animat = AnimatType::B_Loop; containsPrefix = true; }
                    else if (strncmp(seg, "C_", 2) == 0) { outInfo.animat = AnimatType::C_End; containsPrefix = true; }
                    else if (strncmp(seg, "Single_", 7) == 0) { outInfo.animat = AnimatType::Single; containsPrefix = true; }
                }

                bool exactMode = false;
                if (strstr(seg, "Happy")) { outInfo.modeType = ModeType::Happy; exactMode = (strcmp(seg, "Happy") == 0); }
                else if (strstr(seg, "Nomal")) { outInfo.modeType = ModeType::Normal; exactMode = (strcmp(seg, "Nomal") == 0); }
                else if (strstr(seg, "Ill")) { outInfo.modeType = ModeType::Ill; exactMode = (strcmp(seg, "Ill") == 0); }
                else if (strstr(seg, "PoorCondition")) { outInfo.modeType = ModeType::PoorCondition; exactMode = (strcmp(seg, "PoorCondition") == 0); }

                // Nếu không phải Animat và không chính xác bằng ModeName -> Cập nhật Name
                if (!containsPrefix && !exactMode && !matchedNumeric) {
                    strncpy(outInfo.name, seg, 31);
                    outInfo.name[31] = '\0';
                }
            }
        }

        // Helper Map tên -> Enum (Port từ logic VPet C#)
        GraphType _parseGraphType(const char* name) {
            if (strcmp(name, "Default") == 0) return GraphType::Default;
            if (strcmp(name, "Eat") == 0) return GraphType::Eat;
            if (strcmp(name, "Sleep") == 0) return GraphType::Sleep;
            // TODO: Bổ sung thêm các loại khác
            return GraphType::Common;
        }

        ModeType _parseModeType(const char* name) {
            if (strcmp(name, "Happy") == 0) return ModeType::Happy;
            if (strcmp(name, "Ill") == 0) return ModeType::Ill;
            if (strcmp(name, "PoorCondition") == 0) return ModeType::PoorCondition;
            return ModeType::Normal; // Default: Normal (mapped from SD folder "Nomal")
        }

    public:

        // ====================================================================
        // addGraph() — Thêm animation entry vào từ điển
        // Port từ: GraphCore.cs::AddGraph() dòng 58-80
        //
        // Logic C# gốc:
        //   1. Nếu type != Common → thêm name vào GraphsName[type]
        //   2. Thêm graph vào GraphsList[name][animat]
        // ====================================================================
        // [PORTED_TO_ESP32] - GraphCore.hpp::addGraph() - 2026-04-03
        bool addGraph(const AnimationEntry& entry) {
            if (entryCount >= MAX_ANIMATION_ENTRIES) return false;
            entries[entryCount++] = entry;

            // Nếu không phải Common → thêm tên vào chỉ mục loại
            if (entry.info.type != GraphType::Common) {
                auto typeIdx = (uint8_t)entry.info.type;
                auto& nameList = graphsName[typeIdx];

                // Kiểm tra trùng tên
                for (uint8_t i = 0; i < nameList.count; i++) {
                    if (strncmp(nameList.names[i], entry.info.name, 31) == 0)
                        return true; // Đã có, không thêm nữa
                }
                if (nameList.count < MAX_NAMES_PER_TYPE) {
                    strncpy(nameList.names[nameList.count], entry.info.name, 31);
                    nameList.names[nameList.count][31] = '\0';
                    nameList.count++;
                }
            }
            return true;
        }

        // ====================================================================
        // findName() — Lấy tên animation ngẫu nhiên theo loại
        // Port từ: GraphCore.cs::FindName() dòng 87-94
        //
        // C#: return gl.ElementAt(Function.Rnd.Next(gl.Count));
        // ====================================================================
        // [PORTED_TO_ESP32] - GraphCore.hpp::findName() - 2026-04-03
        const char* findName(GraphType type) const {
            auto typeIdx = (uint8_t)type;
            if (typeIdx >= (uint8_t)GraphType::GRAPH_TYPE_COUNT) return nullptr;
            const auto& nameList = graphsName[typeIdx];
            if (nameList.count == 0) return nullptr;
            uint8_t idx = (uint8_t)(Utils::randomInt(nameList.count));
            return nameList.names[idx];
        }

        // ====================================================================
        // findGraph() — Tìm animation theo tên + giai đoạn + trạng thái
        // Port từ: GraphCore.cs::FindGraph() dòng 101-140
        //
        // LOGIC FALLBACK 4 TẦNG (CỰC KỲ QUAN TRỌNG — khớp 100% C#):
        //   Tầng 1: Tìm đúng mode
        //   Tầng 2: Nếu mode=Ill → return null (không fallback)
        //   Tầng 3: Thử mode+1 (xuống 1 bậc — "向下兼容")
        //   Tầng 4: Thử mode-1 (lên 1 bậc — "向上兼容")
        //   Tầng 5: Bất kỳ mode nào (trừ Ill)
        // ====================================================================
        // [PORTED_TO_ESP32] - GraphCore.hpp::findGraph() - 2026-04-03
        const AnimationEntry* findGraph(const char* name, AnimatType animat, ModeType mode) const {
            if (!name) return nullptr;

            VPET_LOG_V("GFX", "Searching animation: %s, animat: %u, mode: %u", name, (uint8_t)animat, (uint8_t)mode);

            // Thu thập candidates khớp name + animat
            const AnimationEntry* candidates[MAX_ANIMATION_ENTRIES];
            uint16_t candCount = 0;
            for (uint16_t i = 0; i < entryCount; i++) {
                if (strncmp(entries[i].info.name, name, 31) == 0
                    && entries[i].info.animat == animat) {
                    candidates[candCount++] = &entries[i];
                }
            }
            
            if (candCount == 0) {
                VPET_LOG_W("GFX", "  Animation not found: %s", name);
                return nullptr;
            }

            // Tầng 1: Tìm đúng mode
            const AnimationEntry* result = _pickByMode(candidates, candCount, mode);
            if (result) return result;

            VPET_LOG_V("GFX", "  Exact mode %u not found, starting fallback...", (uint8_t)mode);

            // Tầng 2: Mode=Ill không fallback (dòng 114-117 C#)
            if (mode == ModeType::Ill) {
                VPET_LOG_V("GFX", "  Mode is Ill, skip fallback.");
                return nullptr;
            }

            // Tầng 3: Thử mode+1 ("向下兼容", dòng 118-125 C#)
            int nextMode = (int)mode + 1;
            if (nextMode <= (int)ModeType::PoorCondition) {
                VPET_LOG_V("GFX", "  Trying fallback mode +1: %d", nextMode);
                result = _pickByMode(candidates, candCount, (ModeType)nextMode);
                if (result) return result;
            }

            // Tầng 4: Thử mode-1 ("向上兼容", dòng 126-133 C#)
            int prevMode = (int)mode - 1;
            if (prevMode >= (int)ModeType::Happy) {
                VPET_LOG_V("GFX", "  Trying fallback mode -1: %d", prevMode);
                result = _pickByMode(candidates, candCount, (ModeType)prevMode);
                if (result) return result;
            }

            // Tầng 5: Bất kỳ mode nào (trừ Ill) — dòng 134-137 C#
            VPET_LOG_V("GFX", "  Trying any mode fallback (excluding Ill)...");
            const AnimationEntry* nonIll[MAX_ANIMATION_ENTRIES];
            uint16_t nonIllCount = 0;
            for (uint16_t i = 0; i < candCount; i++) {
                if (candidates[i]->info.modeType != ModeType::Ill)
                    nonIll[nonIllCount++] = candidates[i];
            }
            if (nonIllCount > 0) {
                const AnimationEntry* picked = nonIll[Utils::randomInt(nonIllCount)];
                VPET_LOG_V("GFX", "  Picked random fallback candidate: %s", picked->path);
                return picked;
            }

            VPET_LOG_E("GFX", "  All fallbacks failed for: %s", name);
            return nullptr;
        }

        // Phiên bản tìm tất cả (cho FindGraphs C#)
        // [PORTED_TO_ESP32] - GraphCore.hpp::findGraphs() - 2026-04-03
        uint16_t findGraphs(const char* name, AnimatType animat, ModeType mode,
                            const AnimationEntry** outBuf, uint16_t bufSize) const {
            if (!name || !outBuf || bufSize == 0) return 0;
            uint16_t count = 0;
            for (uint16_t i = 0; i < entryCount && count < bufSize; i++) {
                if (strncmp(entries[i].info.name, name, 31) == 0
                    && entries[i].info.animat == animat
                    && entries[i].info.modeType == mode) {
                    count++;
                }
            }
            return count;
        }

    private:
        // Helper: random pick từ filtered candidates theo mode
        const AnimationEntry* _pickByMode(const AnimationEntry* const* cands,
                                           uint16_t count, ModeType mode) const {
            const AnimationEntry* filtered[MAX_ANIMATION_ENTRIES];
            uint16_t fCount = 0;
            for (uint16_t i = 0; i < count; i++) {
                if (cands[i]->info.modeType == mode)
                    filtered[fCount++] = cands[i];
            }
            if (fCount == 0) return nullptr;
            if (fCount == 1) return filtered[0];
            return filtered[Utils::randomInt(fCount)];
        }
    };

} // namespace VPet
