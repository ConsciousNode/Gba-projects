# GBA-Craft Architecture

## 1. Renderer Overview: Column-Span Voxel Raycaster

`GBA-Craft` combines a **Wolfenstein-style 2D Amanatides–Woo DDA grid traversal** in the horizontal `(X, Z)` plane with a **Voxel Space / Voxlap-inspired vertical column-span renderer** along the `Y` axis.

### Display Pipeline

- **Video Mode**: GBA **Mode 4** (8-bit paletted `240 × 160` double-buffered bitmap on `BG2`) + **1D OBJ Sprites** (`DCNT_MODE4 | DCNT_BG2 | DCNT_OBJ | DCNT_OBJ_1D`).
- **Resolution & Hardware Affine Scaling**: The 3D viewport is rendered at **`120 × 80`** into a 32-bit aligned scratch framebuffer (`g_framebuf[80][120]`, `9,600` bytes) in zero-waitstate **IWRAM**. During `STATE_PLAYING`, the GBA hardware stretches the top-left `120 × 80` region of the active Mode 4 page by `2×` in both axes to fill the `240 × 160` display via the `BG2` affine matrix (`REG_BG2PA = 0x0080`, `REG_BG2PD = 0x0080`), cutting pixel fill-rate cost by `4×`.
- **Why Render in IWRAM First**: GBA VRAM ignores 8-bit `STRB` writes (an 8-bit write duplicates the byte across both halves of a 16-bit halfword). Rendering vertical column spans into `g_framebuf` in 32-bit IWRAM allows single-cycle `STRB` writes down each column, followed by a contiguous 32-bit word burst (`render_blit_to_vram`) into the Mode 4 VRAM back page before `vid_flip()`.

### ASCII Diagram: 2D DDA + Vertical Voxel Span Projection

```
  Top-Down 2D DDA (X/Z Grid, max 16 steps)           Vertical Slice Along One Ray (Column c)
  ========================================           =======================================

       +Z (South)                                    Y ^
        |   .       .   |   .                          |          [Overhang / Cave Ceiling]
        |       +-------+                              |          +-------+  <- wy_run_top
        |       |Cell k |  <- t_exit                   |          | STONE |
        |       | (x,z) |                              |          +-------+  <- wy_run_bot (Bottom Face)
        |       +-------+  <- t_enter                  |         /       /
        |      /        |                              |   Eye (ox, oy, oz) - - - - - - - - Horizon (row 40 +- 32)
        |     / Ray c   |                              |         \       \
        |    /          |                              |          \ Top   \  <- [sy_top_ex .. run_sy_top_en]
        |   * Eye       |                              |           +-------+ <- wy_run_top (GRASS)
        +-------------------> +X (East)                |           |Side   | <- [run_sy_top_en .. run_sy_bot_en]
                                                       |           |Face   |    at t_enter
                                                       +-----------+-------+-----------------> Distance t
                                                                t_enter  t_exit

  Screen Column c (Rows 0..79, Front-to-Back Occlusion Window):
  +-------------------------------------------------------------------+
  | 0      .. ytop-1 : Already occluded by near ceiling / overhang    |
  | ytop   .. ybot   : Active unclipped window [ytop, ybot]           |
  |                    - Side face projected at t_enter               |
  |                    - Top face projected across [t_enter, t_exit]  |
  |                    - Bottom face projected across [t_enter,t_exit]|
  |                    - Tightens ytop downward / ybot upward         |
  |                    - Early-outs ray as soon as ytop > ybot        |
  | ybot+1 .. 79     : Already occluded by near floor / terrain       |
  +-------------------------------------------------------------------+
```

### Projection & Zero-Division Inner Loop

Because the GBA's `ARM7TDMI` has no hardware divide instruction, all perspective projection uses a precomputed **1,024-entry reciprocal table** (`g_recip_lut` in IWRAM) indexed by perpendicular fixed-point distance (`idx = p_dist >> 10`, giving `1/64` block resolution out to `16.0` blocks):

```c
s32 dy_s = (world_y - eye_y) >> 8;               /* 24.8 signed height delta */
int screen_y = horizon - ((dy_s * inv_dist) >> 16);
```

- **Pitch (Looking Up/Down)**: Implemented via **y-shearing** (`horizon` slides across rows `40 ± 32`, i.e., `8..72`).
- **3D DDA Crosshair Sync**: In `source/target.iwram.c`, the vertical ray component is computed as `dir_y = ((horizon - 40) * FIX_ONE) / PROJ_CONST`, which matches the y-sheared projection at `screen_y = 40` to the exact pixel.

---

## 2. Fixed-Point Format (`16.16`)

Every coordinate, velocity, ray vector, and distance in `GBA-Craft` uses signed 32-bit **`16.16` fixed-point** (`typedef s32 fix16;`). There are **zero `float` or `double` types or literals** anywhere in the codebase.

