/*
 * serializer.c — TOML serialization for level definitions.
 *
 * Converts between LevelDef structs (the engine's in-memory level format)
 * and TOML files on disk.  This lets the editor save levels as human-
 * readable .toml files and load them back for editing or playtesting.
 *
 * The emitter (level_save_toml) writes TOML using plain fprintf calls.
 * The parser (level_load_toml) uses tomlc17 to parse the file and then
 * walks the resulting table/array tree to fill a LevelDef.
 *
 * All enum values are serialized as readable strings ("RECT", "SPIN", etc.)
 * rather than raw integers, so the TOML files are easy to understand and
 * edit by hand.
 */

#include <stdio.h>   /* fprintf, fopen, fclose, fread, fseek, ftell */
#include <stdlib.h>  /* malloc, free, exit */
#include <string.h>  /* strcmp, memset, strncpy */

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
#include <sys/stat.h> /* open, O_WRONLY, O_CREAT, O_TRUNC */
#include <fcntl.h>
#endif

#include "serializer.h"
#include "../../vendor/tomlc17/tomlc17.h" /* tomlc17 API */
#include "../levels/level.h"              /* LevelDef, all placement types */
#include "../game.h"                      /* MAX_* constants */

/* ================================================================== */
/* Float formatting helper                                             */
/* ================================================================== */

/*
 * fmt_float — Format a float with up to 2 decimal places, stripping
 * unnecessary trailing zeros but always keeping at least one decimal
 * digit so the TOML parser reads it as a float (not an integer).
 *
 * Examples:  80.0f  → "80.0"     (not "80.00" or "80")
 *            0.08f  → "0.08"     (preserved — was lost with %.1f)
 *            536.2f → "536.2"    (not "536.20")
 *            0.50f  → "0.5"      (trailing zero stripped)
 *           -380.0f → "-380.0"
 *
 * Returns a pointer to a static buffer — valid until the next call.
 * Safe for single-float-per-fprintf usage (which is all we do here).
 */
static const char *fmt_float(double val) {
    static char buf[64];
    snprintf(buf, sizeof(buf), "%.2f", val);

    /* Find the decimal point */
    char *dot = strchr(buf, '.');
    if (dot) {
        /* Walk back from the end, stripping trailing '0' characters,
         * but always keep at least one digit after the decimal point
         * so "80.00" becomes "80.0" (not "80." or "80"). */
        char *end = buf + strlen(buf) - 1;
        while (end > dot + 1 && *end == '0') {
            *end = '\0';
            end--;
        }
    }
    return buf;
}

/* ================================================================== */
/* Enum <-> string conversion helpers                                  */
/* ================================================================== */

/*
 * Each enum has a pair of functions: one converts the C enum value to a
 * human-readable string for TOML output; the other parses a string back
 * into the enum value.  This keeps the TOML portable and self-documenting.
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
/* TOML helpers: safely read values from a toml_datum_t table          */
/* ================================================================== */

/*
 * get_float — Read a numeric field from a TOML table, returning float.
 *
 * TOML distinguishes integers (no decimal point) from floats (with decimal).
 * A field written as "79" comes back as TOML_INT64, while "79.0" comes as
 * TOML_FP64.  We accept both and convert to float.
 */
static float get_float(toml_datum_t tab, const char *key, float fallback) {
    toml_datum_t d = toml_get(tab, key);
    if (d.type == TOML_FP64)  return (float)d.u.fp64;
    if (d.type == TOML_INT64) return (float)d.u.int64;
    return fallback;
}

/*
 * get_int — Read an integer field from a TOML table.
 *
 * Accepts both TOML_INT64 and TOML_FP64 for resilience (a hand-edited
 * file might use "1.0" instead of "1").
 */
static int get_int(toml_datum_t tab, const char *key, int fallback) {
    toml_datum_t d = toml_get(tab, key);
    if (d.type == TOML_INT64) return (int)d.u.int64;
    if (d.type == TOML_FP64)  return (int)d.u.fp64;
    return fallback;
}

/*
 * get_str — Read a string field from a TOML table.
 *
 * Returns a pointer into the parsed TOML tree (valid until toml_free).
 * Returns `fallback` if the field is missing or not a string.
 */
static const char *get_str(toml_datum_t tab, const char *key,
                           const char *fallback) {
    toml_datum_t d = toml_get(tab, key);
    if (d.type == TOML_STRING) return d.u.s;
    return fallback;
}

/* ================================================================== */
/* level_save_toml — Write a LevelDef to a human-readable TOML file    */
/* ================================================================== */

