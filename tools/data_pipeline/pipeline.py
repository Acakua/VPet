import os
import glob
import re
import struct
import json
import sys
from PIL import Image

# Thiết lập encode cho terminal để in được tiếng Trung
if sys.stdout.encoding != 'utf-8':
    try:
        import codecs
        sys.stdout = codecs.getwriter('utf-8')(sys.stdout.detach())
    except:
        pass

# Cấu hình kích thước mục tiêu cho màn hình ESP32-S3 (320x320 hoặc tùy chỉnh)
TARGET_WIDTH = 320
TARGET_HEIGHT = 320

# Ngưỡng Alpha: Dưới ngưỡng này sẽ coi là trong suốt tuyệt đối (0, 255, 0)
# Trên ngưỡng này sẽ coi là đặc (Opaque). Việc này giúp khử viền xanh.
ALPHA_THRESHOLD = 128

def get_rgb565(r, g, b):
    """Chuyển đổi RGB888 sang RGB565 (16-bit)"""
    r5 = (r >> 3) & 0x1F
    g6 = (g >> 2) & 0x3F
    b5 = (b >> 3) & 0x1F
    return (r5 << 11) | (g6 << 5) | b5

def process_image(img_path, out_bin_path):
    """
    Xử lý ảnh PNG sang Binary RGB565 với cơ chế khử viền xanh.
    """
    try:
        with Image.open(img_path) as img:
            img = img.convert("RGBA")
            
            # 1. Resize ảnh (Sử dụng Resampling.LANCZOS để giữ chất lượng cao nhất)
            # Lưu ý: Việc resize có thể tạo thêm các điểm ảnh bán trong suốt ở mép
            img = img.resize((TARGET_WIDTH, TARGET_HEIGHT), Image.Resampling.LANCZOS)
            
            # 2. Xử lý khử viền xanh bằng Thresholding
            datas = img.getdata()
            new_data = []
            
            for item in datas:
                # item là tuple (R, G, B, A)
                r, g, b, a = item
                
                if a < ALPHA_THRESHOLD:
                    # Nếu quá trong suốt -> Ép về màu xanh Chroma Key thuần (0, 255, 0)
                    new_data.append((0, 255, 0))
                else:
                    # Nếu đủ đặc -> Giữ nguyên màu gốc, KHÔNG pha trộn với màu xanh
                    # Điều này triệt tiêu viền xanh lá cây (halo)
                    # Lưu ý: Nếu nhân vật có màu xanh lá cây trùng khớp (0,255,0), 
                    # ta nên điều chỉnh r hoặc b một chút để không bị trong suốt nhầm.
                    if r == 0 and g == 255 and b == 0:
                        g = 254 # Thay đổi nhẹ để tránh bị trùng Chroma Key
                    new_data.append((r, g, b))
            
            # 3. Tạo ảnh RGB mới từ dữ liệu đã xử lý
            final_img = Image.new("RGB", (TARGET_WIDTH, TARGET_HEIGHT))
            final_img.putdata(new_data)
            
            # 4. Xuất ra định dạng Binary RGB565 Little-Endian
            pixels = final_img.load()
            bin_data = bytearray()
            
            for y in range(TARGET_HEIGHT):
                for x in range(TARGET_WIDTH):
                    r, g, b = pixels[x, y]
                    rgb565 = get_rgb565(r, g, b)
                    # Đóng gói theo chuẩn Little-Endian (ESP32 thường dùng để nạp vào PSRAM)
                    bin_data.extend(struct.pack('<H', rgb565))
            
            # 5. Ghi file
            with open(out_bin_path, "wb") as f:
                f.write(bin_data)
                
    except Exception as e:
        print(f"Error processing {img_path}: {e}")

def process_animation_folder(input_dir, output_dir, relative_path):
    """
    Xử lý một thư mục chứa các file PNG.
    """
    png_files = glob.glob(os.path.join(input_dir, "*.png"))
    if not png_files:
        return None
        
    os.makedirs(output_dir, exist_ok=True)
    frames_meta = []
    
    for png in png_files:
        basename = os.path.basename(png)
        # Pattern chuẩn của VPet: _[index]_[duration].png
        match = re.search(r'_(\d+)_(\d+)', basename)
        if match:
            frame_idx = int(match.group(1))
            duration = int(match.group(2))
            
            out_name = f"{frame_idx:03d}_{duration}.bin"
            out_path = os.path.join(output_dir, out_name)
            
            # Chỉ in basename để tránh lỗi Unicode terminal nếu có
            print(f" -> {relative_path}: {basename} -> {out_name}")
            process_image(png, out_path)
            
            frames_meta.append({
                "id": frame_idx,
                "duration_ms": duration,
                "bin_file": os.path.join(relative_path, out_name).replace("\\", "/")
            })
            
    if frames_meta:
        frames_meta.sort(key=lambda x: x["id"])
        return {
            "total_frames": len(frames_meta),
            "frames": frames_meta
        }
    return None

def main():
    root_input = r"C:\Users\admin\Documents\Work\VPet\VPet-Simulator.Windows\mod\0000_core\pet\vup"
    root_output = r"c:\Users\admin\Documents\PlatformIO\Projects\Vpet-ESP32S3\sdcard_data"
    
    print("=== VPet ESP32 Deep Pipeline V3 (Recursive Scan) ===")
    
    if not os.path.exists(root_input):
        print(f"Error: Input directory not found: {root_input}")
        return

    os.makedirs(root_output, exist_ok=True)
    index_data = {}
    
    # Sử dụng os.walk để quét toàn bộ cây thư mục đệ quy
    for root, dirs, files in os.walk(root_input):
        # Kiểm tra xem thư mục hiện tại có file PNG không
        if any(f.lower().endswith('.png') for f in files):
            # Tính toán đường dẫn tương đối so với vup (ví dụ: WORK\WorkONE\A_Nomal)
            rel_path = os.path.relpath(root, root_input)
            
            # Tạo đường dẫn đầu ra tương ứng
            out_dir = os.path.join(root_output, rel_path)
            
            # Xử lý folder
            anim_meta = process_animation_folder(root, out_dir, rel_path)
            
            if anim_meta:
                # Phân cấp index_data theo đường dẫn để dễ quản lý
                parts = rel_path.split(os.sep)
                current = index_data
                for i, part in enumerate(parts[:-1]):
                    if part not in current:
                        current[part] = {}
                    current = current[part]
                
                current[parts[-1]] = anim_meta

    # Xuất file index
    index_path = os.path.join(root_output, "animations.idx")
    with open(index_path, "w", encoding='utf-8') as f:
        json.dump(index_data, f, indent=4, ensure_ascii=False)
        
    print(f"\nDone! All animations converted recursively.")
    print(f"Output folder: {root_output}")

if __name__ == "__main__":
    main()

