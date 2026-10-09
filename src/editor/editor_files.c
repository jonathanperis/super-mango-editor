/*
 * editor_files.c — Opening and saving levels, playtest copies and the
 * recent-file list.
 *
 * Autosave and crash recovery live in editor_recovery.c, which borrows the
 * loading, atomic-write and preference-folder helpers listed at the end of
 * editor_files.h.
 */

#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

#include "editor_files.h"

#include <stdio.h>      /* FILE, fopen, fprintf, stderr */
#include <stdint.h>     /* uint64_t */
#include <stdlib.h>     /* free */
#include <string.h>     /* memset, strcmp, strlen, strncpy, strpbrk */

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#include <windows.h>    /* GetCurrentProcessId */
#else
#include <unistd.h>     /* getpid */
#endif

#include "editor_recovery.h"   /* recovery paths and retirement on load/save */
#include "editor_session.h"    /* editor status/title/persist helpers */
#include "file_dialog.h"       /* file_dialog_open */
#include "../shared/serializer.h"    /* shared level format */
#include "../shared/serializer_io.h" /* UTF-8 file I/O */
#include "undo.h"              /* undo_clear */

#define EDITOR_RECENT_MAX    5
#define EDITOR_PREF_ORG      "Super Mango"
#define EDITOR_PREF_APP      "Editor"
#define EDITOR_RECENT_NAME   "editor_recent.txt"

static unsigned long editor_playtest_sequence;
static void (*editor_test_load_hook)(const char *path);

static int editor_save_recent_files(const EditorState *es);
static int editor_recent_path_storable(const char *path);
static void editor_add_recent_file(EditorState *es, const char *path);
static void editor_replace_texture(Texture2D **slot, char *loaded_path,
                                   size_t loaded_path_size, const char *path);
static int editor_make_playtest_path(EditorState *es);
static int editor_path_is_private(const EditorState *es, const char *path);
static int editor_save_current_level_as_validated(EditorState *es);
static int editor_source_changed(EditorState *es);
static int editor_finalize_save(EditorState *es, const char *path,
                                const char *status_prefix);

/*
 * editor_report_save_failure — Explain a failed level save in the status bar.
 *
 * result is what the level_save_toml_* call returned.  Most failures leave
 * the file on disk as it was, and "Save failed: <path>" says enough.  One
 * does not: on Windows the old file can be moved away before the new one
 * fails to move in.  The level is then kept in a temporary file next to the
 * destination, and the designer needs its name to get the work back.
 */
static void editor_report_save_failure(EditorState *es, int result,
                                       const char *path)
{
    if (result == SERIALIZER_REPLACE_TEMP_KEPT) {
        editor_set_status(es, "Save incomplete: your level is safe in %s",
                          level_save_kept_temp_path());
    } else {
        editor_set_status(es, "Save failed: %s", path);
    }
}

int editor_path_fits(const char *path)
{
    return path && strlen(path) < EDITOR_PATH_MAX;
}

void editor_path_for_display(const char *path, char *out, size_t out_size)
{
    if (!out || out_size == 0) return;
    out[0] = '\0';
    if (!path || out_size < 4) return;

    size_t length = strlen(path);
    if (length < out_size) {                 /* fits: copy with its NUL */
        memcpy(out, path, length + 1);
        return;
    }
    /* Keep the last (out_size - 4) bytes: 3 for "..." and 1 for the NUL.
     * Then step past UTF-8 continuation bytes so the tail starts on a
     * whole character. */
    const char *tail = path + length - (out_size - 4);
    while (((unsigned char)*tail & 0xc0) == 0x80) tail++;
    size_t tail_length = strlen(tail);
    memcpy(out, "...", 3);
    memcpy(out + 3, tail, tail_length + 1);
}

int editor_set_preference_root(EditorState *es, const char *root)
{
    if (!es || !root || !editor_path_fits(root) || root[0] == '\0') return -1;
    memcpy(es->preference_root, root, strlen(root) + 1);
    return 0;
}

