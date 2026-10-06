/*
 * entity_meta.c — Per-type editor metadata and LevelDef storage access.
 *
 * Two questions come up for every entity type, all over the editor:
 *   1. "What is it called and how many may exist?"  → s_entity_meta below.
 *   2. "Where does it live inside LevelDef?"         → editor_entity_array().
 *
 * Answering both in exactly one place means tools, undo, clipboard and tests
 * cannot disagree about an array's capacity or forget a type when a new one
 * is added.  Reading, writing, inserting and removing one placement all go
 * through the same table, so there is one memmove-based remove path instead
 * of a copy per caller.
 */

#include "entity_meta.h"

#include <string.h> /* memcpy, memmove, memset */

typedef struct {
    EntityType type;
    const char *type_name;
    const char *palette_name;
    EditorEntityCategory category;
    int singleton;
    int capacity;   /* MAX_* length of the LevelDef array (1 for singletons) */
} EditorEntityMeta;

static const EditorEntityMeta s_entity_meta[ENT_COUNT] = {
    [ENT_FLOOR_GAP] = {
        ENT_FLOOR_GAP, "Floor Gap", "Floor Gap",
        EDITOR_ENTITY_CATEGORY_WORLD, 0, MAX_FLOOR_GAPS
    },
    [ENT_CHECKPOINT] = {
        ENT_CHECKPOINT, "Checkpoint", "Checkpoint",
        EDITOR_ENTITY_CATEGORY_WORLD, 0, MAX_CHECKPOINTS
    },
    [ENT_RAIL] = {
        ENT_RAIL, "Rail", "Rail", EDITOR_ENTITY_CATEGORY_WORLD, 0, MAX_RAILS
    },
    [ENT_PLATFORM] = {
        ENT_PLATFORM, "Platform", "Platform", EDITOR_ENTITY_CATEGORY_SURFACES,
        0, MAX_PLATFORMS
    },
    [ENT_COIN] = {
        ENT_COIN, "Coin", "Coin", EDITOR_ENTITY_CATEGORY_COLLECTIBLES,
        0, MAX_COINS
    },
    [ENT_STAR_YELLOW] = {
        ENT_STAR_YELLOW, "Star Yellow", "Star Yellow",
        EDITOR_ENTITY_CATEGORY_COLLECTIBLES, 0, MAX_STAR_YELLOWS
    },
    [ENT_STAR_GREEN] = {
        ENT_STAR_GREEN, "Star Green", "Star Green",
        EDITOR_ENTITY_CATEGORY_COLLECTIBLES, 0, MAX_STAR_GREENS
    },
    [ENT_STAR_RED] = {
        ENT_STAR_RED, "Star Red", "Star Red",
        EDITOR_ENTITY_CATEGORY_COLLECTIBLES, 0, MAX_STAR_REDS
    },
    [ENT_LAST_STAR] = {
        ENT_LAST_STAR, "Last Star", "Last Star",
        EDITOR_ENTITY_CATEGORY_COLLECTIBLES, 1, 1
    },
    [ENT_SPIDER] = {
        ENT_SPIDER, "Spider", "Spider", EDITOR_ENTITY_CATEGORY_ENEMIES,
        0, MAX_SPIDERS
    },
    [ENT_JUMPING_SPIDER] = {
        ENT_JUMPING_SPIDER, "Jumping Spider", "Jumping Spider",
        EDITOR_ENTITY_CATEGORY_ENEMIES, 0, MAX_JUMPING_SPIDERS
    },
    [ENT_BIRD] = {
        ENT_BIRD, "Bird", "Bird", EDITOR_ENTITY_CATEGORY_ENEMIES, 0, MAX_BIRDS
    },
    [ENT_FASTER_BIRD] = {
        ENT_FASTER_BIRD, "Faster Bird", "Faster Bird",
        EDITOR_ENTITY_CATEGORY_ENEMIES, 0, MAX_FASTER_BIRDS
    },
    [ENT_FISH] = {
        ENT_FISH, "Fish", "Fish", EDITOR_ENTITY_CATEGORY_ENEMIES, 0, MAX_FISH
    },
    [ENT_FASTER_FISH] = {
        ENT_FASTER_FISH, "Faster Fish", "Faster Fish",
        EDITOR_ENTITY_CATEGORY_ENEMIES, 0, MAX_FASTER_FISH
    },
    [ENT_AXE_TRAP] = {
        ENT_AXE_TRAP, "Axe Trap", "Axe Trap",
        EDITOR_ENTITY_CATEGORY_HAZARDS, 0, MAX_AXE_TRAPS
    },
    [ENT_CIRCULAR_SAW] = {
        ENT_CIRCULAR_SAW, "Circular Saw", "Circular Saw",
        EDITOR_ENTITY_CATEGORY_HAZARDS, 0, MAX_CIRCULAR_SAWS
    },
    [ENT_SPIKE_ROW] = {
        ENT_SPIKE_ROW, "Spike Row", "Spike Row",
        EDITOR_ENTITY_CATEGORY_HAZARDS, 0, MAX_SPIKE_ROWS
    },
    [ENT_SPIKE_PLATFORM] = {
        ENT_SPIKE_PLATFORM, "Spike Platform", "Spike Platform",
        EDITOR_ENTITY_CATEGORY_HAZARDS, 0, MAX_SPIKE_PLATFORMS
    },
    [ENT_SPIKE_BLOCK] = {
        ENT_SPIKE_BLOCK, "Spike Block", "Spike Block",
        EDITOR_ENTITY_CATEGORY_HAZARDS, 0, MAX_SPIKE_BLOCKS
    },
    [ENT_BLUE_FLAME] = {
        ENT_BLUE_FLAME, "Blue Flame", "Blue Flame",
        EDITOR_ENTITY_CATEGORY_HAZARDS, 0, MAX_BLUE_FLAMES
    },
    [ENT_FIRE_FLAME] = {
        ENT_FIRE_FLAME, "Fire Flame", "Fire Flame",
        EDITOR_ENTITY_CATEGORY_HAZARDS, 0, MAX_FIRE_FLAMES
    },
    [ENT_FLOAT_PLATFORM] = {
        ENT_FLOAT_PLATFORM, "Float Platform", "Float Platform",
        EDITOR_ENTITY_CATEGORY_SURFACES, 0, MAX_FLOAT_PLATFORMS
    },
    [ENT_BRIDGE] = {
        ENT_BRIDGE, "Bridge", "Bridge", EDITOR_ENTITY_CATEGORY_SURFACES,
        0, MAX_BRIDGES
    },
    [ENT_BOUNCEPAD_SMALL] = {
        ENT_BOUNCEPAD_SMALL, "Bouncepad (S)", "Bouncepad Small",
        EDITOR_ENTITY_CATEGORY_SURFACES, 0, MAX_BOUNCEPADS_SMALL
    },
    [ENT_BOUNCEPAD_MEDIUM] = {
        ENT_BOUNCEPAD_MEDIUM, "Bouncepad (M)", "Bouncepad Medium",
        EDITOR_ENTITY_CATEGORY_SURFACES, 0, MAX_BOUNCEPADS_MEDIUM
    },
    [ENT_BOUNCEPAD_HIGH] = {
        ENT_BOUNCEPAD_HIGH, "Bouncepad (H)", "Bouncepad High",
        EDITOR_ENTITY_CATEGORY_SURFACES, 0, MAX_BOUNCEPADS_HIGH
    },
    [ENT_VINE] = {
        ENT_VINE, "Vine", "Vine", EDITOR_ENTITY_CATEGORY_DECORATIONS,
        0, MAX_VINES
    },
    [ENT_LADDER] = {
        ENT_LADDER, "Ladder", "Ladder", EDITOR_ENTITY_CATEGORY_DECORATIONS,
        0, MAX_LADDERS
    },
    [ENT_ROPE] = {
        ENT_ROPE, "Rope", "Rope", EDITOR_ENTITY_CATEGORY_DECORATIONS,
        0, MAX_ROPES
    },
    [ENT_PLAYER_SPAWN] = {
        ENT_PLAYER_SPAWN, "Player Spawn", "Player Spawn",
        EDITOR_ENTITY_CATEGORY_WORLD, 1, 1
    }
};

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

