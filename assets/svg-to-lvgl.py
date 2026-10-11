#!/usr/bin/env python3
# svg-to-lvgl.py - render a white Material Design icon SVG into an LVGL 8
# LV_IMG_CF_TRUE_COLOR_ALPHA C image, the format of every icon in material/
# (64 px) and material_46/ (46 px). Needs Inkscape and Pillow on the host.
#
#   assets/svg-to-lvgl.py assets/material_svg/display_img.svg
#
# writes material/display_img.c and material_46/display_img.c; the symbol is
# the file name (LV_IMG_DECLARE(display_img)).
import os
import re
import subprocess
import sys
import tempfile

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
SIZES = {"material": 64, "material_46": 46}

HEADER = """#ifdef __has_include
    #if __has_include("lvgl.h")
        #ifndef LV_LVGL_H_INCLUDE_SIMPLE
            #define LV_LVGL_H_INCLUDE_SIMPLE
        #endif
    #endif
#endif

#if defined(LV_LVGL_H_INCLUDE_SIMPLE)
    #include "lvgl.h"
#else
    #include "lvgl/lvgl.h"
#endif


#ifndef LV_ATTRIBUTE_MEM_ALIGN
#define LV_ATTRIBUTE_MEM_ALIGN
#endif

#ifndef LV_ATTRIBUTE_IMG_{UP}
#define LV_ATTRIBUTE_IMG_{UP}
#endif

const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST LV_ATTRIBUTE_IMG_{UP} uint8_t {name}_map[] = {{
"""

SECTIONS = [
    ("#if LV_COLOR_DEPTH == 1 || LV_COLOR_DEPTH == 8",
     "Alpha 8 bit, Red: 3 bit, Green: 3 bit, Blue: 2 bit",
     lambda r, g, b, a: [(r & 0xE0) | ((g & 0xE0) >> 3) | (b >> 6), a]),
    ("#if LV_COLOR_DEPTH == 16 && LV_COLOR_16_SWAP == 0",
     "Alpha 8 bit, Red: 5 bit, Green: 6 bit, Blue: 5 bit",
     lambda r, g, b, a: [(((g & 0x1C) << 3) | (b >> 3)), ((r & 0xF8) | (g >> 5)), a]),
    ("#if LV_COLOR_DEPTH == 16 && LV_COLOR_16_SWAP != 0",
     "Alpha 8 bit, Red: 5 bit, Green: 6 bit, Blue: 5 bit  BUT the 2  color bytes are swapped",
     lambda r, g, b, a: [((r & 0xF8) | (g >> 5)), (((g & 0x1C) << 3) | (b >> 3)), a]),
    ("#if LV_COLOR_DEPTH == 32", None,
     lambda r, g, b, a: [b, g, r, a]),
]


def render(svg, size):
    with open(svg) as handle:
        text = handle.read()
    # Material Design Icons ship black paths; the GUI icons are white.
    text = re.sub(r"<path ", '<path fill="#ffffff" ', text)
    with tempfile.TemporaryDirectory() as tmp:
        src = os.path.join(tmp, "in.svg")
        png = os.path.join(tmp, "out.png")
        with open(src, "w") as handle:
            handle.write(text)
        subprocess.run(["inkscape", src, "-w", str(size), "-h", str(size),
                        "--export-type=png", "-o", png],
                       check=True, capture_output=True)
        return Image.open(png).convert("RGBA").copy()


def c_source(name, img):
    w, h = img.size
    px = list(img.getdata())
    out = [HEADER.format(UP=name.upper(), name=name)]
    for cond, comment, enc in SECTIONS:
        out.append(cond + "\n")
        if comment:
            out.append("  /*Pixel format: %s*/\n" % comment)
        for y in range(h):
            row = []
            for x in range(w):
                r, g, b, a = px[y * w + x]
                if a == 0:          # fully transparent pixels are stored black
                    r = g = b = 0
                row += enc(r, g, b, a)
            out.append("  " + "".join("0x%02x, " % v for v in row) + "\n")
        out.append("#endif\n")
    out.append("};\n\n")
    out.append("const lv_img_dsc_t %s = {\n" % name)
    out.append("  .header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA,\n")
    out.append("  .header.always_zero = 0,\n")
    out.append("  .header.reserved = 0,\n")
    out.append("  .header.w = %d,\n" % w)
    out.append("  .header.h = %d,\n" % h)
    out.append("  .data_size = %d * LV_IMG_PX_SIZE_ALPHA_BYTE,\n" % (w * h))
    out.append("  .data = %s_map,\n" % name)
    out.append("};\n")
    return "".join(out)


def main(paths):
    for svg in paths:
        name = os.path.splitext(os.path.basename(svg))[0]
        for folder, size in SIZES.items():
            dest = os.path.join(HERE, folder, name + ".c")
            with open(dest, "w") as handle:
                handle.write(c_source(name, render(svg, size)))
            print("wrote", os.path.relpath(dest, HERE))


if __name__ == "__main__":
    main(sys.argv[1:])