| Quantity | Representation | Example Value |
|---|---|---|
| `1.0` block (`FIX_ONE`) | `1 << 16` | `65536` |
| `0.5` block (`FIX_HALF`) | `1 << 15` | `32768` |
| Player half-width (`0.30` blocks) | `PLAYER_HALF_W` | `19661` |
| Player height (`1.80` blocks) | `PLAYER_HEIGHT` | `117965` |
| Player eye height (`1.62` blocks) | `PLAYER_EYE_H` | `106168` |
| Full circle angle (`ANGLE_FULL`) | `1024` units (`10-bit`) | `0..1023` (`90° = 256`) |
| Fast multiply (`FIX_MUL_FAST(a,b)`) | `((a >> 8) * (b >> 8))` | Single-cycle 32-bit ARM `MUL` |

---

## 3. Memory Budget Table

| Memory Region | Hardware Limit | GBA-Craft Allocation | Used | Remaining Headroom |
|---|---:|---|---:|---:|
| **IWRAM** (`0x03000000`, 32-bit bus, 0 waitstates) | `32,768 B` (`32 KB`) | `g_framebuf[80][120]` (`9,600 B`)<br>`g_sin_lut[1024]` (`4,096 B`)<br>`g_recip_lut[1024]` (`4,096 B`)<br>`g_delta_lut[257]` (`1,028 B`)<br>`g_fisheye_lut[120]` (`480 B`)<br>`render.iwram.c` + `target.iwram.c` + SRAM loops (`~3,200 B`)<br>Scalar globals & OAM shadow (`~1,200 B`) | **`~23,700 B`** (`~23.1 KB`) | **`~9,068 B`** (`~8.9 KB` for IRQ/Supervisor/User stack) |
| **EWRAM** (`0x02000000`, 16-bit bus) | `262,144 B` (`256 KB`) | `g_world[64][64][32]` (`131,072 B` = `128 KB`)<br>`s_test_sram[32768]` (`32,768 B` = `32 KB`, self-test scratch) | **`163,840 B`** (`160.0 KB`) | **`98,304 B`** (`96.0 KB` free) |
| **VRAM** (`0x06000000`, 16-bit bus) | `98,304 B` (`96 KB`) | Mode 4 Page 0 `0x06000000` (`38,400 B`)<br>Mode 4 Page 1 `0x0600A000` (`38,400 B`)<br>Upper OBJ Tiles `512..607` at `0x06014000` (`3,072 B`) | **`79,872 B`** (`78.0 KB`) | **`18,432 B`** (`18.0 KB` free in upper OBJ tiles `608..1023`) |
| **OAM** (`0x07000000`, 128 sprites max) | `1,024 B` (`128 OBJs`) | Crosshair (`1 OBJ`), Break bar (`2 OBJs`), 9-slot Hotbar (`27..36 OBJs`), FPS/Coords (`19 OBJs`), Status text (`<=28 OBJs`) | **`~86 OBJs`** (`688 B`) | **`42 OBJs`** (`336 B` free) |
| **SRAM** (`0x0E000000`, 8-bit bus) | `32,768 B` (`32 KB`) | `SaveHeader` (`64 B`) + up to `8,176` packed 4-byte `u32` diff records (`32,704 B`) | **`32,768 B`** (`32.0 KB`) | Capacity for **`8,176` voxel edits** before graceful overflow notice |

---

## 4. Frame Timeline (`STATE_PLAYING`)

Each rendered frame executes the following deterministic sequence:

```
VBlank IRQ fires (REG_VCOUNT == 160)
  |
  +-- 1. VBlankIntrWait() wakes main loop; key_poll() samples REG_KEYINPUT
  +-- 2. player_update_input() & player_step_physics() (~1,500 cycles)
  |      - Resolves X, Z, Y AABB (0.6 x 1.8) against g_world separately
  +-- 3. target_cast_ray() in IWRAM (~900 cycles)
  |      - 3D Amanatides-Woo DDA out to 4 blocks
  +-- 4. edit_update() (~400 cycles)
  |      - Break timer, block drop, B-button placement, SELECT+A plank crafting
  +-- 5. render_frame() in IWRAM (~750,000 - 980,000 cycles, ~600-800 scanlines)
  |      - 32-bit zero fill of g_framebuf (2,400 words in IWRAM)
  |      - 120 columns x up to 16 DDA steps with front-to-back [ytop, ybot] clipping
  |      - Sky gradient & ground fog fill on unoccluded pixels
  +-- 6. render_blit_to_vram() in IWRAM (~35,000 cycles)
  |      - Copies 80 rows x 30 u32 words to Mode 4 back page
  +-- 7. hud_update() & vid_flip() (~4,000 cycles)
         - Updates OAM shadow buffer (tiles >= 512) and flips DCNT_PAGE
```

At `16.78 MHz` (`280,896` CPU cycles per `60 Hz` VBlank, or `1,232` cycles per scanline), a typical scene completes in `3..4` VBlank intervals, yielding **`15–20 FPS`** at the full 16-block view distance.