int level_save_toml(const LevelDef *def, const char *path) {
    if (!def || !path) return -1;

    /*
     * Open the output file.  On POSIX we use open() with explicit
     * 0644 permissions so the file is owner-writable only, not world-writable.
     */
#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    FILE *fp = fd >= 0 ? fdopen(fd, "w") : NULL;
#else
    FILE *fp = fopen(path, "w");
#endif
    if (!fp) {
        fprintf(stderr, "serializer: cannot open '%s' for writing\n", path);
        return -1;
    }

    /* ---- Header fields ------------------------------------------- */

    fprintf(fp, "name = \"%s\"\n", def->name[0] ? def->name : "Untitled");

    if (def->description[0] != '\0') {
        /* Strip trailing newlines from the description, then add exactly
         * one so the TOML multiline literal has a clean closing """ */
        const char *desc = def->description;
        int len = (int)strlen(desc);
        while (len > 0 && desc[len - 1] == '\n') len--;
        fprintf(fp, "description = \"\"\"\n");
        fwrite(desc, 1, len, fp);
        fprintf(fp, "\n\"\"\"\n");
    }

    if (def->generated_by[0] != '\0') {
        fprintf(fp, "generated_by = \"%s\"\n", def->generated_by);
    }

    fprintf(fp, "screen_count = %d\n", def->screen_count);

    /* ---- Level-wide configuration (must come before [[arrays]]) --- */

    fprintf(fp, "player_start_x = %s\n", fmt_float(def->player_start_x));
    fprintf(fp, "player_start_y = %s\n", fmt_float(def->player_start_y));
    fprintf(fp, "music_path = \"%s\"\n", def->music_path);
    fprintf(fp, "music_volume = %d\n", def->music_volume);
    fprintf(fp, "floor_tile_path = \"%s\"\n", def->floor_tile_path);
    fprintf(fp, "initial_hearts = %d\n", def->initial_hearts);
    fprintf(fp, "initial_lives = %d\n", def->initial_lives);
    fprintf(fp, "score_per_life = %d\n", def->score_per_life);
    fprintf(fp, "coin_score = %d\n", def->coin_score);

    /* ---- Floor gaps (plain integer array) ------------------------- */

    if (def->floor_gap_count > 0) {
        fprintf(fp, "floor_gaps = [");
        for (int i = 0; i < def->floor_gap_count; i++) {
            if (i > 0) fprintf(fp, ", ");
            fprintf(fp, "%d", def->floor_gaps[i]);
        }
        fprintf(fp, "]\n");
    }

    fprintf(fp, "\n");

    /* ---- Rails --------------------------------------------------- */

    for (int i = 0; i < def->rail_count; i++) {
        const RailPlacement *r = &def->rails[i];
        fprintf(fp, "[[rails]]\n");
        fprintf(fp, "layout = \"%s\"\n", rail_layout_to_str(r->layout));
        fprintf(fp, "x = %d\n", r->x);
        fprintf(fp, "y = %d\n", r->y);
        fprintf(fp, "w = %d\n", r->w);
        fprintf(fp, "h = %d\n", r->h);
        fprintf(fp, "end_cap = %d\n", r->end_cap);
        fprintf(fp, "\n");
    }

    /* ---- Platforms ------------------------------------------------ */

    for (int i = 0; i < def->platform_count; i++) {
        const PlatformPlacement *p = &def->platforms[i];
        fprintf(fp, "[[platforms]]\n");
        fprintf(fp, "x = %s\n", fmt_float(p->x));
        fprintf(fp, "tile_height = %d\n", p->tile_height);
        fprintf(fp, "tile_width = %d\n", p->tile_width);
        fprintf(fp, "\n");
    }

    /* ---- Coins --------------------------------------------------- */

    for (int i = 0; i < def->coin_count; i++) {
        const CoinPlacement *c = &def->coins[i];
        fprintf(fp, "[[coins]]\n");
        fprintf(fp, "x = %s\n", fmt_float(c->x));
        fprintf(fp, "y = %s\n", fmt_float(c->y));
        fprintf(fp, "\n");
    }

    /* ---- Star yellows --------------------------------------------- */

    for (int i = 0; i < def->star_yellow_count; i++) {
        const StarYellowPlacement *s = &def->star_yellows[i];
        fprintf(fp, "[[star_yellows]]\n");
        fprintf(fp, "x = %s\n", fmt_float(s->x));
        fprintf(fp, "y = %s\n", fmt_float(s->y));
        fprintf(fp, "\n");
    }

    /* ---- Star greens ---------------------------------------------- */

    for (int i = 0; i < def->star_green_count; i++) {
        const StarGreenPlacement *s = &def->star_greens[i];
        fprintf(fp, "[[star_greens]]\n");
        fprintf(fp, "x = %s\n", fmt_float(s->x));
        fprintf(fp, "y = %s\n", fmt_float(s->y));
        fprintf(fp, "\n");
    }

    /* ---- Star reds ------------------------------------------------ */

    for (int i = 0; i < def->star_red_count; i++) {
        const StarRedPlacement *s = &def->star_reds[i];
        fprintf(fp, "[[star_reds]]\n");
        fprintf(fp, "x = %s\n", fmt_float(s->x));
        fprintf(fp, "y = %s\n", fmt_float(s->y));
        fprintf(fp, "\n");
    }

    /* ---- Last star (single table, not array) --------------------- */

    {
        const LastStarPlacement *ls = &def->last_star;
        fprintf(fp, "[last_star]\n");
        fprintf(fp, "x = %s\n", fmt_float(ls->x));
        fprintf(fp, "y = %s\n", fmt_float(ls->y));
        fprintf(fp, "\n");
    }

    /* ---- Spiders ------------------------------------------------- */

    for (int i = 0; i < def->spider_count; i++) {
        const SpiderPlacement *sp = &def->spiders[i];
        fprintf(fp, "[[spiders]]\n");
        fprintf(fp, "x = %s\n", fmt_float(sp->x));
        fprintf(fp, "vx = %s\n", fmt_float(sp->vx));
        fprintf(fp, "patrol_x0 = %s\n", fmt_float(sp->patrol_x0));
        fprintf(fp, "patrol_x1 = %s\n", fmt_float(sp->patrol_x1));
        fprintf(fp, "frame_index = %d\n", sp->frame_index);
        fprintf(fp, "\n");
    }

    /* ---- Jumping spiders ----------------------------------------- */

    for (int i = 0; i < def->jumping_spider_count; i++) {
        const JumpingSpiderPlacement *js = &def->jumping_spiders[i];
        fprintf(fp, "[[jumping_spiders]]\n");
        fprintf(fp, "x = %s\n", fmt_float(js->x));
        fprintf(fp, "vx = %s\n", fmt_float(js->vx));
        fprintf(fp, "patrol_x0 = %s\n", fmt_float(js->patrol_x0));
        fprintf(fp, "patrol_x1 = %s\n", fmt_float(js->patrol_x1));
        fprintf(fp, "\n");
    }

    /* ---- Birds --------------------------------------------------- */

    for (int i = 0; i < def->bird_count; i++) {
        const BirdPlacement *b = &def->birds[i];
        fprintf(fp, "[[birds]]\n");
        fprintf(fp, "x = %s\n", fmt_float(b->x));
        fprintf(fp, "base_y = %s\n", fmt_float(b->base_y));
        fprintf(fp, "vx = %s\n", fmt_float(b->vx));
        fprintf(fp, "patrol_x0 = %s\n", fmt_float(b->patrol_x0));
        fprintf(fp, "patrol_x1 = %s\n", fmt_float(b->patrol_x1));
        fprintf(fp, "frame_index = %d\n", b->frame_index);
        fprintf(fp, "\n");
    }

    /* ---- Faster birds -------------------------------------------- */

    for (int i = 0; i < def->faster_bird_count; i++) {
        const BirdPlacement *b = &def->faster_birds[i];
        fprintf(fp, "[[faster_birds]]\n");
        fprintf(fp, "x = %s\n", fmt_float(b->x));
        fprintf(fp, "base_y = %s\n", fmt_float(b->base_y));
        fprintf(fp, "vx = %s\n", fmt_float(b->vx));
        fprintf(fp, "patrol_x0 = %s\n", fmt_float(b->patrol_x0));
        fprintf(fp, "patrol_x1 = %s\n", fmt_float(b->patrol_x1));
        fprintf(fp, "frame_index = %d\n", b->frame_index);
        fprintf(fp, "\n");
    }

    /* ---- Fish ---------------------------------------------------- */

    for (int i = 0; i < def->fish_count; i++) {
        const FishPlacement *f = &def->fish[i];
        fprintf(fp, "[[fish]]\n");
        fprintf(fp, "x = %s\n", fmt_float(f->x));
        fprintf(fp, "vx = %s\n", fmt_float(f->vx));
        fprintf(fp, "patrol_x0 = %s\n", fmt_float(f->patrol_x0));
        fprintf(fp, "patrol_x1 = %s\n", fmt_float(f->patrol_x1));
        fprintf(fp, "\n");
    }

    /* ---- Faster fish --------------------------------------------- */

    for (int i = 0; i < def->faster_fish_count; i++) {
        const FishPlacement *f = &def->faster_fish[i];
        fprintf(fp, "[[faster_fish]]\n");
        fprintf(fp, "x = %s\n", fmt_float(f->x));
        fprintf(fp, "vx = %s\n", fmt_float(f->vx));
        fprintf(fp, "patrol_x0 = %s\n", fmt_float(f->patrol_x0));
        fprintf(fp, "patrol_x1 = %s\n", fmt_float(f->patrol_x1));
        fprintf(fp, "\n");
    }

    /* ---- Axe traps ----------------------------------------------- */

    for (int i = 0; i < def->axe_trap_count; i++) {
        const AxeTrapPlacement *a = &def->axe_traps[i];
        fprintf(fp, "[[axe_traps]]\n");
        fprintf(fp, "pillar_x = %s\n", fmt_float(a->pillar_x));
        fprintf(fp, "y = %s\n", fmt_float(a->y));
        fprintf(fp, "mode = \"%s\"\n", axe_mode_to_str(a->mode));
        fprintf(fp, "\n");
    }

    /* ---- Circular saws ------------------------------------------- */

    for (int i = 0; i < def->circular_saw_count; i++) {
        const CircularSawPlacement *cs = &def->circular_saws[i];
        fprintf(fp, "[[circular_saws]]\n");
        fprintf(fp, "x = %s\n", fmt_float(cs->x));
        fprintf(fp, "y = %s\n", fmt_float(cs->y));
        fprintf(fp, "patrol_x0 = %s\n", fmt_float(cs->patrol_x0));
        fprintf(fp, "patrol_x1 = %s\n", fmt_float(cs->patrol_x1));
        fprintf(fp, "direction = %d\n", cs->direction);
        fprintf(fp, "\n");
    }

    /* ---- Spike rows ---------------------------------------------- */

    for (int i = 0; i < def->spike_row_count; i++) {
        const SpikeRowPlacement *sr = &def->spike_rows[i];
        fprintf(fp, "[[spike_rows]]\n");
        fprintf(fp, "x = %s\n", fmt_float(sr->x));
        fprintf(fp, "count = %d\n", sr->count);
        fprintf(fp, "\n");
    }

    /* ---- Spike platforms ----------------------------------------- */

    for (int i = 0; i < def->spike_platform_count; i++) {
        const SpikePlatformPlacement *sp = &def->spike_platforms[i];
        fprintf(fp, "[[spike_platforms]]\n");
        fprintf(fp, "x = %s\n", fmt_float(sp->x));
        fprintf(fp, "y = %s\n", fmt_float(sp->y));
        fprintf(fp, "tile_count = %d\n", sp->tile_count);
        fprintf(fp, "\n");
    }

    /* ---- Spike blocks -------------------------------------------- */

    for (int i = 0; i < def->spike_block_count; i++) {
        const SpikeBlockPlacement *sb = &def->spike_blocks[i];
        fprintf(fp, "[[spike_blocks]]\n");
        fprintf(fp, "rail_index = %d\n", sb->rail_index);
        fprintf(fp, "t_offset = %s\n", fmt_float(sb->t_offset));
        fprintf(fp, "speed = %s\n", fmt_float(sb->speed));
        fprintf(fp, "\n");
    }

    /* ---- Blue flames --------------------------------------------- */

    for (int i = 0; i < def->blue_flame_count; i++) {
        fprintf(fp, "[[blue_flames]]\n");
        fprintf(fp, "x = %s\n", fmt_float(def->blue_flames[i].x));
        fprintf(fp, "\n");
    }

    /* ---- Fire flames --------------------------------------------- */

    for (int i = 0; i < def->fire_flame_count; i++) {
        fprintf(fp, "[[fire_flames]]\n");
        fprintf(fp, "x = %s\n", fmt_float(def->fire_flames[i].x));
        fprintf(fp, "\n");
    }

    /* ---- Float platforms ----------------------------------------- */

    for (int i = 0; i < def->float_platform_count; i++) {
        const FloatPlatformPlacement *fl = &def->float_platforms[i];
        fprintf(fp, "[[float_platforms]]\n");
        fprintf(fp, "mode = \"%s\"\n", float_mode_to_str(fl->mode));
        fprintf(fp, "x = %s\n", fmt_float(fl->x));
        fprintf(fp, "y = %s\n", fmt_float(fl->y));
        fprintf(fp, "tile_count = %d\n", fl->tile_count);
        fprintf(fp, "rail_index = %d\n", fl->rail_index);
        fprintf(fp, "t_offset = %s\n", fmt_float(fl->t_offset));
        fprintf(fp, "speed = %s\n", fmt_float(fl->speed));
        fprintf(fp, "\n");
    }

    /* ---- Bridges ------------------------------------------------- */

    for (int i = 0; i < def->bridge_count; i++) {
        const BridgePlacement *br = &def->bridges[i];
        fprintf(fp, "[[bridges]]\n");
        fprintf(fp, "x = %s\n", fmt_float(br->x));
        fprintf(fp, "y = %s\n", fmt_float(br->y));
        fprintf(fp, "brick_count = %d\n", br->brick_count);
        fprintf(fp, "\n");
    }

    /* ---- Bouncepads (small) -------------------------------------- */

    for (int i = 0; i < def->bouncepad_small_count; i++) {
        const BouncepadPlacement *bp = &def->bouncepads_small[i];
        fprintf(fp, "[[bouncepads_small]]\n");
        fprintf(fp, "x = %s\n", fmt_float(bp->x));
        fprintf(fp, "launch_vy = %s\n", fmt_float(bp->launch_vy));
        fprintf(fp, "pad_type = \"%s\"\n", bouncepad_type_to_str(bp->pad_type));
        fprintf(fp, "\n");
    }

    /* ---- Bouncepads (medium) ------------------------------------- */

    for (int i = 0; i < def->bouncepad_medium_count; i++) {
        const BouncepadPlacement *bp = &def->bouncepads_medium[i];
        fprintf(fp, "[[bouncepads_medium]]\n");
        fprintf(fp, "x = %s\n", fmt_float(bp->x));
        fprintf(fp, "launch_vy = %s\n", fmt_float(bp->launch_vy));
        fprintf(fp, "pad_type = \"%s\"\n", bouncepad_type_to_str(bp->pad_type));
        fprintf(fp, "\n");
    }

    /* ---- Bouncepads (high) --------------------------------------- */

    for (int i = 0; i < def->bouncepad_high_count; i++) {
        const BouncepadPlacement *bp = &def->bouncepads_high[i];
        fprintf(fp, "[[bouncepads_high]]\n");
        fprintf(fp, "x = %s\n", fmt_float(bp->x));
        fprintf(fp, "launch_vy = %s\n", fmt_float(bp->launch_vy));
        fprintf(fp, "pad_type = \"%s\"\n", bouncepad_type_to_str(bp->pad_type));
        fprintf(fp, "\n");
    }

    /* ---- Vines --------------------------------------------------- */

    for (int i = 0; i < def->vine_count; i++) {
        const VinePlacement *v = &def->vines[i];
        fprintf(fp, "[[vines]]\n");
        fprintf(fp, "x = %s\n", fmt_float(v->x));
        fprintf(fp, "y = %s\n", fmt_float(v->y));
        fprintf(fp, "tile_count = %d\n", v->tile_count);
        fprintf(fp, "\n");
    }

    /* ---- Ladders ------------------------------------------------- */

    for (int i = 0; i < def->ladder_count; i++) {
        const LadderPlacement *l = &def->ladders[i];
        fprintf(fp, "[[ladders]]\n");
        fprintf(fp, "x = %s\n", fmt_float(l->x));
        fprintf(fp, "y = %s\n", fmt_float(l->y));
        fprintf(fp, "tile_count = %d\n", l->tile_count);
        fprintf(fp, "\n");
    }

    /* ---- Ropes --------------------------------------------------- */

    for (int i = 0; i < def->rope_count; i++) {
        const RopePlacement *rp = &def->ropes[i];
        fprintf(fp, "[[ropes]]\n");
        fprintf(fp, "x = %s\n", fmt_float(rp->x));
        fprintf(fp, "y = %s\n", fmt_float(rp->y));
        fprintf(fp, "tile_count = %d\n", rp->tile_count);
        fprintf(fp, "\n");
    }

    /* ---- Background layers --------------------------------------- */

    for (int i = 0; i < def->background_layer_count; i++) {
        fprintf(fp, "[[background_layers]]\n");
        fprintf(fp, "path = \"%s\"\n", def->background_layers[i].path);
        fprintf(fp, "speed = %s\n", fmt_float(def->background_layers[i].speed));
        fprintf(fp, "\n");
    }

    /* ---- Foreground layers --------------------------------------- */

    for (int i = 0; i < def->foreground_layer_count; i++) {
        fprintf(fp, "[[foreground_layers]]\n");
        fprintf(fp, "path = \"%s\"\n", def->foreground_layers[i].path);
        fprintf(fp, "speed = %s\n", fmt_float(def->foreground_layers[i].speed));
        fprintf(fp, "\n");
    }

    /* ---- Fog layers ------------------------------------------------- */

    for (int i = 0; i < def->fog_layer_count; i++) {
        fprintf(fp, "[[fog_layers]]\n");
        fprintf(fp, "path = \"%s\"\n", def->fog_layers[i].path);
        fprintf(fp, "speed = %s\n", fmt_float(def->fog_layers[i].speed));
        fprintf(fp, "\n");
    }

    fclose(fp);
    return 0;
}

