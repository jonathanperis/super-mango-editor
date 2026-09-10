/*
 * serializer_emit.c — Internal TOML emitter helpers.
 */

#include "serializer_emit.h"

#include <string.h>  /* strchr, strlen */

/*
 * fmt_float — Format a float with enough significant digits for a float
 * round trip, always keeping TOML's value classified as a float.
 *
 * Examples:  80.0f  → "80.0"     (not "80.00" or "80")
 *            0.08f  → round-trippable decimal
 *            536.2f → round-trippable decimal
 *           -380.0f → "-380.0"
 *
 * Returns a pointer to a static buffer — valid until the next call.
 * Safe for single-float-per-fprintf usage (which is all we do here).
 */
const char *fmt_float(double val)
{
    static char buf[64];
    snprintf(buf, sizeof(buf), "%.9g", val);

    /* TOML parses a bare integer as an integer, not a float. */
    if (!strchr(buf, '.') && !strchr(buf, 'e') && !strchr(buf, 'E')) {
        size_t len = strlen(buf);
        if (len + 2 < sizeof(buf)) {
            buf[len] = '.';
            buf[len + 1] = '0';
            buf[len + 2] = '\0';
        }
    }
    return buf;
}

/*
 * write_toml_string — Emit a TOML basic string with required escaping.
 *
 * Level text can come from editor fields or hand-edited TOML.  Writing it back
 * with raw "%s" breaks as soon as a quote, backslash, or newline appears.
 * TOML basic strings use JSON-like escapes, so keep it boring and explicit.
 */
void write_toml_string(FILE *fp, const char *s)
{
    fputc('"', fp);
    if (s) {
        for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
            switch (*p) {
                case '"': fputs("\\\"", fp); break;
                case '\\': fputs("\\\\", fp); break;
                case '\b': fputs("\\b", fp); break;
                case '\t': fputs("\\t", fp); break;
                case '\n': fputs("\\n", fp); break;
                case '\f': fputs("\\f", fp); break;
                case '\r': fputs("\\r", fp); break;
                default:
                    if (*p < 0x20) fprintf(fp, "\\u%04x", *p);
                    else fputc(*p, fp);
                    break;
            }
        }
    }
    fputc('"', fp);
}

void write_toml_key_string(FILE *fp, const char *key, const char *value)
{
    fprintf(fp, "%s = ", key);
    write_toml_string(fp, value);
    fputc('\n', fp);
}