int editor_init_persistence_paths(EditorState *es)
{
    if (!es) return -1;

    es->autosave_path[0] = '\0';
    es->playtest_path[0] = '\0';
    es->recent_path[0] = '\0';
    es->recovery_root_path[0] = '\0';
    es->recovery_entry_count = 0;
    es->pending_recovery_id = 0;

    if (editor_preference_root_path(es, es->recovery_root_path,
                                    sizeof(es->recovery_root_path)) != 0 ||
        editor_clean_orphan_recoveries(es) < 0 ||
        editor_discover_recoveries(es) < 0 ||
        editor_set_recovery_document(es, es->file_path) != 0 ||
        editor_make_playtest_path(es) != 0 ||
        editor_preference_file_path(es, EDITOR_RECENT_NAME,
                                    es->recent_path,
                                    sizeof(es->recent_path)) != 0) {
        es->autosave_path[0] = '\0';
        es->playtest_path[0] = '\0';
        es->recent_path[0] = '\0';
        return -1;
    }

    return 0;
}

#ifdef MANGO_TESTING
void editor_test_set_load_hook(void (*hook)(const char *path))
{
    editor_test_load_hook = hook;
}
#endif

/*
 * editor_read_stable_level — Parse a level and fingerprint the same bytes.
 *
 * The fingerprint is the baseline Save later uses to detect that someone
 * else changed the file.  Another program (a text editor, a git checkout)
 * may be rewriting the file while we read it, so a fingerprint taken
 * before *or* after parsing alone could describe different bytes than the
 * ones parsed.  Like the game's read_stable_level, fingerprint before and
 * after and require both to match.  A writer usually finishes quickly, so
 * one retry is allowed.
 *
 * Returns EDITOR_LOAD_OK, EDITOR_LOAD_PARSE_ERROR (stable but invalid
 * bytes), EDITOR_LOAD_IO_ERROR (cannot fingerprint) or EDITOR_LOAD_CHANGING
 * (the file changed during both attempts).
 */
enum {
    EDITOR_LOAD_OK = 0,
    EDITOR_LOAD_PARSE_ERROR,
    EDITOR_LOAD_IO_ERROR,
    EDITOR_LOAD_CHANGING
};

static int editor_read_stable_level(const char *path, LevelDef *level,
                                    SerializerFileFingerprint *fingerprint)
{
    for (int attempt = 0; attempt < 2; attempt++) {
        SerializerFileFingerprint before;
        SerializerFileFingerprint after;
        int parsed;

        if (serializer_fingerprint_utf8(path, &before) != 1)
            return EDITOR_LOAD_IO_ERROR;
        parsed = level_load_toml(path, level) == 0;
        if (editor_test_load_hook) editor_test_load_hook(path);
        if (serializer_fingerprint_utf8(path, &after) != 1)
            return EDITOR_LOAD_IO_ERROR;
        if (serializer_fingerprint_equal(&before, &after)) {
            if (!parsed) return EDITOR_LOAD_PARSE_ERROR;
            *fingerprint = after;
            return EDITOR_LOAD_OK;
        }
        /* The bytes changed under us (a parse error may just be a half-
         * written file); try once more. */
    }
    return EDITOR_LOAD_CHANGING;
}

int editor_load_level(EditorState *es, const char *path)
{
    LevelDef new_level;
    SerializerFileFingerprint fingerprint;
    char target[EDITOR_PATH_MAX];
    int result;
    memset(&new_level, 0, sizeof(new_level));

    if (!es || !editor_path_fits(path)) {
        if (es) editor_set_status(es, "Load failed: path too long");
        return -1;
    }

    /* Resolve a symlink once and read that file, so file_target names
     * exactly the bytes shown, even if the link is repointed meanwhile. */
    if (serializer_resolve_save_target(path, target, sizeof(target)) != 0) {
        editor_set_status(es, "Load failed: cannot resolve %s", path);
        return -1;
    }

    result = editor_read_stable_level(target, &new_level, &fingerprint);
    if (result == EDITOR_LOAD_PARSE_ERROR) {
        fprintf(stderr, "Error: failed to load %s\n", path);
        editor_set_status(es, "Load failed: %s", path);
        return -1;
    }
    if (result == EDITOR_LOAD_IO_ERROR) {
        editor_set_status(es, "Load failed: cannot fingerprint %s", path);
        return -1;
    }
    if (result == EDITOR_LOAD_CHANGING) {
        editor_set_status(es, "Load failed: %s kept changing while it was read; try again",
                          path);
        return -1;
    }

    /* The load succeeded; only now retire the old session's recovery. */
    editor_retire_current_recovery(es);
    editor_apply_loaded_level(es, &new_level, path, 0, 1);
    memcpy(es->file_target, target, strlen(target) + 1);
    if (editor_set_recovery_document(es, path) != 0) {
        editor_set_status(es, "Load failed: recovery path unavailable");
        return -1;
    }
    es->source_fingerprint = fingerprint;
    es->source_state = EDITOR_SOURCE_EXPECTED_EXISTING;

    fprintf(stderr, "Loaded %s (%d entities)\n", path,
            es->level.coin_count + es->level.spider_count +
            es->level.platform_count + es->level.rail_count +
            es->level.bird_count + es->level.fish_count);
    editor_set_status(es, "Loaded %s", path);
    return 0;
}

