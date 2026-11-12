/*
 * serializer_load_header.c — The format_version check.
 */

#include <stdio.h>  /* fprintf */

#include "serializer_load_header.h"
#include "../levels/level.h" /* LevelDef, LEVEL_FORMAT_VERSION */

/*
 * format_version is the one key that is not a row with storage in the
 * schema table (serializer_parse.c): it is not a value a level chooses but
 * a statement about the file, and the answer to a wrong one is to refuse
 * the file, not to fall back to a default.
 */
int serializer_load_header(toml_datum_t top, LevelDef *def) {
    toml_datum_t version;

    if (!def) return -1;

    /* Missing format_version is the supported legacy v1 representation. */
    version = toml_get(top, "format_version");
    if (version.type == TOML_UNKNOWN ||
        (version.type == TOML_INT64 && version.u.int64 == LEVEL_FORMAT_VERSION)) {
        def->format_version = LEVEL_FORMAT_VERSION;
        return 0;
    }
    if (version.type != TOML_INT64) {
        fprintf(stderr, "serializer: format_version must be TOML integer %d\n",
                LEVEL_FORMAT_VERSION);
        return -1;
    }
    fprintf(stderr, "serializer: unsupported format_version %lld (expected %d)\n",
            (long long)version.u.int64, LEVEL_FORMAT_VERSION);
    return -1;
}
