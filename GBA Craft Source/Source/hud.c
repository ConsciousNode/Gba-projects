/*
 * GBA-Craft — OBJ Sprite HUD (Tiles 512..1023) & Mode 4 Bitmap Text
 * Copyright (c) 2026 Khamerron Kizer / ConsciousNode SoftWorks
 * SPDX-License-Identifier: MIT
 */

#include "gba_craft.h"

u8   g_show_fps = 1;
u16  g_last_fps = 20;
u16  g_last_scanlines = 80;
char g_status_msg[44];
u16  g_status_timer = 0;

/* Shadow OAM buffer (128 OBJs) copied to hardware OAM each frame */
static OBJ_ATTR s_oam_shadow[128];

/* OBJ Tile allocation in Mode 4 (must be >= 512 because 0..511 overlap bitmap VRAM):
 * 512      : Crosshair (8x8)
 * 513..521 : Block icons for types 0..8 (8x8)
 * 522      : Hotbar slot frame unselected (8x8)
 * 523      : Hotbar slot frame selected (8x8)
 * 524..532 : Break progress bar levels 0..8 (8x8)
 * 544..607 : 64-glyph 8x8 ASCII font (codes 32..95: ' ' .. '_') */
#define TILE_CROSSHAIR   512
#define TILE_BLOCK_ICON  513
#define TILE_SLOT_NORM   522
#define TILE_SLOT_SEL    523
#define TILE_BREAK_BASE  524
#define TILE_FONT_BASE   544