/* ================================================================== */
/* level_load_toml — Read a TOML file and populate a LevelDef          */
/* ================================================================== */

/*
 * PARSE_ARRAY — helper macro to reduce repetition when reading entity arrays.
 *
 * For each TOML array-of-tables named `toml_key`, this macro:
 *   1. Looks up the array in the top-level table.
 *   2. Checks that its size doesn't exceed `max_count`.
 *   3. Iterates over each element and calls the `parse_body` block,
 *      where `elem` is the current toml_datum_t (a table) and `idx`
 *      is the 0-based index into the destination array.
 *
 * If the key is missing from the TOML, the count stays at 0 (lenient).
 * If the array exceeds the maximum, the function returns -1 immediately.
 */
#define PARSE_ARRAY(toml_key, count_field, max_count, parse_body)          \
    do {                                                                    \
        toml_datum_t arr_d = toml_get(top, toml_key);                      \
        if (arr_d.type == TOML_ARRAY) {                                    \
            int n = arr_d.u.arr.size;                                      \
            if (n > (max_count)) {                                         \
                fprintf(stderr, "serializer: %s array has %d items "       \
                        "(max %d)\n", toml_key, n, (max_count));           \
                toml_free(r);                                              \
                return -1;                                                 \
            }                                                              \
            def->count_field = n;                                          \
            for (int idx = 0; idx < n; idx++) {                            \
                toml_datum_t elem = arr_d.u.arr.elem[idx];                 \
                parse_body                                                 \
            }                                                              \
        }                                                                  \
    } while (0)

