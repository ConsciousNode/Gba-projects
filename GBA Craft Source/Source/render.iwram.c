/*
 * GBA-Craft — Column-Span Voxel Raycaster (ARM Code in IWRAM)
 * Copyright (c) 2026 Khamerron Kizer / ConsciousNode SoftWorks
 * SPDX-License-Identifier: MIT
 */

#include "gba_craft.h"

/* 120 x 80 8-bit scratch framebuffer in 32-bit zero-waitstate IWRAM (9,600 bytes) */
IWRAM_DATA __attribute__((aligned(4))) u8 g_framebuf[RENDER_H][RENDER_W];

/* Project world Y (in 16.16) to screen row [0..RENDER_H] using reciprocal table */
static inline int project_y(int horizon, fix16 world_y, fix16 eye_y, s32 inv_dist) {
    s32 dy_s = (world_y - eye_y) >> 8;
    return horizon - ((dy_s * inv_dist) >> 16);
}

/* Fill vertical column span [y0 .. y1] clipped to [ytop .. ybot] */
static inline void draw_col_span(
    u8 *col_ptr, int y0, int y1, int ytop, int ybot,
    u8 color, u8 highlight_edges, int has_mid_span
) {
    int orig_y0 = y0;
    int orig_y1 = y1;
    if (y0 < ytop) y0 = ytop;
    if (y1 > ybot) y1 = ybot;
    if (y0 > y1) return;

    u8 *dst = col_ptr + y0 * RENDER_W;
    if (!has_mid_span) {
        for (int y = y0; y <= y1; y++) {
            *dst = color;
            dst += RENDER_W;
        }
    } else {
        for (int y = y0; y <= y1; y++) {
            if (*dst == 0) *dst = color;
            dst += RENDER_W;
        }
    }

    if (highlight_edges) {
        if (orig_y0 >= ytop && orig_y0 <= ybot) {
            col_ptr[orig_y0 * RENDER_W] = PAL_HIGHLIGHT;
        }
        if (orig_y1 >= ytop && orig_y1 <= ybot) {
            col_ptr[orig_y1 * RENDER_W] = PAL_HIGHLIGHT;
        }
    }
}

