#!/usr/bin/env python3
"""Собирает font_data.inc из системных шрифтов. Запуск из любой папки."""

import os
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "font_data.inc")
BOLD = "/System/Library/Fonts/Supplemental/Arial Bold.ttf"
REGULAR = "/System/Library/Fonts/Supplemental/Arial Unicode.ttf"

CODES = list(range(0x20, 0x7F))
CODES += [0xB0, 0x401, 0x451]
CODES += list(range(0x410, 0x450))


def ink(font, ch):
    img = Image.new("L", (96, 96), 0)
    ImageDraw.Draw(img).text((8, 8), ch, font=font, fill=255)
    return sum(img.getdata())


def choose_font(size):
    bold = ImageFont.truetype(BOLD, size)
    regular = ImageFont.truetype(REGULAR, size)
    # Arial Bold на macOS содержит кириллицу. Если глиф пустой — берём Unicode.
    if ink(bold, "Ж") < 20:
        return regular, regular
    return bold, regular


def render_face(px):
    primary, fallback = choose_font(px)
    ascent, descent = primary.getmetrics()
    line = ascent + descent + 1
    glyphs = []
    blob = bytearray()
    for code in CODES:
        ch = chr(code)
        font = primary if ink(primary, ch) >= 8 or code == 0x20 else fallback
        if code == 0x20:
            adv = max(3, int(round(font.getlength(ch))))
            glyphs.append((code, 0, 0, adv, 0))
            continue
        bbox = font.getbbox(ch)
        shift = -bbox[0] if bbox[0] < 0 else 0
        width = max(1, bbox[2] + shift + 1)
        img = Image.new("L", (width, line), 0)
        ImageDraw.Draw(img).text((shift, 0), ch, font=font, fill=255)
        pixels = []
        for y in range(line):
            for x in range(width):
                pixels.append((img.getpixel((x, y)) * 15 + 127) // 255)
        if len(pixels) % 2:
            pixels.append(0)
        offset = len(blob)
        for i in range(0, len(pixels), 2):
            blob.append(((pixels[i] & 15) << 4) | (pixels[i + 1] & 15))
        glyphs.append((code, width, line, width + 1, offset))
    return line, glyphs, blob


def emit_array(name, data, per_line=20):
    lines = []
    for i in range(0, len(data), per_line):
        chunk = ", ".join(f"0x{b:02X}" for b in data[i : i + per_line])
        lines.append(f"  {chunk},")
    body = "\n".join(lines)
    return f"static const uint8_t {name}[] = {{\n{body}\n}};\n"


def emit_glyphs(name, glyphs):
    lines = [f"static const Glyph {name}[] = {{"]
    for code, width, height, advance, offset in glyphs:
        lines.append(
            f"  {{0x{code:04X}, {width}, {height}, {advance}, {offset}}},"
        )
    lines.append("};")
    return "\n".join(lines)


def main():
    parts = [
        "// Сгенерировано tools/make_font.py. Не править руками.\n",
        "// Подключать один раз из Text.cpp после font.h.\n",
    ]
    for px, face in ((16, "font16"), (34, "font34")):
        line, glyphs, blob = render_face(px)
        parts.append(emit_glyphs(f"{face}_glyphs", glyphs))
        parts.append(emit_array(f"{face}_bits", blob))
        parts.append(
            f"const FontFace {face} = {{{line}, {len(glyphs)}, {face}_glyphs, {face}_bits}};\n"
        )
        print(f"{face}: line {line}, glyphs {len(glyphs)}, bytes {len(blob)}")
    text = "\n".join(parts)
    with open(OUT, "w", encoding="utf-8") as fh:
        fh.write(text)
    print(f"wrote {OUT} ({len(text)} chars)")


if __name__ == "__main__":
    main()
