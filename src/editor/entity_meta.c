/*
 * entity_meta.c — The editor's per-type table and LevelDef storage access.
 *
 * Every entity type raises the same questions all over the editor:
 *   - What is it called, which palette group is it in, how many may exist?
 *   - Where are its placements stored inside LevelDef?
 *   - What does the Place tool draw under the cursor before a click?
 *
 * s_entity_meta below answers all three with one row per EntityType, so
 * tools, undo, the clipboard, the palette, the canvas and the tests cannot
 * disagree about a type, and adding a type starts with adding one row.
 * Reading, writing, inserting and removing one placement all go through the
 * row's storage columns, so there is one memmove-based remove path instead
 * of a copy per caller.
 *
 * What stays outside the table is per-type *behaviour*: how a placement is
 * clamped, dragged, pasted, hit-tested, drawn and edited touches different
 * fields of each placement struct, so tools.c, hit_test.c, canvas.c,
 * editor_clipboard.c and properties.c keep one small function or switch
 * case per type for those.
 */

#include "entity_meta.h"

#include <math.h>   /* fabsf */
#include <stddef.h> /* offsetof */
#include <string.h> /* memcpy, memmove, memset */

#include "editor_clipboard.h" /* keep a copied rail rider on its rail */
#include "../game.h"          /* GAME_H, FLOOR_Y, TILE_SIZE, FLOOR_GAP_W */

/*
 * EditorEntityMeta — one row of the table: everything about a type that is
 * plain data.
 *
 * type          : the row's own EntityType.  A row left out of the table
 *                 reads as all zeros, so comparing this with the index
 *                 (and checking type_name) tells a real row from a gap.
 * type_name     : shown in the properties header and status messages.
 * palette_name  : shown in the palette list.
 * category      : the palette group the type is listed under.
 * singleton     : 1 for the two types that are one LevelDef field rather
 *                 than an array (Last Star, Player Spawn).
 * capacity      : MAX_* length of the LevelDef array (1 for singletons).
 * array_offset, count_offset, item_size :
 *                 where the placements live; see STORED_IN below.
 * preview       : what the Place tool draws under the cursor.
 */
typedef struct {
    EntityType type;
    const char *type_name;
    const char *palette_name;
    EditorEntityCategory category;
    int singleton;
    int capacity;
    size_t array_offset;
    size_t count_offset;
    size_t item_size;
    EditorEntityPreview preview;
} EditorEntityMeta;

/*
 * STORED_IN(array, count) — fill the three storage columns of a row.
 *
 * offsetof(LevelDef, coins) is how many bytes into a LevelDef the coins
 * array starts; adding it to the address of a real LevelDef gives
 * &level->coins[0] (see editor_entity_array).  The same goes for the count
 * field.  sizeof(((LevelDef *)0)->coins[0]) is the size of one element; the
 * expression inside sizeof is never evaluated, so the null pointer is never
 * dereferenced.  A table can hold these numbers, while it could not hold a
 * pointer into a LevelDef that does not exist yet.
 */
#define STORED_IN(array, count)                                   \
    .array_offset = offsetof(LevelDef, array),                    \
    .count_offset = offsetof(LevelDef, count),                    \
    .item_size    = sizeof(((LevelDef *)0)->array[0])

/*
 * TEXTURE(field) — which EntityTextures member holds the preview sprite,
 * stored as that member's offset for the same reason as STORED_IN.
 * NO_TEXTURE marks types the canvas draws without a sprite.
 * CROP(x, y, w, h) — draw only that rectangle of the sprite sheet.
 */
#define TEXTURE(field)     .texture = offsetof(EntityTextures, field)
#define NO_TEXTURE         .texture = EDITOR_NO_TEXTURE
#define CROP(x, y, w, h)   .crop = 1, .src = { (x), (y), (w), (h) }

/*
 * The table.  Rows follow the EntityType order in editor.h; the [ENT_...]
 * designators make each row land at its own index whatever the order.
 * The array is sized by its rows, not by ENT_COUNT, so the _Static_assert
 * after it fails the build when a type added at the end of the enum (where
 * new types go) has no row yet.
 */
