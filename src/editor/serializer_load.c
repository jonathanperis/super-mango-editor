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
#include "serializer_load_collectibles.h"
#include "serializer_load_config.h"
#include "serializer_load_enemies.h"
#include "serializer_load_geometry.h"
#include "serializer_load_hazards.h"
#include "serializer_load_header.h"
#include "serializer_load_layers.h"
#include "serializer_load_surfaces.h"
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

    if (serializer_load_header(top, def) != 0) {
        toml_free(r);
        return -1;
    }

    if (serializer_load_geometry(top, def) != 0) {
        toml_free(r);
        return -1;
    }

    if (serializer_load_collectibles(top, def) != 0) {
        toml_free(r);
        return -1;
    }

    if (serializer_load_enemies(top, def) != 0) {
        toml_free(r);
        return -1;
    }

    if (serializer_load_hazards(top, def) != 0) {
        toml_free(r);
        return -1;
    }

    if (serializer_load_surfaces(top, def) != 0) {
        toml_free(r);
        return -1;
    }

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

    if (serializer_load_layers(top, def) != 0) {
        toml_free(r);
        return -1;
    }

    serializer_load_config(top, def);

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
