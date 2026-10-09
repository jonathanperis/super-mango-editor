/*
 * serializer_load_climbables.c — TOML climbable/decor parsing helpers.
 */

#include <stdio.h> /* fprintf */

#include "serializer_load_climbables.h"
#include "serializer_parse.h"
#include "../levels/level.h" /* LevelDef and its MAX_* array limits */

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
