/*
 * serializer_emit.c — Internal TOML emitter helpers.
 */

#include "serializer_emit.h"

#include <float.h>   /* FLT_MAX */
#include <math.h>    /* fabs, isfinite */
#include <stdlib.h>  /* strtod */
#include <string.h>  /* strchr, strlen */

/*
 * fmt_float — Format a float with enough significant digits for a float
 * round trip, always keeping TOML's value classified as a float.
 *
 * Examples:  80.0f  → "80.0"     (not "80.00" or "80")
 *            0.08f  → round-trippable decimal
 *            536.2f → round-trippable decimal
 *           -380.0f → "-380.0"
 *            FLT_MAX → "3.4028234663852886e+38" (see below)
 *
 * Returns a pointer to a static buffer — valid until the next call.
 * Safe for single-float-per-fprintf usage (which is all we do here).
 */
const char *fmt_float(double val)
{
    static char buf[64];
    snprintf(buf, sizeof(buf), "%.9g", val);

    /*
     * Nine significant digits are enough to bring any float back to itself,
     * but near the top of the float range the rounded decimal can land just
     * past FLT_MAX: FLT_MAX itself prints as 3.40282347e+38.  The loader
     * reads numbers as doubles and refuses anything beyond FLT_MAX, so the
     * editor would save a file it then could not open.  When the short form
     * does not read back, within range, as this same float, write the value
     * with 17 digits instead, which reads back as exactly this double.
     */
    if (isfinite(val) && fabs(val) <= FLT_MAX) {
        double back = strtod(buf, NULL);
        if (fabs(back) > FLT_MAX || (float)back != (float)val)
            snprintf(buf, sizeof(buf), "%.17g", val);
    }

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
 *
 * TOML forbids every raw control character except tab inside a basic string:
 * U+0000..U+001F and U+007F (DEL).  DEL is easy to forget because it sits at
 * the top of ASCII instead of the bottom; a name such as "A\u007FB" loads
 * fine, so it must save as the same escape or the next load fails.
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
                    if (*p < 0x20 || *p == 0x7F) fprintf(fp, "\\u%04x", *p);
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
