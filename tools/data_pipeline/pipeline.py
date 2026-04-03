import os
import glob
import re
import math
import struct
import json
from PIL import Image

# Resize configuration
TARGET_WIDTH = 320
TARGET_HEIGHT = 320

def get_rgb565(r, g, b):
    r5 = (r >> 3) & 0x1F
    g6 = (g >> 2) & 0x3F
    b5 = (b >> 3) & 0x1F
    return (r5 << 11) | (g6 << 5) | b5

def process_image(img_path, out_bin_path):
    """
    Load image, resize using Lanczos, handle transparency (Chroma Key Green #00FF00),
    and output as raw RGB565 little-endian binary.
    """
    with Image.open(img_path) as img:
        img = img.convert("RGBA")
        
        # Resize to TARGET_WIDTH x TARGET_HEIGHT using LANCZOS
        img = img.resize((TARGET_WIDTH, TARGET_HEIGHT), Image.Resampling.LANCZOS)
        
        # Create a Chroma Key Green background
        bg = Image.new("RGBA", img.size, (0, 255, 0, 255))
        bg.paste(img, (0, 0), img) # Alpha composite
        final_img = bg.convert("RGB")
        
        # Generate RGB565 Little-Endian array
        pixels = final_img.load()
        width, height = final_img.size
        
        bin_data = bytearray()
        for y in range(height):
            for x in range(width):
                r, g, b = pixels[x, y]
                rgb565 = get_rgb565(r, g, b)
                # Pack as little-endian 16-bit
                bin_data.extend(struct.pack('<H', rgb565))
                
        # Write to binary file
        with open(out_bin_path, "wb") as f:
            f.write(bin_data)

def parse_info_lps(lps_path):
    """
    If info.lps exists, read metadata. For simplicity in MVP, we just extract names or ignore.
    Actually, VPet's fallback relies on standard filename formatting: _[index]_[duration].png 
    We will rely primarily on filenames for frame sequencing since the C# core does this too.
    """
    pass

def process_animation_dir(mode_dir, mode_name, output_base):
    """
    Process an animation directory containing numbered frame folders or directly images.
    Input example: .../Default/Nomal
    """
    anim_idx = {}
    
    # Subdirectories represent variants or AnimatTypes (e.g. 1, 2, 3)
    variants = [d for d in os.listdir(mode_dir) if os.path.isdir(os.path.join(mode_dir, d))]
    
    # If there are no subdirectories, the images might be straight in this folder.
    if len(variants) == 0:
        variants = ["."]
        
    for variant in variants:
        variant_dir = os.path.join(mode_dir, variant)
        
        # Output directory structure setup (e.g. sdcard_data/Nomal/1)
        out_var_dir = os.path.join(output_base, mode_name, variant) if variant != "." else os.path.join(output_base, mode_name)
        os.makedirs(out_var_dir, exist_ok=True)
        
        png_files = glob.glob(os.path.join(variant_dir, "*.png"))
        
        frames_meta = []
        for png in png_files:
            basename = os.path.basename(png)
            # Find Pattern: _000_250.png
            match = re.search(r'_(\d+)_(\d+)', basename)
            if match:
                frame_idx = int(match.group(1))
                duration = int(match.group(2))
                
                # Output filename
                out_name = f"{frame_idx:03d}.bin"
                out_path = os.path.join(out_var_dir, out_name)
                
                print(f" -> Converting {basename} to {out_name} | Duration: {duration}ms")
                process_image(png, out_path)
                
                frames_meta.append({
                    "id": frame_idx,
                    "duration_ms": duration,
                    "bin_file": os.path.join(mode_name, variant, out_name).replace("\\", "/") if variant != "." else os.path.join(mode_name, out_name).replace("\\", "/")
                })
        
        # Sort frames by index
        frames_meta.sort(key=lambda x: x["id"])
        
        anim_idx[variant] = {
            "total_frames": len(frames_meta),
            "frames": frames_meta
        }
        
    return anim_idx

def main():
    # Input parameter: Process entire vup tree
    root_input = r"C:\Users\admin\Documents\Work\VPet\VPet-Simulator.Windows\mod\0000_core\pet\vup"
    root_output = r"C:\Users\admin\Documents\Work\VPet\VPet-ESP32\sdcard_data"
    os.makedirs(root_output, exist_ok=True)
    
    print(f"=== VPet ESP32 Data Pipeline (FULL BATCH) ===")
    print(f"Input: {root_input}")
    print(f"Output: {root_output}")
    
    index_data = {}
    
    if os.path.isdir(root_input):
        # The first level is GraphType (e.g. Default, Drink, Eat)
        graph_types = [d for d in os.listdir(root_input) if os.path.isdir(os.path.join(root_input, d))]
        for graph_type in graph_types:
            print(f"> Processing GraphType: {graph_type} ...")
            graph_type_dir = os.path.join(root_input, graph_type)
            
            # The second level is ModeType (e.g. Happy, Nomal, Ill)
            mode_types = [d for d in os.listdir(graph_type_dir) if os.path.isdir(os.path.join(graph_type_dir, d))]
            
            # Keep index_data structured: index_data[GraphType][ModeType][AnimatType]
            if graph_type not in index_data:
                index_data[graph_type] = {}
                
            for mode_name in mode_types:
                # print(f"  -> Mode: {mode_name}")
                input_mode_dir = os.path.join(graph_type_dir, mode_name)
                
                # Output mapping will drop GraphType level folder?
                # Actually, output needs to mirror the folder structure: sdcard_data/GraphType/ModeType/AnimatType/
                # Let's override process_animation_dir's "root_output" dynamically.
                current_mode_out = os.path.join(root_output, graph_type)
                
                anim_meta = process_animation_dir(input_mode_dir, mode_name, current_mode_out)
                index_data[graph_type][mode_name] = anim_meta
    else:
        print("Input directory not found!")
        return

    # Export index file
    index_path = os.path.join(root_output, "animations.idx")
    with open(index_path, "w") as f:
        json.dump(index_data, f, indent=4)
        
    print(f"=== Pipeline Finished! ===")
    print(f"Exported Index: {index_path}")

if __name__ == "__main__":
    main()
