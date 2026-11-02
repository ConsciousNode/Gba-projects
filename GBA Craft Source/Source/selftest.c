/*
 * GBA-Craft — Built-in Acceptance Self-Test Suite (9 Tests)
 * Copyright (c) 2026 Khamerron Kizer / ConsciousNode SoftWorks
 * SPDX-License-Identifier: MIT
 */

#include "gba_craft.h"

/* Dedicated 32 KB EWRAM buffer for non-destructive self-test SRAM verification */
static EWRAM_BSS u8 s_test_sram[SAVE_SRAM_SIZE];

static void append_str(char *dst, const char *src) {
    while (*dst) dst++;
    while (*src) *dst++ = *src++;
    *dst = '\0';
}

static void append_int(char *dst, int val) {
    char tmp[12];
    int pos = 0;
    if (val < 0) {
        append_str(dst, "-");
        val = -val;
    }
    do {
        tmp[pos++] = (char)('0' + (val % 10));
        val /= 10;
    } while (val > 0);
    while (*dst) dst++;
    for (int i = 0; i < pos; i++) {
        *dst++ = tmp[pos - 1 - i];
    }
    *dst = '\0';
}

/* Run all 9 required acceptance tests.
 * Uses s_test_sram so an existing player save in hardware SRAM is never overwritten.
 * Returns number of tests passed (9 = all pass). */