static const EditorEntityMeta s_entity_meta[] = {
    /* ---- World ------------------------------------------------------ */
    [ENT_FLOOR_GAP] = {
        .type = ENT_FLOOR_GAP, .type_name = "Floor Gap", .palette_name = "Floor Gap",
        .category = EDITOR_ENTITY_CATEGORY_WORLD,
        .capacity = MAX_FLOOR_GAPS, STORED_IN(floor_gaps, floor_gap_count),
        /* Drawn as a blue box the size of the hole. */
        .preview = { NO_TEXTURE, .w = FLOOR_GAP_W, .h = GAME_H - FLOOR_Y },
    },
    [ENT_CHECKPOINT] = {
        .type = ENT_CHECKPOINT, .type_name = "Checkpoint", .palette_name = "Checkpoint",
        .category = EDITOR_ENTITY_CATEGORY_WORLD,
        .capacity = MAX_CHECKPOINTS, STORED_IN(checkpoints, checkpoint_count),
        /* No sprite or box: canvas.c draws a small flag pole instead. */
        .preview = { NO_TEXTURE },
    },
    [ENT_RAIL] = {
        .type = ENT_RAIL, .type_name = "Rail", .palette_name = "Rail",
        .category = EDITOR_ENTITY_CATEGORY_WORLD,
        .capacity = MAX_RAILS, STORED_IN(rails, rail_count),
        /* Drawn as a green box three rail tiles square. */
        .preview = { NO_TEXTURE, .w = 3 * RAIL_TILE_W, .h = 3 * RAIL_TILE_H },
    },
    [ENT_PLATFORM] = {
        .type = ENT_PLATFORM, .type_name = "Platform", .palette_name = "Platform",
        .category = EDITOR_ENTITY_CATEGORY_SURFACES,
        .capacity = MAX_PLATFORMS, STORED_IN(platforms, platform_count),
        /* A new pillar is one tile wide and two tiles tall. */
        .preview = { TEXTURE(platform), .w = TILE_SIZE, .h = 2 * TILE_SIZE },
    },
    /* ---- Collectibles ----------------------------------------------- */
    [ENT_COIN] = {
        .type = ENT_COIN, .type_name = "Coin", .palette_name = "Coin",
        .category = EDITOR_ENTITY_CATEGORY_COLLECTIBLES,
        .capacity = MAX_COINS, STORED_IN(coins, coin_count),
        .preview = { TEXTURE(coin), .w = COIN_DISPLAY_W, .h = COIN_DISPLAY_H },
    },
    [ENT_STAR_YELLOW] = {
        .type = ENT_STAR_YELLOW, .type_name = "Star Yellow", .palette_name = "Star Yellow",
        .category = EDITOR_ENTITY_CATEGORY_COLLECTIBLES,
        .capacity = MAX_STAR_YELLOWS, STORED_IN(star_yellows, star_yellow_count),
        .preview = { TEXTURE(star_yellow), .w = YSTAR_DISPLAY_W, .h = YSTAR_DISPLAY_H },
    },
    [ENT_STAR_GREEN] = {
        .type = ENT_STAR_GREEN, .type_name = "Star Green", .palette_name = "Star Green",
        .category = EDITOR_ENTITY_CATEGORY_COLLECTIBLES,
        .capacity = MAX_STAR_GREENS, STORED_IN(star_greens, star_green_count),
        .preview = { TEXTURE(star_green), .w = YSTAR_DISPLAY_W, .h = YSTAR_DISPLAY_H },
    },
    [ENT_STAR_RED] = {
        .type = ENT_STAR_RED, .type_name = "Star Red", .palette_name = "Star Red",
        .category = EDITOR_ENTITY_CATEGORY_COLLECTIBLES,
        .capacity = MAX_STAR_REDS, STORED_IN(star_reds, star_red_count),
        .preview = { TEXTURE(star_red), .w = YSTAR_DISPLAY_W, .h = YSTAR_DISPLAY_H },
    },
    [ENT_LAST_STAR] = {
        .type = ENT_LAST_STAR, .type_name = "Last Star", .palette_name = "Last Star",
        .category = EDITOR_ENTITY_CATEGORY_COLLECTIBLES,
        .singleton = 1, .capacity = 1,   /* the LevelDef field last_star */
        .preview = { TEXTURE(last_star), .w = LSTAR_DISPLAY_W, .h = LSTAR_DISPLAY_H },
    },
    /* ---- Enemies ---------------------------------------------------- */
    [ENT_SPIDER] = {
        .type = ENT_SPIDER, .type_name = "Spider", .palette_name = "Spider",
        .category = EDITOR_ENTITY_CATEGORY_ENEMIES,
        .capacity = MAX_SPIDERS, STORED_IN(spiders, spider_count),
        .preview = { TEXTURE(spider), .w = SPIDER_FRAME_W, .h = SPIDER_ART_H,
                     CROP(0, SPIDER_ART_Y, SPIDER_FRAME_W, SPIDER_ART_H) },
    },
    [ENT_JUMPING_SPIDER] = {
        .type = ENT_JUMPING_SPIDER, .type_name = "Jumping Spider",
        .palette_name = "Jumping Spider",
        .category = EDITOR_ENTITY_CATEGORY_ENEMIES,
        .capacity = MAX_JUMPING_SPIDERS, STORED_IN(jumping_spiders, jumping_spider_count),
        .preview = { TEXTURE(jumping_spider), .w = SPIDER_FRAME_W, .h = SPIDER_ART_H,
                     CROP(0, SPIDER_ART_Y, SPIDER_FRAME_W, SPIDER_ART_H) },
    },
    [ENT_BIRD] = {
        .type = ENT_BIRD, .type_name = "Bird", .palette_name = "Bird",
        .category = EDITOR_ENTITY_CATEGORY_ENEMIES,
        .capacity = MAX_BIRDS, STORED_IN(birds, bird_count),
        .preview = { TEXTURE(bird), .w = BIRD_FRAME_W, .h = BIRD_ART_H,
                     CROP(0, BIRD_ART_Y, BIRD_FRAME_W, BIRD_ART_H) },
    },
    [ENT_FASTER_BIRD] = {
        .type = ENT_FASTER_BIRD, .type_name = "Faster Bird", .palette_name = "Faster Bird",
        .category = EDITOR_ENTITY_CATEGORY_ENEMIES,
        .capacity = MAX_FASTER_BIRDS, STORED_IN(faster_birds, faster_bird_count),
        .preview = { TEXTURE(faster_bird), .w = BIRD_FRAME_W, .h = BIRD_ART_H,
                     CROP(0, BIRD_ART_Y, BIRD_FRAME_W, BIRD_ART_H) },
    },
    [ENT_FISH] = {
        .type = ENT_FISH, .type_name = "Fish", .palette_name = "Fish",
        .category = EDITOR_ENTITY_CATEGORY_ENEMIES,
        .capacity = MAX_FISH, STORED_IN(fish, fish_count),
        .preview = { TEXTURE(fish), .w = FISH_FRAME_W, .h = FISH_FRAME_H },
    },
    [ENT_FASTER_FISH] = {
        .type = ENT_FASTER_FISH, .type_name = "Faster Fish", .palette_name = "Faster Fish",
        .category = EDITOR_ENTITY_CATEGORY_ENEMIES,
        .capacity = MAX_FASTER_FISH, STORED_IN(faster_fish, faster_fish_count),
        .preview = { TEXTURE(faster_fish), .w = FISH_FRAME_W, .h = FISH_FRAME_H },
    },
    /* ---- Hazards ---------------------------------------------------- */
    [ENT_AXE_TRAP] = {
        .type = ENT_AXE_TRAP, .type_name = "Axe Trap", .palette_name = "Axe Trap",
        .category = EDITOR_ENTITY_CATEGORY_HAZARDS,
        .capacity = MAX_AXE_TRAPS, STORED_IN(axe_traps, axe_trap_count),
        .preview = { TEXTURE(axe_trap), .w = AXE_FRAME_W, .h = AXE_FRAME_H },
    },
    [ENT_CIRCULAR_SAW] = {
        .type = ENT_CIRCULAR_SAW, .type_name = "Circular Saw", .palette_name = "Circular Saw",
        .category = EDITOR_ENTITY_CATEGORY_HAZARDS,
        .capacity = MAX_CIRCULAR_SAWS, STORED_IN(circular_saws, circular_saw_count),
        .preview = { TEXTURE(circular_saw), .w = SAW_DISPLAY_W, .h = SAW_DISPLAY_H },
    },
    [ENT_SPIKE_ROW] = {
        .type = ENT_SPIKE_ROW, .type_name = "Spike Row", .palette_name = "Spike Row",
        .category = EDITOR_ENTITY_CATEGORY_HAZARDS,
        .capacity = MAX_SPIKE_ROWS, STORED_IN(spike_rows, spike_row_count),
        .preview = { TEXTURE(spike), .w = SPIKE_TILE_W, .h = SPIKE_TILE_H },
    },
    [ENT_SPIKE_PLATFORM] = {
        .type = ENT_SPIKE_PLATFORM, .type_name = "Spike Platform",
        .palette_name = "Spike Platform",
        .category = EDITOR_ENTITY_CATEGORY_HAZARDS,
        .capacity = MAX_SPIKE_PLATFORMS, STORED_IN(spike_platforms, spike_platform_count),
        .preview = { TEXTURE(spike_platform), .w = SPIKE_PLAT_PIECE_W, .h = SPIKE_PLAT_SRC_H,
                     CROP(0, SPIKE_PLAT_SRC_Y, SPIKE_PLAT_PIECE_W, SPIKE_PLAT_SRC_H) },
    },
    [ENT_SPIKE_BLOCK] = {
        .type = ENT_SPIKE_BLOCK, .type_name = "Spike Block", .palette_name = "Spike Block",
        .category = EDITOR_ENTITY_CATEGORY_HAZARDS,
        .capacity = MAX_SPIKE_BLOCKS, STORED_IN(spike_blocks, spike_block_count),
        .preview = { TEXTURE(spike_block), .w = SPIKE_DISPLAY_W, .h = SPIKE_DISPLAY_H },
    },
    [ENT_BLUE_FLAME] = {
        .type = ENT_BLUE_FLAME, .type_name = "Blue Flame", .palette_name = "Blue Flame",
        .category = EDITOR_ENTITY_CATEGORY_HAZARDS,
        .capacity = MAX_BLUE_FLAMES, STORED_IN(blue_flames, blue_flame_count),
        .preview = { TEXTURE(blue_flame), .w = BLUE_FLAME_W, .h = BLUE_FLAME_H },
    },
    [ENT_FIRE_FLAME] = {
        .type = ENT_FIRE_FLAME, .type_name = "Fire Flame", .palette_name = "Fire Flame",
        .category = EDITOR_ENTITY_CATEGORY_HAZARDS,
        .capacity = MAX_FIRE_FLAMES, STORED_IN(fire_flames, fire_flame_count),
        .preview = { TEXTURE(fire_flame), .w = FIRE_FLAME_W, .h = FIRE_FLAME_H },
    },
    /* ---- Surfaces --------------------------------------------------- */
    [ENT_FLOAT_PLATFORM] = {
        .type = ENT_FLOAT_PLATFORM, .type_name = "Float Platform",
        .palette_name = "Float Platform",
        .category = EDITOR_ENTITY_CATEGORY_SURFACES,
        .capacity = MAX_FLOAT_PLATFORMS, STORED_IN(float_platforms, float_platform_count),
        .preview = { TEXTURE(float_platform), .w = FPLAT_PIECE_W, .h = FPLAT_PIECE_H },
    },
    [ENT_BRIDGE] = {
        .type = ENT_BRIDGE, .type_name = "Bridge", .palette_name = "Bridge",
        .category = EDITOR_ENTITY_CATEGORY_SURFACES,
        .capacity = MAX_BRIDGES, STORED_IN(bridges, bridge_count),
        .preview = { TEXTURE(bridge), .w = BRIDGE_TILE_W, .h = BRIDGE_TILE_H },
    },
    /* The three bouncepads show frame 2 (idle) of their own sheet. */
    [ENT_BOUNCEPAD_SMALL] = {
        .type = ENT_BOUNCEPAD_SMALL, .type_name = "Bouncepad (S)",
        .palette_name = "Bouncepad Small",
        .category = EDITOR_ENTITY_CATEGORY_SURFACES,
        .capacity = MAX_BOUNCEPADS_SMALL, STORED_IN(bouncepads_small, bouncepad_small_count),
        .preview = { TEXTURE(bouncepad_small), .w = BP_FRAME_W, .h = BP_SRC_H,
                     CROP(2 * BP_FRAME_W, BP_SRC_Y, BP_FRAME_W, BP_SRC_H) },
    },
    [ENT_BOUNCEPAD_MEDIUM] = {
        .type = ENT_BOUNCEPAD_MEDIUM, .type_name = "Bouncepad (M)",
        .palette_name = "Bouncepad Medium",
        .category = EDITOR_ENTITY_CATEGORY_SURFACES,
        .capacity = MAX_BOUNCEPADS_MEDIUM, STORED_IN(bouncepads_medium, bouncepad_medium_count),
        .preview = { TEXTURE(bouncepad_medium), .w = BP_FRAME_W, .h = BP_SRC_H,
                     CROP(2 * BP_FRAME_W, BP_SRC_Y, BP_FRAME_W, BP_SRC_H) },
    },
    [ENT_BOUNCEPAD_HIGH] = {
        .type = ENT_BOUNCEPAD_HIGH, .type_name = "Bouncepad (H)",
        .palette_name = "Bouncepad High",
        .category = EDITOR_ENTITY_CATEGORY_SURFACES,
        .capacity = MAX_BOUNCEPADS_HIGH, STORED_IN(bouncepads_high, bouncepad_high_count),
        .preview = { TEXTURE(bouncepad_high), .w = BP_FRAME_W, .h = BP_SRC_H,
                     CROP(2 * BP_FRAME_W, BP_SRC_Y, BP_FRAME_W, BP_SRC_H) },
    },
    /* ---- Decorations (climbables): one tile of the stack ------------ */
    [ENT_VINE] = {
        .type = ENT_VINE, .type_name = "Vine", .palette_name = "Vine",
        .category = EDITOR_ENTITY_CATEGORY_DECORATIONS,
        .capacity = MAX_VINES, STORED_IN(vines, vine_count),
        .preview = { TEXTURE(vine_green), .w = VINE_W, .h = VINE_H,
                     CROP(0, VINE_SRC_Y, VINE_W, VINE_SRC_H) },
    },
    [ENT_LADDER] = {
        .type = ENT_LADDER, .type_name = "Ladder", .palette_name = "Ladder",
        .category = EDITOR_ENTITY_CATEGORY_DECORATIONS,
        .capacity = MAX_LADDERS, STORED_IN(ladders, ladder_count),
        .preview = { TEXTURE(ladder), .w = LADDER_W, .h = LADDER_H,
                     CROP(0, LADDER_SRC_Y, LADDER_W, LADDER_SRC_H) },
    },
    [ENT_ROPE] = {
        .type = ENT_ROPE, .type_name = "Rope", .palette_name = "Rope",
        .category = EDITOR_ENTITY_CATEGORY_DECORATIONS,
        .capacity = MAX_ROPES, STORED_IN(ropes, rope_count),
        .preview = { TEXTURE(rope), .w = ROPE_W, .h = ROPE_H,
                     CROP(ROPE_SRC_X, ROPE_SRC_Y, ROPE_SRC_W, ROPE_SRC_H) },
    },
    /* ---- Player ----------------------------------------------------- */
    [ENT_PLAYER_SPAWN] = {
        .type = ENT_PLAYER_SPAWN, .type_name = "Player Spawn", .palette_name = "Player Spawn",
        .category = EDITOR_ENTITY_CATEGORY_WORLD,
        /* player_start_x/y in LevelDef; first frame of the idle sheet. */
        .singleton = 1, .capacity = 1,
        .preview = { TEXTURE(player), .w = PLAYER_SPAWN_W, .h = PLAYER_SPAWN_H,
                     CROP(0, 0, PLAYER_SPAWN_W, PLAYER_SPAWN_H) },
    },
};