void editor_apply_loaded_level(EditorState *es, const LevelDef *level,
                                      const char *path, int modified,
                                      int add_recent)
{
    es->level = *level;
    if (path) {
        if (!editor_path_fits(path)) return;
        memcpy(es->file_path, path, strlen(path) + 1);
    } else {
        es->file_path[0] = '\0';
    }
    /* Only editor_load_level, which read the file, may vouch for a target. */
    es->file_target[0] = '\0';
    memset(&es->source_fingerprint, 0, sizeof(es->source_fingerprint));
    es->source_state = EDITOR_SOURCE_UNKNOWN;
    undo_clear(es->undo);
    es->selection.index = -1;
    /* A copied rail rider's rail index named a rail in the old document;
     * a paste here matches the rail by shape instead. */
    es->clipboard_rail_index = -1;
    if (modified) {
        editor_set_recovered_dirty(es);
    } else {
        editor_set_document_save_point(es);
    }
    if (add_recent && path && path[0] != '\0') editor_add_recent_file(es, path);

    editor_sync_config_resources(es);

    editor_update_window_title(es);
}

/*
 * editor_replace_texture — Point a preview slot at the texture for `path`.
 *
 * loaded_path remembers which file the slot currently shows.  When the
 * level still names that file, nothing is reloaded: decoding a PNG and
 * uploading it to the GPU on every config edit (even a rename) is wasted
 * work.  A failed load keeps the old texture and leaves loaded_path alone,
 * so the next change tries again.
 */
static void editor_replace_texture(Texture2D **slot, char *loaded_path,
                                   size_t loaded_path_size, const char *path)
{
    if (!slot || !loaded_path || loaded_path_size == 0) return;
    if (!path || path[0] == '\0') {
        texture_unload(*slot);
        *slot = NULL;
        loaded_path[0] = '\0';
        return;
    }
    if (*slot && strcmp(loaded_path, path) == 0) return;  /* unchanged */
    if (!IsWindowReady()) return;

    {
        Texture2D *replacement = texture_load(path);
        if (replacement) {
            texture_unload(*slot);
            *slot = replacement;
            str_copy(loaded_path, path, loaded_path_size);
        } else {
            fprintf(stderr, "Warning: keeping preview texture; cannot load %s\n", path);
        }
    }
}

void editor_sync_config_resources(EditorState *es)
{
    if (!es) return;
    editor_replace_texture(&es->textures.sky, es->preview_sky_path,
                           sizeof(es->preview_sky_path),
                           es->level.background_layer_count > 0
                           ? es->level.background_layers[0].path : NULL);
    editor_replace_texture(&es->textures.floor_tile, es->preview_floor_path,
                           sizeof(es->preview_floor_path),
                           es->level.floor_tile_path);
    editor_replace_texture(&es->textures.water, es->preview_water_path,
                           sizeof(es->preview_water_path),
                           es->level.foreground_layer_count > 0 &&
                           es->level.foreground_layer_count <= MAX_BACKGROUND_LAYERS
                           ? es->level.foreground_layers[
                                 es->level.foreground_layer_count - 1].path
                           : NULL);
}

