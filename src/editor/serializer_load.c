/*
 * serializer_load.c — TOML loading for level definitions.
 *
 * Reads TOML files from disk into LevelDef structs for editing or playtesting.
 * level_save_toml lives in serializer_save.c; this file keeps level_load_toml.
 *
 * All enum values are serialized as readable strings ("RECT", "SPIN", etc.)
 * rather than raw integers, so the TOML files are easy to understand and
 * edit by hand.
 */

#include <stdio.h>   /* fprintf, fopen, fclose, fread, fseek, ftell */
#include <stdlib.h>  /* malloc, free, exit */
#include <string.h>  /* strcmp, memset, strncpy */

#include "serializer.h"
#include "serializer_parse.h"
#include "serializer_types.h"                    /* enum/string conversion helpers */
#include "../../vendor/tomlc17/tomlc17.h" /* tomlc17 API */
#include "../levels/level.h"              /* LevelDef, all placement types */
#include "../levels/level_loader.h"       /* level_validate_runtime */
#include "../game.h"                      /* MAX_* constants */

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

    LevelDef loaded;
    LevelDef *caller_def = def;

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
     * Parse into staging storage so failed validation never leaves the caller
     * with a partially-loaded LevelDef.
     */
    def = &loaded;
    level_def_init_defaults(def);

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
        def->rails[idx].layout  = serializer_rail_layout_from_str(
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
        const char *tp = get_str(elem, "tile_path", "");
        strncpy(def->platforms[idx].tile_path, tp,
                sizeof(def->platforms[idx].tile_path) - 1);
        def->platforms[idx].tile_path[sizeof(def->platforms[idx].tile_path) - 1] = '\0';
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

    PARSE_ARRAY("star_greens", star_green_count, MAX_STAR_GREENS, {
        def->star_greens[idx].x = get_float(elem, "x", 0);
        def->star_greens[idx].y = get_float(elem, "y", 0);
    });

    /* ---- Star reds ------------------------------------------------ */

    PARSE_ARRAY("star_reds", star_red_count, MAX_STAR_REDS, {
        def->star_reds[idx].x = get_float(elem, "x", 0);
        def->star_reds[idx].y = get_float(elem, "y", 0);
    });

    /* ---- Last star (single table, not array) --------------------- */

    {
        toml_datum_t ls = toml_get(top, "last_star");
        if (ls.type == TOML_TABLE) {
            def->last_star.x = get_float(ls, "x", 0);
            def->last_star.y = get_float(ls, "y", 0);
            /* Optional next_phase for level linking */
            toml_datum_t np = toml_get(ls, "next_phase");
            if (np.type == TOML_STRING) {
                strncpy(def->next_phase, np.u.s, sizeof(def->next_phase) - 1);
                def->next_phase[sizeof(def->next_phase) - 1] = '\0';
            }
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
        def->axe_traps[idx].mode     = serializer_axe_mode_from_str(
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
        def->float_platforms[idx].mode       = serializer_float_mode_from_str(
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
        def->bouncepads_small[idx].pad_type  = serializer_bouncepad_type_from_str(
            get_str(elem, "pad_type", "GREEN"));
    });

    /* ---- Bouncepads (medium) ------------------------------------- */

    PARSE_ARRAY("bouncepads_medium", bouncepad_medium_count,
                MAX_BOUNCEPADS_MEDIUM, {
        def->bouncepads_medium[idx].x         = get_float(elem, "x", 0);
        def->bouncepads_medium[idx].launch_vy = get_float(elem, "launch_vy", 0);
        def->bouncepads_medium[idx].pad_type  = serializer_bouncepad_type_from_str(
            get_str(elem, "pad_type", "WOOD"));
    });

    /* ---- Bouncepads (high) --------------------------------------- */

    PARSE_ARRAY("bouncepads_high", bouncepad_high_count,
                MAX_BOUNCEPADS_HIGH, {
        def->bouncepads_high[idx].x         = get_float(elem, "x", 0);
        def->bouncepads_high[idx].launch_vy = get_float(elem, "launch_vy", 0);
        def->bouncepads_high[idx].pad_type  = serializer_bouncepad_type_from_str(
            get_str(elem, "pad_type", "RED"));
    });

    /* ---- Vines --------------------------------------------------- */

    PARSE_ARRAY("vines", vine_count, MAX_VINES, {
        def->vines[idx].x          = get_float(elem, "x", 0);
        def->vines[idx].y          = get_float(elem, "y", 0);
        def->vines[idx].tile_count = get_int(elem, "tile_count", 1);
        def->vines[idx].vine_type  = get_int(elem, "vine_type", 0);
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
            if (n > MAX_BACKGROUND_LAYERS) {
                fprintf(stderr, "serializer: background_layers array has %d items "
                        "(max %d)\n", n, MAX_BACKGROUND_LAYERS);
                toml_free(r);
                return -1;
            }
            def->background_layer_count = n;
            for (int i = 0; i < n; i++) {
                toml_datum_t elem = plx.u.arr.elem[i];
                const char *p = get_str(elem, "path", "");
                strncpy(def->background_layers[i].path, p,
                        sizeof(def->background_layers[i].path) - 1);
                def->background_layers[i].path[
                    sizeof(def->background_layers[i].path) - 1] = '\0';
                def->background_layers[i].speed = get_float(elem, "speed", 0);
            }
        }
    }

    /* Foreground layers */
    {
        toml_datum_t fg = toml_get(top, "foreground_layers");
        if (fg.type == TOML_ARRAY) {
            int n = fg.u.arr.size;
            if (n > MAX_BACKGROUND_LAYERS) {
                fprintf(stderr, "serializer: foreground_layers array has %d items "
                        "(max %d)\n", n, MAX_BACKGROUND_LAYERS);
                toml_free(r);
                return -1;
            }
            def->foreground_layer_count = n;
            for (int i = 0; i < n; i++) {
                toml_datum_t elem = fg.u.arr.elem[i];
                const char *p = get_str(elem, "path", "");
                strncpy(def->foreground_layers[i].path, p,
                        sizeof(def->foreground_layers[i].path) - 1);
                def->foreground_layers[i].path[
                    sizeof(def->foreground_layers[i].path) - 1] = '\0';
                def->foreground_layers[i].speed = get_float(elem, "speed", 0);
            }
        }
    }

    /* Fog layers */
    {
        toml_datum_t fl = toml_get(top, "fog_layers");
        if (fl.type == TOML_ARRAY) {
            int n = fl.u.arr.size;
            if (n > MAX_FOG_TEXTURES) {
                fprintf(stderr, "serializer: fog_layers array has %d items "
                        "(max %d)\n", n, MAX_FOG_TEXTURES);
                toml_free(r);
                return -1;
            }
            def->fog_layer_count = n;
            for (int i = 0; i < n; i++) {
                toml_datum_t elem = fl.u.arr.elem[i];
                const char *p = get_str(elem, "path", "");
                strncpy(def->fog_layers[i].path, p,
                        sizeof(def->fog_layers[i].path) - 1);
                def->fog_layers[i].path[
                    sizeof(def->fog_layers[i].path) - 1] = '\0';
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

    /* Player movement physics — [physics] sub-table; -1.0 = use engine default */
    {
        toml_datum_t ph = toml_get(top, "physics");
        if (ph.type == TOML_TABLE) {
            def->physics.walk_max_speed          = get_float(ph, "walk_max_speed",          -1.0f);
            def->physics.run_max_speed           = get_float(ph, "run_max_speed",           -1.0f);
            def->physics.walk_ground_accel       = get_float(ph, "walk_ground_accel",       -1.0f);
            def->physics.run_ground_accel        = get_float(ph, "run_ground_accel",        -1.0f);
            def->physics.ground_friction         = get_float(ph, "ground_friction",         -1.0f);
            def->physics.ground_counter_accel    = get_float(ph, "ground_counter_accel",    -1.0f);
            def->physics.air_accel_walk          = get_float(ph, "air_accel_walk",          -1.0f);
            def->physics.air_accel_run           = get_float(ph, "air_accel_run",           -1.0f);
            def->physics.air_friction            = get_float(ph, "air_friction",            -1.0f);
            def->physics.cam_lookahead_vx_factor = get_float(ph, "cam_lookahead_vx_factor", -1.0f);
            def->physics.cam_lookahead_max       = get_float(ph, "cam_lookahead_max",       -1.0f);
        }
    }

    {
        char err[128];
        if (level_validate_runtime(def, err, sizeof(err)) != 0) {
            fprintf(stderr, "serializer: invalid level '%s': %s\n", path, err);
            toml_free(r);
            return -1;
        }
    }

    /*
     * toml_free — release all memory allocated by toml_parse_file_ex.
     * The LevelDef struct now holds its own copies of all data, so
     * the TOML tree is safe to destroy.
     */
    toml_free(r);

    *caller_def = loaded;

    return 0;
}

#undef PARSE_ARRAY