_Static_assert(sizeof(s_entity_meta) / sizeof(s_entity_meta[0]) == ENT_COUNT,
               "s_entity_meta needs one row per EntityType");

#undef STORED_IN
#undef TEXTURE
#undef NO_TEXTURE
#undef CROP

static const EntityType s_palette_order[] = {
    ENT_PLAYER_SPAWN,
    ENT_FLOOR_GAP,
    ENT_CHECKPOINT,
    ENT_RAIL,
    ENT_COIN,
    ENT_STAR_YELLOW,
    ENT_STAR_GREEN,
    ENT_STAR_RED,
    ENT_LAST_STAR,
    ENT_SPIDER,
    ENT_JUMPING_SPIDER,
    ENT_BIRD,
    ENT_FASTER_BIRD,
    ENT_FISH,
    ENT_FASTER_FISH,
    ENT_AXE_TRAP,
    ENT_CIRCULAR_SAW,
    ENT_SPIKE_ROW,
    ENT_SPIKE_PLATFORM,
    ENT_SPIKE_BLOCK,
    ENT_BLUE_FLAME,
    ENT_FIRE_FLAME,
    ENT_PLATFORM,
    ENT_FLOAT_PLATFORM,
    ENT_BRIDGE,
    ENT_BOUNCEPAD_SMALL,
    ENT_BOUNCEPAD_MEDIUM,
    ENT_BOUNCEPAD_HIGH,
    ENT_VINE,
    ENT_LADDER,
    ENT_ROPE
};

