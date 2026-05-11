/*
 * serializer_load_header.c — TOML header and floor-gap parsing.
 */

#include <stddef.h> /* size_t */
#include <stdio.h>  /* fprintf */
#include <string.h> /* strncpy */

#include "serializer_load_header.h"
#include "serializer_parse.h"
#include "../game.h" /* MAX_FLOOR_GAPS */

static void copy_header_string(char *dst, size_t dst_size, const char *src) {
    if (!dst || dst_size == 0) return;

    strncpy(dst, src ? src : "", dst_size - 1);
    dst[dst_size - 1] = '\0';
}

int serializer_load_header(toml_datum_t top, LevelDef *def) {
    if (!def) return -1;

    /* Name */
    {
        const char *name_str = get_str(top, "name", "Untitled");
        copy_header_string(def->name, sizeof(def->name), name_str);
    }

    /* Description — multi-line string or single-line */
    {
        const char *desc = get_str(top, "description", "");
        copy_header_string(def->description, sizeof(def->description), desc);
    }

    /* Author attribution */
    {
        const char *gen = get_str(top, "generated_by", "");
        copy_header_string(def->generated_by, sizeof(def->generated_by), gen);
    }

    def->screen_count = get_int(top, "screen_count", 4);

    /* Floor gaps */
    {
        toml_datum_t sg = toml_get(top, "floor_gaps");
        if (sg.type == TOML_ARRAY) {
            int n = sg.u.arr.size;
            if (n > MAX_FLOOR_GAPS) {
                fprintf(stderr, "serializer: floor_gaps array has %d items "
                        "(max %d)\n", n, MAX_FLOOR_GAPS);
                return -1;
            }
            def->floor_gap_count = n;
            for (int i = 0; i < n; i++) {
                toml_datum_t v = sg.u.arr.elem[i];
                if (v.type == TOML_INT64) {
                    def->floor_gaps[i] = (int)v.u.int64;
                } else if (v.type == TOML_FP64) {
                    def->floor_gaps[i] = (int)v.u.fp64;
                } else {
                    fprintf(stderr, "serializer: floor_gaps[%d] must be numeric\n", i);
                    return -1;
                }
            }
        }
    }

    return 0;
}
