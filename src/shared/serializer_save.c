/*
 * serializer_save.c — TOML writer for level definitions.
 */

#include <stdio.h>  /* FILE, fprintf, fclose, fgets */
#include <string.h> /* strncmp, strlen */

#include "serializer.h"
#include "serializer_emit.h"
#include "serializer_parse.h"   /* serializer_write_entry */
#include "serializer_io.h"
#include "serializer_types.h"
#include "../levels/level_loader.h"

/*
 * s_key_order — the level-wide keys, in the order a saved level lists
 * them, then the floor gaps and the checkpoints.  format_version is not
 * here: it is written first, by hand, because it is a constant.
 */
static const char *const s_key_order[] = {
    /* Header. */
    "name", "description", "generated_by", "screen_count",
    /* Level-wide configuration. */
    "player_start_x", "player_start_y", "music_path", "music_volume",
    "floor_tile_path", "initial_hearts", "initial_lives", "score_per_life",
    "coin_score",
    /* Geometry and authored respawn points. */
    "floor_gaps", "checkpoints",
};

/*
 * write_header_and_config — Recovery comment, format_version, then the
 * level-wide keys, floor gaps, checkpoints and physics.
 */
static void write_header_and_config(FILE *fp, const LevelDef *def,
                                    const char *original_path)
{
    if (original_path) {
        /*
         * A TOML comment may not contain raw control characters either, so
         * the path goes through the same escaping as a basic string.  Control
         * bytes become \u00XX escapes, which level_read_recovery_path rejects
         * (destination unknown) while the recovered level itself still loads.
         */
        fprintf(fp, "# super_mango_recovery_path = ");
        write_toml_string(fp, original_path);
        fputc('\n', fp);
    }

    /* Keep the schema marker first among top-level scalar fields. */
    fprintf(fp, "format_version = %d\n", LEVEL_FORMAT_VERSION);

    /* ---- Level-wide keys, floor gaps and checkpoints ---------------- */
    /*
     * Each name is a row of the schema table in serializer_parse.c, which
     * knows the key's type, where its value lives in LevelDef and when it
     * is left out (an empty description, an empty floor_gaps).  Plain keys
     * must come before the first [table], or TOML would read them as keys
     * of that table.
     */
    for (size_t i = 0; i < sizeof(s_key_order) / sizeof(s_key_order[0]); i++)
        serializer_write_entry(fp, def, s_key_order[i]);

    /* ---- Player movement physics ---------------------------------- */
    /*
     * Always written as a [physics] table so the level file documents the
     * current values.  The engine treats any value >= 0.0 as an override;
     * -1.0 means "use the #define default".  Set a field to -1.0 in the
     * editor to let the engine pick its own value for that parameter.
     */
    fputc('\n', fp);
    serializer_write_entry(fp, def, "physics");
}

/*
 * s_table_order — the [table] and [[array]] sections after [physics], in
 * the order they appear in a saved level.
 *
 * Like s_key_order, each name is a row of the schema table in
 * serializer_parse.c, which says what the section's keys are and in which
 * order they are written (serializer_write_entry).  This list only decides
 * the order of the sections themselves.  Both orders are part of the file
 * format: changing either changes every saved level, and the round-trip
 * tests compare bytes.
 */
static const char *const s_table_order[] = {
    /* The world and what is collected in it. */
    "rails", "platforms", "coins", "star_yellows", "star_greens", "star_reds",
    "last_star",   /* one [last_star] table, with the optional next_phase */
    /* Enemies. */
    "spiders", "jumping_spiders", "birds", "faster_birds", "fish", "faster_fish",
    /* Hazards. */
    "axe_traps", "circular_saws", "spike_rows", "spike_platforms", "spike_blocks",
    "blue_flames", "fire_flames",
    /* Surfaces and climbables. */
    "float_platforms", "bridges", "bouncepads_small", "bouncepads_medium",
    "bouncepads_high", "vines", "ladders", "ropes",
    /* Layers: drawn, not solid. */
    "background_layers", "foreground_layers", "fog_layers",
};