static const char *s_category_names[EDITOR_ENTITY_CATEGORY_COUNT] = {
    [EDITOR_ENTITY_CATEGORY_WORLD]       = "World",
    [EDITOR_ENTITY_CATEGORY_COLLECTIBLES] = "Collectibles",
    [EDITOR_ENTITY_CATEGORY_ENEMIES]     = "Enemies",
    [EDITOR_ENTITY_CATEGORY_HAZARDS]     = "Hazards",
    [EDITOR_ENTITY_CATEGORY_SURFACES]    = "Surfaces",
    [EDITOR_ENTITY_CATEGORY_DECORATIONS] = "Decorations"
};

#define PALETTE_ENTRY_COUNT ((int)(sizeof(s_palette_order) / sizeof(s_palette_order[0])))

_Static_assert(PALETTE_ENTRY_COUNT == ENT_COUNT,
               "palette order must include every editor entity type");

/* The row for `type`, or NULL for ENT_COUNT, a bad value or a missing row. */
static const EditorEntityMeta *editor_entity_meta(EntityType type)
{
    if (type < 0 || type >= ENT_COUNT) return 0;
    if (s_entity_meta[type].type != type || !s_entity_meta[type].type_name)
        return 0;
    return &s_entity_meta[type];
}

const char *editor_entity_type_name(EntityType type)
{
    const EditorEntityMeta *meta = editor_entity_meta(type);
    return meta ? meta->type_name : "Unknown";
}

