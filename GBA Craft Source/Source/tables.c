/*
 * GBA-Craft — Lookup Tables & Mode 4 Palette Setup
 * Copyright (c) 2026 Khamerron Kizer / ConsciousNode SoftWorks
 * SPDX-License-Identifier: MIT
 */

#include "gba_craft.h"

/* Placed in fast 32-bit IWRAM so the ARM raycaster reads with zero waitstates */
IWRAM_DATA s32 g_sin_lut[ANGLE_FULL];
IWRAM_DATA s32 g_recip_lut[RECIP_LUT_SIZE];
IWRAM_DATA s32 g_delta_lut[DELTA_LUT_SIZE];
IWRAM_DATA s32 g_fisheye_lut[RENDER_W];

/* Quarter-wave sine knots: sin(i * pi / 128) * 65536 for i = 0..64 */
static const s32 s_sin_q64[65] = {
        0,  1608,  3216,  4821,  6424,  8022,  9616, 11204,
    12785, 14359, 15924, 17479, 19024, 20557, 22078, 23586,
    25080, 26558, 28020, 29466, 30893, 32303, 33692, 35062,
    36410, 37736, 39040, 40320, 41576, 42806, 44011, 45190,
    46341, 47464, 48559, 49624, 50660, 51665, 52639, 53581,
    54491, 55368, 56212, 57022, 57798, 58538, 59244, 59914,
    60547, 61145, 61705, 62228, 62714, 63162, 63572, 63944,
    64277, 64571, 64827, 65043, 65220, 65358, 65457, 65516,
    65536
};

/* Base RGB15 components (0..31 each) for the 8 block types */
typedef struct {
    u8 r, g, b;
} Rgb5;

static const Rgb5 s_block_rgb[BLK_COUNT] = {
    {  0,  0,  0 }, /* 0: AIR (unused) */
    {  9, 25,  7 }, /* 1: GRASS (lush bright green) */
    { 17, 11,  6 }, /* 2: DIRT (warm earthy brown) */
    { 16, 16, 17 }, /* 3: STONE (cool slate grey) */
    { 28, 25, 15 }, /* 4: SAND (warm desert gold) */
    { 13,  8,  4 }, /* 5: WOOD (rich bark brown) */
    {  5, 17,  5 }, /* 6: LEAVES (deep forest green) */
    { 23, 18, 10 }, /* 7: PLANKS (crafted timber tan) */
    {  7,  7,  9 }  /* 8: BEDROCK (dark obsidian grey) */
};

/* Fog target RGB (matches horizon sky/ground haze) */
#define FOG_R 18
#define FOG_G 23
#define FOG_B 29

void tables_init(void) {
    /* 1. Build 1024-entry 16.16 sine table via 4x linear interpolation */
    for (int i = 0; i < 256; i++) {
        int k = i >> 2;
        int f = i & 3;
        s32 v0 = s_sin_q64[k];
        s32 v1 = s_sin_q64[k + 1];
        s32 val = v0 + (((v1 - v0) * f) >> 2);

        g_sin_lut[i]       =  val;
        g_sin_lut[512 - i] =  val;
        g_sin_lut[512 + i] = -val;
        if (i > 0) {
            g_sin_lut[1024 - i] = -val;
        }
    }
    g_sin_lut[256] =  FIX_ONE;
    g_sin_lut[512] =  0;
    g_sin_lut[768] = -FIX_ONE;

    /* 2. Build reciprocal distance table:
     * idx = dist_fix >> 10 (64 entries per block, 1024 entries = 16 blocks).
     * pixel_offset = (dy_8_8 * g_recip_lut[idx]) >> 16
     * where PROJ_CONST = 100 -> 100 * 256 * 64 = 1638400. */
    for (int i = 0; i < RECIP_LUT_SIZE; i++) {
        int clamped = (i < 6) ? 6 : i;
        g_recip_lut[i] = 1638400 / clamped;
    }

    /* 3. Build DDA delta-distance table:
     * k = (|dir| + 128) >> 8, for k in 0..256.
     * delta = (1.0 / (k / 256.0)) in 16.16 = 16777216 / k. */
    g_delta_lut[0] = (16 << FIX_SHIFT);
    for (int k = 1; k < DELTA_LUT_SIZE; k++) {
        g_delta_lut[k] = 16777216 / k;
    }

    /* 4. Build fisheye correction cosine table for columns 0..119 */
    for (int col = 0; col < RENDER_W; col++) {
        int ang_ofs = ((col - (RENDER_W / 2)) * 3) / 2;
        g_fisheye_lut[col] = lut_cos(ang_ofs);
    }
}

