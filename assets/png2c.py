from PIL import Image
import sys
import os

def png_to_c_header(input_file):
    # Derive output file name and array name from input file
    base_name = os.path.splitext(os.path.basename(input_file))[0]
    output_file = f"{base_name}.h"
    array_name = base_name

    # Open the PNG file
    img = Image.open(input_file).convert("RGBA")
    img_width, img_height = img.size

    if img_width != 8:
        raise ValueError("This script only supports images with a width of 8 pixels.")

    # Convert image to 1-bit (black/white) representation
    c_array = []
    for y in range(img_height):
        byte = 0
        for x in range(img_width):
            r, g, b, a = img.getpixel((x, y))
            is_white = r > 0 or g > 0 or b > 0  # Non-black pixels are considered white
            byte = (byte << 1) | is_white
        c_array.append(byte)

    # Generate C header
    with open(output_file, "w") as f:
        f.write(f"#ifndef _{array_name.upper()}_H\n")
        f.write(f"#define _{array_name.upper()}_H\n\n")
        f.write(f"// Image dimensions\n")
        f.write(f"#define {array_name.upper()}_WIDTH {img_width}\n")
        f.write(f"#define {array_name.upper()}_HEIGHT {img_height}\n\n")
        f.write("// Image data (1 bit per pixel, packed into bytes)\n")
        f.write("// Each byte is one row (8 pixels), MSB first\n")
        f.write("// 0 = black, 1 = white\n")
        f.write(f"const unsigned char {array_name}_data[] = {{\n    ")
        
        hex_array = [f"0x{byte:02X}" for byte in c_array]
        for i in range(0, len(hex_array), 12):
            f.write(", ".join(hex_array[i:i+12]))
            if i + 12 < len(hex_array):
                f.write(",\n    ")
        f.write("\n};\n\n")
        f.write(f"#endif // _{array_name.upper()}_H\n")
    
    print(f"Header file '{output_file}' generated successfully.")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python png_to_c_header.py <input_file>")
        sys.exit(1)

    input_file = sys.argv[1]
    try:
        png_to_c_header(input_file)
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)
