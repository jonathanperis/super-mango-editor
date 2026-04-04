/*
 * serializer.c — JSON serialization for level definitions.
 *
 * Converts between LevelDef structs (the engine's in-memory level format)
 * and cJSON trees (a lightweight JSON DOM).  This lets the editor save
 * levels as human-readable .json files and load them back for editing or
 * playtesting.
 *
 * cJSON works like a tree of nodes.  Each node has a type (object, array,
 * number, string) and can have children.  We build trees by creating nodes
 * and attaching them, then render the whole tree to a JSON string.  For
 * parsing, cJSON reads a JSON string and gives us back a tree we can walk
 * with cJSON_GetObjectItem / cJSON_GetArrayItem.
 *
 * All enum values are serialized as readable strings ("RECT", "SPIN", etc.)
 * rather than raw integers, so the JSON files are easy to understand and
 * edit by hand.
 */

#include <stdio.h>   /* fprintf, fopen, fclose, fread, fseek, ftell */
#include <stdlib.h>  /* malloc, free, exit */
#include <string.h>  /* strcmp, memset, strdup */

#include "serializer.h"
#include "../../vendor/cJSON/cJSON.h" /* cJSON API */
#include "../levels/level.h"          /* LevelDef, all placement types */
#include "../game.h"                  /* MAX_* constants */

/* ================================================================== */
/* Enum ↔ string conversion helpers                                    */
/* ================================================================== */

/*
 * Each enum has a pair of functions: one converts the C enum value to a
 * human-readable string for JSON output; the other parses a string back
 * into the enum value.  This keeps the JSON portable and self-documenting.
 */

/* ---- RailLayout -------------------------------------------------- */

static const char *rail_layout_to_str(RailLayout layout) {
    switch (layout) {
        case RAIL_LAYOUT_RECT:  return "RECT";
        case RAIL_LAYOUT_HORIZ: return "HORIZ";
        default:                return "RECT";
    }
}

static RailLayout rail_layout_from_str(const char *s) {
    if (s && strcmp(s, "HORIZ") == 0) return RAIL_LAYOUT_HORIZ;
    return RAIL_LAYOUT_RECT;  /* default to RECT for unknown values */
}

/* ---- AxeTrapMode ------------------------------------------------- */

static const char *axe_mode_to_str(AxeTrapMode mode) {
    switch (mode) {
        case AXE_MODE_PENDULUM: return "PENDULUM";
        case AXE_MODE_SPIN:     return "SPIN";
        default:                return "PENDULUM";
    }
}

static AxeTrapMode axe_mode_from_str(const char *s) {
    if (s && strcmp(s, "SPIN") == 0) return AXE_MODE_SPIN;
    return AXE_MODE_PENDULUM;  /* default to PENDULUM for unknown values */
}

/* ---- FloatPlatformMode ------------------------------------------- */

static const char *float_mode_to_str(FloatPlatformMode mode) {
    switch (mode) {
        case FLOAT_PLATFORM_STATIC:  return "STATIC";
        case FLOAT_PLATFORM_CRUMBLE: return "CRUMBLE";
        case FLOAT_PLATFORM_RAIL:    return "RAIL";
        default:                     return "STATIC";
    }
}

static FloatPlatformMode float_mode_from_str(const char *s) {
    if (s && strcmp(s, "CRUMBLE") == 0) return FLOAT_PLATFORM_CRUMBLE;
    if (s && strcmp(s, "RAIL") == 0)    return FLOAT_PLATFORM_RAIL;
    return FLOAT_PLATFORM_STATIC;  /* default to STATIC for unknown values */
}

/* ---- BouncepadType ----------------------------------------------- */

static const char *bouncepad_type_to_str(BouncepadType type) {
    switch (type) {
        case BOUNCEPAD_GREEN: return "GREEN";
        case BOUNCEPAD_WOOD:  return "WOOD";
        case BOUNCEPAD_RED:   return "RED";
        default:              return "GREEN";
    }
}

static BouncepadType bouncepad_type_from_str(const char *s) {
    if (s && strcmp(s, "WOOD") == 0) return BOUNCEPAD_WOOD;
    if (s && strcmp(s, "RED") == 0)  return BOUNCEPAD_RED;
    return BOUNCEPAD_GREEN;  /* default to GREEN for unknown values */
}

/* ================================================================== */
/* JSON helper: safely read a number from a cJSON object field         */
/* ================================================================== */

/*
 * get_number — Read a numeric field from a JSON object.
 *
 * cJSON stores all numbers as doubles internally.  This helper returns
 * the double value of the named field, or `fallback` if the field is
 * missing or not a number.  We use this everywhere to avoid repeating
 * the NULL-check + type-check boilerplate.
 */
static double get_number(const cJSON *obj, const char *key, double fallback) {
    /*
     * cJSON_GetObjectItemCaseSensitive — look up a child node by key name.
     * Returns NULL if the key doesn't exist in this object.
     */
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);

    /*
     * cJSON_IsNumber — returns true (non-zero) if the node holds a number.
     * We check this to avoid reading garbage from a string or null node.
     */
    if (cJSON_IsNumber(item)) return item->valuedouble;
    return fallback;
}

/*
 * get_string — Read a string field from a JSON object.
 *
 * Returns the string pointer (owned by the cJSON tree — do NOT free it),
 * or `fallback` if the field is missing or not a string.
 */
static const char *get_string(const cJSON *obj, const char *key,
                              const char *fallback) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsString(item) && item->valuestring) return item->valuestring;
    return fallback;
}

