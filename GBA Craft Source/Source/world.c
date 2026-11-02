/*
 * GBA-Craft — Procedural World Generation & Voxel Storage
 * Copyright (c) 2026 Khamerron Kizer / ConsciousNode SoftWorks
 * SPDX-License-Identifier: MIT
 */

#include "gba_craft.h"

/* 64 x 64 x 32 bytes = 128 KB voxel grid in EWRAM */
EWRAM_BSS u8 g_world[WORLD_X][WORLD_Z][WORLD_Y];

u32 g_world_seed = DEFAULT_SEED;

/* Pure integer 2D hash (returns 0..65535 in low 16 bits, full 32-bit mix) */
u32 world_hash2d(u32 seed, int x, int z) {
    u32 h = seed ^ ((u32)x * 0x9E3779B1u) ^ ((u32)z * 0x85EBCA77u);
    h ^= h >> 16;
    h *= 0x7FEB352Du;
    h ^= h >> 15;
    h *= 0x846CA68Bu;
    h ^= h >> 16;
    return h;
}

/* Pure integer 3D hash for cave noise */
u32 world_hash3d(u32 seed, int x, int y, int z) {
    u32 h = seed ^ ((u32)x * 0x9E3779B1u) ^ ((u32)y * 0xC2B2AE3Du) ^ ((u32)z * 0x27D4EB2Fu);
    h ^= h >> 16;
    h *= 0x7FEB352Du;
    h ^= h >> 15;
    h *= 0x846CA68Bu;
    h ^= h >> 16;
    return h;
}

/* Smoothstep-like bilinear value noise on a power-of-two grid */
static int value_noise_2d(u32 seed, int x, int z, int shift) {
    int mask = (1 << shift) - 1;
    int gx = x >> shift;
    int gz = z >> shift;
    int fx = x & mask;
    int fz = z & mask;

    /* Smooth interpolation weights in 0..256 */
    int wx = (fx << 8) >> shift;
    int wz = (fz << 8) >> shift;
    wx = (wx * wx * (768 - (wx << 1))) >> 16;
    wz = (wz * wz * (768 - (wz << 1))) >> 16;

    int v00 = (int)(world_hash2d(seed, gx,     gz    ) & 0xFF);
    int v10 = (int)(world_hash2d(seed, gx + 1, gz    ) & 0xFF);
    int v01 = (int)(world_hash2d(seed, gx,     gz + 1) & 0xFF);
    int v11 = (int)(world_hash2d(seed, gx + 1, gz + 1) & 0xFF);

    int vx0 = v00 + (((v10 - v00) * wx) >> 8);
    int vx1 = v01 + (((v11 - v01) * wx) >> 8);
    return vx0 + (((vx1 - vx0) * wz) >> 8); /* 0..255 */
}

/* Trilinear 3D value noise on a 4x4x4 lattice (returns 0..255) */
static int value_noise_3d_cave(u32 seed, int x, int y, int z) {
    int gx = x >> 2;
    int gy = y >> 1; /* 2-block vertical lattice for walkable horizontal tunnels */
    int gz = z >> 2;

    int wx = (x & 3) << 6; /* 0, 64, 128, 192 */
    int wy = (y & 1) << 7; /* 0, 128 */
    int wz = (z & 3) << 6;

    u32 cseed = seed ^ 0xCA7ECA7Eu;
    int c000 = (int)(world_hash3d(cseed, gx,     gy,     gz    ) & 0xFF);
    int c100 = (int)(world_hash3d(cseed, gx + 1, gy,     gz    ) & 0xFF);
    int c010 = (int)(world_hash3d(cseed, gx,     gy + 1, gz    ) & 0xFF);
    int c110 = (int)(world_hash3d(cseed, gx + 1, gy + 1, gz    ) & 0xFF);
    int c001 = (int)(world_hash3d(cseed, gx,     gy,     gz + 1) & 0xFF);
    int c101 = (int)(world_hash3d(cseed, gx + 1, gy,     gz + 1) & 0xFF);
    int c011 = (int)(world_hash3d(cseed, gx,     gy + 1, gz + 1) & 0xFF);
    int c111 = (int)(world_hash3d(cseed, gx + 1, gy + 1, gz + 1) & 0xFF);

    int x00 = c000 + (((c100 - c000) * wx) >> 8);
    int x10 = c010 + (((c110 - c010) * wx) >> 8);
    int x01 = c001 + (((c101 - c001) * wx) >> 8);
    int x11 = c011 + (((c111 - c011) * wx) >> 8);

    int y0 = x00 + (((x10 - x00) * wy) >> 8);
    int y1 = x01 + (((x11 - x01) * wy) >> 8);

    return y0 + (((y1 - y0) * wz) >> 8);
}

