/*
 * serializer_parse.h — Internal TOML parse helpers.
 */

#pragma once

#include "../../vendor/tomlc17/tomlc17.h"  /* toml_datum_t */

/* Read a TOML numeric field as float, accepting int or float scalars. */
float get_float(toml_datum_t tab, const char *key, float fallback);

/* Read a TOML integer field, accepting int or float scalars. */
int get_int(toml_datum_t tab, const char *key, int fallback);

/* Read a TOML string field, returning fallback for missing/non-string values. */
const char *get_str(toml_datum_t tab, const char *key, const char *fallback);
