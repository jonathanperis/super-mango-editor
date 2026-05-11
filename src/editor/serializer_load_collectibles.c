/*
 * serializer_load_collectibles.c — TOML collectible parsing helpers.
 */

#include <stddef.h> /* size_t */
#include <stdio.h>  /* fprintf */
#include <string.h> /* strncpy */

#include "serializer_load_collectibles.h"
#include "serializer_parse.h"
#include "../game.h" /* MAX_* constants */

static void copy_collectible_string(char *dst, size_t dst_size, const char *src) {
    if (!dst || dst_size == 0) return;

    strncpy(dst, src ? src : "", dst_size - 1);
    dst[dst_size - 1] = '\0';
}

#define LOAD_XY_ARRAY(toml_key, count_field, max_count, dest_field)        \
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
                def->dest_field[idx].x = get_float(elem, "x", 0);          \
                def->dest_field[idx].y = get_float(elem, "y", 0);          \
            }                                                              \
        }                                                                  \
    } while (0)

int serializer_load_collectibles(toml_datum_t top, LevelDef *def) {
    if (!def) return -1;

    LOAD_XY_ARRAY("coins", coin_count, MAX_COINS, coins);
    LOAD_XY_ARRAY("star_yellows", star_yellow_count,
                  MAX_STAR_YELLOWS, star_yellows);
    LOAD_XY_ARRAY("star_greens", star_green_count,
                  MAX_STAR_GREENS, star_greens);
    LOAD_XY_ARRAY("star_reds", star_red_count, MAX_STAR_REDS, star_reds);

    /* Last star (single table, not array) */
    {
        toml_datum_t ls = toml_get(top, "last_star");
        if (ls.type == TOML_TABLE) {
            def->last_star.x = get_float(ls, "x", 0);
            def->last_star.y = get_float(ls, "y", 0);
            /* Optional next_phase for level linking */
            toml_datum_t np = toml_get(ls, "next_phase");
            if (np.type == TOML_STRING) {
                copy_collectible_string(def->next_phase,
                                        sizeof(def->next_phase), np.u.s);
            }
        }
    }

    return 0;
}

#undef LOAD_XY_ARRAY
