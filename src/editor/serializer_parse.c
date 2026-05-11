/*
 * serializer_parse.c — Internal TOML parse helpers.
 */

#include "serializer_parse.h"

/*
 * get_float — Read a numeric field from a TOML table, returning float.
 *
 * TOML distinguishes integers (no decimal point) from floats (with decimal).
 * A field written as "79" comes back as TOML_INT64, while "79.0" comes as
 * TOML_FP64.  We accept both and convert to float.
 */
float get_float(toml_datum_t tab, const char *key, float fallback)
{
    toml_datum_t d = toml_get(tab, key);
    if (d.type == TOML_FP64)  return (float)d.u.fp64;
    if (d.type == TOML_INT64) return (float)d.u.int64;
    return fallback;
}

/*
 * get_int — Read an integer field from a TOML table.
 *
 * Accepts both TOML_INT64 and TOML_FP64 for resilience (a hand-edited
 * file might use "1.0" instead of "1").
 */
int get_int(toml_datum_t tab, const char *key, int fallback)
{
    toml_datum_t d = toml_get(tab, key);
    if (d.type == TOML_INT64) return (int)d.u.int64;
    if (d.type == TOML_FP64)  return (int)d.u.fp64;
    return fallback;
}

/*
 * get_str — Read a string field from a TOML table.
 *
 * Returns a pointer into the parsed TOML tree (valid until toml_free).
 * Returns `fallback` if the field is missing or not a string.
 */
const char *get_str(toml_datum_t tab, const char *key, const char *fallback)
{
    toml_datum_t d = toml_get(tab, key);
    if (d.type == TOML_STRING) return d.u.s;
    return fallback;
}
