/*
 * serializer_load.c — TOML loading for level definitions.
 *
 * Reads TOML files from disk into LevelDef structs for editing or playtesting.
 * level_save_toml lives in serializer_save.c; this file keeps level_load_toml.
 *
 * All enum values are serialized as readable strings ("RECT", "SPIN", etc.)
 * rather than raw integers, so the TOML files are easy to understand and
 * edit by hand.
 */

#include <stdio.h>   /* fprintf */

#include "serializer.h"
#include "serializer_load_climbables.h"
#include "serializer_load_collectibles.h"
#include "serializer_load_config.h"
#include "serializer_load_enemies.h"
#include "serializer_load_geometry.h"
#include "serializer_load_hazards.h"
#include "serializer_load_header.h"
#include "serializer_load_layers.h"
#include "serializer_load_surfaces.h"
#include "../../vendor/tomlc17/tomlc17.h" /* tomlc17 API */
#include "../levels/level.h"              /* LevelDef, all placement types */
#include "../levels/level_loader.h"       /* level_validate_runtime */

/* ================================================================== */
/* level_load_toml — Read a TOML file and populate a LevelDef          */
/* ================================================================== */

int level_load_toml(const char *path, LevelDef *def) {
    if (!path || !def) return -1;

    LevelDef loaded;
    LevelDef *caller_def = def;

    /*
     * toml_parse_file_ex — open and parse a TOML file in one call.
     *
     * Returns a toml_result_t.  If parsing fails, r.ok is false and
     * r.errmsg contains a human-readable error description.  On success,
     * r.toptab is the root TOML table we can query with toml_get().
     */
    toml_result_t r = toml_parse_file_ex(path);
    if (!r.ok) {
        fprintf(stderr, "serializer: TOML parse error in '%s': %s\n",
                path, r.errmsg);
        return -1;
    }

    /*
     * Parse into staging storage so failed validation never leaves the caller
     * with a partially-loaded LevelDef.
     */
    def = &loaded;
    level_def_init_defaults(def);

    toml_datum_t top = r.toptab;

    if (serializer_load_header(top, def) != 0) {
        toml_free(r);
        return -1;
    }

    if (serializer_load_geometry(top, def) != 0) {
        toml_free(r);
        return -1;
    }

    if (serializer_load_collectibles(top, def) != 0) {
        toml_free(r);
        return -1;
    }

    if (serializer_load_enemies(top, def) != 0) {
        toml_free(r);
        return -1;
    }

    if (serializer_load_hazards(top, def) != 0) {
        toml_free(r);
        return -1;
    }

    if (serializer_load_surfaces(top, def) != 0) {
        toml_free(r);
        return -1;
    }

    if (serializer_load_climbables(top, def) != 0) {
        toml_free(r);
        return -1;
    }

    if (serializer_load_layers(top, def) != 0) {
        toml_free(r);
        return -1;
    }

    serializer_load_config(top, def);

    {
        char err[128];
        if (level_validate_runtime(def, err, sizeof(err)) != 0) {
            fprintf(stderr, "serializer: invalid level '%s': %s\n", path, err);
            toml_free(r);
            return -1;
        }
    }

    /*
     * toml_free — release all memory allocated by toml_parse_file_ex.
     * The LevelDef struct now holds its own copies of all data, so
     * the TOML tree is safe to destroy.
     */
    toml_free(r);

    *caller_def = loaded;

    return 0;
}
