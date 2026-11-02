/*
 * GBA-Craft — Title Screen, Pause Menu, Seed Editor & Help Screen
 * Copyright (c) 2026 Khamerron Kizer / ConsciousNode SoftWorks
 * SPDX-License-Identifier: MIT
 */

#include "gba_craft.h"

static int  s_title_cursor = 0;
static int  s_pause_cursor = 0;
static int  s_seed_digit   = 0;
static u32  s_edit_seed    = DEFAULT_SEED;
static char s_pause_msg[64];

static void format_int(char *dst, int val) {
    char tmp[12];
    int pos = 0;
    if (val < 0) {
        *dst++ = '-';
        val = -val;
    }
    do {
        tmp[pos++] = (char)('0' + (val % 10));
        val /= 10;
    } while (val > 0);
    for (int i = 0; i < pos; i++) {
        dst[i] = tmp[pos - 1 - i];
    }
    dst[pos] = '\0';
}

static void format_hex8(char out[9], u32 val) {
    static const char hex[] = "0123456789ABCDEF";
    for (int i = 0; i < 8; i++) {
        int shift = (7 - i) * 4;
        out[i] = hex[(val >> shift) & 0xFu];
    }
    out[8] = '\0';
}

static void str_copy(char *dst, const char *src) {
    while (*src) {
        *dst++ = *src++;
    }
    *dst = '\0';
}

static void str_append(char *dst, const char *src) {
    while (*dst) dst++;
    str_copy(dst, src);
}

void menu_init(void) {
    s_title_cursor = 0;
    s_pause_cursor = 0;
    s_seed_digit   = 0;
    s_edit_seed    = DEFAULT_SEED;
    s_pause_msg[0] = '\0';
}

/* Title Screen: "GBA-CRAFT", "ConsciousNode SoftWorks", New World / Continue (if valid save)
 * Holding SELECT while title screen appears triggers Self-Test mode. */
int menu_update_title(u32 *io_seed, Player *p) {
    /* Self-test trigger: hold or press SELECT while on title screen */
    if (key_is_down(KEY_SELECT) || key_hit(KEY_SELECT)) {
        return STATE_SELFTEST;
    }

    int has_save = save_has_valid_sram(NULL);
    int item_count = has_save ? 2 : 1;
    if (s_title_cursor >= item_count) s_title_cursor = 0;

    if (key_hit(KEY_UP) || key_hit(KEY_DOWN)) {
        if (item_count > 1) {
            s_title_cursor ^= 1;
        }
    }

    if (key_hit(KEY_A) || key_hit(KEY_START)) {
        if (has_save && s_title_cursor == 1) {
            if (load_world(io_seed, p)) {
                edit_init(&g_edit);
                hud_set_status("LOADED SAVED WORLD", 60);
                return STATE_PLAYING;
            }
        } else {
            /* New World -> open 8-hex-digit seed editor with default derived from frame counter */
            s_edit_seed = DEFAULT_SEED ^ (g_frame_counter * 0x01000193u);
            if (s_edit_seed == 0) s_edit_seed = DEFAULT_SEED;
            s_seed_digit = 0;
            return STATE_SEED_ENTRY;
        }
    }

    /* Render Title Screen into Mode 4 backbuffer (full 240x160 resolution) */
    u16 *page = vid_page;
    hud_fill_bitmap_rect(page, 0, 0, SCREEN_W, SCREEN_H, PAL_UI_BG);
    hud_fill_bitmap_rect(page, 12, 14, SCREEN_W - 24, 38, BLOCK_PAL(BLK_STONE, 6));
    hud_fill_bitmap_rect(page, 14, 16, SCREEN_W - 28, 34, BLOCK_PAL(BLK_GRASS, 3));

    hud_draw_bitmap_text(page, 84, 22, "GBA-CRAFT", PAL_UI_TEXT, 0);
    hud_draw_bitmap_text(page, 48, 36, "CONSCIOUSNODE SOFTWORKS", PAL_UI_ACCENT, 0);

    hud_draw_bitmap_text(
        page, 66, 76,
        (s_title_cursor == 0) ? "> NEW WORLD <" : "  NEW WORLD  ",
        (s_title_cursor == 0) ? PAL_UI_ACCENT : PAL_UI_TEXT, 0
    );

    if (has_save) {
        hud_draw_bitmap_text(
            page, 66, 92,
            (s_title_cursor == 1) ? "> CONTINUE  <" : "  CONTINUE   ",
            (s_title_cursor == 1) ? PAL_UI_ACCENT : PAL_UI_OK, 0
        );
    } else {
        hud_draw_bitmap_text(page, 60, 92, "  (NO SAVED WORLD)", BLOCK_PAL(BLK_STONE, 8), 0);
    }

    hud_draw_bitmap_text(page, 36, 128, "A/START: SELECT OPTION", PAL_UI_TEXT, 0);
    hud_draw_bitmap_text(page, 30, 140, "HOLD SELECT: SELF-TEST", PAL_UI_WARN, 0);

    return STATE_TITLE;
}

