/*
 * serializer_load_surfaces.c — TOML surface parsing helpers.
 */

#include <stdio.h> /* fprintf */

#include "serializer_load_surfaces.h"
#include "serializer_parse.h"
#include "serializer_types.h"
#include "../game.h" /* MAX_* constants */

#define LOAD_ARRAY(toml_key, count_field, max_count, parse_body)           \
    do {                                                                   \
        toml_datum_t arr_d = toml_get(top, toml_key);                      \
        if (arr_d.type == TOML_ARRAY) {                                    \
            int n = arr_d.u.arr.size;                                      \
            if (n > (max_count)) {                                         \
                fprintf(stderr, "serializer: %s array has %d items "       \
                        "(max %d)\n", toml_key, n, (max_count));           \
                return -1;                                                 \
            }                                                              \
            def->count_field = n;                                          \
            for (int idx = 0; idx < n; idx++) {                            \
                toml_datum_t elem = arr_d.u.arr.elem[idx];                 \
                parse_body                                                 \
            }                                                              \
        }                                                                  \
    } while (0)

int serializer_load_surfaces(toml_datum_t top, LevelDef *def) {
    if (!def) return -1;

    /* Float platforms */
    LOAD_ARRAY("float_platforms", float_platform_count,
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

    /* Bridges */
    LOAD_ARRAY("bridges", bridge_count, MAX_BRIDGES, {
        def->bridges[idx].x           = get_float(elem, "x", 0);
        def->bridges[idx].y           = get_float(elem, "y", 0);
        def->bridges[idx].brick_count = get_int(elem, "brick_count", 1);
    });

    /* Bouncepads (small) */
    LOAD_ARRAY("bouncepads_small", bouncepad_small_count,
               MAX_BOUNCEPADS_SMALL, {
        def->bouncepads_small[idx].x         = get_float(elem, "x", 0);
        def->bouncepads_small[idx].launch_vy = get_float(elem, "launch_vy", 0);
        def->bouncepads_small[idx].pad_type  = serializer_bouncepad_type_from_str(
            get_str(elem, "pad_type", "GREEN"));
    });

    /* Bouncepads (medium) */
    LOAD_ARRAY("bouncepads_medium", bouncepad_medium_count,
               MAX_BOUNCEPADS_MEDIUM, {
        def->bouncepads_medium[idx].x         = get_float(elem, "x", 0);
        def->bouncepads_medium[idx].launch_vy = get_float(elem, "launch_vy", 0);
        def->bouncepads_medium[idx].pad_type  = serializer_bouncepad_type_from_str(
            get_str(elem, "pad_type", "WOOD"));
    });

    /* Bouncepads (high) */
    LOAD_ARRAY("bouncepads_high", bouncepad_high_count,
               MAX_BOUNCEPADS_HIGH, {
        def->bouncepads_high[idx].x         = get_float(elem, "x", 0);
        def->bouncepads_high[idx].launch_vy = get_float(elem, "launch_vy", 0);
        def->bouncepads_high[idx].pad_type  = serializer_bouncepad_type_from_str(
            get_str(elem, "pad_type", "RED"));
    });

    return 0;
}

#undef LOAD_ARRAY