static const EditorEntityMeta *editor_entity_meta(EntityType type)
{
    if (type < 0 || type >= ENT_COUNT) return 0;
    if (s_entity_meta[type].type != type) return 0;
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

static EditorEntityArray entity_array(void *items, size_t item_size, int *count)
{
    EditorEntityArray array;
    array.items = items;
    array.item_size = item_size;
    array.count = count;
    return array;
}

/*
 * editor_entity_array — Fill *out with the array that stores `type`.
 *
 * Returns 1 for array-backed types and 0 for the two singletons (Last Star,
 * Player Spawn), which are plain fields instead of arrays.  This is the only
 * switch that names every LevelDef array; read/write/insert/remove and the
 * entity count all start here.
 */
static int editor_entity_array(LevelDef *level, EntityType type,
                               EditorEntityArray *out)
{
    switch (type) {
    case ENT_FLOOR_GAP:
        *out = entity_array(level->floor_gaps, sizeof(level->floor_gaps[0]),
                            &level->floor_gap_count);
        return 1;
    case ENT_CHECKPOINT:
        *out = entity_array(level->checkpoints, sizeof(level->checkpoints[0]),
                            &level->checkpoint_count);
        return 1;
    case ENT_RAIL:
        *out = entity_array(level->rails, sizeof(level->rails[0]),
                            &level->rail_count);
        return 1;
    case ENT_PLATFORM:
        *out = entity_array(level->platforms, sizeof(level->platforms[0]),
                            &level->platform_count);
        return 1;
    case ENT_COIN:
        *out = entity_array(level->coins, sizeof(level->coins[0]),
                            &level->coin_count);
        return 1;
    case ENT_STAR_YELLOW:
        *out = entity_array(level->star_yellows, sizeof(level->star_yellows[0]),
                            &level->star_yellow_count);
        return 1;
    case ENT_STAR_GREEN:
        *out = entity_array(level->star_greens, sizeof(level->star_greens[0]),
                            &level->star_green_count);
        return 1;
    case ENT_STAR_RED:
        *out = entity_array(level->star_reds, sizeof(level->star_reds[0]),
                            &level->star_red_count);
        return 1;
    case ENT_SPIDER:
        *out = entity_array(level->spiders, sizeof(level->spiders[0]),
                            &level->spider_count);
        return 1;
    case ENT_JUMPING_SPIDER:
        *out = entity_array(level->jumping_spiders,
                            sizeof(level->jumping_spiders[0]),
                            &level->jumping_spider_count);
        return 1;
    case ENT_BIRD:
        *out = entity_array(level->birds, sizeof(level->birds[0]),
                            &level->bird_count);
        return 1;
    case ENT_FASTER_BIRD:
        *out = entity_array(level->faster_birds, sizeof(level->faster_birds[0]),
                            &level->faster_bird_count);
        return 1;
    case ENT_FISH:
        *out = entity_array(level->fish, sizeof(level->fish[0]),
                            &level->fish_count);
        return 1;
    case ENT_FASTER_FISH:
        *out = entity_array(level->faster_fish, sizeof(level->faster_fish[0]),
                            &level->faster_fish_count);
        return 1;
    case ENT_AXE_TRAP:
        *out = entity_array(level->axe_traps, sizeof(level->axe_traps[0]),
                            &level->axe_trap_count);
        return 1;
    case ENT_CIRCULAR_SAW:
        *out = entity_array(level->circular_saws,
                            sizeof(level->circular_saws[0]),
                            &level->circular_saw_count);
        return 1;
    case ENT_SPIKE_ROW:
        *out = entity_array(level->spike_rows, sizeof(level->spike_rows[0]),
                            &level->spike_row_count);
        return 1;
    case ENT_SPIKE_PLATFORM:
        *out = entity_array(level->spike_platforms,
                            sizeof(level->spike_platforms[0]),
                            &level->spike_platform_count);
        return 1;
    case ENT_SPIKE_BLOCK:
        *out = entity_array(level->spike_blocks, sizeof(level->spike_blocks[0]),
                            &level->spike_block_count);
        return 1;
    case ENT_BLUE_FLAME:
        *out = entity_array(level->blue_flames, sizeof(level->blue_flames[0]),
                            &level->blue_flame_count);
        return 1;
    case ENT_FIRE_FLAME:
        *out = entity_array(level->fire_flames, sizeof(level->fire_flames[0]),
                            &level->fire_flame_count);
        return 1;
    case ENT_FLOAT_PLATFORM:
        *out = entity_array(level->float_platforms,
                            sizeof(level->float_platforms[0]),
                            &level->float_platform_count);
        return 1;
    case ENT_BRIDGE:
        *out = entity_array(level->bridges, sizeof(level->bridges[0]),
                            &level->bridge_count);
        return 1;
    case ENT_BOUNCEPAD_SMALL:
        *out = entity_array(level->bouncepads_small,
                            sizeof(level->bouncepads_small[0]),
                            &level->bouncepad_small_count);
        return 1;
    case ENT_BOUNCEPAD_MEDIUM:
        *out = entity_array(level->bouncepads_medium,
                            sizeof(level->bouncepads_medium[0]),
                            &level->bouncepad_medium_count);
        return 1;
    case ENT_BOUNCEPAD_HIGH:
        *out = entity_array(level->bouncepads_high,
                            sizeof(level->bouncepads_high[0]),
                            &level->bouncepad_high_count);
        return 1;
    case ENT_VINE:
        *out = entity_array(level->vines, sizeof(level->vines[0]),
                            &level->vine_count);
        return 1;
    case ENT_LADDER:
        *out = entity_array(level->ladders, sizeof(level->ladders[0]),
                            &level->ladder_count);
        return 1;
    case ENT_ROPE:
        *out = entity_array(level->ropes, sizeof(level->ropes[0]),
                            &level->rope_count);
        return 1;
    case ENT_LAST_STAR:
    case ENT_PLAYER_SPAWN:
    case ENT_COUNT:
        break;
    }
    return 0;
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

void editor_selection_reconcile(EditorState *es)
{
    if (!es) return;
    if (!editor_selection_is_valid(es)) es->selection.index = -1;
}

void editor_selection_after_remove(EditorState *es, EntityType type, int index)
{
    if (!es) return;
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
    if (select_inserted) {
        es->selection.type = type;
        es->selection.index = index;
    } else if (es->selection.type == type && es->selection.index >= index) {
        es->selection.index++;
    }
    editor_selection_reconcile(es);
}

int editor_rail_placement_tile_count(const RailPlacement *rp)
{
    Rail rail;
    return rail_build(&rail, rp) == 0 ? rail.count : 0;
}

void editor_rail_placement_position_at(const RailPlacement *rp, float t,
                                       float *x, float *y)
{
    Rail rail;
    if (!x || !y) return;
    (void)rail_build(&rail, rp);
    rail_get_world_pos(&rail, t, x, y);
}
