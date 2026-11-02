/*
 * GBA-Craft — Block Breaking, Placing, Hotbar & Crafting
 * Copyright (c) 2026 Khamerron Kizer / ConsciousNode SoftWorks
 * SPDX-License-Identifier: MIT
 */

#include "gba_craft.h"

TargetHit g_target;
EditState g_edit;

void edit_init(EditState *es) {
    es->target_x = -1;
    es->target_y = -1;
    es->target_z = -1;
    es->progress_ticks = 0;
    es->required_ticks = 0;
    es->active = 0;
    es->select_craft_used = 0;
}

u16 edit_break_ticks_for_block(u8 block) {
    switch (block) {
        case BLK_GRASS:
        case BLK_DIRT:
        case BLK_SAND:
        case BLK_LEAVES:
            return BREAK_TICKS_SOFT; /* 18 VBlanks = 0.3s */
        case BLK_WOOD:
        case BLK_PLANKS:
            return BREAK_TICKS_WOOD; /* 36 VBlanks = 0.6s */
        case BLK_STONE:
            return BREAK_TICKS_ROCK; /* 60 VBlanks = 1.0s */
        case BLK_BEDROCK:
        default:
            return 0; /* Unbreakable */
    }
}

/* Add `count` items of `block` to the 9-slot hotbar (stack limit 64). Returns number added. */
int hotbar_add_block(Player *p, u8 block, int count) {
    if (block == BLK_AIR || count <= 0) return 0;
    int added = 0;

    /* First stack into existing matching slots */
    for (int i = 0; i < HOTBAR_SLOTS && count > 0; i++) {
        if (p->hotbar_type[i] == block && p->hotbar_count[i] < STACK_MAX) {
            int room = STACK_MAX - p->hotbar_count[i];
            int take = (count < room) ? count : room;
            p->hotbar_count[i] = (u8)(p->hotbar_count[i] + take);
            count -= take;
            added += take;
        }
    }

    /* Then place remainder into empty slots */
    for (int i = 0; i < HOTBAR_SLOTS && count > 0; i++) {
        if (p->hotbar_count[i] == 0 || p->hotbar_type[i] == BLK_AIR) {
            int take = (count < STACK_MAX) ? count : STACK_MAX;
            p->hotbar_type[i]  = block;
            p->hotbar_count[i] = (u8)take;
            count -= take;
            added += take;
        }
    }

    return added;
}

/* Break targeted block immediately (called when break timer finishes, or directly by self-test #3) */
int edit_break_targeted_block(Player *p, const TargetHit *tgt, u32 rng_val) {
    if (!tgt || !tgt->hit) return 0;
    int bx = tgt->x;
    int by = tgt->y;
    int bz = tgt->z;

    if (bx <= 0 || bx >= WORLD_X - 1 || bz <= 0 || bz >= WORLD_Z - 1 || by <= 0 || by >= WORLD_Y) {
        return 0;
    }

    u8 blk = g_world[bx][bz][by];
    if (blk == BLK_AIR || blk == BLK_BEDROCK) {
        return 0;
    }

    g_world[bx][bz][by] = BLK_AIR;

    /* Determine drop: grass drops dirt; leaves have a 1-in-4 chance to drop nothing */
    u8 drop = blk;
    if (blk == BLK_GRASS) {
        drop = BLK_DIRT;
    } else if (blk == BLK_LEAVES) {
        if ((rng_val & 3u) == 0u) {
            drop = BLK_AIR;
        }
    }

    if (drop != BLK_AIR) {
        hotbar_add_block(p, drop, 1);
    }
    return 1;
}

/* Place selected hotbar block into the cell adjacent along the hit normal (B button / self-test #4 & #5) */
int edit_place_targeted_block(Player *p, const TargetHit *tgt) {
    if (!tgt || !tgt->hit) return 0;

    int slot = p->selected_slot;
    if (slot < 0 || slot >= HOTBAR_SLOTS) return 0;

    u8 blk = p->hotbar_type[slot];
    u8 cnt = p->hotbar_count[slot];
    if (blk == BLK_AIR || cnt == 0) return 0;

    int px = tgt->x + tgt->nx;
    int py = tgt->y + tgt->ny;
    int pz = tgt->z + tgt->nz;

    /* Keep within editable world interior */
    if (px <= 0 || px >= WORLD_X - 1 || pz <= 0 || pz >= WORLD_Z - 1 || py <= 0 || py >= WORLD_Y) {
        return 0;
    }

    if (g_world[px][pz][py] != BLK_AIR) {
        return 0;
    }

    /* Refuse placement if the target voxel intersects the player's 0.6 x 1.8 collision box */
    if (player_aabb_intersects_block(p, px, py, pz)) {
        return 0;
    }

    g_world[px][pz][py] = blk;
    p->hotbar_count[slot] = (u8)(cnt - 1);
    if (p->hotbar_count[slot] == 0) {
        p->hotbar_type[slot] = BLK_AIR;
    }
    return 1;
}

