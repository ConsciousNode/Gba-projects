# GBA-Craft Feature Truth Table

| Feature | File + Function | Status | Notes |
|---|---|---|---|
| **64×64×32 voxel grid in EWRAM (`128 KB`)** | `source/world.c` (`g_world`, `world_get`, `world_set`) | **Working** | `EWRAM_BSS u8 g_world[64][64][32]`; finite bedrock walls at edges and `Y=0`. |
| **8 block types + AIR** | `include/gba_craft.h` (`enum BlockType`), `source/tables.c` (`palette_init`) | **Working** | `AIR, GRASS, DIRT, STONE, SAND, WOOD, LEAVES, PLANKS, BEDROCK`. |
| **Pure per-column world generator** | `source/world.c` (`gen_column`, `world_generate`) | **Working** | Pure function of `(seed, x, z)`; 2-octave integer value noise (`Y=8..22`), sand at `Y<=10`. Verified by Self-Tests #1 and #2. |
| **3D value-noise caves** | `source/world.c` (`value_noise_3d_cave`, `gen_column`) | **Working** | Carves walkable air pockets between `Y=2` and `surface - 4`. |
| **Neighbor-deterministic trees** | `source/world.c` (`column_has_tree`, `gen_column`) | **Working** | `~1 in 40` grass columns gets a 4–5 block `WOOD` trunk and `3×3×2` `LEAVES` crown reproduced purely by neighboring columns. |
| **Mode 4 120×80 column-span raycaster in IWRAM** | `source/render.iwram.c` (`render_frame`, `render_blit_to_vram`) | **Working** | Compiled as ARM in IWRAM; 2D Amanatides–Woo DDA out to 16 blocks, front-to-back `[ytop, ybot]` clipping, top/bottom/side faces, 2× BG2 affine stretch. Verified by Self-Test #9. |
| **Reciprocal & trig lookup tables (no inner-loop division)** | `source/tables.c` (`tables_init`) | **Working** | `g_sin_lut`, `g_recip_lut`, `g_delta_lut`, and `g_fisheye_lut` stored in `IWRAM_DATA`. Zero floats/doubles across entire project. |
| **256-color palette with 16 shades per block + fog fade** | `source/tables.c` (`palette_init`) | **Working** | 8 blocks × 16 shades (`128` entries); X-side darker than Z-side; last 4 blocks blend into horizon sky/fog color. |
| **Y-shearing pitch (`40 ± 32`)** | `source/player.c` (`player_update_input`), `source/render.iwram.c` (`render_frame`) | **Working** | `L + D-pad Up/Down` slides horizon between rows `8` and `72`. |
| **3D DDA targeting ray (`4` blocks reach)** | `source/target.iwram.c` (`target_cast_ray`) | **Working** | ARM in IWRAM; steps through 3D voxels from eye along yaw + horizon pitch slope, returning hit voxel and face normal. |
| **Targeted block highlight** | `source/render.iwram.c` (`draw_col_span`, `render_frame`) | **Working** | Targeted voxel is brightened to shade `0` and outlined on upper/lower span edges with `PAL_HIGHLIGHT`. |
| **Block breaking & drops** | `source/edit.c` (`edit_update`, `edit_break_targeted_block`) | **Working** | Per-type break timers (`0.3s / 0.6s / 1.0s`), grass drops dirt, leaves drop nothing 1-in-4 times, bedrock unbreakable. Verified by Self-Test #3. |
| **Block placement & player AABB refusal** | `source/edit.c` (`edit_place_targeted_block`) | **Working** | Places along hit normal and refuses if target cell overlaps player's `0.6 × 1.8` box. Verified by Self-Tests #4 and #5. |
| **Crafting (`1 WOOD -> 4 PLANKS`)** | `source/edit.c` (`edit_craft_planks`, `edit_update`) | **Working** | `SELECT held + A` converts `1 WOOD` anywhere in hotbar into `4 PLANKS`. |
| **Player AABB physics & wall sliding** | `source/player.c` (`player_step_physics`, `player_check_aabb_world`) | **Working** | `0.6 × 1.8` AABB with eye at `1.62`, separate X/Z/Y axis resolution, `1.25`-block jump, no auto step-up. Verified by Self-Test #6. |
| **Seed + diff SRAM persistence (`32 KB`)** | `source/save.c` (`save_world_ex`, `load_world_ex`, `save_has_valid_sram`) | **Working** | Includes `"SRAM_V113"` string, 8-bit SRAM loops in IWRAM, stack `gen_column` diffing (`8,176` records max), FNV-1a checksum, and overflow warning. Verified by Self-Tests #7 and #8. |
| **OBJ Sprite HUD in tiles `512..1023`** | `source/hud.c` (`hud_init`, `hud_update`) | **Working** | Procedural 4bpp tiles starting at tile `512`: crosshair, 9-slot hotbar with icons & stack counts, break bar, FPS & coordinates. |
| **Menus (Title, Pause, Seed Entry, Help)** | `source/menu.c` (`menu_update_title`, `menu_update_pause`, `menu_update_seed_entry`, `menu_update_help`) | **Working** | Mode 4 bitmap text menus; `Continue` enabled only when valid SRAM save exists. |
| **Built-in 9-test acceptance suite** | `source/selftest.c` (`selftest_run_all`) | **Working** | Triggered by holding `SELECT` at title screen; uses separate EWRAM buffer so player SRAM is untouched. All `9/9` pass. |