/* Seed Entry Screen: 8 hex digits, D-pad to edit, default derived from frame counter */
int menu_update_seed_entry(u32 *io_seed, Player *p, int return_state) {
    if (key_hit(KEY_LEFT)) {
        s_seed_digit = (s_seed_digit + 7) & 7;
    }
    if (key_hit(KEY_RIGHT)) {
        s_seed_digit = (s_seed_digit + 1) & 7;
    }
    if (key_hit(KEY_UP) || key_hit(KEY_DOWN)) {
        int shift = (7 - s_seed_digit) * 4;
        u32 nib = (s_edit_seed >> shift) & 0xFu;
        if (key_hit(KEY_UP)) {
            nib = (nib + 1) & 0xFu;
        } else {
            nib = (nib + 15) & 0xFu;
        }
        s_edit_seed = (s_edit_seed & ~(0xFu << shift)) | (nib << shift);
    }

    if (key_hit(KEY_B)) {
        return return_state;
    }

    if (key_hit(KEY_A) || key_hit(KEY_START)) {
        *io_seed = s_edit_seed;
        world_generate(*io_seed);
        player_init(p, *io_seed);
        edit_init(&g_edit);
        hud_set_status("GENERATED NEW WORLD", 60);
        return STATE_PLAYING;
    }

    u16 *page = vid_page;
    hud_fill_bitmap_rect(page, 0, 0, SCREEN_W, SCREEN_H, PAL_UI_BG);
    hud_draw_bitmap_text(page, 54, 24, "NEW WORLD SEED (HEX)", PAL_UI_ACCENT, 0);
    hud_draw_bitmap_text(page, 30, 44, "D-PAD L/R: SELECT DIGIT", PAL_UI_TEXT, 0);
    hud_draw_bitmap_text(page, 30, 56, "D-PAD U/D: CHANGE VALUE", PAL_UI_TEXT, 0);

    char hex_str[9];
    format_hex8(hex_str, s_edit_seed);

    int base_x = 72;
    int base_y = 84;
    for (int i = 0; i < 8; i++) {
        int dx = base_x + i * 12;
        u8 col = (i == s_seed_digit) ? PAL_UI_ACCENT : PAL_UI_TEXT;
        if (i == s_seed_digit) {
            hud_draw_bitmap_char(page, dx, base_y - 10, '^', PAL_UI_ACCENT, 0);
            hud_draw_bitmap_char(page, dx, base_y + 10, '^', PAL_UI_ACCENT, 0);
        }
        hud_draw_bitmap_char(page, dx, base_y, hex_str[i], col, 0);
    }

    hud_draw_bitmap_text(page, 36, 124, "A/START: GENERATE WORLD", PAL_UI_OK, 0);
    hud_draw_bitmap_text(page, 36, 136, "B: CANCEL", PAL_UI_WARN, 0);

    return STATE_SEED_ENTRY;
}

