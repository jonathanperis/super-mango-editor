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

#include <stdarg.h>  /* va_list: report_problem */
#include <stdio.h>   /* fprintf */
#include <stdlib.h>  /* malloc, free: the staging LevelDef, strtol */
#include <string.h>  /* strstr, strlen */

#include "serializer.h"
#include "serializer_load_climbables.h"
#include "serializer_load_checkpoints.h"
#include "serializer_load_collectibles.h"
#include "serializer_load_config.h"
#include "serializer_load_enemies.h"
#include "serializer_load_geometry.h"
#include "serializer_load_hazards.h"
#include "serializer_load_header.h"
#include "serializer_load_layers.h"
#include "serializer_parse.h"
#include "serializer_io.h"
#include "serializer_load_surfaces.h"
#include "../../vendor/tomlc17/tomlc17.h" /* tomlc17 API */
#include "../levels/level.h"              /* LevelDef, all placement types */
#include "../levels/level_loader.h"       /* level_validate_runtime */
#include "../levels/level_validate.h"     /* level_validate_runtime_each */
#include "printf_format.h"                /* PRINTF_FORMAT */

/* ================================================================== */
/* Explaining a failed load                                            */
/* ================================================================== */

/* Who hears about the problems of one level_load_toml_explained call. */
typedef struct {
    LevelLoadProblemFn report;  /* NULL: nobody (plain level_load_toml) */
    void *context;
    toml_datum_t top;           /* the parsed tree, for line lookups     */
} LoadProblems;

PRINTF_FORMAT(3, 4)
static void report_problem(LoadProblems *problems, int line, const char *format, ...)
{
    char message[256];
    va_list args;

    if (!problems->report) return;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    problems->report(problems->context, message, line);
}

/*
 * report_parse_error — tomlc17 writes the line into its message, as
 * "(line 12) expected '='" or "... on line 12".  Pass the number on
 * separately and keep the words as the message.
 */
static void report_parse_error(LoadProblems *problems, const char *errmsg)
{
    const char *at = strstr(errmsg, "line ");
    const char *words = errmsg;
    char cleaned[200];
    int line = 0;
    size_t n;

    if (at) line = (int)strtol(at + 5, NULL, 10);
    if (strncmp(errmsg, "(line ", 6) == 0) {
        const char *close = strchr(errmsg, ')');
        if (close) words = close + 1;
        while (*words == ' ') words++;
    }
    /* "Error near line 3\n" ends in a newline; a status line cannot. */
    n = strlen(words);
    if (n >= sizeof(cleaned)) n = sizeof(cleaned) - 1;
    memcpy(cleaned, words, n);
    while (n > 0 && (cleaned[n - 1] == '\n' || cleaned[n - 1] == '\r')) n--;
    cleaned[n] = '\0';
    report_problem(problems, line, "TOML syntax: %s", cleaned);
}

/* level_validate_runtime_each hands each runtime error here; the message
 * starts with the TOML path, which gives the line it came from. */
static void report_runtime_issue(void *context, const char *message,
                                 const LevelIssueLocation *where)
{
    LoadProblems *problems = context;
    (void)where;
    report_problem(problems, serializer_toml_line(problems->top, message), "%s", message);
}

/* ================================================================== */
/* The section loaders                                                 */
/* ================================================================== */

/*
 * s_section_loaders — the parts of a level file, in the order they are read.
 *
 * Each row is a serializer_load_<section>() function (one per
 * serializer_load_*.c file) and the name of the section for the error
 * message.  `load` is a function pointer: the row stores which function to
 * call, and level_load_toml_explained calls it through the pointer, the
 * way game_update.c walks s_update_steps and level_loader.c walks
 * s_level_loaders.  Every loader has the same shape (SectionLoadFn): it
 * reads its keys from `top`, the parsed TOML root, into `def`, and returns
 * 0, or -1 when a value cannot be used.
 *
 * The rows run top to bottom and the first one that fails ends the load,
 * so the order decides which problem is reported when several sections
 * are bad.  Reading a new section means one serializer_load_<section>()
 * file and one row here.
 */
typedef int (*SectionLoadFn)(toml_datum_t top, LevelDef *def);

typedef struct {
    SectionLoadFn load;  /* the function that reads the section       */
    const char   *name;  /* "cannot read the level's <name>" on error */
} SectionLoader;

