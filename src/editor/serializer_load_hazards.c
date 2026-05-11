/*
 * serializer_load_hazards.c — TOML hazard parsing helpers.
 */

#include <stdio.h> /* fprintf */

#include "serializer_load_hazards.h"
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

int serializer_load_hazards(toml_datum_t top, LevelDef *def) {
    if (!def) return -1;

    /* Axe traps */
    LOAD_ARRAY("axe_traps", axe_trap_count, MAX_AXE_TRAPS, {
        def->axe_traps[idx].pillar_x = get_float(elem, "pillar_x", 0);
        def->axe_traps[idx].y        = get_float(elem, "y", 0);
        def->axe_traps[idx].mode     = serializer_axe_mode_from_str(
            get_str(elem, "mode", "PENDULUM"));
    });

    /* Circular saws */
    LOAD_ARRAY("circular_saws", circular_saw_count, MAX_CIRCULAR_SAWS, {
        def->circular_saws[idx].x         = get_float(elem, "x", 0);
        def->circular_saws[idx].y         = get_float(elem, "y", 0);
        def->circular_saws[idx].patrol_x0 = get_float(elem, "patrol_x0", 0);
        def->circular_saws[idx].patrol_x1 = get_float(elem, "patrol_x1", 0);
        def->circular_saws[idx].direction = get_int(elem, "direction", 1);
    });

    /* Spike rows */
    LOAD_ARRAY("spike_rows", spike_row_count, MAX_SPIKE_ROWS, {
        def->spike_rows[idx].x     = get_float(elem, "x", 0);
        def->spike_rows[idx].count = get_int(elem, "count", 1);
    });

    /* Spike platforms */
    LOAD_ARRAY("spike_platforms", spike_platform_count,
               MAX_SPIKE_PLATFORMS, {
        def->spike_platforms[idx].x          = get_float(elem, "x", 0);
        def->spike_platforms[idx].y          = get_float(elem, "y", 0);
        def->spike_platforms[idx].tile_count = get_int(elem, "tile_count", 1);
    });

    /* Spike blocks */
    LOAD_ARRAY("spike_blocks", spike_block_count, MAX_SPIKE_BLOCKS, {
        def->spike_blocks[idx].rail_index = get_int(elem, "rail_index", 0);
        def->spike_blocks[idx].t_offset   = get_float(elem, "t_offset", 0);
        def->spike_blocks[idx].speed      = get_float(elem, "speed", 0);
    });

    /* Blue flames */
    LOAD_ARRAY("blue_flames", blue_flame_count, MAX_BLUE_FLAMES, {
        def->blue_flames[idx].x = get_float(elem, "x", 0);
    });

    /* Fire flames */
    LOAD_ARRAY("fire_flames", fire_flame_count, MAX_FIRE_FLAMES, {
        def->fire_flames[idx].x = get_float(elem, "x", 0);
    });

    return 0;
}

#undef LOAD_ARRAY