/* Pause Menu: Resume / Save / Load / New World / Toggle FPS counter / Help */
int menu_update_pause(u32 *io_seed, Player *p) {
    static const char *const s_items[6] = {
        "RESUME",
        "SAVE WORLD",
        "LOAD WORLD",
        "NEW WORLD",
        "TOGGLE FPS COUNTER",
        "HELP / CONTROLS"
    };

    if (key_hit(KEY_START) || key_hit(KEY_B)) {
        s_pause_msg[0] = '\0';
        return STATE_PLAYING;
    }

    if (key_hit(KEY_UP)) {
        s_pause_cursor = (s_pause_cursor + 5) % 6;
    }
    if (key_hit(KEY_DOWN)) {
        s_pause_cursor = (s_pause_cursor + 1) % 6;
    }

    if (key_hit(KEY_A)) {
        if (s_pause_cursor == 0) {
            s_pause_msg[0] = '\0';
            return STATE_PLAYING;
        } else if (s_pause_cursor == 1) {
            u16 saved = 0, dropped = 0;
            save_world(*io_seed, p, &saved, &dropped);
            if (dropped > 0) {
                char num[12];
                format_int(num, (int)dropped);
                str_copy(s_pause_msg, "World too changed to save fully: ");
                str_append(s_pause_msg, num);
                str_append(s_pause_msg, " edits dropped");
                hud_set_status(s_pause_msg, 120);
            } else {
                char num[12];
                format_int(num, (int)saved);
                str_copy(s_pause_msg, "SAVED OK (");
                str_append(s_pause_msg, num);
                str_append(s_pause_msg, " EDITS)");
                hud_set_status(s_pause_msg, 90);
            }
        } else if (s_pause_cursor == 2) {
            if (load_world(io_seed, p)) {
                edit_init(&g_edit);
                str_copy(s_pause_msg, "WORLD LOADED FROM SRAM");
                hud_set_status("WORLD LOADED", 60);
                return STATE_PLAYING;
            } else {
                str_copy(s_pause_msg, "NO VALID SAVE IN SRAM");
            }
        } else if (s_pause_cursor == 3) {
            s_edit_seed = (*io_seed ^ (g_frame_counter * 0x9E3779B1u));
            if (s_edit_seed == 0) s_edit_seed = DEFAULT_SEED;
            s_seed_digit = 0;
            return STATE_SEED_ENTRY;
        } else if (s_pause_cursor == 4) {
            g_show_fps ^= 1;
            str_copy(s_pause_msg, g_show_fps ? "FPS COUNTER: ON" : "FPS COUNTER: OFF");
        } else if (s_pause_cursor == 5) {
            return STATE_HELP;
        }
    }

    u16 *page = vid_page;
    hud_fill_bitmap_rect(page, 0, 0, SCREEN_W, SCREEN_H, PAL_UI_BG);
    hud_draw_bitmap_text(page, 78, 12, "- PAUSE MENU -", PAL_UI_ACCENT, 0);

    char seed_line[24];
    char hex_buf[9];
    format_hex8(hex_buf, *io_seed);
    str_copy(seed_line, "SEED: 0X");
    str_append(seed_line, hex_buf);
    hud_draw_bitmap_text(page, 72, 24, seed_line, BLOCK_PAL(BLK_SAND, 2), 0);

    for (int i = 0; i < 6; i++) {
        int y = 42 + i * 13;
        u8 col = (i == s_pause_cursor) ? PAL_UI_ACCENT : PAL_UI_TEXT;
        if (i == s_pause_cursor) {
            hud_draw_bitmap_char(page, 42, y, '>', PAL_UI_ACCENT, 0);
        }
        hud_draw_bitmap_text(page, 54, y, s_items[i], col, 0);
    }

    if (s_pause_msg[0] != '\0') {
        hud_draw_bitmap_text(page, 12, 128, s_pause_msg, PAL_UI_WARN, 0);
    }

    return STATE_PAUSED;
}

/* Help Screen showing the final controls table */
int menu_update_help(int return_state) {
    if (key_hit(KEY_A) || key_hit(KEY_B) || key_hit(KEY_START)) {
        return return_state;
    }

    u16 *page = vid_page;
    hud_fill_bitmap_rect(page, 0, 0, SCREEN_W, SCREEN_H, PAL_UI_BG);
    hud_draw_bitmap_text(page, 66, 8, "GBA-CRAFT CONTROLS", PAL_UI_ACCENT, 0);

    hud_draw_bitmap_text(page, 12, 26,  "D-PAD U/D   : WALK FWD / BACK", PAL_UI_TEXT, 0);
    hud_draw_bitmap_text(page, 12, 38,  "D-PAD L/R   : TURN LEFT / RIGHT", PAL_UI_TEXT, 0);
    hud_draw_bitmap_text(page, 12, 50,  "L + D-PAD UD: LOOK UP / DOWN", PAL_UI_TEXT, 0);
    hud_draw_bitmap_text(page, 12, 62,  "L + D-PAD LR: STRAFE L / R", PAL_UI_TEXT, 0);
    hud_draw_bitmap_text(page, 12, 74,  "R BUTTON    : JUMP (1.25 BLK)", PAL_UI_TEXT, 0);
    hud_draw_bitmap_text(page, 12, 86,  "A (HOLD)    : BREAK BLOCK", PAL_UI_TEXT, 0);
    hud_draw_bitmap_text(page, 12, 98,  "B BUTTON    : PLACE BLOCK", PAL_UI_TEXT, 0);
    hud_draw_bitmap_text(page, 12, 110, "SELECT      : NEXT HOTBAR SLOT", PAL_UI_TEXT, 0);
    hud_draw_bitmap_text(page, 12, 122, "SELECT + A  : 1 WOOD->4 PLANKS", PAL_UI_OK, 0);
    hud_draw_bitmap_text(page, 12, 134, "START       : PAUSE MENU", PAL_UI_TEXT, 0);
    hud_draw_bitmap_text(page, 48, 148, "PRESS A/B/START TO RETURN", PAL_UI_ACCENT, 0);

    return STATE_HELP;
}