/* Compact 8x8 1bpp bitmap font for ASCII 32 (' ') through 95 ('_') */
static const u8 s_font_8x8[64][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* 32 ' ' */
    {0x18,0x18,0x18,0x18,0x18,0x00,0x18,0x00}, /* 33 '!' */
    {0x6C,0x6C,0x24,0x00,0x00,0x00,0x00,0x00}, /* 34 '"' */
    {0x24,0x7E,0x24,0x24,0x7E,0x24,0x00,0x00}, /* 35 '#' */
    {0x18,0x3E,0x60,0x3C,0x06,0x7C,0x18,0x00}, /* 36 '$' */
    {0x62,0x64,0x08,0x10,0x26,0x46,0x00,0x00}, /* 37 '%' */
    {0x38,0x44,0x38,0x4E,0x44,0x3A,0x00,0x00}, /* 38 '&' */
    {0x18,0x18,0x08,0x00,0x00,0x00,0x00,0x00}, /* 39 ''' */
    {0x0C,0x18,0x30,0x30,0x30,0x18,0x0C,0x00}, /* 40 '(' */
    {0x30,0x18,0x0C,0x0C,0x0C,0x18,0x30,0x00}, /* 41 ')' */
    {0x00,0x24,0x18,0x7E,0x18,0x24,0x00,0x00}, /* 42 '*' */
    {0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00}, /* 43 '+' */
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x30}, /* 44 ',' */
    {0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00}, /* 45 '-' */
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00}, /* 46 '.' */
    {0x02,0x04,0x08,0x10,0x20,0x40,0x00,0x00}, /* 47 '/' */
    {0x3C,0x66,0x6E,0x76,0x66,0x66,0x3C,0x00}, /* 48 '0' */
    {0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00}, /* 49 '1' */
    {0x3C,0x66,0x06,0x1C,0x30,0x66,0x7E,0x00}, /* 50 '2' */
    {0x3C,0x66,0x06,0x1C,0x06,0x66,0x3C,0x00}, /* 51 '3' */
    {0x0C,0x1C,0x3C,0x6C,0x7E,0x0C,0x0C,0x00}, /* 52 '4' */
    {0x7E,0x60,0x7C,0x06,0x06,0x66,0x3C,0x00}, /* 53 '5' */
    {0x1C,0x30,0x60,0x7C,0x66,0x66,0x3C,0x00}, /* 54 '6' */
    {0x7E,0x66,0x06,0x0C,0x18,0x18,0x18,0x00}, /* 55 '7' */
    {0x3C,0x66,0x66,0x3C,0x66,0x66,0x3C,0x00}, /* 56 '8' */
    {0x3C,0x66,0x66,0x3E,0x06,0x0C,0x38,0x00}, /* 57 '9' */
    {0x00,0x18,0x18,0x00,0x18,0x18,0x00,0x00}, /* 58 ':' */
    {0x00,0x18,0x18,0x00,0x18,0x18,0x30,0x00}, /* 59 ';' */
    {0x06,0x0C,0x18,0x30,0x18,0x0C,0x06,0x00}, /* 60 '<' */
    {0x00,0x00,0x7E,0x00,0x7E,0x00,0x00,0x00}, /* 61 '=' */
    {0x60,0x30,0x18,0x0C,0x18,0x30,0x60,0x00}, /* 62 '>' */
    {0x3C,0x66,0x06,0x1C,0x18,0x00,0x18,0x00}, /* 63 '?' */
    {0x3C,0x66,0x6E,0x6E,0x60,0x62,0x3C,0x00}, /* 64 '@' */
    {0x18,0x3C,0x66,0x66,0x7E,0x66,0x66,0x00}, /* 65 'A' */
    {0x7C,0x66,0x66,0x7C,0x66,0x66,0x7C,0x00}, /* 66 'B' */
    {0x3C,0x66,0x60,0x60,0x60,0x66,0x3C,0x00}, /* 67 'C' */
    {0x78,0x6C,0x66,0x66,0x66,0x6C,0x78,0x00}, /* 68 'D' */
    {0x7E,0x60,0x60,0x7C,0x60,0x60,0x7E,0x00}, /* 69 'E' */
    {0x7E,0x60,0x60,0x7C,0x60,0x60,0x60,0x00}, /* 70 'F' */
    {0x3C,0x66,0x60,0x6E,0x66,0x66,0x3C,0x00}, /* 71 'G' */
    {0x66,0x66,0x66,0x7E,0x66,0x66,0x66,0x00}, /* 72 'H' */
    {0x3C,0x18,0x18,0x18,0x18,0x18,0x3C,0x00}, /* 73 'I' */
    {0x1E,0x0C,0x0C,0x0C,0x6C,0x6C,0x38,0x00}, /* 74 'J' */
    {0x66,0x6C,0x78,0x70,0x78,0x6C,0x66,0x00}, /* 75 'K' */
    {0x60,0x60,0x60,0x60,0x60,0x60,0x7E,0x00}, /* 76 'L' */
    {0x63,0x77,0x7F,0x6B,0x63,0x63,0x63,0x00}, /* 77 'M' */
    {0x66,0x76,0x7E,0x7E,0x6E,0x66,0x66,0x00}, /* 78 'N' */
    {0x3C,0x66,0x66,0x66,0x66,0x66,0x3C,0x00}, /* 79 'O' */
    {0x7C,0x66,0x66,0x7C,0x60,0x60,0x60,0x00}, /* 80 'P' */
    {0x3C,0x66,0x66,0x66,0x6A,0x6C,0x36,0x00}, /* 81 'Q' */
    {0x7C,0x66,0x66,0x7C,0x6C,0x66,0x66,0x00}, /* 82 'R' */
    {0x3C,0x66,0x60,0x3C,0x06,0x66,0x3C,0x00}, /* 83 'S' */
    {0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x00}, /* 84 'T' */
    {0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0x00}, /* 85 'U' */
    {0x66,0x66,0x66,0x66,0x66,0x3C,0x18,0x00}, /* 86 'V' */
    {0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00}, /* 87 'W' */
    {0x66,0x66,0x3C,0x18,0x3C,0x66,0x66,0x00}, /* 88 'X' */
    {0x66,0x66,0x66,0x3C,0x18,0x18,0x18,0x00}, /* 89 'Y' */
    {0x7E,0x06,0x0C,0x18,0x30,0x60,0x7E,0x00}, /* 90 'Z' */
    {0x3C,0x30,0x30,0x30,0x30,0x30,0x3C,0x00}, /* 91 '[' */
    {0x40,0x20,0x10,0x08,0x04,0x02,0x00,0x00}, /* 92 '\' */
    {0x3C,0x0C,0x0C,0x0C,0x0C,0x0C,0x3C,0x00}, /* 93 ']' */
    {0x18,0x3C,0x66,0x00,0x00,0x00,0x00,0x00}, /* 94 '^' */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x7E,0x00}  /* 95 '_' */
};

/* Helper: return pointer to 8x8 4bpp OBJ tile `tile_id` (32 bytes per tile in VRAM) */
static inline u32 *obj_tile_ptr(int tile_id) {
    return (u32 *)(MEM_VRAM + 0x10000 + (tile_id * 32));
}