/* ================================================================== */
/* level_to_json — Build a cJSON tree from a LevelDef                  */
/* ================================================================== */

cJSON *level_to_json(const LevelDef *def) {
    if (!def) return NULL;

    /*
     * cJSON_CreateObject — allocate a new JSON object node (the root {}).
     * Every key/value pair we add becomes a child of this root.
     * Returns NULL if memory allocation fails.
     */
    cJSON *root = cJSON_CreateObject();
    if (!root) return NULL;

    /* ---- Name ---------------------------------------------------- */

    /*
     * cJSON_AddStringToObject — create a string node and attach it to
     * the parent object under the given key.  cJSON copies the string
     * internally, so `def->name` doesn't need to outlive this call.
     */
    cJSON_AddStringToObject(root, "name",
                            def->name ? def->name : "Untitled");

    /* ---- Sea gaps ------------------------------------------------ */

    /*
     * cJSON_AddArrayToObject — create an empty JSON array [] and attach
     * it to the parent object.  We then fill it by adding items one by one.
     */
    cJSON *sea_arr = cJSON_AddArrayToObject(root, "sea_gaps");
    for (int i = 0; i < def->sea_gap_count; i++) {
        /*
         * cJSON_CreateNumber — allocate a number node with the given value.
         * cJSON_AddItemToArray — append that node to the array.
         * Sea gaps are plain integers (left-edge x coordinates).
         */
        cJSON_AddItemToArray(sea_arr, cJSON_CreateNumber(def->sea_gaps[i]));
    }

    /* ---- Rails --------------------------------------------------- */

    cJSON *rail_arr = cJSON_AddArrayToObject(root, "rails");
    for (int i = 0; i < def->rail_count; i++) {
        const RailPlacement *r = &def->rails[i];

        /*
         * cJSON_CreateObject — each rail is a JSON object with named fields.
         * We build it, add all fields, then append the whole object to the
         * rails array.
         */
        cJSON *obj = cJSON_CreateObject();
        cJSON_AddStringToObject(obj, "layout", rail_layout_to_str(r->layout));
        cJSON_AddNumberToObject(obj, "x", r->x);
        cJSON_AddNumberToObject(obj, "y", r->y);
        cJSON_AddNumberToObject(obj, "w", r->w);
        cJSON_AddNumberToObject(obj, "h", r->h);
        cJSON_AddNumberToObject(obj, "end_cap", r->end_cap);
        cJSON_AddItemToArray(rail_arr, obj);
    }

    /* ---- Platforms ------------------------------------------------ */

    cJSON *plat_arr = cJSON_AddArrayToObject(root, "platforms");
    for (int i = 0; i < def->platform_count; i++) {
        const PlatformPlacement *p = &def->platforms[i];
        cJSON *obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(obj, "x", p->x);
        cJSON_AddNumberToObject(obj, "tile_height", p->tile_height);
        cJSON_AddItemToArray(plat_arr, obj);
    }

    /* ---- Coins --------------------------------------------------- */

    cJSON *coin_arr = cJSON_AddArrayToObject(root, "coins");
    for (int i = 0; i < def->coin_count; i++) {
        const CoinPlacement *c = &def->coins[i];
        cJSON *obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(obj, "x", c->x);
        cJSON_AddNumberToObject(obj, "y", c->y);
        cJSON_AddItemToArray(coin_arr, obj);
    }

    /* ---- Yellow stars --------------------------------------------- */

    cJSON *ystar_arr = cJSON_AddArrayToObject(root, "yellow_stars");
    for (int i = 0; i < def->yellow_star_count; i++) {
        const YellowStarPlacement *s = &def->yellow_stars[i];
        cJSON *obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(obj, "x", s->x);
        cJSON_AddNumberToObject(obj, "y", s->y);
        cJSON_AddItemToArray(ystar_arr, obj);
    }

    /* ---- Last star (single object, not array) -------------------- */

    {
        /*
         * last_star is a single struct, not an array.  We serialize it as
         * a JSON object with x and y fields, attached directly to the root.
         */
        const LastStarPlacement *ls = &def->last_star;
        cJSON *obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(obj, "x", ls->x);
        cJSON_AddNumberToObject(obj, "y", ls->y);
        cJSON_AddItemToObject(root, "last_star", obj);
    }

    /* ---- Spiders ------------------------------------------------- */

    cJSON *spider_arr = cJSON_AddArrayToObject(root, "spiders");
    for (int i = 0; i < def->spider_count; i++) {
        const SpiderPlacement *sp = &def->spiders[i];
        cJSON *obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(obj, "x", sp->x);
        cJSON_AddNumberToObject(obj, "vx", sp->vx);
        cJSON_AddNumberToObject(obj, "patrol_x0", sp->patrol_x0);
        cJSON_AddNumberToObject(obj, "patrol_x1", sp->patrol_x1);
        cJSON_AddNumberToObject(obj, "frame_index", sp->frame_index);
        cJSON_AddItemToArray(spider_arr, obj);
    }

    /* ---- Jumping spiders ----------------------------------------- */

    cJSON *jspider_arr = cJSON_AddArrayToObject(root, "jumping_spiders");
    for (int i = 0; i < def->jumping_spider_count; i++) {
        const JumpingSpiderPlacement *js = &def->jumping_spiders[i];
        cJSON *obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(obj, "x", js->x);
        cJSON_AddNumberToObject(obj, "vx", js->vx);
        cJSON_AddNumberToObject(obj, "patrol_x0", js->patrol_x0);
        cJSON_AddNumberToObject(obj, "patrol_x1", js->patrol_x1);
        cJSON_AddItemToArray(jspider_arr, obj);
    }

    /* ---- Birds --------------------------------------------------- */

    cJSON *bird_arr = cJSON_AddArrayToObject(root, "birds");
    for (int i = 0; i < def->bird_count; i++) {
        const BirdPlacement *b = &def->birds[i];
        cJSON *obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(obj, "x", b->x);
        cJSON_AddNumberToObject(obj, "base_y", b->base_y);
        cJSON_AddNumberToObject(obj, "vx", b->vx);
        cJSON_AddNumberToObject(obj, "patrol_x0", b->patrol_x0);
        cJSON_AddNumberToObject(obj, "patrol_x1", b->patrol_x1);
        cJSON_AddNumberToObject(obj, "frame_index", b->frame_index);
        cJSON_AddItemToArray(bird_arr, obj);
    }

    /* ---- Faster birds -------------------------------------------- */

    cJSON *fbird_arr = cJSON_AddArrayToObject(root, "faster_birds");
    for (int i = 0; i < def->faster_bird_count; i++) {
        const BirdPlacement *b = &def->faster_birds[i];
        cJSON *obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(obj, "x", b->x);
        cJSON_AddNumberToObject(obj, "base_y", b->base_y);
        cJSON_AddNumberToObject(obj, "vx", b->vx);
        cJSON_AddNumberToObject(obj, "patrol_x0", b->patrol_x0);
        cJSON_AddNumberToObject(obj, "patrol_x1", b->patrol_x1);
        cJSON_AddNumberToObject(obj, "frame_index", b->frame_index);
        cJSON_AddItemToArray(fbird_arr, obj);
    }

    /* ---- Fish ---------------------------------------------------- */

    cJSON *fish_arr = cJSON_AddArrayToObject(root, "fish");
    for (int i = 0; i < def->fish_count; i++) {
        const FishPlacement *f = &def->fish[i];
        cJSON *obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(obj, "x", f->x);
        cJSON_AddNumberToObject(obj, "vx", f->vx);
        cJSON_AddNumberToObject(obj, "patrol_x0", f->patrol_x0);
        cJSON_AddNumberToObject(obj, "patrol_x1", f->patrol_x1);
        cJSON_AddItemToArray(fish_arr, obj);
    }

    /* ---- Faster fish --------------------------------------------- */

    cJSON *ffish_arr = cJSON_AddArrayToObject(root, "faster_fish");
    for (int i = 0; i < def->faster_fish_count; i++) {
        const FishPlacement *f = &def->faster_fish[i];
        cJSON *obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(obj, "x", f->x);
        cJSON_AddNumberToObject(obj, "vx", f->vx);
        cJSON_AddNumberToObject(obj, "patrol_x0", f->patrol_x0);
        cJSON_AddNumberToObject(obj, "patrol_x1", f->patrol_x1);
        cJSON_AddItemToArray(ffish_arr, obj);
    }

    /* ---- Axe traps ----------------------------------------------- */

    cJSON *axe_arr = cJSON_AddArrayToObject(root, "axe_traps");
    for (int i = 0; i < def->axe_trap_count; i++) {
        const AxeTrapPlacement *a = &def->axe_traps[i];
        cJSON *obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(obj, "pillar_x", a->pillar_x);
        cJSON_AddStringToObject(obj, "mode", axe_mode_to_str(a->mode));
        cJSON_AddItemToArray(axe_arr, obj);
    }

    /* ---- Circular saws ------------------------------------------- */

    cJSON *saw_arr = cJSON_AddArrayToObject(root, "circular_saws");
    for (int i = 0; i < def->circular_saw_count; i++) {
        const CircularSawPlacement *cs = &def->circular_saws[i];
        cJSON *obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(obj, "x", cs->x);
        cJSON_AddNumberToObject(obj, "patrol_x0", cs->patrol_x0);
        cJSON_AddNumberToObject(obj, "patrol_x1", cs->patrol_x1);
        cJSON_AddNumberToObject(obj, "direction", cs->direction);
        cJSON_AddItemToArray(saw_arr, obj);
    }

    /* ---- Spike rows ---------------------------------------------- */

    cJSON *srow_arr = cJSON_AddArrayToObject(root, "spike_rows");
    for (int i = 0; i < def->spike_row_count; i++) {
        const SpikeRowPlacement *sr = &def->spike_rows[i];
        cJSON *obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(obj, "x", sr->x);
        cJSON_AddNumberToObject(obj, "count", sr->count);
        cJSON_AddItemToArray(srow_arr, obj);
    }

    /* ---- Spike platforms ----------------------------------------- */

    cJSON *splat_arr = cJSON_AddArrayToObject(root, "spike_platforms");
    for (int i = 0; i < def->spike_platform_count; i++) {
        const SpikePlatformPlacement *sp = &def->spike_platforms[i];
        cJSON *obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(obj, "x", sp->x);
        cJSON_AddNumberToObject(obj, "y", sp->y);
        cJSON_AddNumberToObject(obj, "tile_count", sp->tile_count);
        cJSON_AddItemToArray(splat_arr, obj);
    }

    /* ---- Spike blocks -------------------------------------------- */

    cJSON *sblk_arr = cJSON_AddArrayToObject(root, "spike_blocks");
    for (int i = 0; i < def->spike_block_count; i++) {
        const SpikeBlockPlacement *sb = &def->spike_blocks[i];
        cJSON *obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(obj, "rail_index", sb->rail_index);
        cJSON_AddNumberToObject(obj, "t_offset", sb->t_offset);
        cJSON_AddNumberToObject(obj, "speed", sb->speed);
        cJSON_AddItemToArray(sblk_arr, obj);
    }

    /* ---- Float platforms ----------------------------------------- */

    cJSON *fp_arr = cJSON_AddArrayToObject(root, "float_platforms");
    for (int i = 0; i < def->float_platform_count; i++) {
        const FloatPlatformPlacement *fp = &def->float_platforms[i];
        cJSON *obj = cJSON_CreateObject();
        cJSON_AddStringToObject(obj, "mode", float_mode_to_str(fp->mode));
        cJSON_AddNumberToObject(obj, "x", fp->x);
        cJSON_AddNumberToObject(obj, "y", fp->y);
        cJSON_AddNumberToObject(obj, "tile_count", fp->tile_count);
        cJSON_AddNumberToObject(obj, "rail_index", fp->rail_index);
        cJSON_AddNumberToObject(obj, "t_offset", fp->t_offset);
        cJSON_AddNumberToObject(obj, "speed", fp->speed);
        cJSON_AddItemToArray(fp_arr, obj);
    }

    /* ---- Bridges ------------------------------------------------- */

    cJSON *br_arr = cJSON_AddArrayToObject(root, "bridges");
    for (int i = 0; i < def->bridge_count; i++) {
        const BridgePlacement *br = &def->bridges[i];
        cJSON *obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(obj, "x", br->x);
        cJSON_AddNumberToObject(obj, "y", br->y);
        cJSON_AddNumberToObject(obj, "brick_count", br->brick_count);
        cJSON_AddItemToArray(br_arr, obj);
    }

    /* ---- Bouncepads (small) -------------------------------------- */

    cJSON *bs_arr = cJSON_AddArrayToObject(root, "bouncepads_small");
    for (int i = 0; i < def->bouncepad_small_count; i++) {
        const BouncepadPlacement *bp = &def->bouncepads_small[i];
        cJSON *obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(obj, "x", bp->x);
        cJSON_AddNumberToObject(obj, "launch_vy", bp->launch_vy);
        cJSON_AddStringToObject(obj, "pad_type",
                                bouncepad_type_to_str(bp->pad_type));
        cJSON_AddItemToArray(bs_arr, obj);
    }

    /* ---- Bouncepads (medium) ------------------------------------- */

    cJSON *bm_arr = cJSON_AddArrayToObject(root, "bouncepads_medium");
    for (int i = 0; i < def->bouncepad_medium_count; i++) {
        const BouncepadPlacement *bp = &def->bouncepads_medium[i];
        cJSON *obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(obj, "x", bp->x);
        cJSON_AddNumberToObject(obj, "launch_vy", bp->launch_vy);
        cJSON_AddStringToObject(obj, "pad_type",
                                bouncepad_type_to_str(bp->pad_type));
        cJSON_AddItemToArray(bm_arr, obj);
    }

    /* ---- Bouncepads (high) --------------------------------------- */

    cJSON *bh_arr = cJSON_AddArrayToObject(root, "bouncepads_high");
    for (int i = 0; i < def->bouncepad_high_count; i++) {
        const BouncepadPlacement *bp = &def->bouncepads_high[i];
        cJSON *obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(obj, "x", bp->x);
        cJSON_AddNumberToObject(obj, "launch_vy", bp->launch_vy);
        cJSON_AddStringToObject(obj, "pad_type",
                                bouncepad_type_to_str(bp->pad_type));
        cJSON_AddItemToArray(bh_arr, obj);
    }

    /* ---- Vines --------------------------------------------------- */

    cJSON *vine_arr = cJSON_AddArrayToObject(root, "vines");
    for (int i = 0; i < def->vine_count; i++) {
        const VinePlacement *v = &def->vines[i];
        cJSON *obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(obj, "x", v->x);
        cJSON_AddNumberToObject(obj, "y", v->y);
        cJSON_AddNumberToObject(obj, "tile_count", v->tile_count);
        cJSON_AddItemToArray(vine_arr, obj);
    }

    /* ---- Ladders ------------------------------------------------- */

    cJSON *ladder_arr = cJSON_AddArrayToObject(root, "ladders");
    for (int i = 0; i < def->ladder_count; i++) {
        const LadderPlacement *l = &def->ladders[i];
        cJSON *obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(obj, "x", l->x);
        cJSON_AddNumberToObject(obj, "y", l->y);
        cJSON_AddNumberToObject(obj, "tile_count", l->tile_count);
        cJSON_AddItemToArray(ladder_arr, obj);
    }

    /* ---- Ropes --------------------------------------------------- */

    cJSON *rope_arr = cJSON_AddArrayToObject(root, "ropes");
    for (int i = 0; i < def->rope_count; i++) {
        const RopePlacement *rp = &def->ropes[i];
        cJSON *obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(obj, "x", rp->x);
        cJSON_AddNumberToObject(obj, "y", rp->y);
        cJSON_AddNumberToObject(obj, "tile_count", rp->tile_count);
        cJSON_AddItemToArray(rope_arr, obj);
    }

    /* ---- Level-wide configuration ----------------------------------- */

    /* Parallax layers */
    cJSON *plx_arr = cJSON_AddArrayToObject(root, "parallax_layers");
    for (int i = 0; i < def->parallax_layer_count; i++) {
        cJSON *obj = cJSON_CreateObject();
        cJSON_AddStringToObject(obj, "path", def->parallax_layers[i].path);
        cJSON_AddNumberToObject(obj, "speed", (double)def->parallax_layers[i].speed);
        cJSON_AddItemToArray(plx_arr, obj);
    }

    /* Player spawn */
    cJSON_AddNumberToObject(root, "player_start_x", (double)def->player_start_x);
    cJSON_AddNumberToObject(root, "player_start_y", (double)def->player_start_y);

    /* Music */
    cJSON_AddStringToObject(root, "music_path", def->music_path);
    cJSON_AddNumberToObject(root, "music_volume", def->music_volume);

    /* Fog */
    cJSON_AddNumberToObject(root, "fog_enabled", def->fog_enabled);

    return root;
}