/* Deterministic surface height (Y in 8..22) from 2-octave value noise */
static int column_surface_height(u32 seed, int x, int z) {
    int oct0 = value_noise_2d(seed, x, z, 3);                   /* 8-block lattice */
    int oct1 = value_noise_2d(seed ^ 0xA5A55A5Au, x, z, 2);     /* 4-block lattice */
    int combined = (oct0 * 3 + oct1) >> 2;                      /* 0..255 */
    int h = 8 + ((combined * 15) >> 8);                         /* 8..22 */
    return h;
}

/* Pure helper: checks whether column (tx, tz) has a tree trunk (~1 in 40 grass columns).
 * Rejects trees if a lower-index neighbour already has one so crowns remain distinct. */
static int column_has_tree(u32 seed, int tx, int tz, int *out_surf_y, int *out_trunk_h) {
    if (tx < 3 || tx >= WORLD_X - 3 || tz < 3 || tz >= WORLD_Z - 3) {
        return 0;
    }
    /* Keep spawn 5x5 area clear of tree trunks so player never spawns inside wood */
    if (tx >= (WORLD_X / 2 - 2) && tx <= (WORLD_X / 2 + 2) &&
        tz >= (WORLD_Z / 2 - 2) && tz <= (WORLD_Z / 2 + 2)) {
        return 0;
    }

    u32 h = world_hash2d(seed ^ 0x72EE1234u, tx, tz);
    if ((h % 40u) != 0u) {
        return 0;
    }

    int sy = column_surface_height(seed, tx, tz);
    if (sy <= 10 || sy + 6 >= WORLD_Y - 1) {
        return 0; /* Only on grass columns (sy > 10) with headroom */
    }

    if (out_surf_y)  *out_surf_y  = sy;
    if (out_trunk_h) *out_trunk_h = 4 + (int)((h >> 8) & 1); /* 4 or 5 blocks */
    return 1;
}