/* Convert an 8x8 1bpp glyph into an 8x8 4bpp OBJ tile with 1px drop shadow */
static void upload_1bpp_to_4bpp_tile(int tile_id, const u8 rows[8], u8 fg_nib, u8 shadow_nib) {
    u32 *dst = obj_tile_ptr(tile_id);
    for (int y = 0; y < 8; y++) {
        u8 row = rows[y];
        u8 prev_row = (y > 0) ? rows[y - 1] : 0;
        u32 word = 0;
        for (int x = 0; x < 8; x++) {
            int bit = (row >> (7 - x)) & 1;
            u8 nib = 0;
            if (bit) {
                nib = fg_nib;
            } else if (x > 0 && ((row >> (8 - x)) & 1)) {
                nib = shadow_nib;
            } else if ((prev_row >> (7 - x)) & 1) {
                nib = shadow_nib;
            }
            word |= ((u32)(nib & 0xF)) << (x * 4);
        }
        dst[y] = word;
    }
}

void hud_init(void) {
    g_status_msg[0] = '\0';
    g_status_timer = 0;

    /* 1. Upload 64 ASCII glyphs into OBJ tiles 544..607 */
    for (int i = 0; i < 64; i++) {
        upload_1bpp_to_4bpp_tile(TILE_FONT_BASE + i, s_font_8x8[i], 1, 2);
    }

    /* 2. Upload Crosshair tile (512) */
    static const u8 s_crosshair_bits[8] = {
        0x18, 0x18, 0x18, 0xE7, 0xE7, 0x18, 0x18, 0x18
    };
    upload_1bpp_to_4bpp_tile(TILE_CROSSHAIR, s_crosshair_bits, 1, 2);

    /* 3. Upload 9 Block Icon tiles (513..521) */
    for (int b = 0; b < BLK_COUNT; b++) {
        u32 *dst = obj_tile_ptr(TILE_BLOCK_ICON + b);
        u8 fill_nib = (b == BLK_AIR) ? 0 : (u8)(7 + b);
        u8 border_nib = (b == BLK_AIR) ? 4 : 2;
        for (int y = 0; y < 8; y++) {
            u32 word = 0;
            for (int x = 0; x < 8; x++) {
                u8 nib = 0;
                if (b == BLK_AIR) {
                    if (x == 0 || x == 7 || y == 0 || y == 7) nib = 4;
                } else {
                    if (x == 0 || x == 7 || y == 0 || y == 7) {
                        nib = border_nib;
                    } else if (b == BLK_GRASS && y <= 2) {
                        nib = 7 + BLK_GRASS;
                    } else if (b == BLK_GRASS && y > 2) {
                        nib = 7 + BLK_DIRT;
                    } else {
                        nib = fill_nib;
                    }
                }
                word |= ((u32)(nib & 0xF)) << (x * 4);
            }
            dst[y] = word;
        }
    }

    /* 4. Upload Hotbar Slot Frame tiles (522 = normal, 523 = selected gold highlight) */
    for (int sel = 0; sel < 2; sel++) {
        u32 *dst = obj_tile_ptr(TILE_SLOT_NORM + sel);
        u8 ring_nib = sel ? 3 : 4;
        for (int y = 0; y < 8; y++) {
            u32 word = 0;
            for (int x = 0; x < 8; x++) {
                u8 nib = 2;
                if (x == 0 || x == 7 || y == 0 || y == 7) {
                    nib = ring_nib;
                }
                word |= ((u32)(nib & 0xF)) << (x * 4);
            }
            dst[y] = word;
        }
    }

    /* 5. Upload Break Progress Bar tiles (524..532, 0..8 filled columns) */
    for (int lvl = 0; lvl <= 8; lvl++) {
        u32 *dst = obj_tile_ptr(TILE_BREAK_BASE + lvl);
        for (int y = 0; y < 8; y++) {
            u32 word = 0;
            for (int x = 0; x < 8; x++) {
                u8 nib = 0;
                if (y >= 2 && y <= 5) {
                    if (y == 2 || y == 5 || x == 0 || x == 7) {
                        nib = 2;
                    } else if (x <= lvl) {
                        nib = 5; /* Green fill */
                    } else {
                        nib = 6; /* Dark red/brown empty */
                    }
                }
                word |= ((u32)(nib & 0xF)) << (x * 4);
            }
            dst[y] = word;
        }
    }

    oam_init(s_oam_shadow, 128);
    oam_copy(oam_mem, s_oam_shadow, 128);
}

