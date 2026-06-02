import re
import os
import sys

def read_font_from_file(filepath):
    if not os.path.exists(filepath):
        print(f"Ошибка: Файл '{filepath}' не найден.")
        sys.exit(1)
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()
    pattern = r'unsigned\s+char\s+font8x8_basic\s*\[.*?\]\s*=\s*\{(.*?)\};'
    match = re.search(pattern, content, re.DOTALL)

    if not match:
        print("Ошибка: Не удалось найти массив 'font8x8_basic' в файле.")
        sys.exit(1)

    return match.group(1)

def parse_font_data(raw_data):
    font = {}
    entry_pattern = re.compile(r'\[(\d+)\]\s*=\s*\{([^}]+)\}')
    matches = entry_pattern.findall(raw_data)
    
    for index_str, bytes_str in matches:
        idx = int(index_str)
        byte_vals = []
        for b in bytes_str.split(','):
            b = b.strip()
            if not b: continue
            
            try:
                if b.startswith('0x') or b.startswith('0X'):
                    byte_vals.append(int(b, 16))
                else:
                    byte_vals.append(int(b))
            except ValueError:
                continue 
        
        if len(byte_vals) == 8:
            font[idx] = byte_vals
            
    return font

def render_char_detailed(pixel_bytes):
    lines = []
    for byte in pixel_bytes:
        line = ""
        for bit_pos in range(7, -1, -1): # Старший бит слева
            if byte & (1 << bit_pos):
                line += "██" 
            else:
                line += "··"
        lines.append(line)
    return lines

def print_all_glyphs_detailed(font):
    print("=" * 40)
    print("DETAILED FONT VIEW (ALL GLYPHS)")
    print("=" * 40)
    sorted_keys = sorted(font.keys())
    
    for code in sorted_keys:
        if 32 <= code <= 126:
            char_display = f"'{chr(code)}'"
        elif code == 32:
            char_display = "'SPC'"
        else:
            char_display = f"Ctrl-{code}" if code < 32 else f"Ext-{code}"
        print(f"\n--- Char: {char_display:<6} | Code: {code:<3} (0x{code:02X}) ---")
        glyph_lines = render_char_detailed(font[code])
        for line in glyph_lines:
            print(f"  {line}")

def main():
    font_file_path = "../src/libc/font.c"
    if not os.path.exists(font_file_path):
        font_file_path = "src/libc/font.c"
    if not os.path.exists(font_file_path):
        font_file_path = "font.c"
    print(f"Reading font from: {font_file_path}...")
    try:
        raw_array_content = read_font_from_file(font_file_path)
    except SystemExit:
        return
    print("Parsing font data...")
    font_map = parse_font_data(raw_array_content)
    if not font_map:
        print("Ошибка: Не удалось распарсить ни одного символа.")
        return
    print(f"Loaded {len(font_map)} glyphs.\n")
    print_all_glyphs_detailed(font_map)
    print("\n" + "=" * 40)
    print("Rendering complete.")

if __name__ == "__main__":
    main()