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

/*
 * serializer_message_text — Make untrusted bytes safe to quote in a message.
 *
 * A key or a string from a level file can hold any byte, terminal escape
 * sequences (ESC [ ...) included. Printing it as it is would let a file
 * change the colours or the title of the terminal that reads the log. This
 * copies the len bytes of text into out with every byte outside printable
 * ASCII written as \xHH and a backslash as \\, so the message shows what
 * the file holds without the terminal acting on it. out always ends in a
 * NUL; a text too long for it is cut at a whole escape. Returns out.
 */
const char *serializer_message_text(char *out, size_t out_size,
                                    const char *text, size_t len);