/* One recipe: when hotbar has >= 1 WOOD, holding SELECT + pressing A converts 1 WOOD -> 4 PLANKS */
int edit_craft_planks(Player *p) {
    int wood_slot = -1;
    /* Check selected slot first, then scan all slots */
    if (p->hotbar_type[p->selected_slot] == BLK_WOOD && p->hotbar_count[p->selected_slot] >= 1) {
        wood_slot = p->selected_slot;
    } else {
        for (int i = 0; i < HOTBAR_SLOTS; i++) {
            if (p->hotbar_type[i] == BLK_WOOD && p->hotbar_count[i] >= 1) {
                wood_slot = i;
                break;
            }
        }
    }

    if (wood_slot < 0) {
        hud_set_status("NEED WOOD TO CRAFT PLANKS", 45);
        return 0;
    }

    /* Check if hotbar has space for 4 PLANKS (either after freeing wood_slot if count==1, or in existing/empty slots) */
    int room = 0;
    for (int i = 0; i < HOTBAR_SLOTS; i++) {
        if (p->hotbar_type[i] == BLK_PLANKS && p->hotbar_count[i] < STACK_MAX) {
            room += (STACK_MAX - p->hotbar_count[i]);
        } else if (p->hotbar_count[i] == 0 || p->hotbar_type[i] == BLK_AIR) {
            room += STACK_MAX;
        }
    }
    if (p->hotbar_count[wood_slot] == 1) {
        room += STACK_MAX;
    }
    if (room < 4) {
        hud_set_status("HOTBAR FULL", 45);
        return 0;
    }

    p->hotbar_count[wood_slot]--;
    if (p->hotbar_count[wood_slot] == 0) {
        p->hotbar_type[wood_slot] = BLK_AIR;
    }

    hotbar_add_block(p, BLK_PLANKS, 4);
    hud_set_status("CRAFTED 4 PLANKS", 45);
    return 1;
}

void edit_update(Player *p, const TargetHit *tgt, EditState *es, u32 vblanks_elapsed) {
    if (vblanks_elapsed == 0) vblanks_elapsed = 1;
    if (vblanks_elapsed > 6)  vblanks_elapsed = 6;

    /* Crafting: SELECT held + A pressed */
    if (key_is_down(KEY_SELECT) && key_hit(KEY_A)) {
        edit_craft_planks(p);
        es->select_craft_used = 1;
        es->active = 0;
        es->progress_ticks = 0;
        return;
    }

    /* Hotbar slot cycling: SELECT released (if not used for crafting) or SELECT hit without A */
    if (key_hit(KEY_SELECT) && !key_is_down(KEY_A)) {
        es->select_craft_used = 0;
    }
    if (key_released(KEY_SELECT)) {
        if (!es->select_craft_used) {
            p->selected_slot = (u8)((p->selected_slot + 1) % HOTBAR_SLOTS);
        }
        es->select_craft_used = 0;
    }

    /* Block placement: B button pressed */
    if (key_hit(KEY_B)) {
        edit_place_targeted_block(p, tgt);
    }

    /* Block breaking: hold A (when SELECT is not held) */
    if (key_is_down(KEY_A) && !key_is_down(KEY_SELECT) && tgt && tgt->hit) {
        u16 req = edit_break_ticks_for_block(tgt->block);
        if (req == 0) {
            /* Bedrock: unbreakable */
            es->active = 0;
            es->progress_ticks = 0;
            return;
        }

        if (es->active &&
            es->target_x == tgt->x &&
            es->target_y == tgt->y &&
            es->target_z == tgt->z) {
            es->progress_ticks = (u16)(es->progress_ticks + vblanks_elapsed);
            if (es->progress_ticks >= req) {
                edit_break_targeted_block(p, tgt, g_frame_counter ^ (u32)(tgt->x * 31 + tgt->z * 17 + tgt->y));
                es->active = 0;
                es->progress_ticks = 0;
            }
        } else {
            es->active = 1;
            es->target_x = tgt->x;
            es->target_y = tgt->y;
            es->target_z = tgt->z;
            es->required_ticks = req;
            es->progress_ticks = (u16)vblanks_elapsed;
        }
    } else {
        es->active = 0;
        es->progress_ticks = 0;
    }
}