/* CRITICAL REQUIREMENT: Pure function of (seed, x, z) generating 32 Y-voxels */
void gen_column(u32 seed, int x, int z, u8 out[WORLD_Y]) {
    for (int y = 0; y < WORLD_Y; y++) {
        out[y] = BLK_AIR;
    }

    if (x < 0 || x >= WORLD_X || z < 0 || z >= WORLD_Z) {
        for (int y = 0; y < WORLD_Y; y++) {
            out[y] = BLK_BEDROCK;
        }
        return;
    }

    int surf_y = column_surface_height(seed, x, z);

    /* Finite world border walls of bedrock at edges (x=0, x=63, z=0, z=63) */
    if (x == 0 || x == WORLD_X - 1 || z == 0 || z == WORLD_Z - 1) {
        int wall_top = surf_y + 3;
        if (wall_top >= WORLD_Y) wall_top = WORLD_Y - 1;
        for (int y = 0; y <= wall_top; y++) {
            out[y] = BLK_BEDROCK;
        }
        return;
    }

    /* Base terrain strata */
    out[0] = BLK_BEDROCK;
    u8 top_blk = (surf_y <= 10) ? BLK_SAND : BLK_GRASS;
    u8 sub_blk = (surf_y <= 10) ? BLK_SAND : BLK_DIRT;

    for (int y = 1; y <= surf_y; y++) {
        if (y == surf_y) {
            out[y] = top_blk;
        } else if (y >= surf_y - 3) {
            out[y] = sub_blk;
        } else {
            out[y] = BLK_STONE;
        }
    }

    /* 3D value-noise caves between Y=2 and surface - 4 */
    int cave_max_y = surf_y - 4;
    for (int y = 2; y <= cave_max_y; y++) {
        int n = value_noise_3d_cave(seed, x, y, z);
        if (n > 166) {
            out[y] = BLK_AIR;
        }
    }

    /* Deterministic trees: check 3x3 neighbourhood for tree trunks */
    for (int dx = -1; dx <= 1; dx++) {
        for (int dz = -1; dz <= 1; dz++) {
            int t_surf = 0;
            int t_height = 0;
            if (column_has_tree(seed, x + dx, z + dz, &t_surf, &t_height)) {
                /* 3x3x2 crown of leaves around the top of the trunk */
                int crown_y0 = t_surf + t_height - 1;
                int crown_y1 = t_surf + t_height;
                if (crown_y0 >= 1 && crown_y0 < WORLD_Y && out[crown_y0] == BLK_AIR) {
                    out[crown_y0] = BLK_LEAVES;
                }
                if (crown_y1 >= 1 && crown_y1 < WORLD_Y && out[crown_y1] == BLK_AIR) {
                    out[crown_y1] = BLK_LEAVES;
                }
                /* Center column gets the WOOD trunk and top cap leaf */
                if (dx == 0 && dz == 0) {
                    for (int ty = t_surf + 1; ty <= t_surf + t_height && ty < WORLD_Y; ty++) {
                        out[ty] = BLK_WOOD;
                    }
                    int cap_y = t_surf + t_height + 1;
                    if (cap_y < WORLD_Y && out[cap_y] == BLK_AIR) {
                        out[cap_y] = BLK_LEAVES;
                    }
                }
            }
        }
    }
}

void world_generate(u32 seed) {
    g_world_seed = seed;
    for (int x = 0; x < WORLD_X; x++) {
        for (int z = 0; z < WORLD_Z; z++) {
            gen_column(seed, x, z, g_world[x][z]);
        }
    }
}

u8 world_get(int x, int z, int y) {
    if (y < 0) return BLK_BEDROCK;
    if (y >= WORLD_Y) return BLK_AIR;
    if (x < 0 || x >= WORLD_X || z < 0 || z >= WORLD_Z) return BLK_BEDROCK;
    return g_world[x][z][y];
}

void world_set(int x, int z, int y, u8 block) {
    if (x <= 0 || x >= WORLD_X - 1 || z <= 0 || z >= WORLD_Z - 1) return;
    if (y <= 0 || y >= WORLD_Y) return;
    g_world[x][z][y] = block;
}

int world_surface_y(int x, int z) {
    if (x < 0) x = 0;
    if (x >= WORLD_X) x = WORLD_X - 1;
    if (z < 0) z = 0;
    if (z >= WORLD_Z) z = WORLD_Z - 1;
    for (int y = WORLD_Y - 1; y >= 0; y--) {
        u8 b = g_world[x][z][y];
        if (b != BLK_AIR && b != BLK_LEAVES) {
            return y;
        }
    }
    return 1;
}

/* FNV-1a 32-bit checksum over all 131,072 voxels in g_world */
u32 world_checksum(void) {
    u32 hash = 0x811C9DC5u;
    const u8 *ptr = &g_world[0][0][0];
    int total = WORLD_X * WORLD_Z * WORLD_Y;
    for (int i = 0; i < total; i++) {
        hash ^= (u32)ptr[i];
        hash *= 0x01000193u;
    }
    return hash;
}