void hud_hide_all(void) {
    oam_init(s_oam_shadow, 128);
    oam_copy(oam_mem, s_oam_shadow, 128);
}

void hud_set_status(const char *msg, u16 duration_frames) {
    int i = 0;
    if (msg) {
        while (msg[i] != '\0' && i < 42) {
            char c = msg[i];
            if (c >= 'a' && c <= 'z') c = (char)(c - 32);
            g_status_msg[i] = c;
            i++;
        }
    }
    g_status_msg[i] = '\0';
    g_status_timer = duration_frames;
}

static int add_obj_sprite(int idx, int x, int y, int tile_id) {
    if (idx >= 128) return idx;
    obj_set_attr(
        &s_oam_shadow[idx],
        ATTR0_SQUARE | ATTR0_4BPP | ATTR0_Y(y & 0xFF),
        ATTR1_SIZE_8 | ATTR1_X(x & 0x1FF),
        ATTR2_ID(tile_id) | ATTR2_PRIO(0)
    );
    return idx + 1;
}

static int add_obj_text(int idx, int x, int y, const char *str) {
    for (int i = 0; str[i] != '\0' && idx < 128; i++) {
        char c = str[i];
        if (c >= 'a' && c <= 'z') c = (char)(c - 32);
        if (c > 32 && c <= 95) {
            idx = add_obj_sprite(idx, x, y, TILE_FONT_BASE + (c - 32));
        }
        x += 6;
    }
    return idx;
}

static int add_obj_int(int idx, int x, int y, int val, int min_digits) {
    char buf[12];
    int neg = 0;
    if (val < 0) {
        neg = 1;
        val = -val;
    }
    int pos = 0;
    do {
        buf[pos++] = (char)('0' + (val % 10));
        val /= 10;
    } while (val > 0 && pos < 10);
    while (pos < min_digits) {
        buf[pos++] = '0';
    }
    if (neg) {
        buf[pos++] = '-';
    }

    char out[12];
    for (int i = 0; i < pos; i++) {
        out[i] = buf[pos - 1 - i];
    }
    out[pos] = '\0';
    return add_obj_text(idx, x, y, out);
}

void hud_update(const Player *p, const EditState *es) {
    oam_init(s_oam_shadow, 128);
    int idx = 0;

    /* 1. Center Crosshair at (116, 76) */
    idx = add_obj_sprite(idx, (SCREEN_W / 2) - 4, (SCREEN_H / 2) - 4, TILE_CROSSHAIR);

    /* 2. Break-progress bar right below crosshair when breaking */
    if (es && es->active && es->required_ticks > 0) {
        int lvl = (es->progress_ticks * 16) / es->required_ticks;
        if (lvl > 16) lvl = 16;
        int left_lvl  = (lvl > 8) ? 8 : lvl;
        int right_lvl = (lvl > 8) ? (lvl - 8) : 0;
        idx = add_obj_sprite(idx, (SCREEN_W / 2) - 8, (SCREEN_H / 2) + 6, TILE_BREAK_BASE + left_lvl);
        idx = add_obj_sprite(idx, (SCREEN_W / 2),     (SCREEN_H / 2) + 6, TILE_BREAK_BASE + right_lvl);
    }

    /* 3. 9-slot Hotbar along the bottom (x = 30..209, y = 140) */
    const int hb_start_x = 30;
    const int hb_step_x  = 20;
    const int hb_y       = 142;

    for (int s = 0; s < HOTBAR_SLOTS; s++) {
        int sx = hb_start_x + s * hb_step_x;
        u8 blk = p->hotbar_type[s];
        u8 cnt = p->hotbar_count[s];

        /* Selection indicator above active slot */
        if (s == p->selected_slot) {
            idx = add_obj_sprite(idx, sx, hb_y - 7, TILE_SLOT_SEL);
        }

        /* Block icon */
        u8 icon_blk = (cnt > 0 && blk < BLK_COUNT) ? blk : BLK_AIR;
        idx = add_obj_sprite(idx, sx, hb_y, TILE_BLOCK_ICON + icon_blk);

        /* Stack count (1..64) below/beside icon */
        if (cnt > 0 && icon_blk != BLK_AIR) {
            idx = add_obj_int(idx, sx, hb_y + 9, cnt, 1);
        }
    }

    /* 4. Optional FPS counter and player block coordinates (top-left) */
    if (g_show_fps) {
        idx = add_obj_text(idx, 4, 4, "FPS:");
        idx = add_obj_int(idx, 28, 4, g_last_fps, 2);

        int bx = FIX_TO_INT(p->x);
        int by = FIX_TO_INT(p->y);
        int bz = FIX_TO_INT(p->z);

        idx = add_obj_text(idx, 52, 4, "X:");
        idx = add_obj_int(idx, 64, 4, bx, 2);
        idx = add_obj_text(idx, 82, 4, "Y:");
        idx = add_obj_int(idx, 94, 4, by, 2);
        idx = add_obj_text(idx, 112, 4, "Z:");
        idx = add_obj_int(idx, 124, 4, bz, 2);
    }

    /* 5. Selected block name or active status banner */
    if (g_status_timer > 0 && g_status_msg[0] != '\0') {
        g_status_timer--;
        idx = add_obj_text(idx, 4, 16, g_status_msg);
    } else {
        u8 sel_blk = p->hotbar_type[p->selected_slot];
        if (p->hotbar_count[p->selected_slot] > 0 && sel_blk != BLK_AIR) {
            idx = add_obj_text(idx, 4, 130, block_name(sel_blk));
        }
    }

    oam_copy(oam_mem, s_oam_shadow, 128);
}