/* ================================================================== */
/* level_from_json — Populate a LevelDef from a cJSON tree             */
/* ================================================================== */

/*
 * PARSE_ARRAY — helper macro to reduce repetition when reading arrays.
 *
 * For each JSON array named `json_key`, this macro:
 *   1. Looks up the array in the root object.
 *   2. Checks that its size doesn't exceed `max_count`.
 *   3. Iterates over each element and calls the `parse_body` block,
 *      where `elem` is the current cJSON array element and `idx` is
 *      the 0-based index into the destination array.
 *
 * If the array is missing from the JSON, the count stays at 0 (lenient).
 * If the array exceeds the maximum, the function returns -1 immediately.
 */
#define PARSE_ARRAY(json_key, dest_array, count_field, max_count, parse_body) \
    do {                                                                       \
        const cJSON *arr = cJSON_GetObjectItemCaseSensitive(json, json_key);   \
        if (cJSON_IsArray(arr)) {                                              \
            int n = cJSON_GetArraySize(arr);                                   \
            if (n > (max_count)) {                                             \
                fprintf(stderr, "serializer: %s array has %d items "           \
                        "(max %d)\n", json_key, n, (max_count));               \
                return -1;                                                     \
            }                                                                  \
            def->count_field = n;                                              \
            for (int idx = 0; idx < n; idx++) {                                \
                const cJSON *elem = cJSON_GetArrayItem(arr, idx);              \
                parse_body                                                     \
            }                                                                  \
        }                                                                      \
    } while (0)

