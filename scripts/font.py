import re
import os
import sys


def read_font_from_file(filepath, array_name):
    if not os.path.exists(filepath):
        print(f"Ошибка: Файл '{filepath}' не найден.")
        sys.exit(1)

    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    pattern = (
        rf'\b(?:static\s+)?(?:const\s+)?unsigned\s+char\s+'
        rf'{re.escape(array_name)}\s*\[.*?\]\s*=\s*\{{(.*?)\}};'
    )
    match = re.search(pattern, content, re.DOTALL)

    if not match:
        print(f"Ошибка: Не удалось найти массив '{array_name}' в файле.")
        sys.exit(1)

    return match.group(1)


def parse_font_data(raw_data, expected_len=None):
    font = {}
    entry_pattern = re.compile(r'\[(\d+)\]\s*=\s*\{([^}]+)\}')
    matches = entry_pattern.findall(raw_data)

    for index_str, bytes_str in matches:
        idx = int(index_str)
        byte_vals = []

        for b in bytes_str.split(','):
            b = b.strip()
            if not b:
                continue

            try:
                if b.startswith('0x') or b.startswith('0X'):
                    byte_vals.append(int(b, 16))
                else:
                    byte_vals.append(int(b))
            except ValueError:
                continue

        if expected_len is None or len(byte_vals) == expected_len:
            font[idx] = byte_vals

    return font


def render_char_detailed(pixel_bytes):
    lines = []

    for byte in pixel_bytes:
        line = ""
        for bit_pos in range(7, -1, -1):
            if byte & (1 << bit_pos):
                line += "██"
            else:
                line += "··"
        lines.append(line)

    return lines


def print_all_glyphs_detailed(font, title="FONT"):
    print("=" * 40)
    print(f"{title} DETAILED VIEW (ALL GLYPHS)")
    print("=" * 40)

    sorted_keys = sorted(font.keys())

    for code in sorted_keys:
        if code == 32:
            char_display = "'SPC'"
        elif 32 <= code <= 126:
            char_display = f"'{chr(code)}'"
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

    # --- font8x8_basic ---
    try:
        raw8 = read_font_from_file(font_file_path, "font8x8_basic")
        font8 = parse_font_data(raw8, expected_len=8)
    except SystemExit:
        return

    if font8:
        print(f"Loaded {len(font8)} glyphs for font8x8_basic.\n")
        print_all_glyphs_detailed(font8, "FONT 8x8")
    else:
        print("Ошибка: Не удалось распарсить font8x8_basic.")

    # --- font8x16_basic ---
    try:
        raw16 = read_font_from_file(font_file_path, "font8x16_basic")
        font16 = parse_font_data(raw16, expected_len=16)
    except SystemExit:
        print("\nfont8x16_basic не найден, пропускаю.")
        return

    if font16:
        print(f"\nLoaded {len(font16)} glyphs for font8x16_basic.\n")
        print_all_glyphs_detailed(font16, "FONT 8x16")
    else:
        print("Ошибка: Не удалось распарсить font8x16_basic.")

    print("\n" + "=" * 40)
    print("Rendering complete.")


if __name__ == "__main__":
    main()