/*
 * write_level_toml — Emit every LevelDef field as TOML text into fp.
 *
 * This is only the *format*: the order of keys and tables the loader
 * expects. level_save_toml_internal below owns the file itself (temporary
 * file, flush, fingerprint check and atomic replace), so a write error is
 * detected there through the stream's error flag after this returns.
 * original_path is non-NULL only for crash-recovery snapshots.
 */
static void write_level_toml(FILE *fp, const LevelDef *def,
                             const char *original_path)
{
    /* Sections are written in the order the loader and humans expect:
     * plain keys first (s_key_order), then the tables in s_table_order. */
    write_header_and_config(fp, def, original_path);
    for (size_t i = 0; i < sizeof(s_table_order) / sizeof(s_table_order[0]); i++)
        serializer_write_entry(fp, def, s_table_order[i]);
}

/*
 * The whole order in one list, for the test that every row of the schema
 * table is saved exactly once (tests/level_serializer_test.c): a row
 * added to the table but to neither list above would load and then be
 * lost on the next save, silently.
 */
#define KEY_ORDER_COUNT   (sizeof(s_key_order) / sizeof(s_key_order[0]))
#define TABLE_ORDER_COUNT (sizeof(s_table_order) / sizeof(s_table_order[0]))

size_t serializer_save_order_count(void)
{
    return KEY_ORDER_COUNT + 1 + TABLE_ORDER_COUNT;   /* + [physics] */
}

const char *serializer_save_order_name(size_t index)
{
    if (index < KEY_ORDER_COUNT) return s_key_order[index];
    if (index == KEY_ORDER_COUNT) return "physics";   /* write_header_and_config */
    index -= KEY_ORDER_COUNT + 1;
    if (index < TABLE_ORDER_COUNT) return s_table_order[index];
    return NULL;
}

/* ================================================================== */
/* level_save_toml — Write a LevelDef to a human-readable TOML file    */
/* ================================================================== */

/*
 * s_kept_temp_path — Where the last save that returned
 * SERIALIZER_REPLACE_TEMP_KEPT left the level (see level_save_kept_temp_path).
 * Like errno, it describes only the most recent save and is "" otherwise.
 */
static char s_kept_temp_path[SERIALIZER_IO_PATH_MAX];

const char *level_save_kept_temp_path(void)
{
    return s_kept_temp_path;
}

/*
 * write_temp — The first half of every save: write what emit(fp, context)
 * prints to a new sibling temporary file of `path`, flush it and close it.
 * temp_path receives the name of the file actually created.  Returns 0, or
 * -1 with nothing left on disk.
 *
 * The file written later is `path` itself.  A symlink there is replaced, not
 * followed (see serializer_replace_file); callers that mean to save through
 * a link resolve it first with serializer_resolve_save_target.
 */
static int write_temp(const char *path, SerializerEmitFn emit,
                      const void *context, int private_file,
                      char *temp_path, size_t temp_path_size)
{
    FILE *fp;

    if (!path || !emit || !temp_path || temp_path_size == 0) return -1;
    if (serializer_make_temp_path(path, temp_path, temp_path_size) != 0) {
        fprintf(stderr, "serializer: path too long for temporary save '%s'\n", path);
        return -1;
    }

    fp = private_file
       ? serializer_open_temp(path, temp_path, temp_path_size)
       : serializer_open_temp_shared(path, temp_path, temp_path_size);
    if (!fp) {
        fprintf(stderr, "serializer: cannot open '%s' for writing\n", path);
        serializer_remove_temp(temp_path);
        return -1;
    }

    /* Emit the contents; stream errors are checked once, just below. */
    emit(fp, context);

    {
        int write_result = serializer_stream_has_error(fp);
        int flush_result = write_result != 0 ? -1 : serializer_flush(fp);
        int close_result = fclose(fp);

        if (write_result != 0 || flush_result != 0 || close_result != 0) {
            fprintf(stderr, "serializer: failed to flush/close '%s'\n", path);
            serializer_remove_temp(temp_path);
            return -1;
        }
    }
    return 0;
}

