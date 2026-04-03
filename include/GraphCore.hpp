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
            File rootDir = SD.open(root);
            if (!rootDir || !rootDir.isDirectory()) {
                if (rootDir) rootDir.close();
                return false;
            }

            // 1. Quét các GraphName (Default, Eat, ...)
            File graphDir = rootDir.openNextFile();
            while (graphDir) {
                if (graphDir.isDirectory()) {
                    const char* graphName = graphDir.name();
                    // Bỏ qua thư mục ẩn/hệ thống
                    if (graphName[0] != '.') {
                        _scanModeFolders(graphDir, graphName);
                    }
                }
                graphDir.close();
                graphDir = rootDir.openNextFile();
            }
            rootDir.close();
            return entryCount > 0;
        }

    private:
        // Quét các ModeName (Nomal, Happy, ...)
        void _scanModeFolders(File& graphDir, const char* graphName) {
            File modeDir = graphDir.openNextFile();
            while (modeDir) {
                if (modeDir.isDirectory()) {
                    const char* modeName = modeDir.name();
                    _scanAnimatFolders(modeDir, graphName, modeName);
                }
                modeDir.close();
                modeDir = graphDir.openNextFile();
            }
        }

        // Quét các AnimatIndex (1, 2, 3, 4)
        void _scanAnimatFolders(File& modeDir, const char* graphName, const char* modeName) {
            File animatDir = modeDir.openNextFile();
            while (animatDir) {
                if (animatDir.isDirectory()) {
                    const char* animatIdxStr = animatDir.name();
                    uint8_t animatIdx = (uint8_t)atoi(animatIdxStr);
                    
                    if (animatIdx >= 1 && animatIdx <= 4) {
                        _registerAnimation(animatDir, graphName, modeName, animatIdx);
                    }
                }
                animatDir.close();
                animatDir = modeDir.openNextFile();
            }
        }

        // Đăng ký 1 thư mục chứa các file .bin làm 1 AnimationEntry
        void _registerAnimation(File& animatDir, const char* graphName, 
                               const char* modeName, uint8_t animatIdx) {
            // Xác định metadata
            GraphInfo info;
            strncpy(info.name, graphName, 31);
            info.type = _parseGraphType(graphName);
            info.modeType = _parseModeType(modeName);
            info.animat = (AnimatType)(animatIdx - 1); // 1->A_Start (0), 2->B_Loop (1), ...

            // Tạo đường dẫn đầy đủ từ root (ví dụ: /pet/Default/Nomal/1)
            char fullPath[128];
            snprintf(fullPath, sizeof(fullPath), "/pet/%s/%s/%u", graphName, modeName, animatIdx);

            // Đếm số lượng frame trong thư mục (đơn giản hóa: lấy entry đầu tiên của .bin)
            uint16_t frames = 0;
            File f = animatDir.openNextFile();
            while (f) {
                if (!f.isDirectory()) frames++;
                f.close();
                f = animatDir.openNextFile();
            }

            if (frames > 0) {
                addGraph(AnimationEntry(fullPath, info, frames, 100));
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

            // Thu thập candidates khớp name + animat
            const AnimationEntry* candidates[MAX_ANIMATION_ENTRIES];
            uint16_t candCount = 0;
            for (uint16_t i = 0; i < entryCount; i++) {
                if (strncmp(entries[i].info.name, name, 31) == 0
                    && entries[i].info.animat == animat) {
                    candidates[candCount++] = &entries[i];
                }
            }
            if (candCount == 0) return nullptr;

            // Tầng 1: Tìm đúng mode
            const AnimationEntry* result = _pickByMode(candidates, candCount, mode);
            if (result) return result;

            // Tầng 2: Mode=Ill không fallback (dòng 114-117 C#)
            if (mode == ModeType::Ill) return nullptr;

            // Tầng 3: Thử mode+1 ("向下兼容", dòng 118-125 C#)
            int nextMode = (int)mode + 1;
            if (nextMode <= (int)ModeType::PoorCondition) {
                result = _pickByMode(candidates, candCount, (ModeType)nextMode);
                if (result) return result;
            }

            // Tầng 4: Thử mode-1 ("向上兼容", dòng 126-133 C#)
            int prevMode = (int)mode - 1;
            if (prevMode >= (int)ModeType::Happy) {
                result = _pickByMode(candidates, candCount, (ModeType)prevMode);
                if (result) return result;
            }

            // Tầng 5: Bất kỳ mode nào (trừ Ill) — dòng 134-137 C#
            const AnimationEntry* nonIll[MAX_ANIMATION_ENTRIES];
            uint16_t nonIllCount = 0;
            for (uint16_t i = 0; i < candCount; i++) {
                if (candidates[i]->info.modeType != ModeType::Ill)
                    nonIll[nonIllCount++] = candidates[i];
            }
            if (nonIllCount > 0)
                return nonIll[Utils::randomInt(nonIllCount)];

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