int level_load_toml(const char *path, LevelDef *def) {
    if (!path || !def) return -1;

    /*
     * toml_parse_file_ex — open and parse a TOML file in one call.
     *
     * Returns a toml_result_t.  If parsing fails, r.ok is false and
     * r.errmsg contains a human-readable error description.  On success,
     * r.toptab is the root TOML table we can query with toml_get().
     */
    toml_result_t r = toml_parse_file_ex(path);
    if (!r.ok) {
        fprintf(stderr, "serializer: TOML parse error in '%s': %s\n",
                path, r.errmsg);
        return -1;
    }

    /*
     * Zero the entire struct first.  This sets all counts to 0, all
     * floats to 0.0, and all pointers to NULL — a clean starting state.
     * Any arrays missing from the TOML will naturally have count = 0.
     */
    memset(def, 0, sizeof(*def));

    toml_datum_t top = r.toptab;

    /* ---- Name ---------------------------------------------------- */

    {
        const char *name_str = get_str(top, "name", "Untitled");
        strncpy(def->name, name_str, sizeof(def->name) - 1);
        def->name[sizeof(def->name) - 1] = '\0';
    }

    /* Description — multi-line string or single-line */
    {
        const char *desc = get_str(top, "description", "");
        strncpy(def->description, desc, sizeof(def->description) - 1);
        def->description[sizeof(def->description) - 1] = '\0';
    }

    /* Author attribution */
    {
        const char *gen = get_str(top, "generated_by", "");
        strncpy(def->generated_by, gen, sizeof(def->generated_by) - 1);
        def->generated_by[sizeof(def->generated_by) - 1] = '\0';
    }

    def->screen_count = get_int(top, "screen_count", 4);

    /* ---- Floor gaps ----------------------------------------------- */

    {
        toml_datum_t sg = toml_get(top, "floor_gaps");
        if (sg.type == TOML_ARRAY) {
            int n = sg.u.arr.size;
            if (n > MAX_FLOOR_GAPS) {
                fprintf(stderr, "serializer: floor_gaps array has %d items "
                        "(max %d)\n", n, MAX_FLOOR_GAPS);
                toml_free(r);
                return -1;
            }
            def->floor_gap_count = n;
            for (int i = 0; i < n; i++) {
                toml_datum_t v = sg.u.arr.elem[i];
                if (v.type == TOML_INT64)     def->floor_gaps[i] = (int)v.u.int64;
                else if (v.type == TOML_FP64) def->floor_gaps[i] = (int)v.u.fp64;
                else                          def->floor_gaps[i] = 0;
            }
        }
    }

    /* ---- Rails --------------------------------------------------- */

    PARSE_ARRAY("rails", rail_count, MAX_RAILS, {
        def->rails[idx].layout  = rail_layout_from_str(
            get_str(elem, "layout", "RECT"));
        def->rails[idx].x       = get_int(elem, "x", 0);
        def->rails[idx].y       = get_int(elem, "y", 0);
        def->rails[idx].w       = get_int(elem, "w", 0);
        def->rails[idx].h       = get_int(elem, "h", 0);
        def->rails[idx].end_cap = get_int(elem, "end_cap", 0);
    });

    /* ---- Platforms ------------------------------------------------ */

    PARSE_ARRAY("platforms", platform_count, MAX_PLATFORMS, {
        def->platforms[idx].x           = get_float(elem, "x", 0);
        def->platforms[idx].tile_height = get_int(elem, "tile_height", 1);
        def->platforms[idx].tile_width  = get_int(elem, "tile_width", 1);
    });

    /* ---- Coins --------------------------------------------------- */

    PARSE_ARRAY("coins", coin_count, MAX_COINS, {
        def->coins[idx].x = get_float(elem, "x", 0);
        def->coins[idx].y = get_float(elem, "y", 0);
    });

    /* ---- Star yellows --------------------------------------------- */

    PARSE_ARRAY("star_yellows", star_yellow_count, MAX_STAR_YELLOWS, {
        def->star_yellows[idx].x = get_float(elem, "x", 0);
        def->star_yellows[idx].y = get_float(elem, "y", 0);
    });

    /* ---- Star greens ---------------------------------------------- */

    PARSE_ARRAY("star_greens", star_green_count, MAX_STAR_YELLOWS, {
        def->star_greens[idx].x = get_float(elem, "x", 0);
        def->star_greens[idx].y = get_float(elem, "y", 0);
    });

    /* ---- Star reds ------------------------------------------------ */

    PARSE_ARRAY("star_reds", star_red_count, MAX_STAR_YELLOWS, {
        def->star_reds[idx].x = get_float(elem, "x", 0);
        def->star_reds[idx].y = get_float(elem, "y", 0);
    });

    /* ---- Last star (single table, not array) --------------------- */

    {
        toml_datum_t ls = toml_get(top, "last_star");
        if (ls.type == TOML_TABLE) {
            def->last_star.x = get_float(ls, "x", 0);
            def->last_star.y = get_float(ls, "y", 0);
        }
    }

    /* ---- Spiders ------------------------------------------------- */

    PARSE_ARRAY("spiders", spider_count, MAX_SPIDERS, {
        def->spiders[idx].x           = get_float(elem, "x", 0);
        def->spiders[idx].vx          = get_float(elem, "vx", 0);
        def->spiders[idx].patrol_x0   = get_float(elem, "patrol_x0", 0);
        def->spiders[idx].patrol_x1   = get_float(elem, "patrol_x1", 0);
        def->spiders[idx].frame_index = get_int(elem, "frame_index", 0);
    });

    /* ---- Jumping spiders ----------------------------------------- */

    PARSE_ARRAY("jumping_spiders", jumping_spider_count,
                MAX_JUMPING_SPIDERS, {
        def->jumping_spiders[idx].x         = get_float(elem, "x", 0);
        def->jumping_spiders[idx].vx        = get_float(elem, "vx", 0);
        def->jumping_spiders[idx].patrol_x0 = get_float(elem, "patrol_x0", 0);
        def->jumping_spiders[idx].patrol_x1 = get_float(elem, "patrol_x1", 0);
    });

    /* ---- Birds --------------------------------------------------- */

    PARSE_ARRAY("birds", bird_count, MAX_BIRDS, {
        def->birds[idx].x           = get_float(elem, "x", 0);
        def->birds[idx].base_y      = get_float(elem, "base_y", 0);
        def->birds[idx].vx          = get_float(elem, "vx", 0);
        def->birds[idx].patrol_x0   = get_float(elem, "patrol_x0", 0);
        def->birds[idx].patrol_x1   = get_float(elem, "patrol_x1", 0);
        def->birds[idx].frame_index = get_int(elem, "frame_index", 0);
    });

    /* ---- Faster birds -------------------------------------------- */

    PARSE_ARRAY("faster_birds", faster_bird_count, MAX_FASTER_BIRDS, {
        def->faster_birds[idx].x           = get_float(elem, "x", 0);
        def->faster_birds[idx].base_y      = get_float(elem, "base_y", 0);
        def->faster_birds[idx].vx          = get_float(elem, "vx", 0);
        def->faster_birds[idx].patrol_x0   = get_float(elem, "patrol_x0", 0);
        def->faster_birds[idx].patrol_x1   = get_float(elem, "patrol_x1", 0);
        def->faster_birds[idx].frame_index = get_int(elem, "frame_index", 0);
    });

    /* ---- Fish ---------------------------------------------------- */

    PARSE_ARRAY("fish", fish_count, MAX_FISH, {
        def->fish[idx].x         = get_float(elem, "x", 0);
        def->fish[idx].vx        = get_float(elem, "vx", 0);
        def->fish[idx].patrol_x0 = get_float(elem, "patrol_x0", 0);
        def->fish[idx].patrol_x1 = get_float(elem, "patrol_x1", 0);
    });

    /* ---- Faster fish --------------------------------------------- */

    PARSE_ARRAY("faster_fish", faster_fish_count, MAX_FASTER_FISH, {
        def->faster_fish[idx].x         = get_float(elem, "x", 0);
        def->faster_fish[idx].vx        = get_float(elem, "vx", 0);
        def->faster_fish[idx].patrol_x0 = get_float(elem, "patrol_x0", 0);
        def->faster_fish[idx].patrol_x1 = get_float(elem, "patrol_x1", 0);
    });

    /* ---- Axe traps ----------------------------------------------- */

    PARSE_ARRAY("axe_traps", axe_trap_count, MAX_AXE_TRAPS, {
        def->axe_traps[idx].pillar_x = get_float(elem, "pillar_x", 0);
        def->axe_traps[idx].y        = get_float(elem, "y", 0);
        def->axe_traps[idx].mode     = axe_mode_from_str(
            get_str(elem, "mode", "PENDULUM"));
    });

    /* ---- Circular saws ------------------------------------------- */

    PARSE_ARRAY("circular_saws", circular_saw_count, MAX_CIRCULAR_SAWS, {
        def->circular_saws[idx].x         = get_float(elem, "x", 0);
        def->circular_saws[idx].y         = get_float(elem, "y", 0);
        def->circular_saws[idx].patrol_x0 = get_float(elem, "patrol_x0", 0);
        def->circular_saws[idx].patrol_x1 = get_float(elem, "patrol_x1", 0);
        def->circular_saws[idx].direction = get_int(elem, "direction", 1);
    });

    /* ---- Spike rows ---------------------------------------------- */

    PARSE_ARRAY("spike_rows", spike_row_count, MAX_SPIKE_ROWS, {
        def->spike_rows[idx].x     = get_float(elem, "x", 0);
        def->spike_rows[idx].count = get_int(elem, "count", 1);
    });

    /* ---- Spike platforms ----------------------------------------- */

    PARSE_ARRAY("spike_platforms", spike_platform_count,
                MAX_SPIKE_PLATFORMS, {
        def->spike_platforms[idx].x          = get_float(elem, "x", 0);
        def->spike_platforms[idx].y          = get_float(elem, "y", 0);
        def->spike_platforms[idx].tile_count = get_int(elem, "tile_count", 1);
    });

    /* ---- Spike blocks -------------------------------------------- */

    PARSE_ARRAY("spike_blocks", spike_block_count, MAX_SPIKE_BLOCKS, {
        def->spike_blocks[idx].rail_index = get_int(elem, "rail_index", 0);
        def->spike_blocks[idx].t_offset   = get_float(elem, "t_offset", 0);
        def->spike_blocks[idx].speed      = get_float(elem, "speed", 0);
    });

    /* ---- Blue flames --------------------------------------------- */

    PARSE_ARRAY("blue_flames", blue_flame_count, MAX_BLUE_FLAMES, {
        def->blue_flames[idx].x = get_float(elem, "x", 0);
    });

    /* ---- Fire flames --------------------------------------------- */

    PARSE_ARRAY("fire_flames", fire_flame_count, MAX_BLUE_FLAMES, {
        def->fire_flames[idx].x = get_float(elem, "x", 0);
    });

    /* ---- Float platforms ----------------------------------------- */

    PARSE_ARRAY("float_platforms", float_platform_count,
                MAX_FLOAT_PLATFORMS, {
        def->float_platforms[idx].mode       = float_mode_from_str(
            get_str(elem, "mode", "STATIC"));
        def->float_platforms[idx].x          = get_float(elem, "x", 0);
        def->float_platforms[idx].y          = get_float(elem, "y", 0);
        def->float_platforms[idx].tile_count = get_int(elem, "tile_count", 1);
        def->float_platforms[idx].rail_index = get_int(elem, "rail_index", 0);
        def->float_platforms[idx].t_offset   = get_float(elem, "t_offset", 0);
        def->float_platforms[idx].speed      = get_float(elem, "speed", 0);
    });

    /* ---- Bridges ------------------------------------------------- */

    PARSE_ARRAY("bridges", bridge_count, MAX_BRIDGES, {
        def->bridges[idx].x           = get_float(elem, "x", 0);
        def->bridges[idx].y           = get_float(elem, "y", 0);
        def->bridges[idx].brick_count = get_int(elem, "brick_count", 1);
    });

    /* ---- Bouncepads (small) -------------------------------------- */

    PARSE_ARRAY("bouncepads_small", bouncepad_small_count,
                MAX_BOUNCEPADS_SMALL, {
        def->bouncepads_small[idx].x         = get_float(elem, "x", 0);
        def->bouncepads_small[idx].launch_vy = get_float(elem, "launch_vy", 0);
        def->bouncepads_small[idx].pad_type  = bouncepad_type_from_str(
            get_str(elem, "pad_type", "GREEN"));
    });

    /* ---- Bouncepads (medium) ------------------------------------- */

    PARSE_ARRAY("bouncepads_medium", bouncepad_medium_count,
                MAX_BOUNCEPADS_MEDIUM, {
        def->bouncepads_medium[idx].x         = get_float(elem, "x", 0);
        def->bouncepads_medium[idx].launch_vy = get_float(elem, "launch_vy", 0);
        def->bouncepads_medium[idx].pad_type  = bouncepad_type_from_str(
            get_str(elem, "pad_type", "WOOD"));
    });

    /* ---- Bouncepads (high) --------------------------------------- */

    PARSE_ARRAY("bouncepads_high", bouncepad_high_count,
                MAX_BOUNCEPADS_HIGH, {
        def->bouncepads_high[idx].x         = get_float(elem, "x", 0);
        def->bouncepads_high[idx].launch_vy = get_float(elem, "launch_vy", 0);
        def->bouncepads_high[idx].pad_type  = bouncepad_type_from_str(
            get_str(elem, "pad_type", "RED"));
    });

    /* ---- Vines --------------------------------------------------- */

    PARSE_ARRAY("vines", vine_count, MAX_VINES, {
        def->vines[idx].x          = get_float(elem, "x", 0);
        def->vines[idx].y          = get_float(elem, "y", 0);
        def->vines[idx].tile_count = get_int(elem, "tile_count", 1);
    });

    /* ---- Ladders ------------------------------------------------- */

    PARSE_ARRAY("ladders", ladder_count, MAX_LADDERS, {
        def->ladders[idx].x          = get_float(elem, "x", 0);
        def->ladders[idx].y          = get_float(elem, "y", 0);
        def->ladders[idx].tile_count = get_int(elem, "tile_count", 1);
    });

    /* ---- Ropes --------------------------------------------------- */

    PARSE_ARRAY("ropes", rope_count, MAX_ROPES, {
        def->ropes[idx].x          = get_float(elem, "x", 0);
        def->ropes[idx].y          = get_float(elem, "y", 0);
        def->ropes[idx].tile_count = get_int(elem, "tile_count", 1);
    });

    /* ---- Level-wide configuration -------------------------------- */

    /* Background layers */
    {
        toml_datum_t plx = toml_get(top, "background_layers");
        if (plx.type == TOML_ARRAY) {
            int n = plx.u.arr.size;
            if (n > MAX_BACKGROUND_LAYERS) n = MAX_BACKGROUND_LAYERS;
            def->background_layer_count = n;
            for (int i = 0; i < n; i++) {
                toml_datum_t elem = plx.u.arr.elem[i];
                const char *p = get_str(elem, "path", "");
                strncpy(def->background_layers[i].path, p, 63);
                def->background_layers[i].path[63] = '\0';
                def->background_layers[i].speed = get_float(elem, "speed", 0);
            }
        }
    }

    /* Foreground layers */
    {
        toml_datum_t fg = toml_get(top, "foreground_layers");
        if (fg.type == TOML_ARRAY) {
            int n = fg.u.arr.size;
            if (n > MAX_BACKGROUND_LAYERS) n = MAX_BACKGROUND_LAYERS;
            def->foreground_layer_count = n;
            for (int i = 0; i < n; i++) {
                toml_datum_t elem = fg.u.arr.elem[i];
                const char *p = get_str(elem, "path", "");
                strncpy(def->foreground_layers[i].path, p, 63);
                def->foreground_layers[i].path[63] = '\0';
                def->foreground_layers[i].speed = get_float(elem, "speed", 0);
            }
        }
    }

    /* Fog layers */
    {
        toml_datum_t fl = toml_get(top, "fog_layers");
        if (fl.type == TOML_ARRAY) {
            int n = fl.u.arr.size;
            if (n > MAX_FOG_TEXTURES) n = MAX_FOG_TEXTURES;
            def->fog_layer_count = n;
            for (int i = 0; i < n; i++) {
                toml_datum_t elem = fl.u.arr.elem[i];
                const char *p = get_str(elem, "path", "");
                strncpy(def->fog_layers[i].path, p, 63);
                def->fog_layers[i].path[63] = '\0';
                def->fog_layers[i].speed = get_float(elem, "speed", 0);
            }
        }
    }

    /* Player spawn */
    def->player_start_x = get_float(top, "player_start_x", 0);
    def->player_start_y = get_float(top, "player_start_y", 0);

    /* Music */
    {
        const char *mp = get_str(top, "music_path", "");
        strncpy(def->music_path, mp, sizeof(def->music_path) - 1);
        def->music_path[sizeof(def->music_path) - 1] = '\0';
    }
    def->music_volume = get_int(top, "music_volume", 0);

    /* Floor tile */
    {
        const char *ftp = get_str(top, "floor_tile_path", "");
        strncpy(def->floor_tile_path, ftp, sizeof(def->floor_tile_path) - 1);
        def->floor_tile_path[sizeof(def->floor_tile_path) - 1] = '\0';
    }

    /* Game rules */
    def->initial_hearts = get_int(top, "initial_hearts", 0);
    def->initial_lives  = get_int(top, "initial_lives", 0);
    def->score_per_life = get_int(top, "score_per_life", 0);
    def->coin_score     = get_int(top, "coin_score", 0);

    /*
     * toml_free — release all memory allocated by toml_parse_file_ex.
     * The LevelDef struct now holds its own copies of all data, so
     * the TOML tree is safe to destroy.
     */
    toml_free(r);

    return 0;
}

#undef PARSE_ARRAY