/*
 * save_emitted — The one save path behind every public saver: write what
 * emit(fp, context) prints to a sibling temporary file, flush it, then
 * install it as `path` in one step (serializer_replace_file), so a failure
 * at any point leaves the old file whole.
 *
 * private_file picks the permissions of a *new* file on POSIX: 1 for the
 * editor's own recovery and playtest copies (owner only, 0600), 0 for a
 * file the user saves (0666 minus the umask, usually 0644, like any text
 * editor).  Replacing an existing file keeps its permissions either way.
 * expected, when given, must still describe `path` right before the
 * replace (-2 otherwise, nothing written).
 */
static int save_emitted(const char *path, SerializerEmitFn emit,
                        const void *context, SerializerSavePolicy policy,
                        const SerializerFileFingerprint *expected,
                        int private_file)
{
    char temp_path[SERIALIZER_IO_PATH_MAX];
    int install_result;

    s_kept_temp_path[0] = '\0';
    if (write_temp(path, emit, context, private_file,
                   temp_path, sizeof(temp_path)) != 0)
        return -1;

    if (expected) {
        SerializerFileFingerprint actual;
        int fingerprint_result = serializer_fingerprint_utf8(path, &actual);
        if (fingerprint_result != 1 ||
            !serializer_fingerprint_equal(expected, &actual) ||
            serializer_test_take_failure(SERIALIZER_TEST_FAILURE_SOURCE_CHANGED)) {
            fprintf(stderr, "serializer: source changed before replacement '%s'\n",
                    path);
            serializer_remove_temp(temp_path);
            return -2;
        }
    }

    install_result = policy == SERIALIZER_SAVE_CREATE_ONLY
                   ? serializer_create_file(temp_path, path)
                   : serializer_replace_file(temp_path, path);
    if (install_result == SERIALIZER_REPLACE_TEMP_KEPT) {
        /* The old file may be gone already: the temporary file is the only
         * complete copy of the level now, so it stays where it is. */
        fprintf(stderr, "serializer: could not finish replacing '%s'; the level "
                "is kept in '%s'\n", path, temp_path);
        memcpy(s_kept_temp_path, temp_path, strlen(temp_path) + 1);
        return SERIALIZER_REPLACE_TEMP_KEPT;
    }
    if (install_result != 0) {
        fprintf(stderr, "serializer: failed to replace '%s'\n", path);
        serializer_remove_temp(temp_path);
        return -1;
    }

    return 0;
}

int serializer_write_temp(const char *path, SerializerEmitFn emit,
                          const void *context, char *temp_path,
                          size_t temp_path_size)
{
    return write_temp(path, emit, context, 0, temp_path, temp_path_size);
}

/* What write_level_toml needs, passed through save_emitted's context. */
typedef struct {
    const LevelDef *def;
    const char *original_path;
} LevelEmit;

static void emit_level(FILE *fp, const void *context)
{
    const LevelEmit *level = context;
    write_level_toml(fp, level->def, level->original_path);
}

/*
 * level_save_toml_internal — Every level saver: refuse a level the game
 * would refuse, then save it through save_emitted.
 */
static int level_save_toml_internal(const LevelDef *def, const char *path,
                                    const char *original_path,
                                    SerializerSavePolicy policy,
                                    const SerializerFileFingerprint *expected,
                                    int private_file)
{
    LevelEmit level = { def, original_path };

    s_kept_temp_path[0] = '\0';
    if (!def || !path) return -1;
    {
        char err[128];
        if (level_validate_runtime(def, err, sizeof(err)) != 0) {
            fprintf(stderr, "serializer: invalid level for save '%s': %s\n", path, err);
            return -1;
        }
    }
    return save_emitted(path, emit_level, &level, policy, expected, private_file);
}

int level_save_toml(const LevelDef *def, const char *path)
{
    return level_save_toml_internal(def, path, NULL,
                                   SERIALIZER_SAVE_REPLACE, NULL, 0);
}

int level_save_toml_private(const LevelDef *def, const char *path)
{
    return level_save_toml_internal(def, path, NULL,
                                   SERIALIZER_SAVE_REPLACE, NULL, 1);
}

int level_save_toml_with_policy(const LevelDef *def, const char *path,
                                SerializerSavePolicy policy)
{
    return level_save_toml_internal(def, path, NULL, policy, NULL, 0);
}