/* Mode 4 16-bit safe pixel pair helpers for menu / self-test screens (240x160 resolution) */
static inline void plot_mode4_pixel(u16 *page, int x, int y, u8 pal_idx) {
    if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) return;
    int halfword_idx = (y * (SCREEN_W / 2)) + (x >> 1);
    u16 cur = page[halfword_idx];
    if (x & 1) {
        page[halfword_idx] = (u16)((cur & 0x00FFu) | ((u16)pal_idx << 8));
    } else {
        page[halfword_idx] = (u16)((cur & 0xFF00u) | (u16)pal_idx);
    }
}

void hud_fill_bitmap_rect(u16 *page, int x, int y, int w, int h, u8 pal_idx) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > SCREEN_W) w = SCREEN_W - x;
    if (y + h > SCREEN_H) h = SCREEN_H - y;
    if (w <= 0 || h <= 0) return;

    u16 pair = (u16)((u16)pal_idx | ((u16)pal_idx << 8));
    for (int py = y; py < y + h; py++) {
        int px = x;
        if (px & 1) {
            plot_mode4_pixel(page, px, py, pal_idx);
            px++;
        }
        u16 *row = page + py * (SCREEN_W / 2);
        while (px + 1 < x + w) {
            row[px >> 1] = pair;
            px += 2;
        }
        if (px < x + w) {
            plot_mode4_pixel(page, px, py, pal_idx);
        }
    }
}

void hud_draw_bitmap_char(u16 *page, int x, int y, char c, u8 fg_pal, u8 bg_pal) {
    if (c >= 'a' && c <= 'z') c = (char)(c - 32);
    if (c < 32 || c > 95) c = ' ';
    const u8 *glyph = s_font_8x8[c - 32];

    for (int gy = 0; gy < 8; gy++) {
        u8 row = glyph[gy];
        for (int gx = 0; gx < 6; gx++) {
            int bit = (row >> (7 - gx)) & 1;
            if (bit) {
                plot_mode4_pixel(page, x + gx, y + gy, fg_pal);
            } else if (bg_pal != 0) {
                plot_mode4_pixel(page, x + gx, y + gy, bg_pal);
            }
        }
    }
}

/* Draws text at 6px horizontal stride (40 chars per 240px row), wrapping automatically if needed */
void hud_draw_bitmap_text(u16 *page, int x, int y, const char *str, u8 fg_pal, u8 bg_pal) {
    int cx = x;
    int cy = y;
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] == '\n') {
            cx = x;
            cy += 9;
            continue;
        }
        if (cx + 6 > SCREEN_W) {
            cx = x;
            cy += 9;
        }
        hud_draw_bitmap_char(page, cx, cy, str[i], fg_pal, bg_pal);
        cx += 6;
    }
}
