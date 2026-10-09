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

    if (serializer_load_header(top, def) != 0) {
        report_problem(&problems, 0, "cannot read the level's header");
        free(def);
        toml_free(r);
        return -1;
    }

    if (serializer_load_geometry(top, def) != 0) {
        report_problem(&problems, 0, "cannot read the level's geometry");
        free(def);
        toml_free(r);
        return -1;
    }

    if (serializer_load_checkpoints(top, def) != 0) {
        report_problem(&problems, 0, "cannot read the level's checkpoints");
        free(def);
        toml_free(r);
        return -1;
    }

    if (serializer_load_collectibles(top, def) != 0) {
        report_problem(&problems, 0, "cannot read the level's collectibles");
        free(def);
        toml_free(r);
        return -1;
    }

    if (serializer_load_enemies(top, def) != 0) {
        report_problem(&problems, 0, "cannot read the level's enemies");
        free(def);
        toml_free(r);
        return -1;
    }

    if (serializer_load_hazards(top, def) != 0) {
        report_problem(&problems, 0, "cannot read the level's hazards");
        free(def);
        toml_free(r);
        return -1;
    }

    if (serializer_load_surfaces(top, def) != 0) {
        report_problem(&problems, 0, "cannot read the level's surfaces");
        free(def);
        toml_free(r);
        return -1;
    }

    if (serializer_load_climbables(top, def) != 0) {
        report_problem(&problems, 0, "cannot read the level's climbables");
        free(def);
        toml_free(r);
        return -1;
    }

    if (serializer_load_layers(top, def) != 0) {
        report_problem(&problems, 0, "cannot read the level's layers");
        free(def);
        toml_free(r);
        return -1;
    }

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
