#include "TextRenderer.h"
#include "IRenderDevice.h"
#include <bgfx/bgfx.h>
#include <bx/math.h>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <vector>

#include <ft2build.h>
#include FT_FREETYPE_H
#include "render/FreeTypeContext.h"

namespace Caesura {

// ===========================================================================
// UTF-8 Decode Helper ?? consume multi-byte sequences as single codepoints
// ===========================================================================

static int utf8_char_len(uint8_t lead) {
    if (lead < 0x80) return 1;
    if (lead < 0xC0) return 1;  // continuation byte ?? treat as '?'
    if (lead < 0xE0) return 2;
    if (lead < 0xF0) return 3;
    return 4;
}

static uint32_t utf8_codepoint(const uint8_t* data, int len) {
    if (len == 1) return data[0];
    if (len == 2) return ((data[0] & 0x1F) << 6) | (data[1] & 0x3F);
    if (len == 3) return ((data[0] & 0x0F) << 12) | ((data[1] & 0x3F) << 6) | (data[2] & 0x3F);
    return ((data[0] & 0x07) << 18) | ((data[1] & 0x3F) << 12) | ((data[2] & 0x3F) << 6) | (data[3] & 0x3F);
}


// ===========================================================================
// Embedded 8x16 bitmap font ??ASCII 32 (space) through 126 (~)
// Each glyph: 16 bytes, MSB = leftmost pixel, 1 = lit pixel
// Sourced from public-domain VGA ROM font (Linux kbd console font).
// ===========================================================================

static const uint8_t kFont8x16[95][16] = {
    // 32 ' '
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    // 33 '!'
    {0x00,0x00,0x18,0x3C,0x3C,0x3C,0x18,0x18,0x18,0x00,0x18,0x18,0x00,0x00,0x00,0x00},
    // 34 '"'
    {0x00,0x66,0x66,0x66,0x24,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    // 35 '#'
    {0x00,0x00,0x00,0x6C,0x6C,0xFE,0x6C,0x6C,0x6C,0xFE,0x6C,0x6C,0x00,0x00,0x00,0x00},
    // 36 '$'
    {0x18,0x18,0x7C,0xC6,0xC2,0xC0,0x7C,0x06,0x06,0x86,0xC6,0x7C,0x18,0x18,0x00,0x00},
    // 37 '%'
    {0x00,0x00,0x00,0x00,0xC2,0xC6,0x0C,0x18,0x30,0x60,0xC6,0x86,0x00,0x00,0x00,0x00},
    // 38 '&'
    {0x00,0x00,0x38,0x6C,0x6C,0x38,0x76,0xDC,0xCC,0xCC,0xCC,0x76,0x00,0x00,0x00,0x00},
    // 39 '''
    {0x00,0x30,0x30,0x30,0x60,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    // 40 '('
    {0x00,0x00,0x0C,0x18,0x30,0x30,0x30,0x30,0x30,0x30,0x18,0x0C,0x00,0x00,0x00,0x00},
    // 41 ')'
    {0x00,0x00,0x30,0x18,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x18,0x30,0x00,0x00,0x00,0x00},
    // 42 '*'
    {0x00,0x00,0x00,0x00,0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00,0x00,0x00,0x00,0x00},
    // 43 '+'
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00,0x00,0x00,0x00,0x00},
    // 44 ','
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x18,0x30,0x00,0x00,0x00},
    // 45 '-'
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFE,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    // 46 '.'
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00,0x00},
    // 47 '/'
    {0x00,0x00,0x00,0x00,0x02,0x06,0x0C,0x18,0x30,0x60,0xC0,0x80,0x00,0x00,0x00,0x00},
    // 48 '0'
    {0x00,0x00,0x3C,0x66,0xC3,0xC3,0xDB,0xDB,0xC3,0xC3,0x66,0x3C,0x00,0x00,0x00,0x00},
    // 49 '1'
    {0x00,0x00,0x18,0x38,0x78,0x18,0x18,0x18,0x18,0x18,0x18,0x7E,0x00,0x00,0x00,0x00},
    // 50 '2'
    {0x00,0x00,0x7C,0xC6,0x06,0x0C,0x18,0x30,0x60,0xC0,0xC6,0xFE,0x00,0x00,0x00,0x00},
    // 51 '3'
    {0x00,0x00,0x7C,0xC6,0x06,0x06,0x3C,0x06,0x06,0x06,0xC6,0x7C,0x00,0x00,0x00,0x00},
    // 52 '4'
    {0x00,0x00,0x0C,0x1C,0x3C,0x6C,0xCC,0xFE,0x0C,0x0C,0x0C,0x1E,0x00,0x00,0x00,0x00},
    // 53 '5'
    {0x00,0x00,0xFE,0xC0,0xC0,0xC0,0xFC,0x06,0x06,0x06,0xC6,0x7C,0x00,0x00,0x00,0x00},
    // 54 '6'
    {0x00,0x00,0x38,0x60,0xC0,0xC0,0xFC,0xC6,0xC6,0xC6,0xC6,0x7C,0x00,0x00,0x00,0x00},
    // 55 '7'
    {0x00,0x00,0xFE,0xC6,0x06,0x06,0x0C,0x18,0x30,0x30,0x30,0x30,0x00,0x00,0x00,0x00},
    // 56 '8'
    {0x00,0x00,0x7C,0xC6,0xC6,0xC6,0x7C,0xC6,0xC6,0xC6,0xC6,0x7C,0x00,0x00,0x00,0x00},
    // 57 '9'
    {0x00,0x00,0x7C,0xC6,0xC6,0xC6,0x7E,0x06,0x06,0x06,0x0C,0x78,0x00,0x00,0x00,0x00},
    // 58 ':'
    {0x00,0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00,0x00,0x00},
    // 59 ';'
    {0x00,0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00,0x18,0x18,0x30,0x00,0x00,0x00,0x00},
    // 60 '<'
    {0x00,0x00,0x00,0x06,0x0C,0x18,0x30,0x60,0x30,0x18,0x0C,0x06,0x00,0x00,0x00,0x00},
    // 61 '='
    {0x00,0x00,0x00,0x00,0x00,0x7E,0x00,0x00,0x7E,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    // 62 '>'
    {0x00,0x00,0x00,0x60,0x30,0x18,0x0C,0x06,0x0C,0x18,0x30,0x60,0x00,0x00,0x00,0x00},
    // 63 '?'
    {0x00,0x00,0x7C,0xC6,0xC6,0x0C,0x18,0x18,0x18,0x00,0x18,0x18,0x00,0x00,0x00,0x00},
    // 64 '@'
    {0x00,0x00,0x00,0x7C,0xC6,0xC6,0xDE,0xDE,0xDE,0xDC,0xC0,0x7C,0x00,0x00,0x00,0x00},
    // 65 'A'
    {0x00,0x00,0x10,0x38,0x6C,0xC6,0xC6,0xFE,0xC6,0xC6,0xC6,0xC6,0x00,0x00,0x00,0x00},
    // 66 'B'
    {0x00,0x00,0xFC,0x66,0x66,0x66,0x7C,0x66,0x66,0x66,0x66,0xFC,0x00,0x00,0x00,0x00},
    // 67 'C'
    {0x00,0x00,0x3C,0x66,0xC2,0xC0,0xC0,0xC0,0xC0,0xC2,0x66,0x3C,0x00,0x00,0x00,0x00},
    // 68 'D'
    {0x00,0x00,0xF8,0x6C,0x66,0x66,0x66,0x66,0x66,0x66,0x6C,0xF8,0x00,0x00,0x00,0x00},
    // 69 'E'
    {0x00,0x00,0xFE,0x66,0x62,0x68,0x78,0x68,0x60,0x62,0x66,0xFE,0x00,0x00,0x00,0x00},
    // 70 'F'
    {0x00,0x00,0xFE,0x66,0x62,0x68,0x78,0x68,0x60,0x60,0x60,0xF0,0x00,0x00,0x00,0x00},
    // 71 'G'
    {0x00,0x00,0x3C,0x66,0xC2,0xC0,0xC0,0xDE,0xC6,0xC6,0x66,0x3A,0x00,0x00,0x00,0x00},
    // 72 'H'
    {0x00,0x00,0xC6,0xC6,0xC6,0xC6,0xFE,0xC6,0xC6,0xC6,0xC6,0xC6,0x00,0x00,0x00,0x00},
    // 73 'I'
    {0x00,0x00,0x3C,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x3C,0x00,0x00,0x00,0x00},
    // 74 'J'
    {0x00,0x00,0x1E,0x0C,0x0C,0x0C,0x0C,0x0C,0xCC,0xCC,0xCC,0x78,0x00,0x00,0x00,0x00},
    // 75 'K'
    {0x00,0x00,0xE6,0x66,0x66,0x6C,0x78,0x78,0x6C,0x66,0x66,0xE6,0x00,0x00,0x00,0x00},
    // 76 'L'
    {0x00,0x00,0xF0,0x60,0x60,0x60,0x60,0x60,0x60,0x62,0x66,0xFE,0x00,0x00,0x00,0x00},
    // 77 'M'
    {0x00,0x00,0xC3,0xE7,0xFF,0xFF,0xDB,0xC3,0xC3,0xC3,0xC3,0xC3,0x00,0x00,0x00,0x00},
    // 78 'N'
    {0x00,0x00,0xC6,0xE6,0xF6,0xFE,0xDE,0xCE,0xC6,0xC6,0xC6,0xC6,0x00,0x00,0x00,0x00},
    // 79 'O'
    {0x00,0x00,0x7C,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00,0x00,0x00,0x00},
    // 80 'P'
    {0x00,0x00,0xFC,0x66,0x66,0x66,0x7C,0x60,0x60,0x60,0x60,0xF0,0x00,0x00,0x00,0x00},
    // 81 'Q'
    {0x00,0x00,0x7C,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0xD6,0xDE,0x7C,0x0C,0x0E,0x00,0x00},
    // 82 'R'
    {0x00,0x00,0xFC,0x66,0x66,0x66,0x7C,0x6C,0x66,0x66,0x66,0xE6,0x00,0x00,0x00,0x00},
    // 83 'S'
    {0x00,0x00,0x7C,0xC6,0xC6,0x60,0x38,0x0C,0x06,0xC6,0xC6,0x7C,0x00,0x00,0x00,0x00},
    // 84 'T'
    {0x00,0x00,0xFF,0xDB,0x99,0x18,0x18,0x18,0x18,0x18,0x18,0x3C,0x00,0x00,0x00,0x00},
    // 85 'U'
    {0x00,0x00,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00,0x00,0x00,0x00},
    // 86 'V'
    {0x00,0x00,0xC3,0xC3,0xC3,0xC3,0xC3,0xC3,0xC3,0x66,0x3C,0x18,0x00,0x00,0x00,0x00},
    // 87 'W'
    {0x00,0x00,0xC3,0xC3,0xC3,0xC3,0xC3,0xDB,0xDB,0xFF,0x66,0x66,0x00,0x00,0x00,0x00},
    // 88 'X'
    {0x00,0x00,0xC3,0xC3,0x66,0x3C,0x18,0x18,0x3C,0x66,0xC3,0xC3,0x00,0x00,0x00,0x00},
    // 89 'Y'
    {0x00,0x00,0xC3,0xC3,0xC3,0x66,0x3C,0x18,0x18,0x18,0x18,0x3C,0x00,0x00,0x00,0x00},
    // 90 'Z'
    {0x00,0x00,0xFF,0xC3,0x86,0x0C,0x18,0x30,0x60,0xC1,0xC3,0xFF,0x00,0x00,0x00,0x00},
    // 91 '['
    {0x00,0x00,0x3C,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x3C,0x00,0x00,0x00,0x00},
    // 92 '\'
    {0x00,0x00,0x00,0x80,0xC0,0xE0,0x70,0x38,0x1C,0x0E,0x06,0x02,0x00,0x00,0x00,0x00},
    // 93 ']'
    {0x00,0x00,0x3C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x3C,0x00,0x00,0x00,0x00},
    // 94 '^'
    {0x10,0x38,0x6C,0xC6,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    // 95 '_'
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0x00,0x00},
    // 96 '`'
    {0x30,0x30,0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    // 97 'a'
    {0x00,0x00,0x00,0x00,0x00,0x78,0x0C,0x7C,0xCC,0xCC,0xCC,0x76,0x00,0x00,0x00,0x00},
    // 98 'b'
    {0x00,0x00,0xE0,0x60,0x60,0x78,0x6C,0x66,0x66,0x66,0x66,0x7C,0x00,0x00,0x00,0x00},
    // 99 'c'
    {0x00,0x00,0x00,0x00,0x00,0x7C,0xC6,0xC0,0xC0,0xC0,0xC6,0x7C,0x00,0x00,0x00,0x00},
    // 100 'd'
    {0x00,0x00,0x1C,0x0C,0x0C,0x3C,0x6C,0xCC,0xCC,0xCC,0xCC,0x76,0x00,0x00,0x00,0x00},
    // 101 'e'
    {0x00,0x00,0x00,0x00,0x00,0x7C,0xC6,0xFE,0xC0,0xC0,0xC6,0x7C,0x00,0x00,0x00,0x00},
    // 102 'f'
    {0x00,0x00,0x38,0x6C,0x64,0x60,0xF0,0x60,0x60,0x60,0x60,0xF0,0x00,0x00,0x00,0x00},
    // 103 'g'
    {0x00,0x00,0x00,0x00,0x00,0x76,0xCC,0xCC,0xCC,0xCC,0xCC,0x7C,0x0C,0xCC,0x78,0x00},
    // 104 'h'
    {0x00,0x00,0xE0,0x60,0x60,0x6C,0x76,0x66,0x66,0x66,0x66,0xE6,0x00,0x00,0x00,0x00},
    // 105 'i'
    {0x00,0x00,0x18,0x18,0x00,0x38,0x18,0x18,0x18,0x18,0x18,0x3C,0x00,0x00,0x00,0x00},
    // 106 'j'
    {0x00,0x00,0x06,0x06,0x00,0x0E,0x06,0x06,0x06,0x06,0x06,0x06,0x66,0x66,0x3C,0x00},
    // 107 'k'
    {0x00,0x00,0xE0,0x60,0x60,0x66,0x6C,0x78,0x78,0x6C,0x66,0xE6,0x00,0x00,0x00,0x00},
    // 108 'l'
    {0x00,0x00,0x38,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x3C,0x00,0x00,0x00,0x00},
    // 109 'm'
    {0x00,0x00,0x00,0x00,0x00,0xE6,0xFF,0xDB,0xDB,0xDB,0xDB,0xDB,0x00,0x00,0x00,0x00},
    // 110 'n'
    {0x00,0x00,0x00,0x00,0x00,0xDC,0x66,0x66,0x66,0x66,0x66,0x66,0x00,0x00,0x00,0x00},
    // 111 'o'
    {0x00,0x00,0x00,0x00,0x00,0x7C,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00,0x00,0x00,0x00},
    // 112 'p'
    {0x00,0x00,0x00,0x00,0x00,0xDC,0x66,0x66,0x66,0x66,0x66,0x7C,0x60,0x60,0xF0,0x00},
    // 113 'q'
    {0x00,0x00,0x00,0x00,0x00,0x76,0xCC,0xCC,0xCC,0xCC,0xCC,0x7C,0x0C,0x0C,0x1E,0x00},
    // 114 'r'
    {0x00,0x00,0x00,0x00,0x00,0xDC,0x76,0x66,0x60,0x60,0x60,0xF0,0x00,0x00,0x00,0x00},
    // 115 's'
    {0x00,0x00,0x00,0x00,0x00,0x7C,0xC6,0x60,0x38,0x0C,0xC6,0x7C,0x00,0x00,0x00,0x00},
    // 116 't'
    {0x00,0x00,0x10,0x30,0x30,0xFC,0x30,0x30,0x30,0x30,0x36,0x1C,0x00,0x00,0x00,0x00},
    // 117 'u'
    {0x00,0x00,0x00,0x00,0x00,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0x76,0x00,0x00,0x00,0x00},
    // 118 'v'
    {0x00,0x00,0x00,0x00,0x00,0xC3,0xC3,0xC3,0xC3,0x66,0x3C,0x18,0x00,0x00,0x00,0x00},
    // 119 'w'
    {0x00,0x00,0x00,0x00,0x00,0xC3,0xC3,0xC3,0xDB,0xDB,0xFF,0x66,0x00,0x00,0x00,0x00},
    // 120 'x'
    {0x00,0x00,0x00,0x00,0x00,0xC3,0x66,0x3C,0x18,0x3C,0x66,0xC3,0x00,0x00,0x00,0x00},
    // 121 'y'
    {0x00,0x00,0x00,0x00,0x00,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0x7E,0x06,0x0C,0xF8,0x00},
    // 122 'z'
    {0x00,0x00,0x00,0x00,0x00,0xFE,0xCC,0x18,0x30,0x60,0xC6,0xFE,0x00,0x00,0x00,0x00},
    // 123 '{'
    {0x00,0x00,0x0E,0x18,0x18,0x18,0x70,0x18,0x18,0x18,0x18,0x0E,0x00,0x00,0x00,0x00},
    // 124 '|'
    {0x00,0x00,0x18,0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x18,0x18,0x00,0x00,0x00,0x00},
    // 125 '}'
    {0x00,0x00,0x70,0x18,0x18,0x18,0x0E,0x18,0x18,0x18,0x18,0x70,0x00,0x00,0x00,0x00},
    // 126 '~'
    {0x00,0x00,0x76,0xDC,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
};

// ===========================================================================
// 16x32 double-size font: generated by scaling up 8x16 (nearest-neighbour 2x)
// ===========================================================================

static uint8_t kFont16x32[95][64];

static bool s_font16Built = false;

static void buildFont16x32() {
    if (s_font16Built) return;
    s_font16Built = true;
    for (int g = 0; g < 95; ++g) {
        for (int row = 0; row < 16; ++row) {
            uint8_t src = kFont8x16[g][row];
            uint16_t expanded = 0;
            // expand each bit to 2 bits
            for (int b = 7; b >= 0; --b) {
                expanded <<= 2;
                if (src & (1 << b)) expanded |= 3;
            }
            kFont16x32[g][row*2]     = (uint8_t)(expanded >> 8);
            kFont16x32[g][row*2 + 1] = (uint8_t)(expanded);
        }
    }
}

// ===========================================================================
// Lifecycle
// ===========================================================================

TextRenderer::~TextRenderer() {
    shutdown();
}

bool TextRenderer::init(IRenderDevice* device) {
    if (m_initialized) return true;
    if (!device) {
        fprintf(stderr, "[TextRenderer] Null device pointer.\n");
        return false;
    }

    // Borrow shared resources from IRenderDevice
    m_fallbackProgram = device->getFallbackProgram();
    if (!bgfx::isValid(m_fallbackProgram)) {
        fprintf(stderr, "[TextRenderer] Fallback program not ready. "
                "Ensure device::init() runs first.\n");
        return false;
    }

    // Use the same PosTex layout: Position(2F) + TexCoord0(2F)
    m_posTexLayout
        .begin()
        .add(bgfx::Attrib::Position,  2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();

    m_texSampler = bgfx::createUniform("s_texture", bgfx::UniformType::Sampler);
    if (!bgfx::isValid(m_texSampler)) {
        fprintf(stderr, "[TextRenderer] Uniform creation failed.\n");
        return false;
    }

    m_u_color = bgfx::createUniform("u_color", bgfx::UniformType::Vec4);
    if (!bgfx::isValid(m_u_color)) {
        fprintf(stderr, "[TextRenderer] Color uniform creation failed.\n");
        return false;
    }

    if (!loadFontAtlas(FontId::Small)) {
        fprintf(stderr, "[TextRenderer] Font atlas creation failed.\n");
        return false;
    }

    m_cursor.lineHeight = (float)m_fontGlyphH;
    m_screenWidth  = device->getBackbufferWidth();
    m_screenHeight = device->getBackbufferHeight();
    m_initialized = true;
    printf("[TextRenderer] Initialized. Font: %dx%d, atlas: %d cols.\n",
           m_fontGlyphW, m_fontGlyphH, m_atlasCols);
    return true;
}

void TextRenderer::shutdown() {
    if (bgfx::isValid(m_fontTexture)) {
        bgfx::destroy(m_fontTexture);
        m_fontTexture = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(m_texSampler)) {
        bgfx::destroy(m_texSampler);
        m_texSampler = BGFX_INVALID_HANDLE;
    }
    // FreeType cleanup
    if (m_ttf) {
        if (m_ttf->ftFace) { FT_Done_Face(m_ttf->ftFace); m_ttf->ftFace = nullptr; }
        // ftLib now owned by FreeTypeContext singleton
    }
    m_ttf.reset();

    // Track 2: batch cache and CJK atlas cleanup
    if (bgfx::isValid(m_msgCache.vb)) { bgfx::destroy(m_msgCache.vb); m_msgCache.vb = BGFX_INVALID_HANDLE; }
    if (bgfx::isValid(m_msgCache.ib)) { bgfx::destroy(m_msgCache.ib); m_msgCache.ib = BGFX_INVALID_HANDLE; }
    if (bgfx::isValid(m_u_color))    { bgfx::destroy(m_u_color);   m_u_color   = BGFX_INVALID_HANDLE; }
    if (bgfx::isValid(m_cjkAtlas))   { bgfx::destroy(m_cjkAtlas);  m_cjkAtlas  = BGFX_INVALID_HANDLE; }
    m_cjkGlyphs.clear();

    // m_fallbackProgram and m_posTexLayout are borrowed ??do NOT destroy
    m_initialized = false;
}

// ===========================================================================
// Font atlas
// ===========================================================================

bool TextRenderer::loadFontAtlas(FontId id) {
    if (bgfx::isValid(m_fontTexture)) {
        bgfx::destroy(m_fontTexture);
        m_fontTexture = BGFX_INVALID_HANDLE;
    }

    int glyphW = (id == FontId::Small) ? 8 : 16;
    int glyphH = (id == FontId::Small) ? 16 : 32;

    if (id == FontId::Large) buildFont16x32();

    // Atlas: 32 cols ?? 3 rows = 96 glyphs, RGBA8
    int atlasW = glyphW * 32;                     // 256 or 512
    int atlasH = glyphH * 3;                      // 48 or 96
    int totalPixels = atlasW * atlasH;
    std::vector<uint8_t> pixels(totalPixels * 4, 0); // RGBA

    for (int g = 0; g < 95; ++g) {
        int col = g % 32;
        int row = g / 32;
        for (int py = 0; py < glyphH; ++py) {
            uint8_t byteVal;
            if (id == FontId::Small) {
                int p = py;
                if (p < 0 || p >= 16) { byteVal = 0; }
                else byteVal = kFont8x16[g][p];
                for (int px = 0; px < 8; ++px) {
                    int atlasX = col * glyphW + px;
                    int atlasY = row * glyphH + py;
                    int idx = (atlasY * atlasW + atlasX) * 4;
                    if (py < 16 && (byteVal & (0x80 >> px))) {
                        pixels[idx+0] = 255; pixels[idx+1] = 255;
                        pixels[idx+2] = 255; pixels[idx+3] = 255;
                    }
                }
            } else {
                int p = py;
                if (p < 0 || p >= 32) { byteVal = 0; }
                else byteVal = kFont16x32[g][p];
                for (int px = 0; px < 16; ++px) {
                    int atlasX = col * glyphW + px;
                    int atlasY = row * glyphH + py;
                    int idx = (atlasY * atlasW + atlasX) * 4;
                    if (py < 32 && (byteVal & (0x8000 >> px))) {
                        pixels[idx+0] = 255; pixels[idx+1] = 255;
                        pixels[idx+2] = 255; pixels[idx+3] = 255;
                    }
                }
            }
        }
    }

    const bgfx::Memory* mem =
        bgfx::copy(pixels.data(), (uint32_t)pixels.size());

    m_fontTexture = bgfx::createTexture2D(
        (uint16_t)atlasW, (uint16_t)atlasH, false, 1,
        bgfx::TextureFormat::RGBA8,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP, mem);

    if (!bgfx::isValid(m_fontTexture)) {
        fprintf(stderr, "[TextRenderer] Texture creation failed (%dx%d)\n",
                atlasW, atlasH);
        return false;
    }

    m_fontGlyphW = glyphW;
    m_fontGlyphH = glyphH;
    m_atlasCols  = 32;
    m_cursor.lineHeight = (float)glyphH;
    return true;
}

void TextRenderer::setFont(FontId id) {
    if (id == m_currentFont) return;
    m_currentFont = id;
    loadFontAtlas(id);
}

// ===========================================================================
// Glyph building
// ===========================================================================

TextRenderer::GlyphQuad TextRenderer::buildGlyph(
    char ch, float penX, float penY, float scaleW, float scaleH)
{
    return buildGlyph((uint32_t)(unsigned char)ch, penX, penY, scaleW, scaleH);
}

TextRenderer::GlyphQuad TextRenderer::buildGlyph(
    uint32_t cp, float penX, float penY, float scaleW, float scaleH)
{
    // TTF path
    if (m_ttf && m_ttf->glyphs.count(cp)) {
        const auto& gm = m_ttf->glyphs.at(cp);
        float gw = (float)gm.w * scaleW;
        float gh = (float)gm.h * scaleH;
        float atlasW = (float)m_ttf->atlasW;
        float atlasH = (float)m_ttf->atlasH;
        GlyphQuad q;
        q.x = penX + gm.offsetX * scaleW;
        q.y = penY + gm.offsetY * scaleH;
        q.w = gw; q.h = gh;
        q.u0 = (float)gm.x / atlasW; q.v0 = (float)gm.y / atlasH;
        q.u1 = (float)(gm.x + gm.w) / atlasW; q.v1 = (float)(gm.y + gm.h) / atlasH;
        q.w = (float)gm.advance * scaleW;
        return q;
    }

    // Bitmap fallback
    int idx;
    if (cp >= 32 && cp <= 126)
        idx = (int)cp - 32;
    else if (cp >= 0x4E00 && cp <= 0x9FFF)
        idx = 95;
    else if (cp >= 0x3040 && cp <= 0x30FF)
        idx = 95;
    else
        idx = 31;

    int col = idx % m_atlasCols;
    int row = idx / m_atlasCols;

    float gw = (float)m_fontGlyphW * scaleW;
    float gh = (float)m_fontGlyphH * scaleH;

    float atlasW = (float)(m_fontGlyphW * m_atlasCols);
    float atlasH = (float)(m_fontGlyphH * 3);

    GlyphQuad q;
    q.x  = penX;
    q.y  = penY;
    q.w  = gw;
    q.h  = gh;
    q.u0 = ((float)(col * m_fontGlyphW))     / atlasW;
    q.v0 = ((float)(row * m_fontGlyphH))     / atlasH;
    q.u1 = ((float)((col+1) * m_fontGlyphW)) / atlasW;
    q.v1 = ((float)((row+1) * m_fontGlyphH)) / atlasH;
    return q;
}

// ===========================================================================
// Quad submission
// ===========================================================================

void TextRenderer::submitGlyphQuads(uint16_t viewId, const GlyphQuad* quads,
                                     int count, TextColor color,
                                     float scaleW, float scaleH)
{
    if (count <= 0 || !bgfx::isValid(m_fallbackProgram)) return;

    // Ortho projection
    float ortho[16];
    const bgfx::Caps* caps = bgfx::getCaps();
    bx::mtxOrtho(ortho, 0.0f, 1280.0f, 720.0f, 0.0f,
                 -1.0f, 1.0f, 0.0f, caps ? caps->homogeneousDepth : false,
                 bx::Handedness::Left);
    bgfx::setViewTransform(viewId, nullptr, ortho);

    struct PosTexVertex { float x, y, u, v; };

    int quadCount = count;
    int vertCount = quadCount * 4;
    int idxCount  = quadCount * 6;

    bgfx::TransientVertexBuffer tvb;
    if (bgfx::getAvailTransientVertexBuffer((uint32_t)vertCount,
            m_posTexLayout) < (uint32_t)vertCount) return;
    bgfx::allocTransientVertexBuffer(&tvb, (uint32_t)vertCount, m_posTexLayout);
    auto* vtx = (PosTexVertex*)tvb.data;

    bgfx::TransientIndexBuffer tib;
    if (bgfx::getAvailTransientIndexBuffer((uint32_t)idxCount) < (uint32_t)idxCount) return;
    bgfx::allocTransientIndexBuffer(&tib, (uint32_t)idxCount);
    auto* idx = (uint16_t*)tib.data;

    float sw = (float)m_screenWidth;
    float sh = (float)m_screenHeight;
    for (int i = 0; i < quadCount; ++i) {
        const GlyphQuad& q = quads[i];
        // Pixel coords -> NDC (passthrough shader bypasses u_viewProj)
        float nx0 = (q.x / sw) * 2.0f - 1.0f;
        float ny0 = 1.0f - (q.y / sh) * 2.0f;
        float nx1 = ((q.x + q.w) / sw) * 2.0f - 1.0f;
        float ny1 = 1.0f - ((q.y + q.h) / sh) * 2.0f;
        int vi = i * 4;
        vtx[vi+0] = { nx0, ny0, q.u0, q.v0 };
        vtx[vi+1] = { nx1, ny0, q.u1, q.v0 };
        vtx[vi+2] = { nx1, ny1, q.u1, q.v1 };
        vtx[vi+3] = { nx0, ny1, q.u0, q.v1 };

        int ii = i * 6;
        uint16_t base = (uint16_t)vi;
        idx[ii+0] = base + 0; idx[ii+1] = base + 1; idx[ii+2] = base + 2;
        idx[ii+3] = base + 0; idx[ii+4] = base + 2; idx[ii+5] = base + 3;
    }

    uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A
                   | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA,
                                           BGFX_STATE_BLEND_INV_SRC_ALPHA);

    bgfx::setVertexBuffer(0, &tvb);
    bgfx::setIndexBuffer(&tib);
    bgfx::setTexture(0, m_texSampler, m_fontTexture);
    bgfx::setState(state);
    bgfx::submit(viewId, m_fallbackProgram);
}

// ===========================================================================
// Public API
// ===========================================================================

void TextRenderer::renderText(uint16_t viewId, const std::string& text,
                               float x, float y, TextColor color)
{
    if (text.empty() || !m_initialized) return;

    // Build glyph quads
    std::vector<GlyphQuad> quads;
    quads.reserve(text.size());

    float penX = x;
    float scaleW = 1.0f;
    float scaleH = 1.0f;

    const auto* data = (const uint8_t*)text.data();
    int len = (int)text.size();
    for (int i = 0; i < len; ) {
        int clen = utf8_char_len(data[i]);
        if (i + clen > len) clen = len - i;
        uint32_t cp = utf8_codepoint(&data[i], clen);
        i += clen;
        if (cp == '\n') {
            penX = m_cursor.leftMargin;
            m_cursor.y += m_cursor.lineHeight;
            continue;
        }
        GlyphQuad q = buildGlyph(cp, penX, y, scaleW, scaleH);
        quads.push_back(q);
        penX += q.w;
    }

    submitGlyphQuads(viewId, quads.data(), (int)quads.size(), color, scaleW, scaleH);
    m_cursor.x = penX;
}

void TextRenderer::renderRuby(uint16_t viewId, const std::string& text,
                               const std::string& ruby,
                               float x, float y, TextColor color)
{
    if (text.empty() || !m_initialized) return;

    std::vector<GlyphQuad> quads;
    quads.reserve(text.size() + ruby.size());

    float penX = x;

    // Ruby text (0.5x scale, anchored above)
    float rubyScale = 0.5f;
    float rubyPenX = x + ((float)text.size() * (float)m_fontGlyphW -
                          (float)ruby.size() * (float)m_fontGlyphW * rubyScale) / 2.0f;
    {
        const auto* rdata = (const uint8_t*)ruby.data();
        int rlen = (int)ruby.size();
        for (int i = 0; i < rlen; ) {
            int clen = utf8_char_len(rdata[i]);
            if (i + clen > rlen) clen = rlen - i;
            uint32_t cp = utf8_codepoint(&rdata[i], clen);
            i += clen;
            GlyphQuad q = buildGlyph(cp, rubyPenX, y - m_fontGlyphH * rubyScale - 2.0f,
                                      rubyScale, rubyScale);
            quads.push_back(q);
            rubyPenX += q.w;
        }
    }

    // Base text (1.0x scale)
    {
        const auto* tdata = (const uint8_t*)text.data();
        int tlen = (int)text.size();
        for (int i = 0; i < tlen; ) {
            int clen = utf8_char_len(tdata[i]);
            if (i + clen > tlen) clen = tlen - i;
            uint32_t cp = utf8_codepoint(&tdata[i], clen);
            i += clen;
            GlyphQuad q = buildGlyph(cp, penX, y, 1.0f, 1.0f);
            quads.push_back(q);
            penX += q.w;
        }
    }

    submitGlyphQuads(viewId, quads.data(), (int)quads.size(), color, 1.0f, 1.0f);
    m_cursor.x = penX;
}


// ===========================================================================
// TTF loading via FreeType 2
// ===========================================================================

bool TextRenderer::loadTTF(const char* path, float fontSize) {
    // Initialize FreeType via shared context
    m_ttf = std::make_unique<TTFState>();
    m_ttf->ftLib = FreeTypeContext::instance().getLibrary();
    if (!m_ttf->ftLib) {
        fprintf(stderr, "[TextRenderer] FreeTypeContext not initialized\n");
        m_ttf.reset(); return false;
    }

    FT_Error ftErr = FT_New_Face(m_ttf->ftLib, path, 0, &m_ttf->ftFace);
    if (ftErr) {
        fprintf(stderr, "[TextRenderer] FT_New_Face failed: %s (err=%d)\n", path, (int)ftErr);
        m_ttf.reset(); return false;
    }

    FT_Set_Pixel_Sizes(m_ttf->ftFace, 0, (FT_UInt)fontSize);

    m_ttf->ascent  = m_ttf->ftFace->size->metrics.ascender / 64.0f;
    m_ttf->descent = m_ttf->ftFace->size->metrics.descender / 64.0f;
    m_ttf->lineGap = 0.0f;
    m_ttfFontSize = fontSize;
    m_cursor.lineHeight = m_ttf->ftFace->size->metrics.height / 64.0f;

    // Create runtime atlas
    std::vector<uint8_t> atlas(m_ttf->atlasW * m_ttf->atlasH, 0);

    // Rasterize ASCII 32-126
    for (uint32_t cp = 32; cp <= 126; cp++)
        rasterizeTTFGlyph(cp, atlas);

    // Rasterize CJK Unified (partial: most common 500 chars)
    for (uint32_t cp = 0x4E00; cp < 0x4E00 + 500 && cp <= 0x9FFF; cp++)
        rasterizeTTFGlyph(cp, atlas);

    // Rasterize Hiragana + Katakana
    for (uint32_t cp = 0x3040; cp <= 0x30FF; cp++)
        rasterizeTTFGlyph(cp, atlas);

    // Upload atlas as bgfx texture
    if (bgfx::isValid(m_fontTexture))
        bgfx::destroy(m_fontTexture);
    const bgfx::Memory* mem = bgfx::copy(atlas.data(), m_ttf->atlasW * m_ttf->atlasH);
    m_fontTexture = bgfx::createTexture2D(
        (uint16_t)m_ttf->atlasW, (uint16_t)m_ttf->atlasH,
        false, 1, bgfx::TextureFormat::R8,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP, mem);
    m_atlasCols = m_ttf->atlasW;
    m_fontGlyphW = (int)fontSize;
    m_fontGlyphH = (int)fontSize;
    m_currentFont = FontId::TTF;

    printf("[TextRenderer] TTF loaded: %s (%.0fpx), %zu glyphs rasterized\n",
           path, fontSize, m_ttf->glyphs.size());
    return true;
}

bool TextRenderer::rasterizeTTFGlyph(uint32_t cp, std::vector<uint8_t>& atlas) {
    if (!m_ttf || !m_ttf->ftFace) return false;
    if (m_ttf->glyphs.count(cp)) return true;

    FT_UInt glyphIndex = FT_Get_Char_Index(m_ttf->ftFace, cp);

    FT_Error ftErr = FT_Load_Glyph(m_ttf->ftFace, glyphIndex, FT_LOAD_DEFAULT);
    if (ftErr) return false;

    ftErr = FT_Render_Glyph(m_ttf->ftFace->glyph, FT_RENDER_MODE_NORMAL);
    if (ftErr) return false;

    FT_Bitmap* bitmap = &m_ttf->ftFace->glyph->bitmap;
    int w = (int)bitmap->width;
    int h = (int)bitmap->rows;
    if (w <= 0 || h <= 0) return false;

    int advance = (int)(m_ttf->ftFace->glyph->advance.x >> 6);
    int xoff = m_ttf->ftFace->glyph->bitmap_left;
    int yoff = m_ttf->ftFace->glyph->bitmap_top;

    // Pack into atlas (simple row packing)
    if (m_ttf->penX + w + 1 >= m_ttf->atlasW) {
        m_ttf->penX = 1;
        m_ttf->penY += m_ttf->maxRowH + 1;
        m_ttf->maxRowH = 0;
    }
    if (m_ttf->penY + h + 1 >= m_ttf->atlasH) {
        return false;
    }

    // Copy glyph to atlas (FreeType grayscale -> R8 atlas, direct copy)
    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            int ax = m_ttf->penX + col;
            int ay = m_ttf->penY + row;
            atlas[ay * m_ttf->atlasW + ax] = bitmap->buffer[row * bitmap->pitch + col];
        }
    }

    GlyphMetrics gm;
    gm.x = m_ttf->penX; gm.y = m_ttf->penY;
    gm.w = w; gm.h = h;
    gm.advance = advance;
    gm.offsetX = xoff; gm.offsetY = yoff;
    m_ttf->glyphs[cp] = gm;

    m_ttf->penX += w + 1;
    if (h > m_ttf->maxRowH) m_ttf->maxRowH = h;
    return true;
}
void TextRenderer::newline() {
    m_cursor.x = m_cursor.leftMargin;
    m_cursor.y += m_cursor.lineHeight;
}

void TextRenderer::clearText(uint16_t /*viewId*/) {
    m_cursor.x = m_cursor.leftMargin;
    // No GPU clear ??just reset cursor. Layer system handles visibility.
}

// ===========================================================================
//  Track 2: Batch-cached text rendering + CJK static atlas (merged from FontRenderer)
// ===========================================================================

// ---------------------------------------------------------------------------
// Glyph lookup with fallback: TTF atlas > CJK static atlas > built-in bitmap > U+FFFD
// ---------------------------------------------------------------------------

static GlyphMetrics s_emptyGlyph2{0,0,0,0,8,0,0};

GlyphMetrics TextRenderer::getTTFGlyph(uint32_t codepoint) {
    // 1. TTF atlas
    if (m_ttf) {
        auto it = m_ttf->glyphs.find(codepoint);
        if (it != m_ttf->glyphs.end()) return it->second;
    }

    // 2. CJK static atlas
    if (bgfx::isValid(m_cjkAtlas)) {
        auto cjkIt = m_cjkGlyphs.find(codepoint);
        if (cjkIt != m_cjkGlyphs.end()) {
            GlyphMetrics gm;
            gm.x = cjkIt->second.x; gm.y = cjkIt->second.y;
            gm.w = cjkIt->second.w; gm.h = cjkIt->second.h;
            gm.advance = cjkIt->second.advance;
            gm.offsetX = cjkIt->second.offsetX;
            gm.offsetY = cjkIt->second.offsetY;
            return gm;
        }
    }

    // 3. Built-in bitmap fallback (ASCII 32-126)
    if (codepoint >= 32 && codepoint <= 126) {
        GlyphMetrics gm;
        int idx = (int)(codepoint - 32);
        gm.x = (idx % m_atlasCols) * m_fontGlyphW;
        gm.y = (idx / m_atlasCols) * m_fontGlyphH;
        gm.w = m_fontGlyphW; gm.h = m_fontGlyphH;
        gm.advance = m_fontGlyphW;
        gm.offsetX = 0; gm.offsetY = 0;
        return gm;
    }

    // 4. U+FFFD replacement
    if (codepoint != 0xFFFD) return getTTFGlyph(0xFFFD);
    return s_emptyGlyph2;
}

// ---------------------------------------------------------------------------
// CJK static atlas (pre-generated G8-U5 bitmap)
// ---------------------------------------------------------------------------

bool TextRenderer::loadCjkAtlas(const std::string& atlasPath, const std::string& metaPath) {
    FILE* f = fopen(atlasPath.c_str(), "rb");
    if (!f) {
        fprintf(stderr, "[TextRenderer] CJK atlas not found: %s (skipping)\n", atlasPath.c_str());
        return false;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> data(size);
    fread(data.data(), 1, size, f);
    fclose(f);

    const uint16_t cjkW = 4096, cjkH = 4096;
    m_cjkAtlas = bgfx::createTexture2D(cjkW, cjkH, false, 1,
        bgfx::TextureFormat::RGBA8,
        BGFX_SAMPLER_POINT | BGFX_SAMPLER_UVW_CLAMP,
        bgfx::copy(data.data(), (uint32_t)(cjkW * cjkH * 4)));
    if (!bgfx::isValid(m_cjkAtlas)) {
        fprintf(stderr, "[TextRenderer] CJK atlas texture creation failed\n");
        return false;
    }

    FILE* mf = fopen(metaPath.c_str(), "rb");
    if (!mf) {
        fprintf(stderr, "[TextRenderer] CJK metadata not found: %s\n", metaPath.c_str());
        bgfx::destroy(m_cjkAtlas); m_cjkAtlas = BGFX_INVALID_HANDLE;
        return false;
    }
    uint32_t count = 0;
    fread(&count, sizeof(count), 1, mf);
    m_cjkGlyphs.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t cp; CjkGlyph g;
        fread(&cp, sizeof(cp), 1, mf);
        fread(&g.x, sizeof(g.x), 1, mf);
        fread(&g.y, sizeof(g.y), 1, mf);
        fread(&g.w, sizeof(g.w), 1, mf);
        fread(&g.h, sizeof(g.h), 1, mf);
        fread(&g.advance, sizeof(g.advance), 1, mf);
        fread(&g.offsetX, sizeof(g.offsetX), 1, mf);
        fread(&g.offsetY, sizeof(g.offsetY), 1, mf);
        m_cjkGlyphs[cp] = g;
    }
    fclose(mf);

    printf("[TextRenderer] CJK static atlas loaded: %u glyphs (%dx%d)\n", count, cjkW, cjkH);
    return true;
}

// ---------------------------------------------------------------------------
// Batch cache management
// ---------------------------------------------------------------------------

void TextRenderer::invalidateCache() {
    m_msgCache.cachedText.clear();
    m_msgCache.markAllDirty();
}

void TextRenderer::updateDirtyRange(const std::string& newText) {
    const std::string& oldText = m_msgCache.cachedText;
    size_t oldLen = oldText.size();
    size_t newLen = newText.size();

    uint32_t diffStart = 0;
    const uint8_t* oldData = reinterpret_cast<const uint8_t*>(oldText.data());
    const uint8_t* newData = reinterpret_cast<const uint8_t*>(newText.data());

    size_t oldPos = 0, newPos = 0;
    while (oldPos < oldLen && newPos < newLen) {
        int oclen = utf8_char_len(oldData[oldPos]);
        int nclen = utf8_char_len(newData[newPos]);
        if (oclen != nclen || memcmp(oldData + oldPos, newData + newPos, oclen) != 0)
            break;
        oldPos += oclen; newPos += nclen; diffStart++;
    }

    auto countGlyphs = [](const uint8_t* data, size_t len) -> uint32_t {
        uint32_t count = 0;
        for (size_t pos = 0; pos < len; ) {
            int clen = utf8_char_len(data[pos]);
            if (pos + clen > len) clen = (int)(len - pos);
            pos += clen; count++;
        }
        return count;
    };

    uint32_t oldRemain = countGlyphs(oldData + oldPos, oldLen - oldPos);
    uint32_t newRemain = countGlyphs(newData + newPos, newLen - newPos);

    if (oldRemain == 0 && newRemain == 0) { m_msgCache.clearDirty(); return; }

    m_msgCache.dirtyStart = diffStart;
    m_msgCache.dirtyEnd   = diffStart + (oldRemain > newRemain ? oldRemain : newRemain);
    if (m_msgCache.dirtyEnd > m_msgCache.maxGlyphs)
        m_msgCache.dirtyEnd = m_msgCache.maxGlyphs;
    m_msgCache.cachedText = newText;
}

bool TextRenderer::ensureCacheBuffers() {
    uint32_t maxVerts = m_msgCache.maxGlyphs * 6;
    uint32_t maxInds  = m_msgCache.maxGlyphs * 6;

    if (!bgfx::isValid(m_msgCache.vb)) {
        m_msgCache.vb = bgfx::createDynamicVertexBuffer(
            maxVerts, m_posTexLayout, BGFX_BUFFER_ALLOW_RESIZE);
        if (!bgfx::isValid(m_msgCache.vb)) {
            fprintf(stderr, "[TextRenderer] Failed to create dynamic vertex buffer.\n");
            return false;
        }
    }
    if (!bgfx::isValid(m_msgCache.ib)) {
        m_msgCache.ib = bgfx::createDynamicIndexBuffer(
            maxInds, BGFX_BUFFER_ALLOW_RESIZE | BGFX_BUFFER_INDEX32);
        if (!bgfx::isValid(m_msgCache.ib)) {
            fprintf(stderr, "[TextRenderer] Failed to create dynamic index buffer.\n");
            return false;
        }
    }
    return true;
}

float TextRenderer::rebuildCache(uint16_t viewId, const std::string& text,
                                  float x, float y, TextColor color,
                                  bgfx::ProgramHandle program) {
    if (!ensureCacheBuffers() || text.empty()) return x;

    bgfx::TextureHandle tex = (m_ttf && bgfx::isValid(m_fontTexture))
        ? m_fontTexture : m_fontTexture;
    uint16_t texW = m_ttf ? (uint16_t)m_ttf->atlasW : (uint16_t)(m_atlasCols * m_fontGlyphW);
    uint16_t texH = m_ttf ? (uint16_t)m_ttf->atlasH : (uint16_t)(m_fontGlyphH * 13);

    const float invW = 1.0f / float(texW);
    const float invH = 1.0f / float(texH);
    float penX = x;
    const float penY = y;

    const uint8_t* tdata = reinterpret_cast<const uint8_t*>(text.data());
    int tlen = (int)text.size();

    struct PosTexVertex { float x, y, u, v; };
    
    // CJK atlas UV dimensions (when separate atlas is loaded)
    float cjkInvW = 1.0f / float(m_atlasW);
    float cjkInvH = 1.0f / float(m_atlasH);
    bool hasCjk = bgfx::isValid(m_cjkAtlas);
    
    std::vector<GlyphDraw> draws;
    std::vector<bool> glyphFromCjk;  // per-glyph: true if sourced from CJK atlas
    draws.reserve(m_msgCache.maxGlyphs);
    glyphFromCjk.reserve(m_msgCache.maxGlyphs);

    for (int i = 0; i < tlen; ) {
        int clen = utf8_char_len(tdata[i]);
        if (i + clen > tlen) clen = tlen - i;
        uint32_t cp = utf8_codepoint(&tdata[i], clen);
        i += clen;

        GlyphMetrics gm = getTTFGlyph(cp);
        bool fromCjk = false;
        if (hasCjk && !m_ttf) {
            fromCjk = true;
        } else if (hasCjk && m_ttf) {
            // glyph from CJK if not found in TTF atlas
            fromCjk = (m_ttf->glyphs.find(cp) == m_ttf->glyphs.end());
        }
        
        if (gm.w > 0 && gm.h > 0) {
            GlyphDraw d;
            float iw = fromCjk ? cjkInvW : invW;
            float ih = fromCjk ? cjkInvH : invH;
            d.gx = penX + (fromCjk ? (float)gm.offsetX : gm.offsetX);
            d.gy = penY - (fromCjk ? (float)gm.offsetY : gm.offsetY) 
                   + (m_ttf && !fromCjk ? m_ttf->ascent : 8.0f);
            d.u0 = gm.x * iw;  d.v0 = gm.y * ih;
            d.u1 = (gm.x + gm.w) * iw;  d.v1 = (gm.y + gm.h) * ih;
            draws.push_back(d);
        } else {
            draws.push_back({0,0,0,0,0,0});
        }
        glyphFromCjk.push_back(fromCjk);
        penX += gm.advance;
    }

    m_msgCache.glyphCount = static_cast<uint32_t>(draws.size());
    if (m_msgCache.glyphCount > m_msgCache.maxGlyphs)
        m_msgCache.glyphCount = m_msgCache.maxGlyphs;

    std::vector<PosTexVertex> verts;
    std::vector<uint32_t> indices;
    verts.reserve(m_msgCache.glyphCount * 6);
    indices.reserve(m_msgCache.glyphCount * 6);

    for (uint32_t gi = 0; gi < m_msgCache.glyphCount; ++gi) {
        const GlyphDraw& d = draws[gi];
        uint32_t vbase = gi * 6;
        verts.push_back({ d.gx,       d.gy,       d.u0, d.v0 });
        verts.push_back({ d.gx + 1.0f, d.gy,       d.u1, d.v0 });
        verts.push_back({ d.gx + 1.0f, d.gy + 1.0f, d.u1, d.v1 });
        verts.push_back({ d.gx,       d.gy,       d.u0, d.v0 });
        verts.push_back({ d.gx + 1.0f, d.gy + 1.0f, d.u1, d.v1 });
        verts.push_back({ d.gx,       d.gy + 1.0f, d.u0, d.v1 });

        indices.push_back(vbase);     indices.push_back(vbase + 1); indices.push_back(vbase + 2);
        indices.push_back(vbase);     indices.push_back(vbase + 2); indices.push_back(vbase + 3);
    }

    uint32_t nv = m_msgCache.glyphCount * 6, ni = m_msgCache.glyphCount * 6;
    const bgfx::Memory* vm = bgfx::copy(verts.data(), (uint32_t)(verts.size() * sizeof(PosTexVertex)));
    const bgfx::Memory* im = bgfx::copy(indices.data(), (uint32_t)(indices.size() * sizeof(uint32_t)));
    bgfx::update(m_msgCache.vb, 0, vm);
    bgfx::update(m_msgCache.ib, 0, im);

    float fc[4] = { color.r/255.0f, color.g/255.0f, color.b/255.0f, color.a/255.0f };
    bgfx::setUniform(m_u_color, fc);
    // TD-13: Use CJK atlas texture when CJK-only text is detected
    bool allCjk = hasCjk && !glyphFromCjk.empty();
    for (bool c : glyphFromCjk) { if (!c) { allCjk = false; break; } }
    bgfx::TextureHandle useTex = (allCjk && bgfx::isValid(m_cjkAtlas)) ? m_cjkAtlas : tex;
    bgfx::setTexture(0, m_texSampler, useTex);
    bgfx::setVertexBuffer(0, m_msgCache.vb, 0, nv);
    bgfx::setIndexBuffer(m_msgCache.ib, 0, ni);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_BLEND_ALPHA);
    bgfx::submit(viewId, program);