void editor_open_level_file(EditorState *es)
{
    char path[EDITOR_PATH_MAX];
    int dialog_result;

    if (!es || !editor_finish_field_edit(es)) return;
    dialog_result = file_dialog_open(path, (int)sizeof(path));
    if (dialog_result == FILE_DIALOG_SELECTED) {
        if (!editor_path_fits(path)) {
            editor_set_status(es, "Open failed: path too long");
        } else {
            (void)editor_load_level(es, path);
        }
    } else if (dialog_result == FILE_DIALOG_CANCELLED) {
        editor_set_status(es, "Open cancelled");
    } else if (dialog_result == FILE_DIALOG_INVALID_PATH) {
        editor_set_status(es, "Open failed: file names with line breaks are not supported");
    } else {
        editor_set_status(es, "Open failed: dialog error");
    }
}

/*
 * editor_document_write_target — Pick the file a plain Save will replace.
 *
 * Usually that is file_path itself.  When file_path is a symlink, Save
 * writes the file behind it, keeping the link, but only while the link
 * still points at file_target, the file this session actually read.  A link
 * that was repointed, or a regular file swapped for a link, could aim the
 * save at a file the user never opened, so that case returns -1 instead.
 */
static int editor_document_write_target(const EditorState *es,
                                        char *target, size_t target_size)
{
    if (!serializer_path_is_symlink(es->file_path)) {
        if (strlen(es->file_path) >= target_size) return -1;
        memcpy(target, es->file_path, strlen(es->file_path) + 1);
        return 0;
    }
    if (es->file_target[0] == '\0' ||
        serializer_resolve_save_target(es->file_path, target, target_size) != 0 ||
        strcmp(target, es->file_target) != 0) return -1;
    return 0;
}

int editor_save_current_level(EditorState *es)
{
    int source_status;
    EditorExternalChoice choice;
    char target[EDITOR_PATH_MAX];

    if (!es || !editor_finish_field_edit(es)) return -1;
    if (!editor_can_persist(es, "Save")) return -1;

    if (es->file_path[0] == '\0') {
        return editor_save_current_level_as_validated(es);
    }
    if (editor_document_write_target(es, target, sizeof(target)) != 0) {
        editor_set_status(es, "Save failed: %s links to a file this editor "
                          "did not open; use Save As", es->file_path);
        return -1;
    }
    /* Check the file that will really be written, not just the link name. */
    if (editor_path_is_private(es, es->file_path) ||
        editor_path_is_private(es, target)) {
        return editor_save_current_level_as_validated(es);
    }

    source_status = editor_source_changed(es);
    if (source_status < 0) {
        editor_set_status(es, "Save failed: cannot inspect source file");
        return -1;
    }
    if (source_status > 0) {
        choice = editor_confirm_external_change(es);
        if (choice == EDITOR_EXTERNAL_SAVE_AS) {
            return editor_save_current_level_as_validated(es);
        }
        if (choice != EDITOR_EXTERNAL_REPLACE) return -1;
        {
            SerializerFileFingerprint replace_expected;
            if (serializer_fingerprint_utf8(target,
                                            &replace_expected) != 1) {
                editor_set_status(es, "Save cancelled: source unavailable");
                return -1;
            }
            /* Recheck occurs immediately before replacement. */
            int result = level_save_toml_checked(&es->level, target,
                                                 SERIALIZER_SAVE_REPLACE,
                                                 &replace_expected);
            if (result != 0) {
                editor_report_save_failure(es, result, es->file_path);
                return -1;
            }
        }
        return editor_finalize_save(es, es->file_path, "Saved");
    }

    if (es->source_state == EDITOR_SOURCE_EXPECTED_MISSING) {
        int result = level_save_toml_with_policy(&es->level, target,
                                                 SERIALIZER_SAVE_CREATE_ONLY);
        if (result != 0) {
            editor_report_save_failure(es, result, es->file_path);
            return -1;
        }
        return editor_finalize_save(es, es->file_path, "Saved");
    }
    if (es->source_state != EDITOR_SOURCE_EXPECTED_EXISTING ||
        !es->source_fingerprint.valid) {
        editor_set_status(es, "Save failed: source baseline unavailable");
        return -1;
    }
    {
        int result = level_save_toml_checked(&es->level, target,
                                             SERIALIZER_SAVE_REPLACE,
                                             &es->source_fingerprint);
        if (result != 0) {
            editor_report_save_failure(es, result, es->file_path);
            return -1;
        }
    }
    return editor_finalize_save(es, es->file_path, "Saved");
}

