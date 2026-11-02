/*
 * GBA-Craft — Master Header
 * Copyright (c) 2026 Khamerron Kizer / ConsciousNode SoftWorks
 * SPDX-License-Identifier: MIT
 */

#ifndef GBA_CRAFT_H
#define GBA_CRAFT_H

#include <tonc.h>

/* --------------------------------------------------------------------------
 * Fixed-Point Format: 16.16 signed (s32)
 * No floating-point types or literals anywhere in the engine.
 * -------------------------------------------------------------------------- */
typedef s32 fix16;

#define FIX_SHIFT       16
#define FIX_ONE         (1 << FIX_SHIFT)        /* 65536 = 1.0 */
#define FIX_HALF        (1 << (FIX_SHIFT - 1))  /* 32768 = 0.5 */
#define FIX_MASK        (FIX_ONE - 1)

#define INT_TO_FIX(x)   ((fix16)((s32)(x) << FIX_SHIFT))
#define FIX_TO_INT(x)   ((int)((x) >> FIX_SHIFT))
#define FIX_FRAC(x)     ((x) & FIX_MASK)

/* Fast 16.16 multiply via 8.8 intermediate (single cycle ARM MUL, no 64-bit) */
#define FIX_MUL_FAST(a, b)  ((((a) >> 8) * ((b) >> 8)))

/* --------------------------------------------------------------------------
 * Angle System: 1024 units per full circle (0..1023)
 * -------------------------------------------------------------------------- */
#define ANGLE_BITS      10
#define ANGLE_FULL      (1 << ANGLE_BITS)       /* 1024 */
#define ANGLE_MASK      (ANGLE_FULL - 1)        /* 1023 */
#define ANGLE_90        256
#define ANGLE_180       512
#define ANGLE_270       768

/* --------------------------------------------------------------------------
 * World Dimensions & Block Types
 * 64 x 64 x 32 blocks (X x Z x Y, Y up) = 128 KB in EWRAM
 * -------------------------------------------------------------------------- */
#define WORLD_X         64
#define WORLD_Z         64
#define WORLD_Y         32

#define DEFAULT_SEED    0x47424143u             /* "GBAC" */

enum BlockType {
    BLK_AIR     = 0,
    BLK_GRASS   = 1,
    BLK_DIRT    = 2,
    BLK_STONE   = 3,
    BLK_SAND    = 4,
    BLK_WOOD    = 5,
    BLK_LEAVES  = 6,
    BLK_PLANKS  = 7,
    BLK_BEDROCK = 8,
    BLK_COUNT   = 9
};

/* --------------------------------------------------------------------------
 * Display & Renderer Constants (Mode 4, 120x80 scaled 2x via BG2 affine)
 * -------------------------------------------------------------------------- */
#define SCREEN_W        240
#define SCREEN_H        160
#define RENDER_W        120
#define RENDER_H        80

#define HORIZON_BASE    40
#define HORIZON_MIN     8
#define HORIZON_MAX     72

#define MAX_RAY_STEPS   16
#define MAX_VIEW_BLOCKS 16
#define PROJ_CONST      100

/* Reciprocal table size: distance 0..16 blocks at 1/64 block resolution */
#define RECIP_LUT_SIZE  1024
/* Ray delta-distance table: |dir| in 0..1.0 at 1/256 resolution */
#define DELTA_LUT_SIZE  257

/* Palette Layout (256 entries) */
#define PAL_SKY_TOP     1
#define PAL_SKY_MID     2
#define PAL_SKY_HORIZON 3
#define PAL_FOG         4
#define PAL_UI_BG       5
#define PAL_UI_TEXT     6
#define PAL_UI_ACCENT   7
#define PAL_UI_WARN     8
#define PAL_UI_OK       9
#define PAL_UI_FAIL     10
#define PAL_HIGHLIGHT   11
#define PAL_CROSSHAIR   12
#define PAL_BLOCK_BASE  16   /* 16..143: 8 block types * 16 shades */