int level_from_json(const cJSON *json, LevelDef *def) {
    if (!json || !def) return -1;

    /*
     * Zero the entire struct first.  This sets all counts to 0, all
     * floats to 0.0, and all pointers to NULL — a clean starting state.
     * Any arrays missing from the JSON will naturally have count = 0.
     */
    memset(def, 0, sizeof(*def));

    /* ---- Name ---------------------------------------------------- */

    /*
     * get_string returns the cJSON-owned string or our fallback.
     * The LevelDef.name field is a const char* — it points into the cJSON
     * tree in-flight, but we strdup it so the LevelDef outlives the tree.
     */
    const char *name_str = get_string(json, "name", "Untitled");
    def->name = strdup(name_str);

    /* ---- Sea gaps ------------------------------------------------ */

    {
        const cJSON *arr = cJSON_GetObjectItemCaseSensitive(json, "sea_gaps");
        if (cJSON_IsArray(arr)) {
            int n = cJSON_GetArraySize(arr);
            if (n > MAX_SEA_GAPS) {
                fprintf(stderr, "serializer: sea_gaps array has %d items "
                        "(max %d)\n", n, MAX_SEA_GAPS);
                return -1;
            }
            def->sea_gap_count = n;
            for (int i = 0; i < n; i++) {
                /*
                 * cJSON_GetArrayItem — retrieve the i-th element of the array.
                 * Sea gap values are plain integers (left-edge x coordinates),
                 * stored as JSON numbers.  We cast the double to int.
                 */
                const cJSON *item = cJSON_GetArrayItem(arr, i);
                def->sea_gaps[i] = cJSON_IsNumber(item) ? item->valueint : 0;
            }
        }
    }

    /* ---- Rails --------------------------------------------------- */

    PARSE_ARRAY("rails", def->rails, rail_count, MAX_RAILS, {
        def->rails[idx].layout  = rail_layout_from_str(
            get_string(elem, "layout", "RECT"));
        def->rails[idx].x       = (int)get_number(elem, "x", 0);
        def->rails[idx].y       = (int)get_number(elem, "y", 0);
        def->rails[idx].w       = (int)get_number(elem, "w", 0);
        def->rails[idx].h       = (int)get_number(elem, "h", 0);
        def->rails[idx].end_cap = (int)get_number(elem, "end_cap", 0);
    });

    /* ---- Platforms ------------------------------------------------ */

    PARSE_ARRAY("platforms", def->platforms, platform_count, MAX_PLATFORMS, {
        def->platforms[idx].x           = (float)get_number(elem, "x", 0);
        def->platforms[idx].tile_height = (int)get_number(elem, "tile_height", 1);
    });

    /* ---- Coins --------------------------------------------------- */

    PARSE_ARRAY("coins", def->coins, coin_count, MAX_COINS, {
        def->coins[idx].x = (float)get_number(elem, "x", 0);
        def->coins[idx].y = (float)get_number(elem, "y", 0);
    });

    /* ---- Yellow stars --------------------------------------------- */

    PARSE_ARRAY("yellow_stars", def->yellow_stars, yellow_star_count,
                MAX_YELLOW_STARS, {
        def->yellow_stars[idx].x = (float)get_number(elem, "x", 0);
        def->yellow_stars[idx].y = (float)get_number(elem, "y", 0);
    });

    /* ---- Last star (single object, not array) -------------------- */

    {
        /*
         * last_star is a single JSON object, not an array.  We read it
         * directly from the root.  If missing, x and y stay at 0 from
         * the memset above.
         */
        const cJSON *ls = cJSON_GetObjectItemCaseSensitive(json, "last_star");
        if (cJSON_IsObject(ls)) {
            def->last_star.x = (float)get_number(ls, "x", 0);
            def->last_star.y = (float)get_number(ls, "y", 0);
        }
    }

    /* ---- Spiders ------------------------------------------------- */

    PARSE_ARRAY("spiders", def->spiders, spider_count, MAX_SPIDERS, {
        def->spiders[idx].x           = (float)get_number(elem, "x", 0);
        def->spiders[idx].vx          = (float)get_number(elem, "vx", 0);
        def->spiders[idx].patrol_x0   = (float)get_number(elem, "patrol_x0", 0);
        def->spiders[idx].patrol_x1   = (float)get_number(elem, "patrol_x1", 0);
        def->spiders[idx].frame_index = (int)get_number(elem, "frame_index", 0);
    });

    /* ---- Jumping spiders ----------------------------------------- */

    PARSE_ARRAY("jumping_spiders", def->jumping_spiders, jumping_spider_count,
                MAX_JUMPING_SPIDERS, {
        def->jumping_spiders[idx].x         = (float)get_number(elem, "x", 0);
        def->jumping_spiders[idx].vx        = (float)get_number(elem, "vx", 0);
        def->jumping_spiders[idx].patrol_x0 = (float)get_number(elem, "patrol_x0", 0);
        def->jumping_spiders[idx].patrol_x1 = (float)get_number(elem, "patrol_x1", 0);
    });

    /* ---- Birds --------------------------------------------------- */

    PARSE_ARRAY("birds", def->birds, bird_count, MAX_BIRDS, {
        def->birds[idx].x           = (float)get_number(elem, "x", 0);
        def->birds[idx].base_y      = (float)get_number(elem, "base_y", 0);
        def->birds[idx].vx          = (float)get_number(elem, "vx", 0);
        def->birds[idx].patrol_x0   = (float)get_number(elem, "patrol_x0", 0);
        def->birds[idx].patrol_x1   = (float)get_number(elem, "patrol_x1", 0);
        def->birds[idx].frame_index = (int)get_number(elem, "frame_index", 0);
    });

    /* ---- Faster birds -------------------------------------------- */

    PARSE_ARRAY("faster_birds", def->faster_birds, faster_bird_count,
                MAX_FASTER_BIRDS, {
        def->faster_birds[idx].x           = (float)get_number(elem, "x", 0);
        def->faster_birds[idx].base_y      = (float)get_number(elem, "base_y", 0);
        def->faster_birds[idx].vx          = (float)get_number(elem, "vx", 0);
        def->faster_birds[idx].patrol_x0   = (float)get_number(elem, "patrol_x0", 0);
        def->faster_birds[idx].patrol_x1   = (float)get_number(elem, "patrol_x1", 0);
        def->faster_birds[idx].frame_index = (int)get_number(elem, "frame_index", 0);
    });

    /* ---- Fish ---------------------------------------------------- */

    PARSE_ARRAY("fish", def->fish, fish_count, MAX_FISH, {
        def->fish[idx].x         = (float)get_number(elem, "x", 0);
        def->fish[idx].vx        = (float)get_number(elem, "vx", 0);
        def->fish[idx].patrol_x0 = (float)get_number(elem, "patrol_x0", 0);
        def->fish[idx].patrol_x1 = (float)get_number(elem, "patrol_x1", 0);
    });

    /* ---- Faster fish --------------------------------------------- */

    PARSE_ARRAY("faster_fish", def->faster_fish, faster_fish_count,
                MAX_FASTER_FISH, {
        def->faster_fish[idx].x         = (float)get_number(elem, "x", 0);
        def->faster_fish[idx].vx        = (float)get_number(elem, "vx", 0);
        def->faster_fish[idx].patrol_x0 = (float)get_number(elem, "patrol_x0", 0);
        def->faster_fish[idx].patrol_x1 = (float)get_number(elem, "patrol_x1", 0);
    });

    /* ---- Axe traps ----------------------------------------------- */

    PARSE_ARRAY("axe_traps", def->axe_traps, axe_trap_count, MAX_AXE_TRAPS, {
        def->axe_traps[idx].pillar_x = (float)get_number(elem, "pillar_x", 0);
        def->axe_traps[idx].mode     = axe_mode_from_str(
            get_string(elem, "mode", "PENDULUM"));
    });

    /* ---- Circular saws ------------------------------------------- */

    PARSE_ARRAY("circular_saws", def->circular_saws, circular_saw_count,
                MAX_CIRCULAR_SAWS, {
        def->circular_saws[idx].x         = (float)get_number(elem, "x", 0);
        def->circular_saws[idx].patrol_x0 = (float)get_number(elem, "patrol_x0", 0);
        def->circular_saws[idx].patrol_x1 = (float)get_number(elem, "patrol_x1", 0);
        def->circular_saws[idx].direction = (int)get_number(elem, "direction", 1);
    });

    /* ---- Spike rows ---------------------------------------------- */

    PARSE_ARRAY("spike_rows", def->spike_rows, spike_row_count,
                MAX_SPIKE_ROWS, {
        def->spike_rows[idx].x     = (float)get_number(elem, "x", 0);
        def->spike_rows[idx].count = (int)get_number(elem, "count", 1);
    });

    /* ---- Spike platforms ----------------------------------------- */

    PARSE_ARRAY("spike_platforms", def->spike_platforms, spike_platform_count,
                MAX_SPIKE_PLATFORMS, {
        def->spike_platforms[idx].x          = (float)get_number(elem, "x", 0);
        def->spike_platforms[idx].y          = (float)get_number(elem, "y", 0);
        def->spike_platforms[idx].tile_count = (int)get_number(elem, "tile_count", 1);
    });

    /* ---- Spike blocks -------------------------------------------- */

    PARSE_ARRAY("spike_blocks", def->spike_blocks, spike_block_count,
                MAX_SPIKE_BLOCKS, {
        def->spike_blocks[idx].rail_index = (int)get_number(elem, "rail_index", 0);
        def->spike_blocks[idx].t_offset   = (float)get_number(elem, "t_offset", 0);
        def->spike_blocks[idx].speed      = (float)get_number(elem, "speed", 0);
    });

    /* ---- Float platforms ----------------------------------------- */

    PARSE_ARRAY("float_platforms", def->float_platforms, float_platform_count,
                MAX_FLOAT_PLATFORMS, {
        def->float_platforms[idx].mode       = float_mode_from_str(
            get_string(elem, "mode", "STATIC"));
        def->float_platforms[idx].x          = (float)get_number(elem, "x", 0);
        def->float_platforms[idx].y          = (float)get_number(elem, "y", 0);
        def->float_platforms[idx].tile_count = (int)get_number(elem, "tile_count", 1);
        def->float_platforms[idx].rail_index = (int)get_number(elem, "rail_index", 0);
        def->float_platforms[idx].t_offset   = (float)get_number(elem, "t_offset", 0);
        def->float_platforms[idx].speed      = (float)get_number(elem, "speed", 0);
    });

    /* ---- Bridges ------------------------------------------------- */

    PARSE_ARRAY("bridges", def->bridges, bridge_count, MAX_BRIDGES, {
        def->bridges[idx].x           = (float)get_number(elem, "x", 0);
        def->bridges[idx].y           = (float)get_number(elem, "y", 0);
        def->bridges[idx].brick_count = (int)get_number(elem, "brick_count", 1);
    });

    /* ---- Bouncepads (small) -------------------------------------- */

    PARSE_ARRAY("bouncepads_small", def->bouncepads_small, bouncepad_small_count,
                MAX_BOUNCEPADS_SMALL, {
        def->bouncepads_small[idx].x         = (float)get_number(elem, "x", 0);
        def->bouncepads_small[idx].launch_vy = (float)get_number(elem, "launch_vy", 0);
        def->bouncepads_small[idx].pad_type  = bouncepad_type_from_str(
            get_string(elem, "pad_type", "GREEN"));
    });

    /* ---- Bouncepads (medium) ------------------------------------- */

    PARSE_ARRAY("bouncepads_medium", def->bouncepads_medium,
                bouncepad_medium_count, MAX_BOUNCEPADS_MEDIUM, {
        def->bouncepads_medium[idx].x         = (float)get_number(elem, "x", 0);
        def->bouncepads_medium[idx].launch_vy = (float)get_number(elem, "launch_vy", 0);
        def->bouncepads_medium[idx].pad_type  = bouncepad_type_from_str(
            get_string(elem, "pad_type", "WOOD"));
    });

    /* ---- Bouncepads (high) --------------------------------------- */

    PARSE_ARRAY("bouncepads_high", def->bouncepads_high, bouncepad_high_count,
                MAX_BOUNCEPADS_HIGH, {
        def->bouncepads_high[idx].x         = (float)get_number(elem, "x", 0);
        def->bouncepads_high[idx].launch_vy = (float)get_number(elem, "launch_vy", 0);
        def->bouncepads_high[idx].pad_type  = bouncepad_type_from_str(
            get_string(elem, "pad_type", "RED"));
    });

    /* ---- Vines --------------------------------------------------- */

    PARSE_ARRAY("vines", def->vines, vine_count, MAX_VINES, {
        def->vines[idx].x          = (float)get_number(elem, "x", 0);
        def->vines[idx].y          = (float)get_number(elem, "y", 0);
        def->vines[idx].tile_count = (int)get_number(elem, "tile_count", 1);
    });

    /* ---- Ladders ------------------------------------------------- */

    PARSE_ARRAY("ladders", def->ladders, ladder_count, MAX_LADDERS, {
        def->ladders[idx].x          = (float)get_number(elem, "x", 0);
        def->ladders[idx].y          = (float)get_number(elem, "y", 0);
        def->ladders[idx].tile_count = (int)get_number(elem, "tile_count", 1);
    });

    /* ---- Ropes --------------------------------------------------- */

    PARSE_ARRAY("ropes", def->ropes, rope_count, MAX_ROPES, {
        def->ropes[idx].x          = (float)get_number(elem, "x", 0);
        def->ropes[idx].y          = (float)get_number(elem, "y", 0);
        def->ropes[idx].tile_count = (int)get_number(elem, "tile_count", 1);
    });

    /* ---- Level-wide configuration ----------------------------------- */

    /* Parallax layers */
    {
        const cJSON *plx = cJSON_GetObjectItemCaseSensitive(json, "parallax_layers");
        if (cJSON_IsArray(plx)) {
            int n = cJSON_GetArraySize(plx);
            if (n > PARALLAX_MAX_LAYERS) n = PARALLAX_MAX_LAYERS;
            def->parallax_layer_count = n;
            for (int i = 0; i < n; i++) {
                const cJSON *elem = cJSON_GetArrayItem(plx, i);
                const cJSON *p = cJSON_GetObjectItemCaseSensitive(elem, "path");
                if (cJSON_IsString(p) && p->valuestring) {
                    strncpy(def->parallax_layers[i].path, p->valuestring, 63);
                    def->parallax_layers[i].path[63] = '\0';
                }
                def->parallax_layers[i].speed = (float)get_number(elem, "speed", 0);
            }
        }
    }

    /* Player spawn */
    def->player_start_x = (float)get_number(json, "player_start_x", 0);
    def->player_start_y = (float)get_number(json, "player_start_y", 0);

    /* Music */
    {
        const cJSON *mp = cJSON_GetObjectItemCaseSensitive(json, "music_path");
        if (cJSON_IsString(mp) && mp->valuestring) {
            strncpy(def->music_path, mp->valuestring, 63);
            def->music_path[63] = '\0';
        }
    }
    def->music_volume = (int)get_number(json, "music_volume", 0);

    /* Fog */
    def->fog_enabled = (int)get_number(json, "fog_enabled", 0);

    return 0;
}