int level_save_toml_checked(const LevelDef *def, const char *path,
                            SerializerSavePolicy policy,
                            const SerializerFileFingerprint *expected)
{
    if (policy == SERIALIZER_SAVE_REPLACE &&
        (!expected || !expected->valid)) return -1;
    return level_save_toml_internal(def, path, NULL, policy, expected, 0);
}

int level_write_temp(const LevelDef *def, const char *path,
                     char *temp_path, size_t temp_path_size)
{
    LevelEmit level = { def, NULL };
    char err[128];

    if (!def || !path) return -1;
    if (level_validate_runtime(def, err, sizeof(err)) != 0) {
        fprintf(stderr, "serializer: invalid level for save '%s': %s\n", path, err);
        return -1;
    }
    return write_temp(path, emit_level, &level, 0, temp_path, temp_path_size);
}

int level_save_toml_recovery(const LevelDef *def, const char *path,
                             const char *original_path)
{
    return level_save_toml_internal(def, path, original_path ? original_path : "",
                                   SERIALIZER_SAVE_REPLACE, NULL, 1);
}

/*
 * level_read_recovery_path — Read the destination a recovery copy belongs to.
 *
 * write_header_and_config writes it as a comment on the first line of the
 * file, so the first line is the only one read. The rest of the file is
 * the level, and its lines can be far longer than a path (a description of
 * 4095 bytes grows further when its newlines and control bytes are
 * escaped); requiring every line to fit this buffer used to lose the
 * destination of any level with a long description.
 *
 * Returns 1 with the path in buf, 0 when the file has no such comment
 * (not a recovery copy), -1 when the comment is malformed or the file
 * cannot be read.
 */
int level_read_recovery_path(const char *path, char *buf, size_t buf_size)
{
    FILE *fp;
    char line[SERIALIZER_IO_PATH_MAX];
    const char prefix[] = "# super_mango_recovery_path = \"";
    size_t prefix_len = sizeof(prefix) - 1;
    size_t len;
    size_t out_len = 0;
    int escaped = 0;
    int closed = 0;

    if (!path || !buf || buf_size == 0) return -1;
    buf[0] = '\0';

    fp = serializer_fopen_utf8(path, "rb");
    if (!fp) return -1;

    if (!fgets(line, sizeof(line), fp)) {
        /* An empty file has no comment; a read error is a failure. */
        if (ferror(fp)) goto fail;
        fclose(fp);
        return 0;
    }
    /* Any other first line means this is not a recovery copy. The prefix
     * is far shorter than the buffer, so a cut-off line still shows it. */
    if (strncmp(line, prefix, prefix_len) != 0) {
        fclose(fp);
        return 0;
    }

    len = strlen(line);
    if (len == sizeof(line) - 1 && line[len - 1] != '\n' && !feof(fp)) {
        goto fail;   /* the comment itself does not fit: no usable path */
    }
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
        line[--len] = '\0';
    }

    for (size_t i = prefix_len; i < len; i++) {
        unsigned char ch = (unsigned char)line[i];

        if (!escaped && ch == '"') {
            if (i + 1 != len) {
                goto fail;
            }
            closed = 1;
            break;
        }
        if (!escaped && ch == '\\') {
            escaped = 1;
            continue;
        }
        if (escaped) {
            switch (ch) {
            case 'b': ch = '\b'; break;
            case 'f': ch = '\f'; break;
            case 'n': ch = '\n'; break;
            case 'r': ch = '\r'; break;
            case 't': ch = '\t'; break;
            case '\\':
            case '"': break;
            default:
                goto fail;
            }
            escaped = 0;
        }
        /* Destinations never contain control bytes, including DEL. */
        if (ch < 0x20 || ch == 0x7F) {
            goto fail;
        }
        if (out_len + 1 >= buf_size) {
            goto fail;
        }
        buf[out_len++] = (char)ch;
    }
    if (!closed) {
        /* A metadata prefix without a closing quote is malformed. */
        goto fail;
    }
    buf[out_len] = '\0';
    fclose(fp);
    return 1;

fail:
    /* Never leave a half-decoded destination behind for the caller. */
    fclose(fp);
    buf[0] = '\0';
    return -1;
}
