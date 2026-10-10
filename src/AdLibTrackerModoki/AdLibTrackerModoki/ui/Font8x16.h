#pragma once

#include <cstdint>

// 8x16 ドットフォント (adt2data.pas)。各文字 16 バイト、MSB が左端。
extern const uint8_t font8x16[256 * 16];
extern const uint8_t vga_font8x16[256 * 16];

struct Rgb
{
    uint8_t r, g, b;
};

// AT2 SDL 版のパレット (typcons1.inc の rgb_color、各値 SHL 2 済み)
constexpr Rgb rgb_color[16] = {
    { 00 << 2, 00 << 2, 00 << 2 },
    { 05 << 2, 10 << 2, 30 << 2 },
    { 00 << 2, 30 << 2, 30 << 2 },
    { 00 << 2, 30 << 2, 40 << 2 },
    { 00 << 2, 00 << 2, 20 << 2 },
    { 15 << 2, 25 << 2, 35 << 2 },
    { 00 << 2, 10 << 2, 25 << 2 },
    { 20 << 2, 30 << 2, 40 << 2 },
    { 45 << 2, 35 << 2, 50 << 2 },
    { 10 << 2, 20 << 2, 30 << 2 },
    { 20 << 2, 40 << 2, 40 << 2 },
    { 20 << 2, 40 << 2, 50 << 2 },
    { 25 << 2, 35 << 2, 50 << 2 },
    { 30 << 2, 35 << 2, 45 << 2 },
    { 35 << 2, 60 << 2, 60 << 2 },
    { 40 << 2, 50 << 2, 55 << 2 },
};

// 標準 VGA パレット (vga_rgb_color、終了画面で使用)
constexpr Rgb vga_rgb_color[16] = {
    { 0x00, 0x00, 0x00 }, { 0x00, 0x00, 0xaa }, { 0x00, 0xaa, 0x00 }, { 0x00, 0xaa, 0xaa },
    { 0xaa, 0x00, 0x00 }, { 0xaa, 0x00, 0xaa }, { 0xaa, 0x55, 0x00 }, { 0xaa, 0xaa, 0xaa },
    { 0x55, 0x55, 0x55 }, { 0x55, 0x55, 0xff }, { 0x55, 0xff, 0x55 }, { 0x55, 0xff, 0xff },
    { 0xff, 0x55, 0x55 }, { 0xff, 0x55, 0xff }, { 0xff, 0xff, 0x55 }, { 0xff, 0xff, 0xff },
};
