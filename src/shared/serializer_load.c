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
#include "serializer_load_header.h"
#include "serializer_parse.h"
#include "serializer_io.h"
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
    const char *label;          /* the file's name, for stderr           */
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

/* serializer_legacy_warnings hands each skipped key or value here. It goes
 * to stderr and, marked "warning:", to whoever is listening: the level
 * did load, so the listener must be able to tell this from a failure. */
static void report_legacy_warning(void *context, const char *message)
{
    LoadProblems *problems = context;
    fprintf(stderr, "serializer: warning in '%s': %s (no format_version)\n",
            problems->label, message);
    report_problem(problems, serializer_toml_line(problems->top, message),
                   "warning: %s", message);
}

/* ================================================================== */
/* The section loaders                                                 */
/* ================================================================== */

/*
 * The shipped ambient loops were WAV files until they became QOA. A level
 * saved before then names a file that no longer ships, so those three
 * names, and only those, are read as their QOA replacements: the level
 * keeps its music, and the editor saves the new name. Any other .wav path
 * is the level's own sound and is left alone.
 *
 * The bundled levels still name the old files on purpose: ghosts and
 * Continue points are keyed by a hash of the level file's bytes
 * (serializer_fingerprint_utf8), so renaming them would orphan every
 * player's records.  tools/validate_levels.py mirrors this mapping.
 *
 * This is the one step of loading that is not a row of the schema table:
 * it does not read a key, it rewrites a value that was already read.
 */
static void rename_shipped_wav_loop(char *path)
{
    static const char dir[] = "assets/sounds/levels/";
    static const char *const loops[] = {"water.wav", "lava.wav", "winds.wav"};
    const size_t dir_len = sizeof(dir) - 1;

    if (strncmp(path, dir, dir_len) != 0) return;
    for (size_t i = 0; i < sizeof(loops) / sizeof(loops[0]); i++) {
        if (strcmp(path + dir_len, loops[i]) == 0) {
            memcpy(path + strlen(path) - 3, "qoa", 3);
            return;
        }
    }
}

/*
 * s_section_loaders — the parts of a level file, in the order they are read.
 *
 * Each row is a function and the name of the part for the error message.
 * `load` is a function pointer: the row stores which function to call, and
 * level_load_toml_text calls it through the pointer, the way
 * game_update.c walks s_update_steps and level_loader.c walks
 * s_level_loaders.  Every loader has the same shape (SectionLoadFn): it
 * reads its keys from `top`, the parsed TOML root, into `def`, and returns
 * 0, or -1 when a value cannot be used.
 *
 * There are only two, because every value of a level (the level-wide
 * keys, [physics], [last_star], floor_gaps and every [[array]] of tables)
 * is read by one loop over the schema table (serializer_load_schema in
 * serializer_parse.c).  The other row is the format_version check, which
 * decides whether the file may be read at all.
 *
 * The rows run top to bottom and the first one that fails ends the load,
 * so the order decides which problem is reported when several parts are
 * bad.  A new key is a row of the schema table (of an entity's field
 * table, or a ROOT_INT / ROOT_FLOAT / ROOT_TEXT row for a level-wide key); a
 * new kind of entity is a field table and one ROOT_TABLES row there, and
 * nothing here.
 */
typedef int (*SectionLoadFn)(toml_datum_t top, LevelDef *def);

typedef struct {
    SectionLoadFn load;  /* the function that reads the section       */
    const char   *name;  /* "cannot read the level's <name>" on error */
} SectionLoader;

static const SectionLoader s_section_loaders[] = {
    /* load                          name                    */
    { serializer_load_header,        "format version"        },
    /* Every other key and table, in one loop over the schema table. */
    { serializer_load_schema,        "settings and tables"   },
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
    return level_load_toml_file(path, def, report, context, NULL);
}

int level_load_toml_file(const char *path, LevelDef *def,
                         LevelLoadProblemFn report, void *context,
                         SerializerFileFingerprint *fingerprint)
{
    LoadProblems problems = { report, context, {0}, NULL };
    char *text = NULL;
    size_t size = 0;
    int too_large = 0;
    int result;

    if (fingerprint) memset(fingerprint, 0, sizeof(*fingerprint));
    if (!path || !def) return -1;

    /*
     * Read the whole file once, with a size limit, before anything looks
     * at its contents (serializer_read_bytes; UTF-8 paths work on Windows
     * too).  The hash and the parse below both use this one buffer.
     */
    if (serializer_read_bytes(path, LEVEL_TEXT_MAX, &text, &size, &too_large) !=
        SERIALIZER_PATH_EXISTING) {
        if (too_large) {
            fprintf(stderr, "serializer: '%s' is larger than %zu bytes\n",
                    path, LEVEL_TEXT_MAX - 1);
            report_problem(&problems, 0, "the file is larger than %zu bytes",
                           LEVEL_TEXT_MAX - 1);
        } else {
            fprintf(stderr, "serializer: cannot open '%s' for reading\n", path);
            report_problem(&problems, 0, "cannot open the file for reading");
        }
        return -1;
    }
    if (fingerprint) serializer_fingerprint_bytes(text, size, fingerprint);
    result = level_load_toml_text(text, size, path, def, report, context);
    free(text);
    return result;
}