    m_msgCache.cacheIsCjk = allCjk && bgfx::isValid(m_cjkAtlas);
    m_msgCache.clearDirty();
    return penX;
}

float TextRenderer::renderTextCached(uint16_t viewId, const std::string& text,
                                      float x, float y, TextColor color,
                                      bgfx::ProgramHandle program) {
    if (!m_initialized || text.empty()) return x;

    bgfx::ProgramHandle prog = bgfx::isValid(program) ? program : m_fallbackProgram;
    if (!bgfx::isValid(prog)) return x;

    if (text != m_msgCache.cachedText) updateDirtyRange(text);

    if (m_msgCache.isDirty())
        return rebuildCache(viewId, text, x, y, color, prog);

    if (!ensureCacheBuffers()) return x;

    float fc[4] = { color.r/255.0f, color.g/255.0f, color.b/255.0f, color.a/255.0f };
    bgfx::setUniform(m_u_color, fc);
    bgfx::setTexture(0, m_texSampler,
        (m_ttf && bgfx::isValid(m_fontTexture)) ? m_fontTexture : m_fontTexture);
    bgfx::setVertexBuffer(0, m_msgCache.vb, 0, m_msgCache.glyphCount * 6);
    bgfx::setIndexBuffer(m_msgCache.ib, 0, m_msgCache.glyphCount * 6);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_BLEND_ALPHA);
    bgfx::submit(viewId, prog);

    float penX = x;
    const uint8_t* td = reinterpret_cast<const uint8_t*>(text.data());
    int tl = (int)text.size();
    for (int i = 0; i < tl; ) {
        int clen = utf8_char_len(td[i]);
        if (i + clen > tl) clen = tl - i;
        i += clen;
        penX += getTTFGlyph(utf8_codepoint(&td[i - clen], clen)).advance;
    }
    return penX;
}


} // namespace Caesura
