/*
 * serializer_load_enemies.c — TOML enemy parsing helpers.
 */

#include <stdio.h> /* fprintf */

#include "serializer_load_enemies.h"
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

int serializer_load_enemies(toml_datum_t top, LevelDef *def) {
    if (!def) return -1;

    /* Spiders */
    LOAD_ARRAY("spiders", spider_count, MAX_SPIDERS, {
        def->spiders[idx].x           = get_float(elem, "x", 0);
        def->spiders[idx].vx          = get_float(elem, "vx", 0);
        def->spiders[idx].patrol_x0   = get_float(elem, "patrol_x0", 0);
        def->spiders[idx].patrol_x1   = get_float(elem, "patrol_x1", 0);
        def->spiders[idx].frame_index = get_int(elem, "frame_index", 0);
    });

    /* Jumping spiders */
    LOAD_ARRAY("jumping_spiders", jumping_spider_count,
               MAX_JUMPING_SPIDERS, {
        def->jumping_spiders[idx].x         = get_float(elem, "x", 0);
        def->jumping_spiders[idx].vx        = get_float(elem, "vx", 0);
        def->jumping_spiders[idx].patrol_x0 = get_float(elem, "patrol_x0", 0);
        def->jumping_spiders[idx].patrol_x1 = get_float(elem, "patrol_x1", 0);
    });

    /* Birds */
    LOAD_ARRAY("birds", bird_count, MAX_BIRDS, {
        def->birds[idx].x           = get_float(elem, "x", 0);
        def->birds[idx].base_y      = get_float(elem, "base_y", 0);
        def->birds[idx].vx          = get_float(elem, "vx", 0);
        def->birds[idx].patrol_x0   = get_float(elem, "patrol_x0", 0);
        def->birds[idx].patrol_x1   = get_float(elem, "patrol_x1", 0);
        def->birds[idx].frame_index = get_int(elem, "frame_index", 0);
    });

    /* Faster birds */
    LOAD_ARRAY("faster_birds", faster_bird_count, MAX_FASTER_BIRDS, {
        def->faster_birds[idx].x           = get_float(elem, "x", 0);
        def->faster_birds[idx].base_y      = get_float(elem, "base_y", 0);
        def->faster_birds[idx].vx          = get_float(elem, "vx", 0);
        def->faster_birds[idx].patrol_x0   = get_float(elem, "patrol_x0", 0);
        def->faster_birds[idx].patrol_x1   = get_float(elem, "patrol_x1", 0);
        def->faster_birds[idx].frame_index = get_int(elem, "frame_index", 0);
    });

    /* Fish */
    LOAD_ARRAY("fish", fish_count, MAX_FISH, {
        def->fish[idx].x         = get_float(elem, "x", 0);
        def->fish[idx].vx        = get_float(elem, "vx", 0);
        def->fish[idx].patrol_x0 = get_float(elem, "patrol_x0", 0);
        def->fish[idx].patrol_x1 = get_float(elem, "patrol_x1", 0);
    });

    /* Faster fish */
    LOAD_ARRAY("faster_fish", faster_fish_count, MAX_FASTER_FISH, {
        def->faster_fish[idx].x         = get_float(elem, "x", 0);
        def->faster_fish[idx].vx        = get_float(elem, "vx", 0);
        def->faster_fish[idx].patrol_x0 = get_float(elem, "patrol_x0", 0);
        def->faster_fish[idx].patrol_x1 = get_float(elem, "patrol_x1", 0);
    });

    return 0;
}

#undef LOAD_ARRAY