IWRAM_CODE void render_frame(const Player *p, const TargetHit *tgt) {
    /* 1. Fast 32-bit zero fill of 120x80 IWRAM buffer (0 = unwritten pixel) */
    u32 *fb32 = (u32 *)&g_framebuf[0][0];
    int words = (RENDER_W * RENDER_H) >> 2;
    for (int i = 0; i < words; i++) {
        fb32[i] = 0;
    }

    const fix16 eye_x = p->x;
    const fix16 eye_z = p->z;
    const fix16 eye_y = p->y + PLAYER_EYE_H;
    const int start_mx = FIX_TO_INT(eye_x);
    const int start_mz = FIX_TO_INT(eye_z);
    const int frac_x   = FIX_FRAC(eye_x);
    const int frac_z   = FIX_FRAC(eye_z);

    int horizon = p->horizon;
    if (horizon < HORIZON_MIN) horizon = HORIZON_MIN;
    if (horizon > HORIZON_MAX) horizon = HORIZON_MAX;

    const int tgt_active = (tgt && tgt->hit) ? 1 : 0;
    const int tgt_x = tgt_active ? tgt->x : -1;
    const int tgt_y = tgt_active ? tgt->y : -1;
    const int tgt_z = tgt_active ? tgt->z : -1;

    /* 2. Cast 120 screen columns across ~60-degree FOV */
    for (int col = 0; col < RENDER_W; col++) {
        int ray_ang = (p->yaw + ((col - (RENDER_W / 2)) * 3) / 2) & ANGLE_MASK;
        s32 dir_x = lut_cos(ray_ang);
        s32 dir_z = lut_sin(ray_ang);
        s32 fisheye = g_fisheye_lut[col];

        int abs_dx = (dir_x < 0) ? -dir_x : dir_x;
        int abs_dz = (dir_z < 0) ? -dir_z : dir_z;
        int idx_dx = (abs_dx + 128) >> 8;
        int idx_dz = (abs_dz + 128) >> 8;
        if (idx_dx > 256) idx_dx = 256;
        if (idx_dz > 256) idx_dz = 256;

        fix16 delta_x = g_delta_lut[idx_dx];
        fix16 delta_z = g_delta_lut[idx_dz];

        int step_x, step_z;
        fix16 side_x, side_z;

        if (dir_x < 0) {
            step_x = -1;
            side_x = (frac_x >> 8) * (delta_x >> 8);
        } else {
            step_x = 1;
            side_x = ((FIX_ONE - frac_x) >> 8) * (delta_x >> 8);
        }

        if (dir_z < 0) {
            step_z = -1;
            side_z = (frac_z >> 8) * (delta_z >> 8);
        } else {
            step_z = 1;
            side_z = ((FIX_ONE - frac_z) >> 8) * (delta_z >> 8);
        }

        int map_x = start_mx;
        int map_z = start_mz;
        int side_crossed = 0; /* 0 = X-side (darker), 1 = Z-side */
        fix16 t_enter = 0;

        int ytop = 0;
        int ybot = RENDER_H - 1;
        int has_mid_span = 0;
        u8 *col_ptr = &g_framebuf[0][col];

        for (int step = 0; step < MAX_RAY_STEPS && ytop <= ybot; step++) {
            fix16 t_exit;
            int next_mx = map_x;
            int next_mz = map_z;
            int next_side;

            if (side_x < side_z) {
                t_exit = side_x;
                side_x += delta_x;
                next_mx += step_x;
                next_side = 0;
            } else {
                t_exit = side_z;
                side_z += delta_z;
                next_mz += step_z;
                next_side = 1;
            }

            if (map_x >= 0 && map_x < WORLD_X && map_z >= 0 && map_z < WORLD_Z) {
                fix16 p_enter = FIX_MUL_FAST(t_enter, fisheye);
                fix16 p_exit  = FIX_MUL_FAST(t_exit,  fisheye);

                int r_idx_en = p_enter >> 10;
                int r_idx_ex = p_exit  >> 10;
                if (r_idx_en < 6) r_idx_en = 6;
                if (r_idx_en >= RECIP_LUT_SIZE) break;
                if (r_idx_ex < 6) r_idx_ex = 6;
                if (r_idx_ex >= RECIP_LUT_SIZE) r_idx_ex = RECIP_LUT_SIZE - 1;

                s32 inv_en = g_recip_lut[r_idx_en];
                s32 inv_ex = g_recip_lut[r_idx_ex];

                int base_dist_shade = p_enter >> FIX_SHIFT;
                if (base_dist_shade > 15) base_dist_shade = 15;
                int side_bias = (side_crossed == 0) ? 4 : 2;
                int dither = (map_x ^ map_z) & 1;

                const u8 *cells = g_world[map_x][map_z];
                const int is_tgt_col = (map_x == tgt_x && map_z == tgt_z);

                /* Walk the 32 Y-voxels as contiguous runs of solid blocks */
                int y = 0;
                while (y < WORLD_Y && ytop <= ybot) {
                    if (cells[y] == BLK_AIR) {
                        y++;
                        continue;
                    }

                    /* Find contiguous solid run [solid_y0 .. solid_y1] */
                    int solid_y0 = y;
                    int solid_y1 = y;
                    while (solid_y1 + 1 < WORLD_Y && cells[solid_y1 + 1] != BLK_AIR) {
                        solid_y1++;
                    }

                    fix16 wy_run_bot = INT_TO_FIX(solid_y0);
                    fix16 wy_run_top = INT_TO_FIX(solid_y1 + 1);

                    int run_sy_top_en, run_sy_bot_en;
                    if (step == 0) {
                        run_sy_top_en = (wy_run_top < eye_y) ? (RENDER_H - 1) : 0;
                        run_sy_bot_en = (wy_run_bot > eye_y) ? 0 : RENDER_H;
                    } else {
                        run_sy_top_en = project_y(horizon, wy_run_top, eye_y, inv_en);
                        run_sy_bot_en = project_y(horizon, wy_run_bot, eye_y, inv_en);
                    }

                    int span_min = run_sy_top_en;
                    int span_max = run_sy_bot_en;

                    /* 1. Side face at entry distance (grouped by material sub-runs) */
                    if (step > 0 && run_sy_bot_en >= ytop && run_sy_top_en <= ybot) {
                        int sy = solid_y0;
                        while (sy <= solid_y1) {
                            u8 sub_blk = cells[sy];
                            int sub_y0 = sy;
                            int sub_y1 = sy;
                            if (!(is_tgt_col && sy == tgt_y)) {
                                while (sub_y1 + 1 <= solid_y1 &&
                                       cells[sub_y1 + 1] == sub_blk &&
                                       !(is_tgt_col && (sub_y1 + 1) == tgt_y)) {
                                    sub_y1++;
                                }
                            }

                            int is_tgt_blk = (is_tgt_col && sub_y0 == tgt_y);
                            int sub_top = (sub_y1 == solid_y1)
                                ? run_sy_top_en
                                : project_y(horizon, INT_TO_FIX(sub_y1 + 1), eye_y, inv_en);
                            int sub_bot = (sub_y0 == solid_y0)
                                ? run_sy_bot_en
                                : project_y(horizon, INT_TO_FIX(sub_y0), eye_y, inv_en);

                            int s_side = base_dist_shade + side_bias + dither;
                            if (s_side > 15) s_side = 15;
                            if (is_tgt_blk) s_side = 0;

                            if (sub_bot >= sub_top) {
                                draw_col_span(
                                    col_ptr, sub_top, sub_bot,
                                    ytop, ybot, BLOCK_PAL(sub_blk, s_side),
                                    (u8)is_tgt_blk, has_mid_span
                                );
                            }
                            sy = sub_y1 + 1;
                        }
                    }

                    /* 2. Top face if the run's top is below eye height */
                    if (wy_run_top < eye_y) {
                        int sy_top_ex = project_y(horizon, wy_run_top, eye_y, inv_ex);
                        if (sy_top_ex <= run_sy_top_en) {
                            u8 top_blk = cells[solid_y1];
                            int is_tgt_top = (is_tgt_col && solid_y1 == tgt_y);
                            int s_top = base_dist_shade + dither;
                            if (s_top > 15) s_top = 15;
                            if (is_tgt_top) s_top = 0;

                            draw_col_span(
                                col_ptr, sy_top_ex, run_sy_top_en,
                                ytop, ybot, BLOCK_PAL(top_blk, s_top),
                                (u8)is_tgt_top, has_mid_span
                            );
                            if (sy_top_ex < span_min) span_min = sy_top_ex;
                        }
                    }

                    /* 3. Bottom face (cave ceiling / overhang) if the run's bottom is above eye height */
                    if (solid_y0 > 0 && wy_run_bot > eye_y) {
                        int sy_bot_ex = project_y(horizon, wy_run_bot, eye_y, inv_ex);
                        if (sy_bot_ex >= run_sy_bot_en) {
                            u8 bot_blk = cells[solid_y0];
                            int is_tgt_bot = (is_tgt_col && solid_y0 == tgt_y);
                            int s_bot = base_dist_shade + 5;
                            if (s_bot > 15) s_bot = 15;
                            if (is_tgt_bot) s_bot = 0;

                            draw_col_span(
                                col_ptr, run_sy_bot_en, sy_bot_ex,
                                ytop, ybot, BLOCK_PAL(bot_blk, s_bot),
                                (u8)is_tgt_bot, has_mid_span
                            );
                            if (sy_bot_ex > span_max) span_max = sy_bot_ex;
                        }
                    }

                    /* 4. Tighten [ytop, ybot] occlusion window with whatever the run covers */
                    if (solid_y0 == 0) {
                        if (span_min - 1 < ybot) {
                            ybot = span_min - 1;
                        }
                    } else if (span_min <= ytop && span_max >= ytop) {
                        ytop = span_max + 1;
                    } else if (span_max >= ybot && span_min <= ybot) {
                        ybot = span_min - 1;
                    } else if (span_min > ytop && span_max < ybot) {
                        has_mid_span = 1;
                    }

                    y = solid_y1 + 1;
                }
            }

            t_enter = t_exit;
            map_x = next_mx;
            map_z = next_mz;
            side_crossed = next_side;
        }

        /* 3. Fill any remaining unoccluded pixels with sky gradient and ground fog */
        for (int y = 0; y < RENDER_H; y++) {
            u8 *dst = col_ptr + y * RENDER_W;
            if (*dst == 0) {
                if (y < (horizon >> 1)) {
                    *dst = PAL_SKY_TOP;
                } else if (y < horizon - 4) {
                    *dst = PAL_SKY_MID;
                } else if (y <= horizon) {
                    *dst = PAL_SKY_HORIZON;
                } else {
                    *dst = PAL_FOG;
                }
            }
        }
    }
}

/* Copy the 120x80 buffer into the top-left of the 240x160 Mode 4 VRAM back page using 32-bit transfers */
IWRAM_CODE void render_blit_to_vram(u16 *vram_page) {
    const u32 *src = (const u32 *)&g_framebuf[0][0];
    u32 *dst_base = (u32 *)vram_page;
    const int words_per_row = RENDER_W >> 2;       /* 30 words = 120 bytes */
    const int vram_stride_words = SCREEN_W >> 2;   /* 60 words = 240 bytes */

    for (int y = 0; y < RENDER_H; y++) {
        u32 *dst = dst_base + y * vram_stride_words;
        for (int w = 0; w < words_per_row; w++) {
            dst[w] = *src++;
        }
    }
}
