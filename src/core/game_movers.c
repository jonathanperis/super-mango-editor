/*
 * game_movers.c — The table of things drawn between two simulation steps.
 * game_movers.h says what it is for.
 */
#include "game_movers.h"

#include <stddef.h>   /* offsetof, size_t */
#include <string.h>   /* memcpy */

#include "../game.h"
#include "game_timing.h"   /* GAME_INTERP_MAX_JUMP */

/* How one blended float behaves. */
typedef enum {
    VALUE_POSITION,   /* pixels: blend in a straight line, snap on a big jump */
    VALUE_ANGLE       /* degrees: blend along the shorter way round           */
} ValueKind;

#define MOVER_MAX_VALUES 2

/*
 * One row: where a kind's array is in GameWorld, and which floats of each
 * item are blended.
 *
 * Most items hold their floats directly (a spider's x). A bridge is a row
 * of bricks, each with its own y_offset, so an item can also be split into
 * `parts` equal pieces: the floats are then looked up inside each piece,
 * part_stride bytes apart, starting part_offset bytes into the item. For
 * every other kind parts is 1 and the two part fields are 0.
 *
 * offsetof gives the byte offset of a field inside its struct. Copying the
 * floats through those offsets (with memcpy, which needs no cast and no
 * particular alignment) lets one loop serve every struct type.
 */
typedef struct {
    const char *name;
    size_t items_offset;   /* offsetof(GameWorld, <array>)       */
    size_t count_offset;   /* offsetof(GameWorld, <count>)       */
    size_t item_size;      /* sizeof one item                    */
    int    max_items;      /* the array's capacity               */
    int    parts;          /* pieces per item (1, or bricks)     */
    size_t part_offset;    /* first piece inside the item        */
    size_t part_stride;    /* bytes from one piece to the next   */
    int    value_count;    /* floats blended per piece           */
    struct { size_t offset; ValueKind kind; } values[MOVER_MAX_VALUES];
    size_t prev_offset;    /* offsetof(GameMoverPrev, <array>)   */
    size_t prev_size;      /* sizeof that array, checked at start */
} Mover;

#define WORLD(field)  offsetof(GameWorld, field)
#define PREV(field)   offsetof(GameMoverPrev, field), sizeof(((GameMoverPrev *)0)->field)
#define WHOLE_ITEM    1, 0, 0     /* parts, part_offset, part_stride */