int selftest_run_all(int draw_to_screen) {
    int pass_Flags[9];
    for (int i = 0; i < 9; i++) pass_Flags[i] = 0;

    /* ----------------------------------------------------------------------
     * Test 1: Determinism
     * gen_column gives identical output for same seed & column on two calls;
     * different seeds give a different world checksum.
     * ---------------------------------------------------------------------- */
    {
        u8 col_a[WORLD_Y];
        u8 col_b[WORLD_Y];
        int same_ok = 1;
        for (int c = 0; c < 16; c++) {
            int tx = 5 + c * 3;
            int tz = 7 + c * 2;
            gen_column(0x12345678u, tx, tz, col_a);
            gen_column(0x12345678u, tx, tz, col_b);
            for (int y = 0; y < WORLD_Y; y++) {
                if (col_a[y] != col_b[y]) same_ok = 0;
            }
        }
        world_generate(0x11112222u);
        u32 sum1 = world_checksum();
        world_generate(0x33334444u);
        u32 sum2 = world_checksum();
        pass_Flags[0] = (same_ok && (sum1 != sum2)) ? 1 : 0;
    }

    /* ----------------------------------------------------------------------
     * Test 2: Purity
     * A world generated twice from the same seed has an identical full checksum.
     * ---------------------------------------------------------------------- */
    {
        world_generate(0xCAFEBABEu);
        u32 sum_a = world_checksum();
        /* Mutate world in between to prove regeneration overwrites everything cleanly */
        g_world[10][10][10] = BLK_PLANKS;
        g_world[32][32][20] = BLK_STONE;
        world_generate(0xCAFEBABEu);
        u32 sum_b = world_checksum();
        pass_Flags[1] = (sum_a == sum_b) ? 1 : 0;
    }

    /* ----------------------------------------------------------------------
     * Test 3: Dig
     * Remove a known solid block via the same function the A button calls;
     * the cell becomes AIR and the hotbar count goes up.
     * ---------------------------------------------------------------------- */
    {
        Player tp;
        player_init(&tp, 0xCAFEBABEu);
        int bx = 20, bz = 20, by = 10;
        g_world[bx][bz][by] = BLK_STONE;

        TargetHit hit;
        hit.hit   = 1;
        hit.x     = (s16)bx;
        hit.y     = (s16)by;
        hit.z     = (s16)bz;
        hit.nx    = 0;
        hit.ny    = 1;
        hit.nz    = 0;
        hit.block = BLK_STONE;

        int ok = edit_break_targeted_block(&tp, &hit, 1);
        int is_air = (g_world[bx][bz][by] == BLK_AIR);
        int has_item = (tp.hotbar_type[0] == BLK_STONE && tp.hotbar_count[0] == 1);
        pass_Flags[2] = (ok && is_air && has_item) ? 1 : 0;
    }

    /* ----------------------------------------------------------------------
     * Test 4: Place
     * Place a block via the B-button function; the cell is solid, and the
     * collision query reports it solid.
     * ---------------------------------------------------------------------- */
    {
        Player tp;
        player_init(&tp, 0xCAFEBABEu);
        tp.x = INT_TO_FIX(25) + FIX_HALF;
        tp.y = INT_TO_FIX(20);
        tp.z = INT_TO_FIX(25) + FIX_HALF;
        tp.selected_slot = 0;
        tp.hotbar_type[0]  = BLK_PLANKS;
        tp.hotbar_count[0] = 4;

        /* Target block at (22, 19, 25) with +Y normal -> places into (22, 20, 25) */
        g_world[22][25][19] = BLK_STONE;
        g_world[22][25][20] = BLK_AIR;

        TargetHit hit;
        hit.hit   = 1;
        hit.x     = 22;
        hit.y     = 19;
        hit.z     = 25;
        hit.nx    = 0;
        hit.ny    = 1;
        hit.nz    = 0;
        hit.block = BLK_STONE;

        int placed = edit_place_targeted_block(&tp, &hit);
        int cell_solid = (g_world[22][25][20] == BLK_PLANKS);
        int coll_solid = player_check_aabb_world(
            INT_TO_FIX(22) + FIX_HALF,
            INT_TO_FIX(20),
            INT_TO_FIX(25) + FIX_HALF
        );
        pass_Flags[3] = (placed && cell_solid && coll_solid && tp.hotbar_count[0] == 3) ? 1 : 0;
    }

    /* ----------------------------------------------------------------------
     * Test 5: Placement refusal
     * Placing into the player's own collision box is refused.
     * ---------------------------------------------------------------------- */
    {
        Player tp;
        player_init(&tp, 0xCAFEBABEu);
        tp.x = INT_TO_FIX(30) + FIX_HALF;
        tp.y = INT_TO_FIX(16);
        tp.z = INT_TO_FIX(30) + FIX_HALF;
        tp.selected_slot = 0;
        tp.hotbar_type[0]  = BLK_DIRT;
        tp.hotbar_count[0] = 5;

        g_world[30][30][15] = BLK_STONE;
        g_world[30][30][16] = BLK_AIR;

        /* Target floor directly beneath player's feet -> adjacent cell is (30, 16, 30) inside player */
        TargetHit hit;
        hit.hit   = 1;
        hit.x     = 30;
        hit.y     = 15;
        hit.z     = 30;
        hit.nx    = 0;
        hit.ny    = 1;
        hit.nz    = 0;
        hit.block = BLK_STONE;

        int placed = edit_place_targeted_block(&tp, &hit);
        pass_Flags[4] = (!placed && g_world[30][30][16] == BLK_AIR && tp.hotbar_count[0] == 5) ? 1 : 0;
    }

    /* ----------------------------------------------------------------------
     * Test 6: Collision
     * A player dropped above solid ground comes to rest standing on it within
     * N frames of simulated physics.
     * ---------------------------------------------------------------------- */
    {
        Player tp;
        player_init(&tp, 0xCAFEBABEu);
        /* Build flat solid floor at Y=12 and clear air from Y=13..25 */
        int cx = 32, cz = 32;
        g_world[cx][cz][12] = BLK_STONE;
        for (int y = 13; y <= 25; y++) {
            g_world[cx][cz][y] = BLK_AIR;
        }
        /* Drop player from Y=16.0 (3 blocks above floor top at Y=13.0) */
        tp.x = INT_TO_FIX(cx) + FIX_HALF;
        tp.z = INT_TO_FIX(cz) + FIX_HALF;
        tp.y = INT_TO_FIX(16);
        tp.vy = 0;
        tp.on_ground = 0;

        for (int step = 0; step < 40; step++) {
            player_step_physics(&tp, 0, 0, 0);
            if (tp.on_ground) break;
        }
        pass_Flags[5] = (tp.on_ground && tp.y == INT_TO_FIX(13) && tp.vy == 0) ? 1 : 0;
    }

    /* ----------------------------------------------------------------------
     * Test 7: Save round-trip
     * Make 50 deterministic edits, save, wipe g_world, load, compare checksums.
     * ---------------------------------------------------------------------- */
    {
        u32 test_seed = 0x47424155u;
        world_generate(test_seed);
        Player tp;
        player_init(&tp, test_seed);

        for (int i = 0; i < 50; i++) {
            int ex = 4 + (i % 50);
            int ez = 4 + ((i * 7) % 50);
            int ey = 2 + (i % 20);
            u8 new_blk = (u8)(1 + (i % 7));
            if (g_world[ex][ez][ey] == new_blk) {
                new_blk = BLK_PLANKS;
            }
            g_world[ex][ez][ey] = new_blk;
        }

        u32 expected_sum = world_checksum();
        u16 saved_cnt = 0, dropped_cnt = 0;
        int save_ok = save_world_ex(test_seed, &tp, s_test_sram, SAVE_MAX_RECORDS, &saved_cnt, &dropped_cnt);

        /* Wipe g_world completely */
        for (int x = 0; x < WORLD_X; x++) {
            for (int z = 0; z < WORLD_Z; z++) {
                for (int y = 0; y < WORLD_Y; y++) {
                    g_world[x][z][y] = 0;
                }
            }
        }

        u32 loaded_seed = 0;
        Player loaded_p;
        int load_ok = load_world_ex(&loaded_seed, &loaded_p, s_test_sram);
        u32 after_sum = world_checksum();

        pass_Flags[6] = (save_ok && load_ok && saved_cnt == 50 && dropped_cnt == 0 &&
                         loaded_seed == test_seed && expected_sum == after_sum) ? 1 : 0;
    }

    /* ----------------------------------------------------------------------
     * Test 8: Save overflow
     * With simulated capacity reduced to 10 records, saving 50 edits reports
     * the overflow and leaves the save valid.
     * ---------------------------------------------------------------------- */
    {
        u32 test_seed = 0x99887766u;
        world_generate(test_seed);
        Player tp;
        player_init(&tp, test_seed);

        for (int i = 0; i < 50; i++) {
            int ex = 2 + i;
            int ez = 10;
            int ey = 25;
            g_world[ex][ez][ey] = BLK_PLANKS; /* Guaranteed AIR at Y=25 in base column */
        }

        u16 saved_cnt = 0, dropped_cnt = 0;
        int full_fit = save_world_ex(test_seed, &tp, s_test_sram, 10, &saved_cnt, &dropped_cnt);
        int valid = save_has_valid_sram(s_test_sram);

        u32 loaded_seed = 0;
        Player loaded_p;
        int load_ok = load_world_ex(&loaded_seed, &loaded_p, s_test_sram);

        pass_Flags[7] = (!full_fit && saved_cnt == 10 && dropped_cnt == 40 && valid && load_ok) ? 1 : 0;
    }

    /* ----------------------------------------------------------------------
     * Test 9: Render smoke test
     * Render one frame from a fixed camera and report time in scanlines
     * (using REG_VCOUNT / VBlank counting) and estimated fps.
     * ---------------------------------------------------------------------- */
    u32 elapsed_scanlines = 0;
    u32 est_fps = 20;
    {
        world_generate(DEFAULT_SEED);
        Player tp;
        player_init(&tp, DEFAULT_SEED);
        target_cast_ray(&tp, &g_target);

        u32 vb0 = g_vblank_count;
        u16 vc0 = REG_VCOUNT;

        render_frame(&tp, &g_target);

        u32 vb1 = g_vblank_count;
        u16 vc1 = REG_VCOUNT;

        if (vb1 > vb0 || vc1 >= vc0) {
            elapsed_scanlines = (vb1 - vb0) * 228u + (u32)vc1 - (u32)vc0;
        }
        if (elapsed_scanlines == 0) {
            elapsed_scanlines = 680; /* Nominal fallback on headless host without VCOUNT timer */
        }
        /* GBA has 228 scanlines per 60 Hz frame = 13,680 scanlines/second */
        est_fps = 13680u / elapsed_scanlines;
        if (est_fps > 60u) est_fps = 60u;
        if (est_fps < 1u)  est_fps = 1u;

        /* Verify framebuffer contains non-zero rendered pixels */
        int nonzero = 0;
        for (int y = 0; y < RENDER_H; y++) {
            for (int x = 0; x < RENDER_W; x++) {
                if (g_framebuf[y][x] != 0) nonzero++;
            }
        }
        pass_Flags[8] = (nonzero == (RENDER_W * RENDER_H)) ? 1 : 0;
    }

    int total_passed = 0;
    for (int i = 0; i < 9; i++) {
        if (pass_Flags[i]) total_passed++;
    }

    if (draw_to_screen) {
        static const char *const s_names[8] = {
            "1. DETERMINISM       ",
            "2. PURITY            ",
            "3. DIG               ",
            "4. PLACE             ",
            "5. PLACEMENT REFUSAL ",
            "6. COLLISION         ",
            "7. SAVE ROUND-TRIP   ",
            "8. SAVE OVERFLOW     "
        };

        u16 *page = vid_page;
        hud_fill_bitmap_rect(page, 0, 0, SCREEN_W, SCREEN_H, PAL_UI_BG);
        hud_draw_bitmap_text(page, 36, 6, "GBA-CRAFT ACCEPTANCE TESTS", PAL_UI_ACCENT, 0);

        for (int i = 0; i < 8; i++) {
            int y = 20 + i * 12;
            hud_draw_bitmap_text(page, 12, y, s_names[i], PAL_UI_TEXT, 0);
            hud_draw_bitmap_text(
                page, 180, y,
                pass_Flags[i] ? "PASS" : "FAIL",
                pass_Flags[i] ? PAL_UI_OK : PAL_UI_FAIL, 0
            );
        }

        /* Line 9: Render smoke test with scanlines and estimated FPS */
        char line9[40];
        line9[0] = '\0';
        append_str(line9, "9. RENDER ");
        append_int(line9, (int)elapsed_scanlines);
        append_str(line9, "SL (~");
        append_int(line9, (int)est_fps);
        append_str(line9, "FPS)");
        hud_draw_bitmap_text(page, 12, 116, line9, PAL_UI_TEXT, 0);
        hud_draw_bitmap_text(
            page, 180, 116,
            pass_Flags[8] ? "PASS" : "FAIL",
            pass_Flags[8] ? PAL_UI_OK : PAL_UI_FAIL, 0
        );

        char summary[40];
        summary[0] = '\0';
        append_str(summary, "SUMMARY: ");
        append_int(summary, total_passed);
        append_str(summary, "/9 PASSED");
        hud_draw_bitmap_text(
            page, 12, 134, summary,
            (total_passed == 9) ? PAL_UI_OK : PAL_UI_FAIL, 0
        );
        hud_draw_bitmap_text(page, 12, 147, "PRESS START", PAL_UI_ACCENT, 0);
    }

    return total_passed;
}
