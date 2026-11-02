/*
 * GBA-Craft — Seed + Diff Persistence in 32 KB SRAM
 * Copyright (c) 2026 Khamerron Kizer / ConsciousNode SoftWorks
 * SPDX-License-Identifier: MIT
 */

#include "gba_craft.h"

/* Required emulator SRAM detection signature, prevented from being stripped */
__attribute__((used, aligned(4))) static const char s_save_id[] = "SRAM_V113";

/* 64-byte Save Header layout (serialized byte-for-byte with 8-bit accesses) */
typedef struct {
    u8  magic[4];                   /* "GBAC" */
    u16 version;                    /* SAVE_VERSION (1) */
    u16 flags;                      /* bit 0 = overflow occurred */
    u32 seed;                       /* 32-bit world seed */
    s32 player_x;                   /* 16.16 fixed-point */
    s32 player_y;
    s32 player_z;
    s16 player_yaw;
    s16 player_horizon;
    u8  selected_slot;
    u8  reserved0;
    u8  hotbar_type[HOTBAR_SLOTS];
    u8  hotbar_count[HOTBAR_SLOTS];
    u16 record_count;               /* Number of packed u32 diff records stored */
    u16 dropped_count;              /* Number of edits dropped if overflowed */
    u32 checksum;                   /* FNV-1a checksum of header + records */
    u8  padding[12];                /* Pad header to exactly 64 bytes */
} SaveHeader;

/* Hardware rule: SRAM (0x0E000000) only supports 8-bit reads/writes, and loops
 * accessing SRAM must execute from IWRAM or EWRAM, never ROM. */
IWRAM_CODE static void sram_write_bytes(volatile u8 *dst_sram, const u8 *src, int len) {
    for (int i = 0; i < len; i++) {
        dst_sram[i] = src[i];
    }
}

IWRAM_CODE static void sram_read_bytes(u8 *dst, const volatile u8 *src_sram, int len) {
    for (int i = 0; i < len; i++) {
        dst[i] = src_sram[i];
    }
}

static void write_u16_le(u8 *p, u16 v) {
    p[0] = (u8)(v & 0xFF);
    p[1] = (u8)((v >> 8) & 0xFF);
}

static void write_u32_le(u8 *p, u32 v) {
    p[0] = (u8)(v & 0xFF);
    p[1] = (u8)((v >> 8) & 0xFF);
    p[2] = (u8)((v >> 16) & 0xFF);
    p[3] = (u8)((v >> 24) & 0xFF);
}

static u16 read_u16_le(const u8 *p) {
    return (u16)((u16)p[0] | ((u16)p[1] << 8));
}