#define BLOCK_PAL(type, shade)  ((u8)(((type) << 4) | ((shade) & 15)))

/* --------------------------------------------------------------------------
 * Player & Physics Constants (16.16 fixed-point)
 * Collision box: 0.6 wide (half-width 0.3) x 1.8 tall, eye at 1.62
 * -------------------------------------------------------------------------- */
#define PLAYER_HALF_W   19661   /* 0.30 * 65536 */
#define PLAYER_HEIGHT   117965  /* 1.80 * 65536 */
#define PLAYER_EYE_H    106168  /* 1.62 * 65536 */

/* Tuned for ~18-20 Hz simulation tick:
 * Walk speed ~4.3 blocks/s -> ~0.235 blocks/tick (15400 in 16.16)
 * Jump apex = 1.25 blocks (82944 in 16.16 over 9 ticks) */
#define PLAYER_WALK_SPD 15400
#define PLAYER_GRAVITY  2048
#define PLAYER_JUMP_VEL 19456
#define PLAYER_MAX_FALL (-32768)
#define PLAYER_TURN_SPD 18
#define PLAYER_LOOK_SPD 2

#define HOTBAR_SLOTS    9
#define STACK_MAX       64

/* --------------------------------------------------------------------------
 * Targeting & Editing Constants
 * -------------------------------------------------------------------------- */
#define TARGET_REACH_FIX (4 * FIX_ONE)

/* Break durations in hardware VBlanks (60 Hz):
 * dirt/sand/grass/leaves = 18 (~0.3s), wood/planks = 36 (~0.6s), stone = 60 (1.0s) */
#define BREAK_TICKS_SOFT 18
#define BREAK_TICKS_WOOD 36
#define BREAK_TICKS_ROCK 60

/* --------------------------------------------------------------------------
 * Save Format Constants (32 KB SRAM)
 * -------------------------------------------------------------------------- */
#define SAVE_SRAM_SIZE   32768
#define SAVE_HEADER_SIZE 64
#define SAVE_MAX_RECORDS ((SAVE_SRAM_SIZE - SAVE_HEADER_SIZE) / 4) /* 8176 */
#define SAVE_VERSION     1

/* --------------------------------------------------------------------------
 * Data Structures
 * -------------------------------------------------------------------------- */
typedef struct {
    fix16 x, y, z;          /* Feet position in world coordinates (16.16) */
    fix16 vx, vy, vz;       /* Velocity (16.16) */
    s16   yaw;              /* 0..1023 */
    s16   horizon;          /* 8..72 (default 40) */
    u8    on_ground;        /* 1 if standing on solid block */
    u8    selected_slot;    /* 0..8 */
    u8    hotbar_type[HOTBAR_SLOTS];
    u8    hotbar_count[HOTBAR_SLOTS];
} Player;

typedef struct {
    u8  hit;                /* 1 if a block is targeted within 4 blocks */
    s16 x, y, z;            /* Targeted voxel coordinates */
    s8  nx, ny, nz;         /* Entered face normal (-1, 0, +1) */
    u8  block;              /* BlockType at (x, y, z) */
} TargetHit;

typedef struct {
    s16   target_x, target_y, target_z;
    u16   progress_ticks;
    u16   required_ticks;
    u8    active;
    u8    select_craft_used;
} EditState;

enum GameState {
    STATE_TITLE = 0,
    STATE_PLAYING,
    STATE_PAUSED,
    STATE_SEED_ENTRY,
    STATE_HELP,
    STATE_SELFTEST
};

/* --------------------------------------------------------------------------
 * Global Variables
 * -------------------------------------------------------------------------- */
extern EWRAM_BSS u8 g_world[WORLD_X][WORLD_Z][WORLD_Y];
extern IWRAM_DATA u8 g_framebuf[RENDER_H][RENDER_W];

extern IWRAM_DATA s32 g_sin_lut[ANGLE_FULL];
extern IWRAM_DATA s32 g_recip_lut[RECIP_LUT_SIZE];
extern IWRAM_DATA s32 g_delta_lut[DELTA_LUT_SIZE];
extern IWRAM_DATA s32 g_fisheye_lut[RENDER_W];

