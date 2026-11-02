/*
 * GBA-Craft — Hardware Initialization, Main Loop & State Machine
 * Copyright (c) 2026 Khamerron Kizer / ConsciousNode SoftWorks
 * SPDX-License-Identifier: MIT
 */

#include "gba_craft.h"

volatile u32 g_vblank_count = 0;
u32 g_frame_counter = 0;

static void vblank_isr(void) {
    g_vblank_count++;
}

/* Configure BG2 affine matrix:
 * In STATE_PLAYING: 2x stretch (0x80 = 0.5 step per screen pixel) so 120x80 fills 240x160.
 * In Menu/Self-Test states: 1x identity (0x100) for crisp 240x160 text. */
static void set_bg2_scale(int playing_2x) {
    if (playing_2x) {
        REG_BG2PA = 0x0080;
        REG_BG2PB = 0x0000;
        REG_BG2PC = 0x0000;
        REG_BG2PD = 0x0080;
    } else {
        REG_BG2PA = 0x0100;
        REG_BG2PB = 0x0000;
        REG_BG2PC = 0x0000;
        REG_BG2PD = 0x0100;
    }
    REG_BG2X = 0;
    REG_BG2Y = 0;
}

int main(void) {
    /* 1. Initialize interrupts and VBlank counter */
    irq_init(NULL);
    irq_add(II_VBLANK, vblank_isr);

    /* 2. Initialize lookup tables, palette, SRAM ID, HUD sprites, and menus */
    tables_init();
    palette_init();
    save_init();
    hud_init();
    menu_init();
    edit_init(&g_edit);

    /* 3. Set Mode 4 bitmap + BG2 + 1D OBJ sprites */
    REG_DISPCNT = DCNT_MODE4 | DCNT_BG2 | DCNT_OBJ | DCNT_OBJ_1D;
    set_bg2_scale(0);

    int state = STATE_TITLE;
    int prev_menu_state = STATE_TITLE;
    int selftest_executed = 0;
    u32 last_vb = g_vblank_count;
    u32 fps_vb_accum = 0;
    u32 fps_frame_accum = 0;

    /* Check if SELECT is held at boot while the title screen first appears */
    key_poll();
    if (key_is_down(KEY_SELECT)) {
        state = STATE_SELFTEST;
    }

    while (1) {
        VBlankIntrWait();
        key_poll();
        g_frame_counter++;

        u32 now_vb = g_vblank_count;
        u32 dt_vb = now_vb - last_vb;
        if (dt_vb == 0) dt_vb = 1;
        last_vb = now_vb;

        fps_vb_accum += dt_vb;
        fps_frame_accum++;
        if (fps_vb_accum >= 30) {
            g_last_fps = (u16)((fps_frame_accum * 60u + (fps_vb_accum >> 1)) / fps_vb_accum);
            fps_vb_accum = 0;
            fps_frame_accum = 0;
        }

        if (state == STATE_PLAYING) {
            set_bg2_scale(1);

            if (key_hit(KEY_START)) {
                hud_hide_all();
                set_bg2_scale(0);
                state = STATE_PAUSED;
                continue;
            }

            player_update_input(&g_player, dt_vb);
            target_cast_ray(&g_player, &g_target);
            edit_update(&g_player, &g_target, &g_edit, dt_vb);

            render_frame(&g_player, &g_target);
            render_blit_to_vram(vid_page);
            hud_update(&g_player, &g_edit);
            vid_flip();
        } else if (state == STATE_TITLE) {
            hud_hide_all();
            set_bg2_scale(0);
            prev_menu_state = STATE_TITLE;
            state = menu_update_title(&g_world_seed, &g_player);
            if (state == STATE_SELFTEST) {
                selftest_run_all(1);
                vid_flip();
                selftest_executed = 1;
            } else if (state == STATE_TITLE) {
                vid_flip();
            }
        } else if (state == STATE_PAUSED) {
            hud_hide_all();
            set_bg2_scale(0);
            prev_menu_state = STATE_PAUSED;
            state = menu_update_pause(&g_world_seed, &g_player);
            if (state == STATE_PAUSED) {
                vid_flip();
            }
        } else if (state == STATE_SEED_ENTRY) {
            hud_hide_all();
            set_bg2_scale(0);
            state = menu_update_seed_entry(&g_world_seed, &g_player, prev_menu_state);
            if (state == STATE_SEED_ENTRY) {
                vid_flip();
            }
        } else if (state == STATE_HELP) {
            hud_hide_all();
            set_bg2_scale(0);
            state = menu_update_help(prev_menu_state);
            if (state == STATE_HELP) {
                vid_flip();
            }
        } else if (state == STATE_SELFTEST) {
            hud_hide_all();
            set_bg2_scale(0);
            if (!selftest_executed) {
                selftest_run_all(1);
                vid_flip();
                selftest_executed = 1;
            } else if (key_hit(KEY_START) || key_hit(KEY_A)) {
                selftest_executed = 0;
                state = STATE_TITLE;
            }
        }
    }

    return 0;
}
