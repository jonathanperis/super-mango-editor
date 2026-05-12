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

int editor_rail_placement_tile_count(const RailPlacement *rp)
{
    if (!rp) return 0;
    if (rp->layout == RAIL_LAYOUT_RECT) {
        return rp->w * 2 + (rp->h - 2) * 2;
    }
    return rp->w;
}

static void rail_rect_tile_position(const RailPlacement *rp, int idx,
                                    float *x, float *y)
{
    int w = rp->w;
    int h = rp->h;

    if (idx < w) {
        *x = (float)(rp->x + idx * RAIL_TILE_W + RAIL_TILE_W / 2);
        *y = (float)(rp->y + RAIL_TILE_H / 2);
        return;
    }

    idx -= w;
    if (idx < h - 2) {
        *x = (float)(rp->x + (w - 1) * RAIL_TILE_W + RAIL_TILE_W / 2);
        *y = (float)(rp->y + (idx + 1) * RAIL_TILE_H + RAIL_TILE_H / 2);
        return;
    }

    idx -= h - 2;
    if (idx < w) {
        *x = (float)(rp->x + (w - 1 - idx) * RAIL_TILE_W + RAIL_TILE_W / 2);
        *y = (float)(rp->y + (h - 1) * RAIL_TILE_H + RAIL_TILE_H / 2);
        return;
    }

    idx -= w;
    *x = (float)(rp->x + RAIL_TILE_W / 2);
    *y = (float)(rp->y + (h - 2 - idx) * RAIL_TILE_H + RAIL_TILE_H / 2);
}

void editor_rail_placement_position_at(const RailPlacement *rp, float t,
                                       float *x, float *y)
{
    int count;

    if (!rp || !x || !y) return;

    count = editor_rail_placement_tile_count(rp);
    if (count <= 0) {
        *x = (float)rp->x;
        *y = (float)rp->y;
        return;
    }

    if (rp->layout == RAIL_LAYOUT_HORIZ) {
        *x = (float)rp->x + t * (float)RAIL_TILE_W + (float)RAIL_TILE_W / 2.0f;
        *y = (float)rp->y + (float)RAIL_TILE_H / 2.0f;
        return;
    }

    while (t < 0.0f) t += (float)count;
    while (t >= (float)count) t -= (float)count;

    {
        int tile = (int)t;
        int next = (tile + 1) % count;
        float frac = t - (float)tile;
        float ax, ay, bx, by;

        rail_rect_tile_position(rp, tile, &ax, &ay);
        rail_rect_tile_position(rp, next, &bx, &by);
        *x = ax + (bx - ax) * frac;
        *y = ay + (by - ay) * frac;
    }
}