const char *editor_entity_palette_name(EntityType type)
{
    const EditorEntityMeta *meta = editor_entity_meta(type);
    return meta ? meta->palette_name : "Unknown";
}

EditorEntityCategory editor_entity_category(EntityType type)
{
    const EditorEntityMeta *meta = editor_entity_meta(type);
    return meta ? meta->category : EDITOR_ENTITY_CATEGORY_WORLD;
}

const char *editor_entity_category_name(EditorEntityCategory category)
{
    if (category < 0 || category >= EDITOR_ENTITY_CATEGORY_COUNT) return "Unknown";
    return s_category_names[category];
}

int editor_entity_palette_entry_count(void)
{
    return PALETTE_ENTRY_COUNT;
}

EntityType editor_entity_palette_entry_type(int index)
{
    if (index < 0 || index >= PALETTE_ENTRY_COUNT) return ENT_COUNT;
    return s_palette_order[index];
}

int editor_entity_type_is_singleton(EntityType type)
{
    const EditorEntityMeta *meta = editor_entity_meta(type);
    return meta ? meta->singleton : 0;
}

int editor_entity_capacity(EntityType type)
{
    const EditorEntityMeta *meta = editor_entity_meta(type);
    return meta ? meta->capacity : 0;
}

const EditorEntityPreview *editor_entity_preview(EntityType type)
{
    const EditorEntityMeta *meta = editor_entity_meta(type);
    return meta ? &meta->preview : 0;
}

