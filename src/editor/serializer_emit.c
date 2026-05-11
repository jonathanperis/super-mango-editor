/*
 * serializer_emit.c — Internal TOML emitter helpers.
 */

#include "serializer_emit.h"

#include <string.h>  /* strchr, strlen */

/*
 * fmt_float — Format a float with up to 2 decimal places, stripping
 * unnecessary trailing zeros but always keeping at least one decimal
 * digit so the TOML parser reads it as a float (not an integer).
 *
 * Examples:  80.0f  → "80.0"     (not "80.00" or "80")
 *            0.08f  → "0.08"     (preserved — was lost with %.1f)
 *            536.2f → "536.2"    (not "536.20")
 *            0.50f  → "0.5"      (trailing zero stripped)
 *           -380.0f → "-380.0"
 *
 * Returns a pointer to a static buffer — valid until the next call.
 * Safe for single-float-per-fprintf usage (which is all we do here).
 */
const char *fmt_float(double val)
{
    static char buf[64];
    snprintf(buf, sizeof(buf), "%.2f", val);

    /* Find the decimal point. */
    char *dot = strchr(buf, '.');
    if (dot) {
        /* Walk back from the end, stripping trailing '0' characters,
         * but always keep at least one digit after the decimal point
         * so "80.00" becomes "80.0" (not "80." or "80"). */
        char *end = buf + strlen(buf) - 1;
        while (end > dot + 1 && *end == '0') {
            *end = '\0';
            end--;
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
