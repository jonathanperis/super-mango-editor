/*
 * serializer_load_checkpoints.c — Authored checkpoint TOML parsing.
 */

#include "serializer_load_checkpoints.h"

#include <math.h>
#include <float.h>
#include <stdio.h>

static int checkpoint_number(toml_datum_t table, const char *key, float *out)
{
    toml_datum_t value = toml_get(table, key);
    float converted;

    if (!out || (value.type != TOML_INT64 && value.type != TOML_FP64)) {
        return -1;
    }
    if (value.type == TOML_FP64 &&
        (!isfinite(value.u.fp64) || fabs(value.u.fp64) > FLT_MAX)) return -1;
    converted = value.type == TOML_INT64
              ? (float)value.u.int64
              : (float)value.u.fp64;
    if (!isfinite(converted)) return -1;
    *out = converted;
    return 0;
}

int serializer_load_checkpoints(toml_datum_t top, LevelDef *def)
{
    toml_datum_t array;
    CheckpointPlacement staged[MAX_CHECKPOINTS];
    int count;

    if (!def) return -1;
    array = toml_get(top, "checkpoints");
    if (array.type == TOML_UNKNOWN) return 0;
    if (array.type != TOML_ARRAY || array.u.arr.size < 0 ||
        array.u.arr.size > MAX_CHECKPOINTS) {
        fprintf(stderr, "serializer: checkpoints must be an array of at most %d tables\n",
                MAX_CHECKPOINTS);
        return -1;
    }

    count = array.u.arr.size;
    for (int i = 0; i < count; i++) {
        toml_datum_t item = array.u.arr.elem[i];
        if (item.type != TOML_TABLE ||
            checkpoint_number(item, "x", &staged[i].x) != 0 ||
            checkpoint_number(item, "y", &staged[i].y) != 0) {
            fprintf(stderr, "serializer: checkpoints[%d] requires finite numeric x and y\n",
                    i);
            return -1;
        }
    }

    for (int i = 0; i < count; i++) def->checkpoints[i] = staged[i];
    def->checkpoint_count = count;
    return 0;
}