Texture2D *editor_entity_texture(const EntityTextures *textures, EntityType type)
{
    const EditorEntityPreview *preview = editor_entity_preview(type);

    if (!textures || !preview || preview->texture == EDITOR_NO_TEXTURE) return 0;
    /* preview->texture is the byte offset of one Texture2D * member inside
     * EntityTextures (see TEXTURE above); read that member. */
    return *(Texture2D *const *)((const char *)textures + preview->texture);
}

float editor_world_width(const LevelDef *level)
{
    int screens = (level && level->screen_count > 0) ? level->screen_count : 4;
    /* A hand-edited file may hold any int; never let it overflow below. */
    if (screens > MAX_LEVEL_SCREENS) screens = MAX_LEVEL_SCREENS;
    return (float)screens * (float)GAME_W;
}

/*
 * editor_nearest_floor_gap — Place, paste, drag and the x field all move a
 * flame through here, so the editor never stores a flame on solid floor.
 * On a tie the earlier gap in the array wins, which keeps the result the
 * same however often it is called.
 */
float editor_nearest_floor_gap(const LevelDef *level, float x)
{
    float best = x;
    float best_distance = 0.0f;
    int count;

    if (!level) return x;
    count = level->floor_gap_count;
    if (count > MAX_FLOOR_GAPS) count = MAX_FLOOR_GAPS;
    for (int i = 0; i < count; i++) {
        float gap = (float)level->floor_gaps[i];
        float distance = fabsf(gap - x);
        if (i == 0 || distance < best_distance) {
            best = gap;
            best_distance = distance;
        }
    }
    return best;
}

/* ------------------------------------------------------------------ */
/* Where each entity type lives inside LevelDef                        */
/* ------------------------------------------------------------------ */

/*
 * EditorEntityArray — a type-erased view of one LevelDef placement array.
 *
 * items     : address of element 0 (a void pointer, so one struct can
 *             describe a CoinPlacement array or a RailPlacement array).
 * item_size : sizeof one element; byte offsets are index * item_size.
 * count     : the LevelDef field that says how many slots are in use.
 *
 * The capacity comes from s_entity_meta, so the MAX_* constants are written
 * down once.
 */
typedef struct {
    void   *items;
    size_t  item_size;
    int    *count;
} EditorEntityArray;

/*
 * editor_entity_array — Fill *out with the array that stores `type`.
 *
 * Returns 1 for array-backed types and 0 for the two singletons (Last Star,
 * Player Spawn), which are plain fields instead of arrays.  The row's
 * offsets turn into real addresses by adding them to the address of this
 * LevelDef; read/write/insert/remove and the entity count all start here.
 */