static const Mover s_movers[MOVER_KIND_COUNT] = {
    /* A spider walks the floor: only x is stored. */
    [MOVER_SPIDERS] = { "spiders", WORLD(spiders), WORLD(spider_count),
        sizeof(Spider), MAX_SPIDERS, WHOLE_ITEM,
        1, {{offsetof(Spider, x), VALUE_POSITION}}, PREV(spiders) },
    [MOVER_JUMPING_SPIDERS] = { "jumping_spiders", WORLD(jumping_spiders), WORLD(jumping_spider_count),
        sizeof(JumpingSpider), MAX_JUMPING_SPIDERS, WHOLE_ITEM,
        2, {{offsetof(JumpingSpider, x), VALUE_POSITION}, {offsetof(JumpingSpider, y), VALUE_POSITION}},
        PREV(jumping_spiders) },
    /* A bird's height is a sine of its x, rebuilt when it is drawn, so
     * blending x alone keeps the sprite on its wave. */
    [MOVER_BIRDS] = { "birds", WORLD(birds), WORLD(bird_count),
        sizeof(Bird), MAX_BIRDS, WHOLE_ITEM,
        1, {{offsetof(Bird, x), VALUE_POSITION}}, PREV(birds) },
    [MOVER_FASTER_BIRDS] = { "faster_birds", WORLD(faster_birds), WORLD(faster_bird_count),
        sizeof(FasterBird), MAX_FASTER_BIRDS, WHOLE_ITEM,
        1, {{offsetof(FasterBird, x), VALUE_POSITION}}, PREV(faster_birds) },
    [MOVER_FISH] = { "fish", WORLD(fish), WORLD(fish_count),
        sizeof(Fish), MAX_FISH, WHOLE_ITEM,
        2, {{offsetof(Fish, x), VALUE_POSITION}, {offsetof(Fish, y), VALUE_POSITION}}, PREV(fish) },
    [MOVER_FASTER_FISH] = { "faster_fish", WORLD(faster_fish), WORLD(faster_fish_count),
        sizeof(FasterFish), MAX_FASTER_FISH, WHOLE_ITEM,
        2, {{offsetof(FasterFish, x), VALUE_POSITION}, {offsetof(FasterFish, y), VALUE_POSITION}},
        PREV(faster_fish) },
    [MOVER_SPIKE_BLOCKS] = { "spike_blocks", WORLD(spike_blocks), WORLD(spike_block_count),
        sizeof(SpikeBlock), MAX_SPIKE_BLOCKS, WHOLE_ITEM,
        2, {{offsetof(SpikeBlock, x), VALUE_POSITION}, {offsetof(SpikeBlock, y), VALUE_POSITION}},
        PREV(spike_blocks) },
    /* Float platforms also keep their own prev_x/prev_y for the landing
     * test; those are simulation state and are not what is used here. */
    [MOVER_FLOAT_PLATFORMS] = { "float_platforms", WORLD(float_platforms), WORLD(float_platform_count),
        sizeof(FloatPlatform), MAX_FLOAT_PLATFORMS, WHOLE_ITEM,
        2, {{offsetof(FloatPlatform, x), VALUE_POSITION}, {offsetof(FloatPlatform, y), VALUE_POSITION}},
        PREV(float_platforms) },
    [MOVER_CIRCULAR_SAWS] = { "circular_saws", WORLD(circular_saws), WORLD(circular_saw_count),
        sizeof(CircularSaw), MAX_CIRCULAR_SAWS, WHOLE_ITEM,
        2, {{offsetof(CircularSaw, x), VALUE_POSITION}, {offsetof(CircularSaw, y), VALUE_POSITION}},
        PREV(circular_saws) },
    /* An axe hangs from a fixed pivot; what moves is its angle. */
    [MOVER_AXE_TRAPS] = { "axe_traps", WORLD(axe_traps), WORLD(axe_trap_count),
        sizeof(AxeTrap), MAX_AXE_TRAPS, WHOLE_ITEM,
        1, {{offsetof(AxeTrap, angle), VALUE_ANGLE}}, PREV(axe_traps) },
    /* A flame rises and falls in its gap and turns over at the top. */
    [MOVER_BLUE_FLAMES] = { "blue_flames", WORLD(blue_flames), WORLD(blue_flame_count),
        sizeof(BlueFlame), MAX_BLUE_FLAMES, WHOLE_ITEM,
        2, {{offsetof(BlueFlame, y), VALUE_POSITION}, {offsetof(BlueFlame, angle), VALUE_ANGLE}},
        PREV(blue_flames) },
    [MOVER_FIRE_FLAMES] = { "fire_flames", WORLD(fire_flames), WORLD(fire_flame_count),
        sizeof(BlueFlame), MAX_FIRE_FLAMES, WHOLE_ITEM,
        2, {{offsetof(BlueFlame, y), VALUE_POSITION}, {offsetof(BlueFlame, angle), VALUE_ANGLE}},
        PREV(fire_flames) },
    /* A bridge does not move, but each of its bricks falls on its own. */
    [MOVER_BRIDGES] = { "bridges", WORLD(bridges), WORLD(bridge_count),
        sizeof(Bridge), MAX_BRIDGES,
        MAX_BRIDGE_BRICKS, offsetof(Bridge, bricks), sizeof(BridgeBrick),
        1, {{offsetof(BridgeBrick, y_offset), VALUE_POSITION}}, PREV(bridges) },
};

/*
 * Each row's storage must hold exactly max_items x parts x value_count
 * floats. _Static_assert is checked by the compiler: a row and an array
 * that disagree stop the build, instead of a loop writing past the array.
 */