static int editor_save_current_level_as_validated(EditorState *es)
{
    char path[EDITOR_PATH_MAX];
    int dialog_result;
    SerializerPathStatus target_status;

    dialog_result = file_dialog_save(path, (int)sizeof(path));
    if (dialog_result == FILE_DIALOG_CANCELLED) {
        editor_set_status(es, "Save cancelled");
        return -1;
    }
    if (dialog_result == FILE_DIALOG_INVALID_PATH) {
        editor_set_status(es, "Save failed: file names with line breaks are not supported");
        return -1;
    }
    if (dialog_result != FILE_DIALOG_SELECTED) {
        editor_set_status(es, "Save failed: dialog error");
        return -1;
    }
    if (!editor_path_fits(path)) {
        editor_set_status(es, "Save failed: path too long");
        return -1;
    }
    /*
     * A new destination is never followed through a symlink.  Following it
     * would write whatever file the link names (possibly a private editor
     * file, or one the user never chose) while the overwrite prompt showed
     * only the link's name.  Silently replacing the link with a regular file
     * is no better: it detaches whatever relied on the link.  So refuse, and
     * let the user pick another name or open the link to edit its target.
     * The save itself still replaces, never follows, a link that appears
     * after this check (see serializer_replace_file).
     */
    if (serializer_path_is_symlink(path)) {
        editor_set_status(es, "Save failed: %s is a symbolic link; choose another name",
                          path);
        return -1;
    }
    if (editor_path_is_private(es, path)) {
        editor_set_status(es, "Save failed: private editor path");
        return -1;
    }
    target_status = serializer_probe_path_utf8(path);
    if (target_status == SERIALIZER_PATH_ERROR) {
        editor_set_status(es, "Save failed: cannot inspect destination");
        return -1;
    }
    if (target_status == SERIALIZER_PATH_EXISTING &&
        !editor_confirm_overwrite(es, path)) {
        editor_set_status(es, "Save cancelled");
        return -1;
    }

    if (target_status == SERIALIZER_PATH_EXISTING) {
        SerializerFileFingerprint baseline;
        int result = -1;
        if (serializer_fingerprint_utf8(path, &baseline) == 1)
            result = level_save_toml_checked(&es->level, path,
                                             SERIALIZER_SAVE_REPLACE, &baseline);
        if (result != 0) {
            editor_report_save_failure(es, result, path);
            return -1;
        }
    } else {
        int result = level_save_toml_with_policy(&es->level, path,
                                                 SERIALIZER_SAVE_CREATE_ONLY);
        if (result != 0) {
            fprintf(stderr, "Error: failed to save %s\n", path);
            editor_report_save_failure(es, result, path);
            return -1;
        }
    }

    memcpy(es->file_path, path, strlen(path) + 1);
    memcpy(es->file_target, path, strlen(path) + 1);  /* written; not a link */
    editor_retire_current_recovery(es);
    if (editor_set_recovery_document(es, es->file_path) != 0) {
        editor_set_status(es, "Saved but recovery path unavailable");
        return -1;
    }
    if (serializer_fingerprint_utf8(es->file_path, &es->source_fingerprint) != 1) {
        memset(&es->source_fingerprint, 0, sizeof(es->source_fingerprint));
        es->source_state = EDITOR_SOURCE_UNKNOWN;
    } else {
        es->source_state = EDITOR_SOURCE_EXPECTED_EXISTING;
    }
    editor_set_document_save_point(es);
    editor_add_recent_file(es, es->file_path);
    editor_set_status(es, "Saved as %s", es->file_path);
    return 0;
}

int editor_save_current_level_as(EditorState *es)
{
    if (!es || !editor_finish_field_edit(es)) return -1;
    if (!editor_can_persist(es, "Save")) return -1;
    return editor_save_current_level_as_validated(es);
}