#undef PARSE_ARRAY

/* ================================================================== */
/* level_save_json — Write a LevelDef to a pretty-printed JSON file    */
/* ================================================================== */

int level_save_json(const LevelDef *def, const char *path) {
    if (!def || !path) return -1;

    /* Build the cJSON tree from the level definition. */
    cJSON *root = level_to_json(def);
    if (!root) {
        fprintf(stderr, "serializer: failed to build JSON tree\n");
        return -1;
    }

    /*
     * cJSON_Print — render the entire tree as a pretty-printed JSON string.
     * Returns a malloc'd char* with indentation and newlines for readability.
     * The caller must free() it when done.
     */
    char *json_str = cJSON_Print(root);
    if (!json_str) {
        fprintf(stderr, "serializer: cJSON_Print failed\n");
        cJSON_Delete(root);
        return -1;
    }

    /*
     * Write the JSON string to disk.  We open in "w" mode (truncate + create)
     * so the file is always cleanly overwritten with the latest data.
     */
    FILE *fp = fopen(path, "w");
    if (!fp) {
        fprintf(stderr, "serializer: cannot open '%s' for writing\n", path);
        free(json_str);
        cJSON_Delete(root);
        return -1;
    }

    /*
     * fputs — write the null-terminated string to the file.
     * We add a trailing newline so the file ends cleanly (POSIX convention).
     */
    fputs(json_str, fp);
    fputc('\n', fp);
    fclose(fp);

    /*
     * Clean up: free the rendered string and the cJSON tree.
     * cJSON_Print uses stdlib malloc internally, so plain free() is correct.
     * cJSON_Delete recursively frees all nodes in the tree.
     */
    free(json_str);
    cJSON_Delete(root);

    return 0;
}

