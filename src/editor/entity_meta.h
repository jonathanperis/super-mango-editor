/*
 * entity_meta.h — The editor's per-type table, LevelDef storage access,
 *                 display dimensions and geometry helpers.
 *
 * entity_meta.c holds one row per EntityType (names, palette group,
 * capacity, where the placements live in LevelDef, the Place-tool preview).
 * The functions below are the only way the rest of the editor reads it.
 */
#pragma once

#include <stddef.h>  /* size_t */

#include "editor.h"
#include "../levels/level.h"
#include "../shared/geometry.h" /* IntRect */

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

/* MAX_* array length for a type (1 for singletons, 0 for invalid types). */
int editor_entity_capacity(EntityType type);

/*
 * EditorEntityPreview — what the Place tool draws under the cursor before
 * a click (canvas.c, render_ghost).
 *
 * w, h    : size in world pixels, centred on the cursor.
 * crop    : 1 to draw only the src rectangle of the sprite sheet, 0 to
 *           draw the whole texture.
 * texture : which EntityTextures member holds the sprite, as a byte offset;
 *           EDITOR_NO_TEXTURE for types drawn as a plain box (floor gaps,
 *           rails) or a marker (checkpoints).  Use editor_entity_texture()
 *           rather than reading it directly.
 */
#define EDITOR_NO_TEXTURE ((size_t)-1)

typedef struct {
    size_t  texture;
    int     w;
    int     h;
    int     crop;
    IntRect src;
} EditorEntityPreview;

/* The preview row for a type, or NULL for an invalid type. */
const EditorEntityPreview *editor_entity_preview(EntityType type);

/* The loaded sprite a type's preview uses, or NULL when it has none (or the
 * texture failed to load). */
Texture2D *editor_entity_texture(const EntityTextures *textures, EntityType type);

/* Level width in world pixels: screen_count screens (4 when unset) of
 * GAME_W each, the same rule level validation uses. */
float editor_world_width(const LevelDef *level);

/* Central LevelDef count and selection safety helpers. */
int editor_entity_count(const LevelDef *level, EntityType type);

/* Every placement in the level except the always-present player spawn;
 * the status bar's "Entities: N". */
int editor_placed_entity_total(const LevelDef *level);

/*
 * One placement, addressed by (type, index).  These are the only functions
 * that copy entity bytes in or out of LevelDef; tools, undo, clipboard and
 * tests all use them so every type is handled the same way.
 *
 * editor_snapshot_entity : copy one placement into a zeroed PlacementData
 *                          (all zeros when the index is out of range).
 * editor_entity_write    : overwrite an existing placement (also singletons).
 * editor_entity_insert   : open a slot at index (0..count) and store data.
 * editor_entity_remove   : delete the slot at index and close the gap.
 *
 * Insert/remove only work for array types and return -1 when the array is
 * full or the index is out of range.  For rails they also renumber
 * spike_blocks[].rail_index and RAIL-mode float_platforms[].rail_index so
 * every reference keeps naming the same rail.  Remove expects the caller to
 * have refused deleting a rail that is still referenced.
 */
PlacementData editor_snapshot_entity(const LevelDef *level,
                                     EntityType type, int index);
int editor_entity_write(LevelDef *level, EntityType type, int index,
                        const PlacementData *data);
int editor_entity_insert(LevelDef *level, EntityType type, int index,
                         const PlacementData *data);
int editor_entity_remove(LevelDef *level, EntityType type, int index);

/* Count spike blocks and RAIL-mode float platforms riding rail_index.
 * Either output pointer may be NULL.  Returns the total. */
int editor_rail_reference_count(const LevelDef *level, int rail_index,
                                int *spike_blocks, int *float_platforms);
int editor_selection_is_valid(const EditorState *es);
void editor_selection_reconcile(EditorState *es);
void editor_selection_after_remove(EditorState *es, EntityType type, int index);
void editor_selection_after_insert(EditorState *es, EntityType type, int index,
                                   int select_inserted);

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

void editor_rail_placement_position_at(const RailPlacement *rp, float t,
                                       float *x, float *y);