int level_load_toml_text(const char *text, size_t size, const char *label,
                         LevelDef *def, LevelLoadProblemFn report, void *context)
{
    LoadProblems problems = { report, context, {0}, NULL };
    const char *path = label ? label : "(memory)";

    if (!text || !def || size >= LEVEL_TEXT_MAX) return -1;

    /*
     * toml_parse returns a toml_result_t.  If parsing fails, r.ok is false
     * and r.errmsg contains a human-readable error description.  On
     * success, r.toptab is the root TOML table we can query with toml_get().
     * The tree keeps its own copies of the strings, so the caller may free
     * text as soon as this function returns.
     */
    toml_result_t r = toml_parse(text, (int)size);
    if (!r.ok) {
        fprintf(stderr, "serializer: TOML parse error in '%s': %s\n",
                path, r.errmsg);
        report_parse_error(&problems, r.errmsg);
        return -1;
    }
    problems.top = r.toptab;
    problems.label = path;

    LevelDef *staged = NULL;
    int result = -1;
    {
        char schema_error[256];
        if (serializer_validate_schema(r.toptab, NULL, schema_error,
                                       sizeof(schema_error)) != 0) {
            fprintf(stderr, "serializer: schema error in '%s': %s\n",
                    path, schema_error);
            report_problem(&problems, serializer_toml_line(r.toptab, schema_error),
                           "%s", schema_error);
            goto finish;
        }
    }

    /*
     * Parse into staging storage so failed validation never leaves the caller
     * with a partially-loaded LevelDef. The staging copy lives on the heap:
     * a LevelDef is about 16 KB (LEVEL_DEF_SIZE_BUDGET in level.h), and the
     * browser build has only a 64 KB stack to share with its callers.
     */
    if (!serializer_test_take_failure(SERIALIZER_TEST_FAILURE_LOAD_MEMORY))
        staged = malloc(sizeof(*staged));
    if (!staged) {
        fprintf(stderr, "serializer: out of memory loading '%s'\n", path);
        report_problem(&problems, 0, "out of memory");
        goto finish;
    }
    level_def_init_defaults(staged);

    /* Read the file one section at a time; the first section that cannot
     * be read stops the load, and the caller's LevelDef is left untouched. */
    for (size_t i = 0; i < SECTION_LOADER_COUNT; i++) {
        const SectionLoader *section = &s_section_loaders[i];

        if (section->load(r.toptab, staged) != 0) {
            report_problem(&problems, 0, "cannot read the level's %s",
                           section->name);
            goto finish;
        }
    }

    /* One value is changed after it is read (see the function). */
    rename_shipped_wav_loop(staged->music_path);

    {
        char err[128];
        if (level_validate_runtime(staged, err, sizeof(err)) != 0) {
            fprintf(stderr, "serializer: invalid level '%s': %s\n", path, err);
            /* The file is well-formed but breaks runtime rules: list every
             * one, each with the line of the value it is about. */
            if (problems.report)
                (void)level_validate_runtime_each(staged, report_runtime_issue, &problems);
            goto finish;
        }
    }

    *def = *staged;
    result = 0;

finish:
    /*
     * A file without format_version was read leniently; say what that
     * skipped, so a misspelt key is not lost in silence. This runs whether
     * the load worked or not: when it failed, a skipped key is often the
     * reason (a misspelt [[checkpoints]] key leaves the level without its
     * checkpoints, say). The warnings come after the errors, so the first
     * problem a listener hears is still why the load failed.
     */
    (void)serializer_legacy_warnings(r.toptab, report_legacy_warning, &problems);

    /*
     * toml_free — release all memory allocated by toml_parse.
     * The LevelDef struct now holds its own copies of all data, so
     * the TOML tree is safe to destroy.
     */
    free(staged);
    toml_free(r);
    return result;
}