extern u32        g_world_seed;
extern Player     g_player;
extern TargetHit  g_target;
extern EditState  g_edit;
extern volatile u32 g_vblank_count;
extern u32        g_frame_counter;
extern u8         g_show_fps;
extern u16        g_last_fps;
extern u16        g_last_scanlines;
extern char       g_status_msg[44];
extern u16        g_status_timer;

static inline s32 lut_sin(int ang) {
    return g_sin_lut[ang & ANGLE_MASK];
}

static inline s32 lut_cos(int ang) {
    return g_sin_lut[(ang + ANGLE_90) & ANGLE_MASK];
}

/* --------------------------------------------------------------------------
 * Function Prototypes
 * -------------------------------------------------------------------------- */

/* tables.c */
void tables_init(void);
void palette_init(void);
const char *block_name(u8 type);

/* world.c */
u32  world_hash2d(u32 seed, int x, int z);
u32  world_hash3d(u32 seed, int x, int y, int z);
void gen_column(u32 seed, int x, int z, u8 out[WORLD_Y]);
void world_generate(u32 seed);
u8   world_get(int x, int z, int y);
void world_set(int x, int z, int y, u8 block);
int  world_surface_y(int x, int z);
u32  world_checksum(void);

/* render.iwram.c */
IWRAM_CODE void render_frame(const Player *p, const TargetHit *tgt);
IWRAM_CODE void render_blit_to_vram(u16 *vram_page);

/* target.iwram.c */
IWRAM_CODE void target_cast_ray(const Player *p, TargetHit *out_hit);

/* player.c */
void player_init(Player *p, u32 seed);
int  player_aabb_intersects_block(const Player *p, int bx, int by, int bz);
int  player_check_aabb_world(fix16 px, fix16 py, fix16 pz);
void player_step_physics(Player *p, fix16 wish_vx, fix16 wish_vz, int jump_req);
void player_update_input(Player *p, u32 vblanks_elapsed);

/* edit.c */
void edit_init(EditState *es);
u16  edit_break_ticks_for_block(u8 block);
int  hotbar_add_block(Player *p, u8 block, int count);
int  edit_break_targeted_block(Player *p, const TargetHit *tgt, u32 rng_val);
int  edit_place_targeted_block(Player *p, const TargetHit *tgt);
int  edit_craft_planks(Player *p);
void edit_update(Player *p, const TargetHit *tgt, EditState *es, u32 vblanks_elapsed);

/* save.c */
void save_init(void);
int  save_has_valid_sram(const u8 *custom_sram);
int  save_world_ex(u32 seed, const Player *p, u8 *custom_sram, u16 max_records, u16 *out_saved, u16 *out_dropped);
int  save_world(u32 seed, const Player *p, u16 *out_saved, u16 *out_dropped);
int  load_world_ex(u32 *out_seed, Player *p, const u8 *custom_sram);
int  load_world(u32 *out_seed, Player *p);

/* hud.c */
void hud_init(void);
void hud_update(const Player *p, const EditState *es);
void hud_hide_all(void);
void hud_draw_bitmap_char(u16 *page, int x, int y, char c, u8 fg_pal, u8 bg_pal);
void hud_draw_bitmap_text(u16 *page, int x, int y, const char *str, u8 fg_pal, u8 bg_pal);
void hud_fill_bitmap_rect(u16 *page, int x, int y, int w, int h, u8 pal_idx);
void hud_set_status(const char *msg, u16 duration_frames);

/* menu.c */
void menu_init(void);
int  menu_update_title(u32 *io_seed, Player *p);
int  menu_update_pause(u32 *io_seed, Player *p);
int  menu_update_seed_entry(u32 *io_seed, Player *p, int return_state);
int  menu_update_help(int return_state);

/* selftest.c */
int  selftest_run_all(int draw_to_screen);

#endif /* GBA_CRAFT_H */
