/*
 * serializer_parse.h — Internal TOML parse helpers.
 */

#pragma once

#include <stddef.h>                         /* size_t */
#include <stdio.h>                          /* fprintf, used by LOAD_ARRAY */
#include "../../vendor/tomlc17/tomlc17.h"  /* toml_datum_t */

/* Read a TOML numeric field as float, accepting int or float scalars. */
float get_float(toml_datum_t tab, const char *key, float fallback);

/* Read a TOML integer field, accepting int or float scalars. */
int get_int(toml_datum_t tab, const char *key, int fallback);

/* Read a TOML string field, returning fallback for missing/non-string values. */
const char *get_str(toml_datum_t tab, const char *key, const char *fallback);

/*
 * Validate explicit v1 schema before any TOML value is normalized into a
 * LevelDef. Missing format_version selects legacy-compatible loading.
 */
int serializer_validate_schema(toml_datum_t top, int *strict,
                               char *error, size_t error_size);

/*
 * LOAD_ARRAY — Copy one TOML array of tables into a LevelDef array.
 *
 * Used inside the serializer_load_*() section loaders, which all have
 * `top` (the parsed TOML root) and `def` (the LevelDef being filled) in
 * scope. If `toml_key` names an array, its length is checked against
 * max_count (too many items fails the whole load with -1), stored in
 * def->count_field, and parse_body runs once per item with `idx` (the
 * item's index) and `elem` (the item's table) in scope:
 *
 *     LOAD_ARRAY("ropes", rope_count, MAX_ROPES, {
 *         def->ropes[idx].x = get_float(elem, "x", 0.0f);
 *     });
 *
 * A missing key leaves the array empty, which is how optional sections
 * work. This is a macro rather than a function because parse_body is a
 * block of code that differs for every array.
 */
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