#define PREV_HOLDS(field, items, floats_each) \
    _Static_assert(sizeof(((GameMoverPrev *)0)->field) == (size_t)(items) * (floats_each) * sizeof(float), \
                   "GameMoverPrev." #field " does not match its s_movers row")
PREV_HOLDS(spiders,         MAX_SPIDERS,         1);
PREV_HOLDS(jumping_spiders, MAX_JUMPING_SPIDERS, 2);
PREV_HOLDS(birds,           MAX_BIRDS,           1);
PREV_HOLDS(faster_birds,    MAX_FASTER_BIRDS,    1);
PREV_HOLDS(fish,            MAX_FISH,            2);
PREV_HOLDS(faster_fish,     MAX_FASTER_FISH,     2);
PREV_HOLDS(spike_blocks,    MAX_SPIKE_BLOCKS,    2);
PREV_HOLDS(float_platforms, MAX_FLOAT_PLATFORMS, 2);
PREV_HOLDS(circular_saws,   MAX_CIRCULAR_SAWS,   2);
PREV_HOLDS(axe_traps,       MAX_AXE_TRAPS,       1);
PREV_HOLDS(blue_flames,     MAX_BLUE_FLAMES,     2);
PREV_HOLDS(fire_flames,     MAX_FIRE_FLAMES,     2);
PREV_HOLDS(bridges,         MAX_BRIDGES,         MAX_BRIDGE_BRICKS);

/* How many floats one item of a row stores. */
static int floats_per_item(const Mover *m)
{
    return m->parts * m->value_count;
}

/* The row's item count in this world, held to the array's capacity. */
static int item_count(const Mover *m, const GameWorld *world)
{
    int count;
    memcpy(&count, (const char *)world + m->count_offset, sizeof(count));
    if (count < 0) count = 0;
    if (count > m->max_items) count = m->max_items;
    return count;
}

const char *game_mover_name(GameMoverKind kind)
{
    if ((int)kind < 0 || kind >= MOVER_KIND_COUNT) return NULL;
    return s_movers[kind].name;
}

void game_movers_note(GameState *gs)
{
    const GameWorld *world = &gs->world;

    for (int kind = 0; kind < MOVER_KIND_COUNT; kind++) {
        const Mover *m = &s_movers[kind];
        const char *item = (const char *)world + m->items_offset;
        float *prev = (float *)((char *)&gs->screen.loop.movers + m->prev_offset);
        int count = item_count(m, world);

        /* A table row and its storage must agree, or this loop would write
         * past the array. The sizes are fixed at compile time, so a wrong
         * row fails on the first step of the first test that runs. */
        if ((size_t)m->max_items * (size_t)floats_per_item(m) * sizeof(float) != m->prev_size)
            continue;

        for (int i = 0; i < count; i++, item += m->item_size) {
            for (int p = 0; p < m->parts; p++) {
                const char *part = item + m->part_offset + (size_t)p * m->part_stride;
                for (int v = 0; v < m->value_count; v++)
                    memcpy(prev++, part + m->values[v].offset, sizeof(float));
            }
        }
    }
}

/* from + (to - from) * t along the shorter way round a circle of 360. */
static float blend_angle(float from, float to, float t)
{
    float d = to - from;
    while (d > 180.0f) d -= 360.0f;
    while (d < -180.0f) d += 360.0f;
    return to - d * (1.0f - t);
}

const void *game_movers_shown(const GameState *gs, GameMoverKind kind, float t,
                              void *shown, size_t shown_size, int *count)
{
    const Mover *m;
    const void *items;
    const float *prev;
    char *item;
    int n;

    *count = 0;
    if ((int)kind < 0 || kind >= MOVER_KIND_COUNT) return NULL;
    m = &s_movers[kind];
    items = (const char *)&gs->world + m->items_offset;
    n = item_count(m, &gs->world);
    *count = n;
    /* The latest step itself: nothing to blend, nothing to copy. */
    if (t >= 1.0f || n == 0 || !gs->screen.loop.has_prev) return items;
    if (!shown || shown_size < (size_t)n * m->item_size ||
        (size_t)m->max_items * (size_t)floats_per_item(m) * sizeof(float) != m->prev_size)
        return items;
    if (t < 0.0f) t = 0.0f;

    memcpy(shown, items, (size_t)n * m->item_size);
    prev = (const float *)((const char *)&gs->screen.loop.movers + m->prev_offset);
    item = shown;
    for (int i = 0; i < n; i++, item += m->item_size) {
        for (int p = 0; p < m->parts; p++, prev += m->value_count) {
            char *part = item + m->part_offset + (size_t)p * m->part_stride;
            float now[MOVER_MAX_VALUES];
            int jumped = 0;

            for (int v = 0; v < m->value_count; v++)
                memcpy(&now[v], part + m->values[v].offset, sizeof(float));
            /* A respawn may move one axis far and not the other; both
             * belong to the same jump, so if either position did, every
             * position of this piece is shown where it is now. */
            for (int v = 0; v < m->value_count; v++) {
                float d = now[v] - prev[v];
                if (m->values[v].kind == VALUE_POSITION &&
                    (d > GAME_INTERP_MAX_JUMP || d < -GAME_INTERP_MAX_JUMP))
                    jumped = 1;
            }
            for (int v = 0; v < m->value_count; v++) {
                float value = now[v];
                if (m->values[v].kind == VALUE_ANGLE)
                    value = blend_angle(prev[v], now[v], t);
                else if (!jumped)
                    value = prev[v] + (now[v] - prev[v]) * t;
                memcpy(part + m->values[v].offset, &value, sizeof(float));
            }
        }
    }
    return shown;
}