static u32 read_u32_le(const u8 *p) {
    return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

/* Pack diff cell as u32: x 6 bits | z 6 bits | y 5 bits | block 4 bits */
static inline u32 pack_diff_record(int x, int z, int y, u8 block) {
    return ((u32)(x & 0x3F)) |
           ((u32)(z & 0x3F) << 6) |
           ((u32)(y & 0x1F) << 12) |
           ((u32)(block & 0x0F) << 17);
}

static inline void unpack_diff_record(u32 rec, int *x, int *z, int *y, u8 *block) {
    *x     = (int)(rec & 0x3Fu);
    *z     = (int)((rec >> 6) & 0x3Fu);
    *y     = (int)((rec >> 12) & 0x1Fu);
    *block = (u8)((rec >> 17) & 0x0Fu);
}

static void serialize_header(u8 buf[SAVE_HEADER_SIZE], const SaveHeader *hdr) {
    for (int i = 0; i < SAVE_HEADER_SIZE; i++) {
        buf[i] = 0;
    }
    buf[0] = hdr->magic[0];
    buf[1] = hdr->magic[1];
    buf[2] = hdr->magic[2];
    buf[3] = hdr->magic[3];
    write_u16_le(&buf[4],  hdr->version);
    write_u16_le(&buf[6],  hdr->flags);
    write_u32_le(&buf[8],  hdr->seed);
    write_u32_le(&buf[12], (u32)hdr->player_x);
    write_u32_le(&buf[16], (u32)hdr->player_y);
    write_u32_le(&buf[20], (u32)hdr->player_z);
    write_u16_le(&buf[24], (u16)hdr->player_yaw);
    write_u16_le(&buf[26], (u16)hdr->player_horizon);
    buf[28] = hdr->selected_slot;
    buf[29] = 0;
    for (int i = 0; i < HOTBAR_SLOTS; i++) {
        buf[30 + i] = hdr->hotbar_type[i];
        buf[39 + i] = hdr->hotbar_count[i];
    }
    write_u16_le(&buf[48], hdr->record_count);
    write_u16_le(&buf[50], hdr->dropped_count);
    write_u32_le(&buf[52], hdr->checksum);
}

static void deserialize_header(const u8 buf[SAVE_HEADER_SIZE], SaveHeader *hdr) {
    hdr->magic[0]       = buf[0];
    hdr->magic[1]       = buf[1];
    hdr->magic[2]       = buf[2];
    hdr->magic[3]       = buf[3];
    hdr->version        = read_u16_le(&buf[4]);
    hdr->flags          = read_u16_le(&buf[6]);
    hdr->seed           = read_u32_le(&buf[8]);
    hdr->player_x       = (s32)read_u32_le(&buf[12]);
    hdr->player_y       = (s32)read_u32_le(&buf[16]);
    hdr->player_z       = (s32)read_u32_le(&buf[20]);
    hdr->player_yaw     = (s16)read_u16_le(&buf[24]);
    hdr->player_horizon = (s16)read_u16_le(&buf[26]);
    hdr->selected_slot  = buf[28];
    hdr->reserved0      = buf[29];
    for (int i = 0; i < HOTBAR_SLOTS; i++) {
        hdr->hotbar_type[i]  = buf[30 + i];
        hdr->hotbar_count[i] = buf[39 + i];
    }
    hdr->record_count   = read_u16_le(&buf[48]);
    hdr->dropped_count  = read_u16_le(&buf[50]);
    hdr->checksum       = read_u32_le(&buf[52]);
}

void save_init(void) {
    /* Touch s_save_id so LTO / linker GC can never discard the SRAM_V113 string */
    volatile char c = s_save_id[0];
    (void)c;
}

int save_has_valid_sram(const u8 *custom_sram) {
    const volatile u8 *sram = custom_sram ? (const volatile u8 *)custom_sram : (const volatile u8 *)MEM_SRAM;
    u8 hdr_buf[SAVE_HEADER_SIZE];
    sram_read_bytes(hdr_buf, sram, SAVE_HEADER_SIZE);

    SaveHeader hdr;
    deserialize_header(hdr_buf, &hdr);

    if (hdr.magic[0] != 'G' || hdr.magic[1] != 'B' ||
        hdr.magic[2] != 'A' || hdr.magic[3] != 'C') {
        return 0;
    }
    if (hdr.version != SAVE_VERSION) {
        return 0;
    }
    if (hdr.record_count > SAVE_MAX_RECORDS) {
        return 0;
    }

    /* Verify FNV-1a checksum over header bytes [0..51] and all stored records */
    u32 hash = 0x811C9DC5u;
    for (int i = 0; i < 52; i++) {
        hash ^= (u32)hdr_buf[i];
        hash *= 0x01000193u;
    }

    int rec_offset = SAVE_HEADER_SIZE;
    for (u16 r = 0; r < hdr.record_count; r++) {
        u8 rbuf[4];
        sram_read_bytes(rbuf, sram + rec_offset, 4);
        rec_offset += 4;
        for (int b = 0; b < 4; b++) {
            hash ^= (u32)rbuf[b];
            hash *= 0x01000193u;
        }
    }

    return (hash == hdr.checksum) ? 1 : 0;
}

int save_world_ex(
    u32 seed, const Player *p, u8 *custom_sram,
    u16 max_records, u16 *out_saved, u16 *out_dropped
) {
    volatile u8 *sram = custom_sram ? (volatile u8 *)custom_sram : (volatile u8 *)MEM_SRAM;
    if (max_records > SAVE_MAX_RECORDS) {
        max_records = SAVE_MAX_RECORDS;
    }

    u16 saved = 0;
    u16 dropped = 0;
    int rec_offset = SAVE_HEADER_SIZE;
    u8 base_col[WORLD_Y];

    /* For each of the 4,096 columns, regenerate into a 32-byte stack buffer and diff */
    for (int x = 0; x < WORLD_X; x++) {
        for (int z = 0; z < WORLD_Z; z++) {
            gen_column(seed, x, z, base_col);
            const u8 *live_col = g_world[x][z];
            for (int y = 0; y < WORLD_Y; y++) {
                if (live_col[y] != base_col[y]) {
                    if (saved < max_records) {
                        u32 rec = pack_diff_record(x, z, y, live_col[y]);
                        u8 rbuf[4];
                        write_u32_le(rbuf, rec);
                        sram_write_bytes(sram + rec_offset, rbuf, 4);
                        rec_offset += 4;
                        saved++;
                    } else {
                        dropped++;
                    }
                }
            }
        }
    }

    SaveHeader hdr;
    hdr.magic[0]       = 'G';
    hdr.magic[1]       = 'B';
    hdr.magic[2]       = 'A';
    hdr.magic[3]       = 'C';
    hdr.version        = SAVE_VERSION;
    hdr.flags          = (dropped > 0) ? 1 : 0;
    hdr.seed           = seed;
    hdr.player_x       = p->x;
    hdr.player_y       = p->y;
    hdr.player_z       = p->z;
    hdr.player_yaw     = p->yaw;
    hdr.player_horizon = p->horizon;
    hdr.selected_slot  = p->selected_slot;
    hdr.reserved0      = 0;
    for (int i = 0; i < HOTBAR_SLOTS; i++) {
        hdr.hotbar_type[i]  = p->hotbar_type[i];
        hdr.hotbar_count[i] = p->hotbar_count[i];
    }
    hdr.record_count   = saved;
    hdr.dropped_count  = dropped;
    hdr.checksum       = 0;

    u8 hdr_buf[SAVE_HEADER_SIZE];
    serialize_header(hdr_buf, &hdr);

    /* Compute checksum over header [0..51] + saved records */
    u32 hash = 0x811C9DC5u;
    for (int i = 0; i < 52; i++) {
        hash ^= (u32)hdr_buf[i];
        hash *= 0x01000193u;
    }

    rec_offset = SAVE_HEADER_SIZE;
    for (u16 r = 0; r < saved; r++) {
        u8 rbuf[4];
        sram_read_bytes(rbuf, sram + rec_offset, 4);
        rec_offset += 4;
        for (int b = 0; b < 4; b++) {
            hash ^= (u32)rbuf[b];
            hash *= 0x01000193u;
        }
    }

    hdr.checksum = hash;
    write_u32_le(&hdr_buf[52], hdr.checksum);
    sram_write_bytes(sram, hdr_buf, SAVE_HEADER_SIZE);

    if (out_saved)   *out_saved   = saved;
    if (out_dropped) *out_dropped = dropped;
    return (dropped == 0) ? 1 : 0;
}

int save_world(u32 seed, const Player *p, u16 *out_saved, u16 *out_dropped) {
    return save_world_ex(seed, p, NULL, SAVE_MAX_RECORDS, out_saved, out_dropped);
}

int load_world_ex(u32 *out_seed, Player *p, const u8 *custom_sram) {
    if (!save_has_valid_sram(custom_sram)) {
        return 0;
    }

    const volatile u8 *sram = custom_sram ? (const volatile u8 *)custom_sram : (const volatile u8 *)MEM_SRAM;
    u8 hdr_buf[SAVE_HEADER_SIZE];
    sram_read_bytes(hdr_buf, sram, SAVE_HEADER_SIZE);

    SaveHeader hdr;
    deserialize_header(hdr_buf, &hdr);

    /* 1. Regenerate base world from seed */
    world_generate(hdr.seed);
    if (out_seed) *out_seed = hdr.seed;

    /* 2. Apply all diff records */
    int rec_offset = SAVE_HEADER_SIZE;
    for (u16 r = 0; r < hdr.record_count; r++) {
        u8 rbuf[4];
        sram_read_bytes(rbuf, sram + rec_offset, 4);
        rec_offset += 4;
        u32 rec = read_u32_le(rbuf);
        int x, z, y;
        u8 blk;
        unpack_diff_record(rec, &x, &z, &y, &blk);
        if (x >= 0 && x < WORLD_X && z >= 0 && z < WORLD_Z && y >= 0 && y < WORLD_Y && blk < BLK_COUNT) {
            g_world[x][z][y] = blk;
        }
    }

    /* 3. Restore player state */
    if (p) {
        p->x             = hdr.player_x;
        p->y             = hdr.player_y;
        p->z             = hdr.player_z;
        p->vx            = 0;
        p->vy            = 0;
        p->vz            = 0;
        p->yaw           = hdr.player_yaw & ANGLE_MASK;
        p->horizon       = hdr.player_horizon;
        if (p->horizon < HORIZON_MIN) p->horizon = HORIZON_MIN;
        if (p->horizon > HORIZON_MAX) p->horizon = HORIZON_MAX;
        p->on_ground     = 0;
        p->selected_slot = (hdr.selected_slot < HOTBAR_SLOTS) ? hdr.selected_slot : 0;
        for (int i = 0; i < HOTBAR_SLOTS; i++) {
            p->hotbar_type[i]  = (hdr.hotbar_type[i] < BLK_COUNT) ? hdr.hotbar_type[i] : BLK_AIR;
            p->hotbar_count[i] = (hdr.hotbar_count[i] <= STACK_MAX) ? hdr.hotbar_count[i] : 0;
            if (p->hotbar_count[i] == 0) p->hotbar_type[i] = BLK_AIR;
        }
    }

    return 1;
}

int load_world(u32 *out_seed, Player *p) {
    return load_world_ex(out_seed, p, NULL);
}