static int editor_entity_array(LevelDef *level, EntityType type,
                               EditorEntityArray *out)
{
    const EditorEntityMeta *meta = editor_entity_meta(type);
    char *base = (char *)level;

    if (!level || !meta || meta->singleton) return 0;
    out->items     = base + meta->array_offset;
    out->item_size = meta->item_size;
    out->count     = (int *)(base + meta->count_offset);
    return 1;
}

int editor_entity_count(const LevelDef *level, EntityType type)
{
    EditorEntityArray array;
    int capacity = editor_entity_capacity(type);
    int count;

    if (!level || capacity == 0) return 0;
    if (editor_entity_type_is_singleton(type)) return 1;

    /* The cast only lets the shared lookup describe a const LevelDef; this
     * function reads *array.count and never writes through it. */
    if (!editor_entity_array((LevelDef *)level, type, &array)) return 0;
    count = *array.count;
    if (count < 0) return 0;
    return count > capacity ? capacity : count;
}

/*
 * editor_placed_entity_total — What the status bar shows as "Entities: N".
 *
 * Walking the table instead of listing count fields by hand means a type
 * added later (as checkpoints were) is counted without anyone having to
 * remember the status bar.  The player spawn is left out: every level has
 * exactly one, so it is not something the designer placed.  The Last Star
 * counts once it has a position, the same "unset" rule the loader uses.
 */
int editor_placed_entity_total(const LevelDef *level)
{
    int total = 0;

    if (!level) return 0;
    for (int type = 0; type < ENT_COUNT; type++) {
        if (type == ENT_PLAYER_SPAWN) continue;
        if (type == ENT_LAST_STAR) {
            if (level->last_star.x != 0.0f || level->last_star.y != 0.0f) total++;
            continue;
        }
        total += editor_entity_count(level, (EntityType)type);
    }
    return total;
}

/* ------------------------------------------------------------------ */
/* Read / write / insert / remove one placement                        */
/* ------------------------------------------------------------------ */

/*
 * Every member of the PlacementData union starts at byte 0 of the union
 * (C11 6.7.2.1p16).  Copying an element's bytes into the union therefore
 * fills the member that matches its type, without one switch per caller.
 * The editor then reads that member using the EntityType tag it stored
 * next to the union.
 */
static char *entity_slot(const EditorEntityArray *array, int index)
{
    return (char *)array->items + (size_t)index * array->item_size;
}

PlacementData editor_snapshot_entity(const LevelDef *level,
                                     EntityType type, int index)
{
    PlacementData pd;
    EditorEntityArray array;

    memset(&pd, 0, sizeof(pd));
    if (!level || index < 0 || index >= editor_entity_count(level, type))
        return pd;

    if (type == ENT_LAST_STAR) {
        pd.last_star = level->last_star;
    } else if (type == ENT_PLAYER_SPAWN) {
        /* Player spawn is two loose fields; reuse the {x, y} last_star
         * member so undo and clipboard can carry it like any placement. */
        pd.last_star.x = level->player_start_x;
        pd.last_star.y = level->player_start_y;
    } else if (editor_entity_array((LevelDef *)level, type, &array) &&
               array.item_size <= sizeof(pd)) {
        memcpy(&pd, entity_slot(&array, index), array.item_size);
    }
    return pd;
}

int editor_entity_write(LevelDef *level, EntityType type, int index,
                        const PlacementData *data)
{
    EditorEntityArray array;

    if (!level || !data || index < 0 || index >= editor_entity_count(level, type))
        return -1;

    if (type == ENT_LAST_STAR) {
        level->last_star = data->last_star;
        return 0;
    }
    if (type == ENT_PLAYER_SPAWN) {
        level->player_start_x = data->last_star.x;
        level->player_start_y = data->last_star.y;
        return 0;
    }
    if (!editor_entity_array(level, type, &array)) return -1;
    memcpy(entity_slot(&array, index), data, array.item_size);
    return 0;
}

/*
 * Rail-bound entities refer to rails by array position.  Inserting or removing
 * a rail moves every later rail one slot, so the stored numbers must move with
 * them.  Only RAIL-mode float platforms use their rail_index; leaving the
 * others untouched keeps remove-then-insert an exact round trip for undo.
 */
static void editor_shift_rail_references(LevelDef *level, int first_index,
                                         int delta)
{
    for (int i = 0; i < level->spike_block_count && i < MAX_SPIKE_BLOCKS; i++) {
        if (level->spike_blocks[i].rail_index >= first_index)
            level->spike_blocks[i].rail_index += delta;
    }
    for (int i = 0; i < level->float_platform_count && i < MAX_FLOAT_PLATFORMS; i++) {
        FloatPlatformPlacement *fp = &level->float_platforms[i];
        if (fp->mode == FLOAT_PLATFORM_RAIL && fp->rail_index >= first_index)
            fp->rail_index += delta;
    }
}