void palette_init(void) {
    /* Clear BG and OBJ palettes */
    for (int i = 0; i < 256; i++) {
        pal_bg_mem[i]  = 0;
        pal_obj_mem[i] = 0;
    }

    /* Sky, fog, and UI colors in BG palette */
    pal_bg_mem[0]               = RGB15(0, 0, 0);
    pal_bg_mem[PAL_SKY_TOP]     = RGB15(10, 17, 28);
    pal_bg_mem[PAL_SKY_MID]     = RGB15(14, 20, 29);
    pal_bg_mem[PAL_SKY_HORIZON] = RGB15(FOG_R, FOG_G, FOG_B);
    pal_bg_mem[PAL_FOG]         = RGB15(16, 20, 25);
    pal_bg_mem[PAL_UI_BG]       = RGB15(3, 4, 6);
    pal_bg_mem[PAL_UI_TEXT]     = RGB15(31, 31, 31);
    pal_bg_mem[PAL_UI_ACCENT]   = RGB15(31, 27, 8);
    pal_bg_mem[PAL_UI_WARN]     = RGB15(31, 18, 4);
    pal_bg_mem[PAL_UI_OK]       = RGB15(8, 29, 10);
    pal_bg_mem[PAL_UI_FAIL]     = RGB15(30, 7, 7);
    pal_bg_mem[PAL_HIGHLIGHT]   = RGB15(31, 31, 18);
    pal_bg_mem[PAL_CROSSHAIR]   = RGB15(31, 31, 31);

    /* 8 block types x 16 shades (indices 16..143):
     * Shade 0..11: directional + distance darkening (100% down to 45% brightness).
     * Shade 12..15: last 4 blocks of view distance blend into horizon fog color. */
    for (int b = 1; b < BLK_COUNT; b++) {
        int base_r = s_block_rgb[b].r;
        int base_g = s_block_rgb[b].g;
        int base_b = s_block_rgb[b].b;

        for (int s = 0; s < 16; s++) {
            /* Brightness factor in 0..256 (shade 0 = 256, shade 15 = 112) */
            int bright = 256 - (s * 9);
            int r = (base_r * bright) >> 8;
            int g = (base_g * bright) >> 8;
            int bl = (base_b * bright) >> 8;

            /* Fade shades 11..15 smoothly into sky/horizon fog */
            if (s >= 11) {
                int fog_w = (s - 10) * 46; /* 46, 92, 138, 184, 230 out of 256 */
                int inv_w = 256 - fog_w;
                r  = (r  * inv_w + FOG_R * fog_w) >> 8;
                g  = (g  * inv_w + FOG_G * fog_w) >> 8;
                bl = (bl * inv_w + FOG_B * fog_w) >> 8;
            }

            if (r > 31)  r = 31;
            if (g > 31)  g = 31;
            if (bl > 31) bl = 31;

            pal_bg_mem[BLOCK_PAL(b, s)] = RGB15(r, g, bl);
        }
    }

    /* Set up OBJ palettes (4bpp banks):
     * Bank 0: HUD text, crosshair, hotbar frame, break bar, and block icon colors */
    pal_obj_mem[0]  = RGB15(0, 0, 0);       /* Transparent */
    pal_obj_mem[1]  = RGB15(31, 31, 31);    /* White text / crosshair */
    pal_obj_mem[2]  = RGB15(3, 3, 5);       /* Dark shadow / outline */
    pal_obj_mem[3]  = RGB15(31, 28, 6);     /* Selected slot gold highlight */
    pal_obj_mem[4]  = RGB15(12, 13, 16);    /* Unselected slot border */
    pal_obj_mem[5]  = RGB15(8, 28, 10);     /* Break progress green */
    pal_obj_mem[6]  = RGB15(30, 12, 6);     /* Break bar background */
    pal_obj_mem[7]  = RGB15(20, 24, 31);    /* HUD cyan label accent */

    /* Block icon colors in OBJ bank 0 indices 8..15 (for blocks 1..8) */
    for (int b = 1; b < BLK_COUNT; b++) {
        pal_obj_mem[7 + b] = RGB15(s_block_rgb[b].r, s_block_rgb[b].g, s_block_rgb[b].b);
    }
}

const char *block_name(u8 type) {
    switch (type) {
        case BLK_GRASS:   return "GRASS";
        case BLK_DIRT:    return "DIRT";
        case BLK_STONE:   return "STONE";
        case BLK_SAND:    return "SAND";
        case BLK_WOOD:    return "WOOD";
        case BLK_LEAVES:  return "LEAVES";
        case BLK_PLANKS:  return "PLANKS";
        case BLK_BEDROCK: return "BEDROCK";
        default:          return "EMPTY";
    }
}
