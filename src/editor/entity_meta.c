/*
 * entity_meta.c — Shared editor geometry helpers.
 */

#include "entity_meta.h"

typedef struct {
    EntityType type;
    const char *type_name;
    const char *palette_name;
    EditorEntityCategory category;
    int singleton;
} EditorEntityMeta;

static const EditorEntityMeta s_entity_meta[ENT_COUNT] = {
    [ENT_FLOOR_GAP] = {
        ENT_FLOOR_GAP, "Floor Gap", "Floor Gap",
        EDITOR_ENTITY_CATEGORY_WORLD, 0
    },
    [ENT_CHECKPOINT] = {
        ENT_CHECKPOINT, "Checkpoint", "Checkpoint",
        EDITOR_ENTITY_CATEGORY_WORLD, 0
    },
    [ENT_RAIL] = {
        ENT_RAIL, "Rail", "Rail", EDITOR_ENTITY_CATEGORY_WORLD, 0
    },
    [ENT_PLATFORM] = {
        ENT_PLATFORM, "Platform", "Platform", EDITOR_ENTITY_CATEGORY_SURFACES, 0
    },
    [ENT_COIN] = {
        ENT_COIN, "Coin", "Coin", EDITOR_ENTITY_CATEGORY_COLLECTIBLES, 0
    },
    [ENT_STAR_YELLOW] = {
        ENT_STAR_YELLOW, "Star Yellow", "Star Yellow",
        EDITOR_ENTITY_CATEGORY_COLLECTIBLES, 0
    },
    [ENT_STAR_GREEN] = {
        ENT_STAR_GREEN, "Star Green", "Star Green",
        EDITOR_ENTITY_CATEGORY_COLLECTIBLES, 0
    },
    [ENT_STAR_RED] = {
        ENT_STAR_RED, "Star Red", "Star Red",
        EDITOR_ENTITY_CATEGORY_COLLECTIBLES, 0
    },
    [ENT_LAST_STAR] = {
        ENT_LAST_STAR, "Last Star", "Last Star",
        EDITOR_ENTITY_CATEGORY_COLLECTIBLES, 1
    },
    [ENT_SPIDER] = {
        ENT_SPIDER, "Spider", "Spider", EDITOR_ENTITY_CATEGORY_ENEMIES, 0
    },
    [ENT_JUMPING_SPIDER] = {
        ENT_JUMPING_SPIDER, "Jumping Spider", "Jumping Spider",
        EDITOR_ENTITY_CATEGORY_ENEMIES, 0
    },
    [ENT_BIRD] = {
        ENT_BIRD, "Bird", "Bird", EDITOR_ENTITY_CATEGORY_ENEMIES, 0
    },
    [ENT_FASTER_BIRD] = {
        ENT_FASTER_BIRD, "Faster Bird", "Faster Bird",
        EDITOR_ENTITY_CATEGORY_ENEMIES, 0
    },
    [ENT_FISH] = {
        ENT_FISH, "Fish", "Fish", EDITOR_ENTITY_CATEGORY_ENEMIES, 0
    },
    [ENT_FASTER_FISH] = {
        ENT_FASTER_FISH, "Faster Fish", "Faster Fish",
        EDITOR_ENTITY_CATEGORY_ENEMIES, 0
    },
    [ENT_AXE_TRAP] = {
        ENT_AXE_TRAP, "Axe Trap", "Axe Trap",
        EDITOR_ENTITY_CATEGORY_HAZARDS, 0
    },
    [ENT_CIRCULAR_SAW] = {
        ENT_CIRCULAR_SAW, "Circular Saw", "Circular Saw",
        EDITOR_ENTITY_CATEGORY_HAZARDS, 0
    },
    [ENT_SPIKE_ROW] = {
        ENT_SPIKE_ROW, "Spike Row", "Spike Row",
        EDITOR_ENTITY_CATEGORY_HAZARDS, 0
    },
    [ENT_SPIKE_PLATFORM] = {
        ENT_SPIKE_PLATFORM, "Spike Platform", "Spike Platform",
        EDITOR_ENTITY_CATEGORY_HAZARDS, 0
    },
    [ENT_SPIKE_BLOCK] = {
        ENT_SPIKE_BLOCK, "Spike Block", "Spike Block",
        EDITOR_ENTITY_CATEGORY_HAZARDS, 0
    },
    [ENT_BLUE_FLAME] = {
        ENT_BLUE_FLAME, "Blue Flame", "Blue Flame",
        EDITOR_ENTITY_CATEGORY_HAZARDS, 0
    },
    [ENT_FIRE_FLAME] = {
        ENT_FIRE_FLAME, "Fire Flame", "Fire Flame",
        EDITOR_ENTITY_CATEGORY_HAZARDS, 0
    },
    [ENT_FLOAT_PLATFORM] = {
        ENT_FLOAT_PLATFORM, "Float Platform", "Float Platform",
        EDITOR_ENTITY_CATEGORY_SURFACES, 0
    },
    [ENT_BRIDGE] = {
        ENT_BRIDGE, "Bridge", "Bridge", EDITOR_ENTITY_CATEGORY_SURFACES, 0
    },
    [ENT_BOUNCEPAD_SMALL] = {
        ENT_BOUNCEPAD_SMALL, "Bouncepad (S)", "Bouncepad Small",
        EDITOR_ENTITY_CATEGORY_SURFACES, 0
    },
    [ENT_BOUNCEPAD_MEDIUM] = {
        ENT_BOUNCEPAD_MEDIUM, "Bouncepad (M)", "Bouncepad Medium",
        EDITOR_ENTITY_CATEGORY_SURFACES, 0
    },
    [ENT_BOUNCEPAD_HIGH] = {
        ENT_BOUNCEPAD_HIGH, "Bouncepad (H)", "Bouncepad High",
        EDITOR_ENTITY_CATEGORY_SURFACES, 0
    },
    [ENT_VINE] = {
        ENT_VINE, "Vine", "Vine", EDITOR_ENTITY_CATEGORY_DECORATIONS, 0
    },
    [ENT_LADDER] = {
        ENT_LADDER, "Ladder", "Ladder", EDITOR_ENTITY_CATEGORY_DECORATIONS, 0
    },
    [ENT_ROPE] = {
        ENT_ROPE, "Rope", "Rope", EDITOR_ENTITY_CATEGORY_DECORATIONS, 0
    },
    [ENT_PLAYER_SPAWN] = {
        ENT_PLAYER_SPAWN, "Player Spawn", "Player Spawn",
        EDITOR_ENTITY_CATEGORY_WORLD, 1
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

static int editor_entity_capacity(EntityType type)
{
    switch (type) {
    case ENT_FLOOR_GAP:        return MAX_FLOOR_GAPS;
    case ENT_CHECKPOINT:       return MAX_CHECKPOINTS;
    case ENT_RAIL:             return MAX_RAILS;
    case ENT_PLATFORM:         return MAX_PLATFORMS;
    case ENT_COIN:             return MAX_COINS;
    case ENT_STAR_YELLOW:      return MAX_STAR_YELLOWS;
    case ENT_STAR_GREEN:       return MAX_STAR_GREENS;
    case ENT_STAR_RED:         return MAX_STAR_REDS;
    case ENT_LAST_STAR:        return 1;
    case ENT_SPIDER:           return MAX_SPIDERS;
    case ENT_JUMPING_SPIDER:   return MAX_JUMPING_SPIDERS;
    case ENT_BIRD:             return MAX_BIRDS;
    case ENT_FASTER_BIRD:      return MAX_FASTER_BIRDS;
    case ENT_FISH:             return MAX_FISH;
    case ENT_FASTER_FISH:      return MAX_FASTER_FISH;
    case ENT_AXE_TRAP:         return MAX_AXE_TRAPS;
    case ENT_CIRCULAR_SAW:     return MAX_CIRCULAR_SAWS;
    case ENT_SPIKE_ROW:        return MAX_SPIKE_ROWS;
    case ENT_SPIKE_PLATFORM:   return MAX_SPIKE_PLATFORMS;
    case ENT_SPIKE_BLOCK:      return MAX_SPIKE_BLOCKS;
    case ENT_BLUE_FLAME:       return MAX_BLUE_FLAMES;
    case ENT_FIRE_FLAME:       return MAX_FIRE_FLAMES;
    case ENT_FLOAT_PLATFORM:   return MAX_FLOAT_PLATFORMS;
    case ENT_BRIDGE:            return MAX_BRIDGES;
    case ENT_BOUNCEPAD_SMALL:  return MAX_BOUNCEPADS_SMALL;
    case ENT_BOUNCEPAD_MEDIUM: return MAX_BOUNCEPADS_MEDIUM;
    case ENT_BOUNCEPAD_HIGH:   return MAX_BOUNCEPADS_HIGH;
    case ENT_VINE:              return MAX_VINES;
    case ENT_LADDER:            return MAX_LADDERS;
    case ENT_ROPE:              return MAX_ROPES;
    case ENT_PLAYER_SPAWN:      return 1;
    case ENT_COUNT:             return 0;
    }
    return 0;
}

int editor_entity_count(const LevelDef *level, EntityType type)
{
    int count;
    int capacity;

    if (!level || type < 0 || type >= ENT_COUNT) return 0;

    switch (type) {
    case ENT_FLOOR_GAP:        count = level->floor_gap_count; break;
    case ENT_CHECKPOINT:       count = level->checkpoint_count; break;
    case ENT_RAIL:             count = level->rail_count; break;
    case ENT_PLATFORM:         count = level->platform_count; break;
    case ENT_COIN:             count = level->coin_count; break;
    case ENT_STAR_YELLOW:      count = level->star_yellow_count; break;
    case ENT_STAR_GREEN:       count = level->star_green_count; break;
    case ENT_STAR_RED:         count = level->star_red_count; break;
    case ENT_LAST_STAR:        count = 1; break;
    case ENT_SPIDER:           count = level->spider_count; break;
    case ENT_JUMPING_SPIDER:   count = level->jumping_spider_count; break;
    case ENT_BIRD:             count = level->bird_count; break;
    case ENT_FASTER_BIRD:      count = level->faster_bird_count; break;
    case ENT_FISH:             count = level->fish_count; break;
    case ENT_FASTER_FISH:      count = level->faster_fish_count; break;
    case ENT_AXE_TRAP:         count = level->axe_trap_count; break;
    case ENT_CIRCULAR_SAW:     count = level->circular_saw_count; break;
    case ENT_SPIKE_ROW:        count = level->spike_row_count; break;
    case ENT_SPIKE_PLATFORM:   count = level->spike_platform_count; break;
    case ENT_SPIKE_BLOCK:      count = level->spike_block_count; break;
    case ENT_BLUE_FLAME:       count = level->blue_flame_count; break;
    case ENT_FIRE_FLAME:       count = level->fire_flame_count; break;
    case ENT_FLOAT_PLATFORM:   count = level->float_platform_count; break;
    case ENT_BRIDGE:           count = level->bridge_count; break;
    case ENT_BOUNCEPAD_SMALL:  count = level->bouncepad_small_count; break;
    case ENT_BOUNCEPAD_MEDIUM: count = level->bouncepad_medium_count; break;
    case ENT_BOUNCEPAD_HIGH:   count = level->bouncepad_high_count; break;
    case ENT_VINE:             count = level->vine_count; break;
    case ENT_LADDER:           count = level->ladder_count; break;
    case ENT_ROPE:             count = level->rope_count; break;
    case ENT_PLAYER_SPAWN:     count = 1; break;
    case ENT_COUNT:            count = 0; break;
    }

    capacity = editor_entity_capacity(type);
    if (count < 0) return 0;
    return count > capacity ? capacity : count;
}

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