static int editor_source_changed(EditorState *es)
{
    SerializerFileFingerprint actual;
    int result;

    if (!es || es->file_path[0] == '\0') return -1;
    if (es->source_state == EDITOR_SOURCE_UNKNOWN) return 1;
    if (es->source_state == EDITOR_SOURCE_EXPECTED_MISSING) {
        result = serializer_probe_path_utf8(es->file_path);
        if (result == SERIALIZER_PATH_ERROR) return -1;
        return result == SERIALIZER_PATH_EXISTING ? 1 : 0;
    }
    if (!es->source_fingerprint.valid) return -1;
    result = serializer_fingerprint_utf8(es->file_path, &actual);
    if (result < 0) return -1;
    if (result == 0 || !serializer_fingerprint_equal(&es->source_fingerprint,
                                                     &actual)) return 1;
    return 0;
}

static int editor_finalize_save(EditorState *es, const char *path,
                                const char *status_prefix)
{
    if (!es || !path || !editor_path_fits(path)) return -1;
    editor_retire_current_recovery(es);
    if (editor_set_recovery_document(es, path) != 0) {
        editor_set_status(es, "Saved but recovery path unavailable");
        return -1;
    }
    if (serializer_fingerprint_utf8(path, &es->source_fingerprint) != 1) {
        memset(&es->source_fingerprint, 0, sizeof(es->source_fingerprint));
        es->source_state = EDITOR_SOURCE_UNKNOWN;
    } else {
        es->source_state = EDITOR_SOURCE_EXPECTED_EXISTING;
    }
    editor_set_document_save_point(es);
    editor_add_recent_file(es, path);
    editor_set_status(es, "%s %s", status_prefix, path);
    return 0;
}

int editor_file_exists(const char *path)
{
    return serializer_file_exists_utf8(path);
}

/*
 * editor_commit_temp_file — Finish an atomic replace begun with
 * serializer_open_temp.
 *
 * Writing straight into the real file would leave it half-written if the
 * editor crashed or the disk filled up mid-write.  Instead the content goes
 * to a temporary file next to it; only when every write, the flush to disk
 * and the close succeeded is the temp file renamed over the target, which
 * the OS does in one step.  On any failure the temp file is deleted and the
 * target keeps its previous contents.  Takes ownership of fp.
 */
int editor_commit_temp_file(FILE *fp, const char *temp_path,
                                   const char *target_path)
{
    int stream_error = serializer_stream_has_error(fp);
    int flush_error = stream_error ? -1 : serializer_flush(fp);
    int close_error = fclose(fp);
    int replace_error = (stream_error || flush_error || close_error)
                      ? -1
                      : serializer_replace_file(temp_path, target_path);

    /* A half-finished Windows replace leaves the temporary file as the only
     * copy (see serializer_replace_file): keep it rather than lose both. */
    if (replace_error == SERIALIZER_REPLACE_TEMP_KEPT) {
        fprintf(stderr, "editor: could not finish replacing '%s'; kept '%s'\n",
                target_path, temp_path);
        return -1;
    }
    if (stream_error || flush_error || close_error || replace_error) {
        serializer_remove_temp(temp_path);
        return -1;
    }
    return 0;
}

int editor_prepare_playtest_level(EditorState *es, char *path, size_t path_size)
{
    if (!es || !path || path_size == 0 ||
        !editor_can_persist(es, "Playtest")) {
        if (es) editor_set_status(es, "Play failed: no private destination");
        return -1;
    }

    if (es->playtest_path[0] == '\0' && editor_make_playtest_path(es) != 0) {
        editor_set_status(es, "Play failed: no private destination");
        return -1;
    }
    if (es->playtest_path[0] == '\0' ||
        strcmp(es->playtest_path, es->file_path) == 0 ||
        strcmp(es->playtest_path, "levels/_playtest.toml") == 0) {
        editor_set_status(es, "Play failed: no private destination");
        return -1;
    }
    if (strlen(es->playtest_path) >= path_size) {
        editor_set_status(es, "Play failed: path too long");
        return -1;
    }
    if (level_save_toml_private(&es->level, es->playtest_path) != 0) {
        editor_set_status(es, "Play failed: save temporary level");
        return -1;
    }
    str_copy(path, es->playtest_path, path_size);
    return 0;
}