static const SectionLoader s_section_loaders[] = {
    /* load                          name            */
    { serializer_load_header,        "header"        },
    { serializer_load_geometry,      "geometry"      },
    { serializer_load_checkpoints,   "checkpoints"   },
    { serializer_load_collectibles,  "collectibles"  },
    { serializer_load_enemies,       "enemies"       },
    { serializer_load_hazards,       "hazards"       },
    { serializer_load_surfaces,      "surfaces"      },
    { serializer_load_climbables,    "climbables"    },
    { serializer_load_layers,        "layers"        },
};

#define SECTION_LOADER_COUNT (sizeof(s_section_loaders) / sizeof(s_section_loaders[0]))

/* ================================================================== */
/* level_load_toml — Read a TOML file and populate a LevelDef          */
/* ================================================================== */

int level_load_toml(const char *path, LevelDef *def)
{
    return level_load_toml_explained(path, def, NULL, NULL);
}

int level_load_toml_explained(const char *path, LevelDef *def,
                              LevelLoadProblemFn report, void *context)
{
    LoadProblems problems = { report, context, {0} };

    if (!path || !def) return -1;

    LevelDef *caller_def = def;

    /*
     * serializer_fopen_utf8 opens the file (UTF-8 paths work on Windows
     * too), then toml_parse_file parses the open stream.
     *
     * toml_parse_file returns a toml_result_t.  If parsing fails, r.ok is
     * false and r.errmsg contains a human-readable error description.  On
     * success, r.toptab is the root TOML table we can query with toml_get().
     */
    FILE *fp = serializer_fopen_utf8(path, "rb");
    if (!fp) {
        fprintf(stderr, "serializer: cannot open '%s' for reading\n", path);
        report_problem(&problems, 0, "cannot open the file for reading");
        return -1;
    }
    toml_result_t r = toml_parse_file(fp);
    fclose(fp);
    if (!r.ok) {
        fprintf(stderr, "serializer: TOML parse error in '%s': %s\n",
                path, r.errmsg);
        report_parse_error(&problems, r.errmsg);
        return -1;
    }
    problems.top = r.toptab;

    {
        char schema_error[256];
        if (serializer_validate_schema(r.toptab, NULL, schema_error,
                                       sizeof(schema_error)) != 0) {
            fprintf(stderr, "serializer: schema error in '%s': %s\n",
                    path, schema_error);
            report_problem(&problems, serializer_toml_line(r.toptab, schema_error),
                           "%s", schema_error);
            toml_free(r);
            return -1;
        }
    }

    /*
     * Parse into staging storage so failed validation never leaves the caller
     * with a partially-loaded LevelDef. The staging copy lives on the heap:
     * a LevelDef is about 16 KB (LEVEL_DEF_SIZE_BUDGET in level.h), and the
     * browser build has only a 64 KB stack to share with its callers.
     */
    def = malloc(sizeof(*def));
    if (!def) {
        fprintf(stderr, "serializer: out of memory loading '%s'\n", path);
        report_problem(&problems, 0, "out of memory");
        toml_free(r);
        return -1;
    }
    level_def_init_defaults(def);

    toml_datum_t top = r.toptab;

    /* Read the file one section at a time; the first section that cannot
     * be read stops the load, and the caller's LevelDef is left untouched. */
    for (size_t i = 0; i < SECTION_LOADER_COUNT; i++) {
        const SectionLoader *section = &s_section_loaders[i];

        if (section->load(top, def) != 0) {
            report_problem(&problems, 0, "cannot read the level's %s",
                           section->name);
            free(def);
            toml_free(r);
            return -1;
        }
    }

    /* The level-wide settings come last.  Their loader cannot fail (it has
     * no arrays to overflow, and a missing value falls back to a default),
     * so it returns nothing and is not a row of the table. */
    serializer_load_config(top, def);

    {
        char err[128];
        if (level_validate_runtime(def, err, sizeof(err)) != 0) {
            fprintf(stderr, "serializer: invalid level '%s': %s\n", path, err);
            /* The file is well-formed but breaks runtime rules: list every
             * one, each with the line of the value it is about. */
            if (problems.report)
                (void)level_validate_runtime_each(def, report_runtime_issue, &problems);
            free(def);
            toml_free(r);
            return -1;
        }
    }

    /*
     * toml_free — release all memory allocated by toml_parse_file.
     * The LevelDef struct now holds its own copies of all data, so
     * the TOML tree is safe to destroy.
     */
    toml_free(r);

    *caller_def = *def;
    free(def);

    return 0;
}
