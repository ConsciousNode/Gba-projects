/*
 * GBA-Craft — 3D DDA Voxel Targeting Ray (ARM Code in IWRAM)
 * Copyright (c) 2026 Khamerron Kizer / ConsciousNode SoftWorks
 * SPDX-License-Identifier: MIT
 */

#include "gba_craft.h"

/* Cast a 3D Amanatides-Woo voxel ray from the player's eye through the center crosshair
 * (accounting for y-shear horizon pitch) up to 4 blocks reach. */
IWRAM_CODE void target_cast_ray(const Player *p, TargetHit *out_hit) {
    out_hit->hit   = 0;
    out_hit->x     = 0;
    out_hit->y     = 0;
    out_hit->z     = 0;
    out_hit->nx    = 0;
    out_hit->ny    = 0;
    out_hit->nz    = 0;
    out_hit->block = BLK_AIR;

    fix16 ox = p->x;
    fix16 oy = p->y + PLAYER_EYE_H;
    fix16 oz = p->z;

    s32 dir_x = lut_cos(p->yaw);
    s32 dir_z = lut_sin(p->yaw);
    /* Exact vertical slope matching y-sheared projection at crosshair (screen_y = 40):
     * (y - eye_y) / dist = (horizon - 40) / PROJ_CONST */
    s32 dir_y = ((p->horizon - HORIZON_BASE) * FIX_ONE) / PROJ_CONST;

    int vx = FIX_TO_INT(ox);
    int vy = FIX_TO_INT(oy);
    int vz = FIX_TO_INT(oz);

    int abs_dx = (dir_x < 0) ? -dir_x : dir_x;
    int abs_dy = (dir_y < 0) ? -dir_y : dir_y;
    int abs_dz = (dir_z < 0) ? -dir_z : dir_z;

    int idx_x = (abs_dx + 128) >> 8;
    int idx_y = (abs_dy + 128) >> 8;
    int idx_z = (abs_dz + 128) >> 8;
    if (idx_x > 256) idx_x = 256;
    if (idx_y > 256) idx_y = 256;
    if (idx_z > 256) idx_z = 256;

    fix16 delta_x = g_delta_lut[idx_x];
    fix16 delta_y = g_delta_lut[idx_y];
    fix16 delta_z = g_delta_lut[idx_z];

    int step_x, step_y, step_z;
    fix16 side_x, side_y, side_z;

    int fx = FIX_FRAC(ox);
    int fy = FIX_FRAC(oy);
    int fz = FIX_FRAC(oz);

    if (dir_x < 0) {
        step_x = -1;
        side_x = (fx >> 8) * (delta_x >> 8);
    } else {
        step_x = 1;
        side_x = ((FIX_ONE - fx) >> 8) * (delta_x >> 8);
    }

    if (dir_y < 0) {
        step_y = -1;
        side_y = (fy >> 8) * (delta_y >> 8);
    } else {
        step_y = 1;
        side_y = ((FIX_ONE - fy) >> 8) * (delta_y >> 8);
    }

    if (dir_z < 0) {
        step_z = -1;
        side_z = (fz >> 8) * (delta_z >> 8);
    } else {
        step_z = 1;
        side_z = ((FIX_ONE - fz) >> 8) * (delta_z >> 8);
    }

    s8 norm_x = 0, norm_y = 0, norm_z = 0;

    for (int step = 0; step < 24; step++) {
        fix16 t_cur;
        if (side_x <= side_y && side_x <= side_z) {
            t_cur  = side_x;
            side_x += delta_x;
            vx     += step_x;
            norm_x = (s8)(-step_x);
            norm_y = 0;
            norm_z = 0;
        } else if (side_y <= side_x && side_y <= side_z) {
            t_cur  = side_y;
            side_y += delta_y;
            vy     += step_y;
            norm_x = 0;
            norm_y = (s8)(-step_y);
            norm_z = 0;
        } else {
            t_cur  = side_z;
            side_z += delta_z;
            vz     += step_z;
            norm_x = 0;
            norm_y = 0;
            norm_z = (s8)(-step_z);
        }

        if (t_cur > TARGET_REACH_FIX) {
            break;
        }

        if (vx >= 0 && vx < WORLD_X && vz >= 0 && vz < WORLD_Z && vy >= 0 && vy < WORLD_Y) {
            u8 blk = g_world[vx][vz][vy];
            if (blk != BLK_AIR) {
                out_hit->hit   = 1;
                out_hit->x     = (s16)vx;
                out_hit->y     = (s16)vy;
                out_hit->z     = (s16)vz;
                out_hit->nx    = norm_x;
                out_hit->ny    = norm_y;
                out_hit->nz    = norm_z;
                out_hit->block = blk;
                return;
            }
        }
    }
}