void editor_retire_playtest_level(EditorState *es)
{
    if (!es || es->playtest_path[0] == '\0') return;
    (void)serializer_remove_utf8(es->playtest_path);
    es->playtest_path[0] = '\0';
}

void editor_load_recent_files(EditorState *es)
{
    FILE *fp;
    char line[EDITOR_PATH_MAX + 1];

    if (!es) return;
    if (es->recent_path[0] == '\0' &&
        editor_preference_file_path(es, EDITOR_RECENT_NAME, es->recent_path,
                                    sizeof(es->recent_path)) != 0) {
        es->recent_file_count = 0;
        return;
    }

    es->recent_file_count = 0;
    fp = serializer_fopen_utf8(es->recent_path, "rb");
    if (!fp) return;

    while (es->recent_file_count < EDITOR_RECENT_MAX &&
           fgets(line, sizeof(line), fp)) {
        size_t len = strlen(line);
        /*
         * No '\n' and not at end of file means the line did not fit in
         * `line`: discard the rest of *this* line only, up to and
         * including its newline, then read the next line normally.
         */
        if (!strchr(line, '\n') && !feof(fp)) {
            int ch;
            while ((ch = fgetc(fp)) != '\n' && ch != EOF) { }
            continue;
        }
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }
        if (line[0] == '\0' || !editor_path_fits(line) ||
            !editor_recent_path_storable(line)) continue;
        memcpy(es->recent_files[es->recent_file_count], line, len + 1);
        es->recent_file_count++;
    }
    fclose(fp);
}

/*
 * The recent list is one path per line, so a path that itself contains a
 * line break would be split into two bogus entries on the next start.
 * Such names are legal on most file systems; they are just never listed.
 */
static int editor_recent_path_storable(const char *path)
{
    return path && path[0] != '\0' && strpbrk(path, "\r\n") == NULL;
}

/* Rewrite the recent list atomically (temp file + rename), so a crash or a
 * full disk mid-write can never leave a truncated list behind. */
static int editor_save_recent_files(const EditorState *es)
{
    char temp_path[SERIALIZER_IO_PATH_MAX];
    FILE *fp;

    if (!es || es->recent_path[0] == '\0') return -1;
    if (serializer_make_temp_path(es->recent_path, temp_path,
                                  sizeof(temp_path)) != 0) return -1;
    fp = serializer_open_temp(es->recent_path, temp_path, sizeof(temp_path));
    if (!fp) return -1;

    for (int i = 0; i < es->recent_file_count; i++) {
        if (!editor_recent_path_storable(es->recent_files[i])) continue;
        if (fprintf(fp, "%s\n", es->recent_files[i]) < 0) {
            fclose(fp);
            serializer_remove_temp(temp_path);
            return -1;
        }
    }
    return editor_commit_temp_file(fp, temp_path, es->recent_path);
}

static void editor_add_recent_file(EditorState *es, const char *path)
{
    int existing = -1;

    if (!path || !editor_recent_path_storable(path) || !editor_path_fits(path) ||
        editor_path_is_private(es, path)) return;
    for (int i = 0; i < es->recent_file_count; i++) {
        if (strcmp(es->recent_files[i], path) == 0) {
            existing = i;
            break;
        }
    }

    if (existing > 0) {
        /* Every row is the same EDITOR_PATH_MAX array and already holds a
         * terminated string, so whole-row memcpy moves entries exactly. */
        char tmp[EDITOR_PATH_MAX];
        memcpy(tmp, es->recent_files[existing], sizeof(tmp));
        for (int i = existing; i > 0; i--) {
            memcpy(es->recent_files[i], es->recent_files[i - 1],
                   sizeof(es->recent_files[i]));
        }
        memcpy(es->recent_files[0], tmp, sizeof(es->recent_files[0]));
    } else if (existing < 0) {
        int limit = es->recent_file_count < EDITOR_RECENT_MAX
                  ? es->recent_file_count : EDITOR_RECENT_MAX - 1;
        for (int i = limit; i > 0; i--) {
            memcpy(es->recent_files[i], es->recent_files[i - 1],
                   sizeof(es->recent_files[i]));
        }
        strncpy(es->recent_files[0], path, sizeof(es->recent_files[0]) - 1);
        es->recent_files[0][sizeof(es->recent_files[0]) - 1] = '\0';
        if (es->recent_file_count < EDITOR_RECENT_MAX) es->recent_file_count++;
    }

    /* The in-memory list stays updated either way; only persistence for
     * the next session is lost, so warn without interrupting the save or
     * load that triggered this. */
    if (es->recent_path[0] != '\0' && editor_save_recent_files(es) != 0)
        fprintf(stderr, "Warning: could not write recent files to %s\n",
                es->recent_path);
}