/* ================================================================== */
/* level_load_json — Read a JSON file and populate a LevelDef          */
/* ================================================================== */

int level_load_json(const char *path, LevelDef *def) {
    if (!path || !def) return -1;

    /*
     * Step 1: Read the entire file into memory.
     *
     * We need the whole file as a contiguous string for cJSON_Parse.
     * fseek/ftell gives us the size; then we malloc a buffer and fread.
     */
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "serializer: cannot open '%s' for reading\n", path);
        return -1;
    }

    /*
     * fseek to the end, ftell to get the byte count, then rewind.
     * This is the standard C idiom for measuring file size.
     */
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (file_size <= 0) {
        fprintf(stderr, "serializer: '%s' is empty or unreadable\n", path);
        fclose(fp);
        return -1;
    }

    /*
     * Allocate file_size + 1 bytes: the extra byte is for the null terminator
     * that cJSON_Parse expects (it parses a C string, not a raw buffer).
     */
    char *buf = (char *)malloc((size_t)file_size + 1);
    if (!buf) {
        fprintf(stderr, "serializer: malloc failed for %ld bytes\n", file_size);
        fclose(fp);
        return -1;
    }

    /*
     * fread — read the entire file into our buffer in one call.
     * The return value is the number of items read (should equal file_size
     * for a single-byte item size).
     */
    size_t bytes_read = fread(buf, 1, (size_t)file_size, fp);
    fclose(fp);

    /* Null-terminate the buffer so cJSON can parse it as a C string. */
    buf[bytes_read] = '\0';

    /*
     * Step 2: Parse the JSON string into a cJSON tree.
     *
     * cJSON_Parse returns the root node of the tree, or NULL if the JSON
     * is malformed.  On failure, cJSON_GetErrorPtr() points to the location
     * in the string where parsing broke down.
     */
    cJSON *root = cJSON_Parse(buf);
    free(buf);  /* buffer is no longer needed; cJSON copies what it needs */

    if (!root) {
        const char *err = cJSON_GetErrorPtr();
        fprintf(stderr, "serializer: JSON parse error in '%s'", path);
        if (err) fprintf(stderr, " near: %.40s", err);
        fprintf(stderr, "\n");
        return -1;
    }

    /*
     * Step 3: Convert the cJSON tree into a LevelDef struct.
     *
     * level_from_json validates all array sizes against MAX_* limits
     * and returns -1 if anything is out of bounds.
     */
    int result = level_from_json(root, def);

    /*
     * cJSON_Delete — recursively free the entire parsed tree.
     * The LevelDef struct now holds its own copies of all data, so
     * the tree is safe to destroy.
     */
    cJSON_Delete(root);

    return result;
}