int editor_entity_insert(LevelDef *level, EntityType type, int index,
                         const PlacementData *data)
{
    EditorEntityArray array;
    int count;

    if (!level || !data || !editor_entity_array(level, type, &array)) return -1;
    count = editor_entity_count(level, type);
    if (count >= editor_entity_capacity(type) || index < 0 || index > count)
        return -1;

    /* Open a gap: elements index..count-1 move one slot to the right. */
    memmove(entity_slot(&array, index + 1), entity_slot(&array, index),
            (size_t)(count - index) * array.item_size);
    memcpy(entity_slot(&array, index), data, array.item_size);
    *array.count = count + 1;

    /* Rails at index and after moved up by one, and so must references. */
    if (type == ENT_RAIL) editor_shift_rail_references(level, index, +1);
    return 0;
}

int editor_entity_remove(LevelDef *level, EntityType type, int index)
{
    EditorEntityArray array;
    int count;

    if (!level || !editor_entity_array(level, type, &array)) return -1;
    count = editor_entity_count(level, type);
    if (index < 0 || index >= count) return -1;

    /* Close the gap: elements index+1..count-1 move one slot to the left. */
    memmove(entity_slot(&array, index), entity_slot(&array, index + 1),
            (size_t)(count - index - 1) * array.item_size);
    *array.count = count - 1;

    /* Callers refuse to remove a referenced rail, so only references to
     * later rails exist; they now live one slot lower. */
    if (type == ENT_RAIL) editor_shift_rail_references(level, index + 1, -1);
    return 0;
}

int editor_rail_reference_count(const LevelDef *level, int rail_index,
                                int *spike_blocks, int *float_platforms)
{
    int blocks = 0;
    int platforms = 0;

    if (level) {
        for (int i = 0; i < level->spike_block_count && i < MAX_SPIKE_BLOCKS; i++)
            if (level->spike_blocks[i].rail_index == rail_index) blocks++;
        for (int i = 0; i < level->float_platform_count && i < MAX_FLOAT_PLATFORMS; i++)
            if (level->float_platforms[i].mode == FLOAT_PLATFORM_RAIL &&
                level->float_platforms[i].rail_index == rail_index) platforms++;
    }
    if (spike_blocks) *spike_blocks = blocks;
    if (float_platforms) *float_platforms = platforms;
    return blocks + platforms;
}

/* ------------------------------------------------------------------ */
/* Selection bookkeeping                                               */
/* ------------------------------------------------------------------ */

int editor_selection_is_valid(const EditorState *es)
{
    return es && es->selection.index >= 0 &&
           editor_entity_count(&es->level, es->selection.type) >
           es->selection.index;
}

int editor_selection_items(const EditorState *es, Selection *out, int max)
{
    if (!es || !out || max <= 0 || !editor_selection_is_valid(es)) return 0;
    out[0] = es->selection;
    return 1;
}

void editor_selection_reconcile(EditorState *es)
{
    if (!es) return;
    if (!editor_selection_is_valid(es)) es->selection.index = -1;
}

/*
 * The two helpers below run after every insert or removal that shifts an
 * entity array (tools and undo/redo alike).  Besides the selection they
 * keep the clipboard's copied rail rider pointing at its own rail.
 */
void editor_selection_after_remove(EditorState *es, EntityType type, int index)
{
    if (!es) return;
    if (type == ENT_RAIL) editor_clipboard_after_rail_remove(es, index);
    if (es->selection.type == type && es->selection.index >= 0) {
        if (es->selection.index == index) {
            es->selection.index = -1;
        } else if (es->selection.index > index) {
            es->selection.index--;
        }
    }
    editor_selection_reconcile(es);
}

void editor_selection_after_insert(EditorState *es, EntityType type, int index,
                                   int select_inserted)
{
    if (!es) return;
    if (type == ENT_RAIL) editor_clipboard_after_rail_insert(es, index);
    if (select_inserted) {
        es->selection.type = type;
        es->selection.index = index;
    } else if (es->selection.type == type && es->selection.index >= index) {
        es->selection.index++;
    }
    editor_selection_reconcile(es);
}

void editor_rail_placement_position_at(const RailPlacement *rp, float t,
                                       float *x, float *y)
{
    Rail rail;
    if (!x || !y) return;
    (void)rail_build(&rail, rp);
    rail_get_world_pos(&rail, t, x, y);
}