int editor_preference_root_path(const EditorState *es, char *buf,
                                       size_t buf_size)
{
    char *pref_path;

    if (!es || !buf || buf_size == 0) return -1;
    buf[0] = '\0';
    if (es->preference_root[0] != '\0') {
        if (!editor_path_fits(es->preference_root)) return -1;
        memcpy(buf, es->preference_root, strlen(es->preference_root) + 1);
        return 0;
    }
    pref_path = preference_path(EDITOR_PREF_ORG, EDITOR_PREF_APP);
    if (!pref_path) return -1;
    if (strlen(pref_path) >= buf_size) {
        free(pref_path);
        return -1;
    }
    memcpy(buf, pref_path, strlen(pref_path) + 1);
    free(pref_path);
    return 0;
}

int editor_preference_file_path(const EditorState *es, const char *name,
                                       char *buf,
                                        size_t buf_size)
{
    char pref_path[EDITOR_PATH_MAX];
    size_t length;
    int written;

    if (!es || !name || !buf || buf_size == 0 ||
        editor_preference_root_path(es, pref_path, sizeof(pref_path)) != 0) return -1;
    buf[0] = '\0';

    length = strlen(pref_path);
    if (length > 0 &&
        (pref_path[length - 1] == '/' || pref_path[length - 1] == '\\')) {
        written = snprintf(buf, buf_size, "%s%s", pref_path, name);
    } else {
#ifdef _WIN32
        written = snprintf(buf, buf_size, "%s\\%s", pref_path, name);
#else
        written = snprintf(buf, buf_size, "%s/%s", pref_path, name);
#endif
    }
    if (written < 0 || (size_t)written >= buf_size) {
        buf[0] = '\0';
        return -1;
    }
    return 0;
}

static int editor_make_playtest_path(EditorState *es)
{
    char name[96];
    unsigned long process_id;

    if (!es) return -1;
#ifdef _WIN32
    process_id = (unsigned long)GetCurrentProcessId();
#else
    process_id = (unsigned long)getpid();
#endif

    for (int attempt = 0; attempt < 100; attempt++) {
        int written;
        editor_playtest_sequence++;
        written = snprintf(name, sizeof(name),
                           "editor_playtest_%lu_%u_%lu.toml",
                           process_id, (unsigned)clock_millis(),
                           editor_playtest_sequence);
        if (written < 0 || (size_t)written >= sizeof(name)) return -1;
        if (editor_preference_file_path(es, name, es->playtest_path,
                                         sizeof(es->playtest_path)) != 0) {
            es->playtest_path[0] = '\0';
            return -1;
        }
        if (!editor_file_exists(es->playtest_path)) return 0;
    }

    es->playtest_path[0] = '\0';
    return -1;
}

static int editor_path_is_private(const EditorState *es, const char *path)
{
    if (!es || !path || path[0] == '\0') return 0;
    /* The same-file checks catch another spelling of a private file, such
     * as one reached through a symlinked folder. */
    return editor_path_is_recovery(path) ||
           (es->autosave_path[0] != '\0' &&
            (strcmp(path, es->autosave_path) == 0 ||
             serializer_same_file_utf8(path, es->autosave_path))) ||
           (es->playtest_path[0] != '\0' &&
            (strcmp(path, es->playtest_path) == 0 ||
             serializer_same_file_utf8(path, es->playtest_path)));
}
