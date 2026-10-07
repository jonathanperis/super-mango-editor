/*
 * serializer_load_geometry.c — TOML rail and platform parsing helpers.
 */

#include <stddef.h> /* size_t */
#include <stdio.h>  /* fprintf */
#include <string.h> /* strncpy */

#include "serializer_load_geometry.h"
#include "serializer_parse.h"
#include "serializer_types.h"
#include "../game.h" /* MAX_* constants */

static void copy_geometry_string(char *dst, size_t dst_size, const char *src) {
    if (!dst || dst_size == 0) return;

    strncpy(dst, src ? src : "", dst_size - 1);
    dst[dst_size - 1] = '\0';
}

int serializer_load_geometry(toml_datum_t top, LevelDef *def) {
    if (!def) return -1;

    /* Rails */
    LOAD_ARRAY("rails", rail_count, MAX_RAILS, {
        def->rails[idx].layout  = serializer_rail_layout_from_str(
            get_str(elem, "layout", "RECT"));
        def->rails[idx].x       = get_int(elem, "x", 0);
        def->rails[idx].y       = get_int(elem, "y", 0);
        def->rails[idx].w       = get_int(elem, "w", 0);
        def->rails[idx].h       = get_int(elem, "h", 0);
        def->rails[idx].end_cap = get_int(elem, "end_cap", 0);
    });

    /* Platforms */
    LOAD_ARRAY("platforms", platform_count, MAX_PLATFORMS, {
        def->platforms[idx].x           = get_float(elem, "x", 0);
        def->platforms[idx].tile_height = get_int(elem, "tile_height", 1);
        def->platforms[idx].tile_width  = get_int(elem, "tile_width", 1);

        {
            const char *tp = get_str(elem, "tile_path", "");
            copy_geometry_string(def->platforms[idx].tile_path,
                                 sizeof(def->platforms[idx].tile_path), tp);
        }
    });

    return 0;
}
