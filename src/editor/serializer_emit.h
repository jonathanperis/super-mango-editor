/*
 * serializer_emit.h — Internal TOML emitter helpers.
 */

#pragma once

#include <stdio.h>  /* FILE */

/* Format a TOML float with compact trailing-zero handling. */
const char *fmt_float(double val);

/* Emit a TOML basic string with required escaping. */
void write_toml_string(FILE *fp, const char *s);

/* Emit a key/value pair whose value is a TOML basic string. */
void write_toml_key_string(FILE *fp, const char *key, const char *value);
