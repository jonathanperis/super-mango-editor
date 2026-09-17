/*
 * serializer_load_climbables.c — TOML climbable/decor parsing helpers.
 */

#include <stdio.h> /* fprintf */

#include "serializer_load_climbables.h"
#include "serializer_parse.h"
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

int serializer_load_climbables(toml_datum_t top, LevelDef *def) {
    if (!def) return -1;

    /* Vines */
    LOAD_ARRAY("vines", vine_count, MAX_VINES, {
        def->vines[idx].x          = get_float(elem, "x", 0);
        def->vines[idx].y          = get_float(elem, "y", 0);
        def->vines[idx].tile_count = get_int(elem, "tile_count", 1);
        def->vines[idx].vine_type  = get_int(elem, "vine_type", 0);
    });

    /* Ladders */
    LOAD_ARRAY("ladders", ladder_count, MAX_LADDERS, {
        def->ladders[idx].x          = get_float(elem, "x", 0);
        def->ladders[idx].y          = get_float(elem, "y", 0);
        def->ladders[idx].tile_count = get_int(elem, "tile_count", 1);
    });

    /* Ropes */
    LOAD_ARRAY("ropes", rope_count, MAX_ROPES, {
        def->ropes[idx].x          = get_float(elem, "x", 0);
        def->ropes[idx].y          = get_float(elem, "y", 0);
        def->ropes[idx].tile_count = get_int(elem, "tile_count", 1);
    });

    return 0;
}

#undef LOAD_ARRAY
