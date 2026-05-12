/*
 * entity_meta.h — Shared editor display dimensions and geometry helpers.
 */
#pragma once

#include "editor.h"
#include "../levels/level.h"

typedef enum {
    EDITOR_ENTITY_CATEGORY_WORLD = 0,
    EDITOR_ENTITY_CATEGORY_COLLECTIBLES,
    EDITOR_ENTITY_CATEGORY_ENEMIES,
    EDITOR_ENTITY_CATEGORY_HAZARDS,
    EDITOR_ENTITY_CATEGORY_SURFACES,
    EDITOR_ENTITY_CATEGORY_DECORATIONS,
    EDITOR_ENTITY_CATEGORY_COUNT
} EditorEntityCategory;

const char *editor_entity_type_name(EntityType type);
const char *editor_entity_palette_name(EntityType type);
EditorEntityCategory editor_entity_category(EntityType type);
const char *editor_entity_category_name(EditorEntityCategory category);
int editor_entity_palette_entry_count(void);
EntityType editor_entity_palette_entry_type(int index);
int editor_entity_type_is_singleton(EntityType type);

/* Spider / Jumping Spider — 64-px frame slot, visible art crop. */
#define SPIDER_FRAME_W     64
#define SPIDER_ART_H       10
#define SPIDER_ART_Y       22

/* Bird / Faster Bird — 48-px frame slot, visible art crop. */
#define BIRD_FRAME_W       48
#define BIRD_ART_H         14
#define BIRD_ART_Y         17

/* Fish / Faster Fish — full 48x48 frame. */
#define FISH_FRAME_W       48
#define FISH_FRAME_H       48

/* Collectibles. */
#define COIN_DISPLAY_W     16
#define COIN_DISPLAY_H     16
#define YSTAR_DISPLAY_W    16
#define YSTAR_DISPLAY_H    16
#define LSTAR_DISPLAY_W    24
#define LSTAR_DISPLAY_H    24

/* Hazards and surfaces. */
#define AXE_FRAME_W        48
#define AXE_FRAME_H        64
#define SAW_DISPLAY_W      32
#define SAW_DISPLAY_H      32
#define SPIKE_TILE_W       16
#define SPIKE_TILE_H       16
#define SPIKE_PLAT_PIECE_W 16
#define SPIKE_PLAT_SRC_Y    5
#define SPIKE_PLAT_SRC_H   11
#define SPIKE_DISPLAY_W    24
#define SPIKE_DISPLAY_H    24
#define BLUE_FLAME_W       48
#define BLUE_FLAME_H       48
#define FIRE_FLAME_W       48
#define FIRE_FLAME_H       48
#define FPLAT_PIECE_W      16
#define FPLAT_PIECE_H      16
#define BRIDGE_TILE_W      16
#define BRIDGE_TILE_H      16
#define BP_SRC_Y           14
#define BP_SRC_H           18
#define BP_FRAME_W         48

/* Climbables. */
#define VINE_W             16
#define VINE_SRC_Y          8
#define VINE_SRC_H         32
#define VINE_H             32
#define VINE_STEP          19
#define LADDER_W           16
#define LADDER_SRC_Y       13
#define LADDER_SRC_H       22
#define LADDER_H           22
#define LADDER_STEP         8
#define ROPE_SRC_X          0
#define ROPE_SRC_Y          6
#define ROPE_SRC_W         16
#define ROPE_SRC_H         36
#define ROPE_W             16
#define ROPE_H             36
#define ROPE_STEP          23

/* Rails and player spawn. */
#define RAIL_TILE_W        16
#define RAIL_TILE_H        16
#define PLAYER_SPAWN_W     48
#define PLAYER_SPAWN_H     48

/* Water art strip height, needed for fish lane derivation. */
#define WATER_ART_H        31

int  editor_rail_placement_tile_count(const RailPlacement *rp);
void editor_rail_placement_position_at(const RailPlacement *rp, float t,
                                       float *x, float *y);
