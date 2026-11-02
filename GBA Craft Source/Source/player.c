/*
 * GBA-Craft — Player Movement, AABB Collision & Physics
 * Copyright (c) 2026 Khamerron Kizer / ConsciousNode SoftWorks
 * SPDX-License-Identifier: MIT
 */

#include "gba_craft.h"

Player g_player;

/* Check if the player's 0.6 x 1.8 AABB at (px, py, pz) overlaps any solid voxel */
int player_check_aabb_world(fix16 px, fix16 py, fix16 pz) {
    int x0 = FIX_TO_INT(px - PLAYER_HALF_W);
    int x1 = FIX_TO_INT(px + PLAYER_HALF_W - 1);
    int z0 = FIX_TO_INT(pz - PLAYER_HALF_W);
    int z1 = FIX_TO_INT(pz + PLAYER_HALF_W - 1);
    int y0 = FIX_TO_INT(py);
    int y1 = FIX_TO_INT(py + PLAYER_HEIGHT - 1);

    for (int x = x0; x <= x1; x++) {
        for (int z = z0; z <= z1; z++) {
            for (int y = y0; y <= y1; y++) {
                if (world_get(x, z, y) != BLK_AIR) {
                    return 1;
                }
            }
        }
    }
    return 0;
}

/* Check if placing a solid block at (bx, by, bz) would intersect the player's AABB */
int player_aabb_intersects_block(const Player *p, int bx, int by, int bz) {
    fix16 b_xmin = INT_TO_FIX(bx);
    fix16 b_xmax = INT_TO_FIX(bx + 1);
    fix16 b_ymin = INT_TO_FIX(by);
    fix16 b_ymax = INT_TO_FIX(by + 1);
    fix16 b_zmin = INT_TO_FIX(bz);
    fix16 b_zmax = INT_TO_FIX(bz + 1);

    fix16 p_xmin = p->x - PLAYER_HALF_W;
    fix16 p_xmax = p->x + PLAYER_HALF_W;
    fix16 p_ymin = p->y;
    fix16 p_ymax = p->y + PLAYER_HEIGHT;
    fix16 p_zmin = p->z - PLAYER_HALF_W;
    fix16 p_zmax = p->z + PLAYER_HALF_W;

    if (p_xmax <= b_xmin || p_xmin >= b_xmax) return 0;
    if (p_ymax <= b_ymin || p_ymin >= b_ymax) return 0;
    if (p_zmax <= b_zmin || p_zmin >= b_zmax) return 0;
    return 1;
}

/* Spawn player at world centre on top of the highest solid block */
void player_init(Player *p, u32 seed) {
    (void)seed;
    int cx = WORLD_X / 2;
    int cz = WORLD_Z / 2;
    int sy = world_surface_y(cx, cz);

    /* Ensure two air blocks above spawn surface in case of leaves overhang */
    if (sy + 2 < WORLD_Y) {
        g_world[cx][cz][sy + 1] = BLK_AIR;
        g_world[cx][cz][sy + 2] = BLK_AIR;
    }

    p->x = INT_TO_FIX(cx) + FIX_HALF;
    p->z = INT_TO_FIX(cz) + FIX_HALF;
    p->y = INT_TO_FIX(sy + 1);
    p->vx = 0;
    p->vy = 0;
    p->vz = 0;
    p->yaw = 0;
    p->horizon = HORIZON_BASE;
    p->on_ground = 1;
    p->selected_slot = 0;

    for (int i = 0; i < HOTBAR_SLOTS; i++) {
        p->hotbar_type[i]  = BLK_AIR;
        p->hotbar_count[i] = 0;
    }
}

/* Advance one step of player physics with separate X, Z, Y axis resolution (wall sliding, no step-up) */
void player_step_physics(Player *p, fix16 wish_vx, fix16 wish_vz, int jump_req) {
    p->vx = wish_vx;
    p->vz = wish_vz;

    if (jump_req && p->on_ground) {
        p->vy = PLAYER_JUMP_VEL;
        p->on_ground = 0;
    }

    /* Apply gravity and clamp to terminal fall velocity */
    p->vy -= PLAYER_GRAVITY;
    if (p->vy < PLAYER_MAX_FALL) {
        p->vy = PLAYER_MAX_FALL;
    }

    /* 1. X-axis movement and collision */
    if (p->vx != 0) {
        fix16 nx = p->x + p->vx;
        if (!player_check_aabb_world(nx, p->y, p->z)) {
            p->x = nx;
        } else {
            /* Clamp flush to block boundary */
            if (p->vx > 0) {
                int wall_x = FIX_TO_INT(nx + PLAYER_HALF_W - 1);
                p->x = INT_TO_FIX(wall_x) - PLAYER_HALF_W;
            } else {
                int wall_x = FIX_TO_INT(nx - PLAYER_HALF_W);
                p->x = INT_TO_FIX(wall_x + 1) + PLAYER_HALF_W;
            }
            p->vx = 0;
        }
    }

    /* 2. Z-axis movement and collision */
    if (p->vz != 0) {
        fix16 nz = p->z + p->vz;
        if (!player_check_aabb_world(p->x, p->y, nz)) {
            p->z = nz;
        } else {
            if (p->vz > 0) {
                int wall_z = FIX_TO_INT(nz + PLAYER_HALF_W - 1);
                p->z = INT_TO_FIX(wall_z) - PLAYER_HALF_W;
            } else {
                int wall_z = FIX_TO_INT(nz - PLAYER_HALF_W);
                p->z = INT_TO_FIX(wall_z + 1) + PLAYER_HALF_W;
            }
            p->vz = 0;
        }
    }

    /* 3. Y-axis movement and collision */
    fix16 ny = p->y + p->vy;
    if (!player_check_aabb_world(p->x, ny, p->z)) {
        p->y = ny;
        p->on_ground = 0;
    } else {
        if (p->vy < 0) {
            /* Landed on solid ground: snap feet to integer block top */
            int floor_y = FIX_TO_INT(ny);
            p->y = INT_TO_FIX(floor_y + 1);
            p->on_ground = 1;
        } else if (p->vy > 0) {
            /* Hit ceiling: snap head below ceiling block */
            int ceil_y = FIX_TO_INT(ny + PLAYER_HEIGHT - 1);
            p->y = INT_TO_FIX(ceil_y) - PLAYER_HEIGHT;
        }
        p->vy = 0;
    }

    /* Keep within world bounds */
    if (p->y < INT_TO_FIX(1)) {
        p->y = INT_TO_FIX(1);
        p->vy = 0;
        p->on_ground = 1;
    }
}

void player_update_input(Player *p, u32 vblanks_elapsed) {
    (void)vblanks_elapsed;
    int l_held = key_is_down(KEY_L) ? 1 : 0;

    fix16 fwd_move = 0;
    fix16 str_move = 0;

    if (l_held) {
        /* L held + D-pad Up/Down: look up / down (y-shear horizon in 40 +- 32) */
        if (key_is_down(KEY_UP)) {
            p->horizon += PLAYER_LOOK_SPD;
            if (p->horizon > HORIZON_MAX) p->horizon = HORIZON_MAX;
        }
        if (key_is_down(KEY_DOWN)) {
            p->horizon -= PLAYER_LOOK_SPD;
            if (p->horizon < HORIZON_MIN) p->horizon = HORIZON_MIN;
        }
        /* L held + D-pad Left/Right: strafe left / right */
        if (key_is_down(KEY_LEFT)) {
            str_move -= PLAYER_WALK_SPD;
        }
        if (key_is_down(KEY_RIGHT)) {
            str_move += PLAYER_WALK_SPD;
        }
    } else {
        /* D-pad Up/Down: walk forward / back */
        if (key_is_down(KEY_UP)) {
            fwd_move += PLAYER_WALK_SPD;
        }
        if (key_is_down(KEY_DOWN)) {
            fwd_move -= PLAYER_WALK_SPD;
        }
        /* D-pad Left/Right: turn left / right */
        if (key_is_down(KEY_LEFT)) {
            p->yaw = (p->yaw - PLAYER_TURN_SPD) & ANGLE_MASK;
        }
        if (key_is_down(KEY_RIGHT)) {
            p->yaw = (p->yaw + PLAYER_TURN_SPD) & ANGLE_MASK;
        }
    }

    s32 cos_y = lut_cos(p->yaw);
    s32 sin_y = lut_sin(p->yaw);
    /* Right vector is yaw + 90 degrees: (cos(yaw+90), sin(yaw+90)) = (-sin_y, cos_y) */
    fix16 wish_vx = FIX_MUL_FAST(fwd_move, cos_y) - FIX_MUL_FAST(str_move, sin_y);
    fix16 wish_vz = FIX_MUL_FAST(fwd_move, sin_y) + FIX_MUL_FAST(str_move, cos_y);

    int jump_req = (key_hit(KEY_R) || key_is_down(KEY_R)) ? 1 : 0;
    player_step_physics(p, wish_vx, wish_vz, jump_req);
}
