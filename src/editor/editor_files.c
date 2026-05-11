/*
 * editor_files.c — Editor file, export, autosave, and recent-file helpers.
 */

#include "editor_files.h"

#include <SDL.h>        /* SDL_GetTicks */
#include <SDL_image.h>  /* IMG_LoadTexture */
#include <stdio.h>      /* FILE, fopen, fprintf, stderr */
#include <string.h>     /* memset, strcmp, strlen, strncpy, strrchr */

#ifndef _WIN32
#include <sys/stat.h>   /* mkdir */
#else
#include <direct.h>     /* _mkdir */
#endif

#include "editor_session.h"    /* editor status/title/persist helpers */
#include "editor_validation.h" /* editor_validate_level */
#include "exporter.h"          /* level_export_c */
#include "file_dialog.h"       /* file_dialog_open */
#include "serializer.h"        /* level_load_toml, level_save_toml */
#include "undo.h"              /* undo_clear */

#define EDITOR_RECENT_PATH "out/editor_recent.txt"
#define EDITOR_RECENT_MAX    5
#define EDITOR_AUTOSAVE_MS   30000u

static void editor_ensure_out_dirs(void);
static void editor_save_recent_files(const EditorState *es);
static void editor_add_recent_file(EditorState *es, const char *path);

int editor_load_level(EditorState *es, const char *path)
{
    LevelDef new_level;
    memset(&new_level, 0, sizeof(new_level));

    if (level_load_toml(path, &new_level) != 0) {
        fprintf(stderr, "Error: failed to load %s\n", path);
        editor_set_status(es, "Load failed: %s", path);
        return -1;
    }

    es->level = new_level;
    strncpy(es->file_path, path, sizeof(es->file_path) - 1);
    es->file_path[sizeof(es->file_path) - 1] = '\0';
    undo_clear(es->undo);
    es->selection.index = -1;
    es->modified = 0;
    editor_add_recent_file(es, path);

    if (es->level.background_layer_count > 0) {
        const char *sky_path = es->level.background_layers[0].path;
        if (sky_path[0] != '\0') {
            SDL_Texture *new_sky = IMG_LoadTexture(es->renderer, sky_path);
            if (new_sky) {
                if (es->textures.sky)
                    SDL_DestroyTexture(es->textures.sky);
                es->textures.sky = new_sky;
            }
        }
    }
    if (es->level.floor_tile_path[0] != '\0') {
        SDL_Texture *new_floor = IMG_LoadTexture(es->renderer,
                                                  es->level.floor_tile_path);
        if (new_floor) {
            if (es->textures.floor_tile)
                SDL_DestroyTexture(es->textures.floor_tile);
            es->textures.floor_tile = new_floor;
        }
    }
    if (es->level.foreground_layer_count > 0) {
        const char *strip = es->level.foreground_layers[
            es->level.foreground_layer_count - 1].path;
        if (strip[0] != '\0') {
            SDL_Texture *new_water = IMG_LoadTexture(es->renderer, strip);
            if (new_water) {
                if (es->textures.water)
                    SDL_DestroyTexture(es->textures.water);
                es->textures.water = new_water;
            }
        }
    }

    editor_update_window_title(es);

    fprintf(stderr, "Loaded %s (%d entities)\n", path,
            es->level.coin_count + es->level.spider_count +
            es->level.platform_count + es->level.rail_count +
            es->level.bird_count + es->level.fish_count);
    editor_set_status(es, "Loaded %s", path);
    return 0;
}

void editor_open_level_file(EditorState *es)
{
    char path[256];

    if (file_dialog_open(path, sizeof(path))) {
        (void)editor_load_level(es, path);
    }
}

int editor_save_current_level(EditorState *es)
{
    if (!editor_can_persist(es, "Save")) return -1;

    if (es->file_path[0] == '\0') {
        strncpy(es->file_path, "levels/untitled.toml",
                sizeof(es->file_path) - 1);
        es->file_path[sizeof(es->file_path) - 1] = '\0';
    }
    if (level_save_toml(&es->level, es->file_path) == 0) {
        es->modified = 0;
        editor_add_recent_file(es, es->file_path);
        editor_update_window_title(es);
        editor_set_status(es, "Saved %s", es->file_path);
        return 0;
    }

    fprintf(stderr, "Error: failed to save %s\n", es->file_path);
    editor_set_status(es, "Save failed: %s", es->file_path);
    return -1;
}

int editor_export_current_level(EditorState *es)
{
    const char *var_name = "untitled";
    char name_buf[128] = {0};

    if (!editor_can_persist(es, "Export")) return -1;

    if (es->file_path[0] != '\0') {
        const char *base = strrchr(es->file_path, '/');
        base = base ? base + 1 : es->file_path;
        strncpy(name_buf, base, sizeof(name_buf) - 1);
        name_buf[sizeof(name_buf) - 1] = '\0';
        char *dot = strrchr(name_buf, '.');
        if (dot) *dot = '\0';
        var_name = name_buf;
    }

    if (level_export_c(&es->level, var_name, "src/levels/exported") == 0) {
        fprintf(stderr, "Exported to src/levels/exported/%s.h/.c\n", var_name);
        editor_set_status(es, "Exported %s", var_name);
        return 0;
    }

    fprintf(stderr, "Error: export failed for '%s'\n", var_name);
    editor_set_status(es, "Export failed: %s", var_name);
    return -1;
}

int editor_file_exists(const char *path)
{
    FILE *fp;

    if (!path || path[0] == '\0') return 0;
    fp = fopen(path, "rb");
    if (!fp) return 0;
    fclose(fp);
    return 1;
}

void editor_maybe_autosave(EditorState *es)
{
    Uint32 now = SDL_GetTicks();

    if (!es->modified) return;
    if (now - es->last_autosave_ms < EDITOR_AUTOSAVE_MS) return;

    es->last_autosave_ms = now;
    editor_validate_level(&es->level, &es->validation_report);
    if (es->validation_report.error_count > 0) return;

    editor_ensure_out_dirs();
    if (level_save_toml(&es->level, es->autosave_path) == 0) {
        editor_set_status(es, "Autosaved %s", es->autosave_path);
    }
}

void editor_load_recent_files(EditorState *es)
{
    FILE *fp = fopen(EDITOR_RECENT_PATH, "r");
    char line[256];

    es->recent_file_count = 0;
    if (!fp) return;

    while (es->recent_file_count < EDITOR_RECENT_MAX &&
           fgets(line, sizeof(line), fp)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }
        if (line[0] == '\0') continue;
        strncpy(es->recent_files[es->recent_file_count], line,
                sizeof(es->recent_files[0]) - 1);
        es->recent_files[es->recent_file_count][sizeof(es->recent_files[0]) - 1] = '\0';
        es->recent_file_count++;
    }
    fclose(fp);
}

static void editor_ensure_out_dirs(void)
{
#ifdef _WIN32
    _mkdir("out");
    _mkdir("out\\autosave");
#else
    mkdir("out", 0755);
    mkdir("out/autosave", 0755);
#endif
}

static void editor_save_recent_files(const EditorState *es)
{
    FILE *fp;

    editor_ensure_out_dirs();
    fp = fopen(EDITOR_RECENT_PATH, "w");
    if (!fp) return;

    for (int i = 0; i < es->recent_file_count; i++) {
        fprintf(fp, "%s\n", es->recent_files[i]);
    }
    fclose(fp);
}

static void editor_add_recent_file(EditorState *es, const char *path)
{
    int existing = -1;

    if (!path || path[0] == '\0') return;
    for (int i = 0; i < es->recent_file_count; i++) {
        if (strcmp(es->recent_files[i], path) == 0) {
            existing = i;
            break;
        }
    }

    if (existing > 0) {
        char tmp[256];
        strncpy(tmp, es->recent_files[existing], sizeof(tmp) - 1);
        tmp[sizeof(tmp) - 1] = '\0';
        for (int i = existing; i > 0; i--) {
            strncpy(es->recent_files[i], es->recent_files[i - 1],
                    sizeof(es->recent_files[i]) - 1);
            es->recent_files[i][sizeof(es->recent_files[i]) - 1] = '\0';
        }
        strncpy(es->recent_files[0], tmp, sizeof(es->recent_files[0]) - 1);
        es->recent_files[0][sizeof(es->recent_files[0]) - 1] = '\0';
    } else if (existing < 0) {
        int limit = es->recent_file_count < EDITOR_RECENT_MAX
                  ? es->recent_file_count : EDITOR_RECENT_MAX - 1;
        for (int i = limit; i > 0; i--) {
            strncpy(es->recent_files[i], es->recent_files[i - 1],
                    sizeof(es->recent_files[i]) - 1);
            es->recent_files[i][sizeof(es->recent_files[i]) - 1] = '\0';
        }
        strncpy(es->recent_files[0], path, sizeof(es->recent_files[0]) - 1);
        es->recent_files[0][sizeof(es->recent_files[0]) - 1] = '\0';
        if (es->recent_file_count < EDITOR_RECENT_MAX) es->recent_file_count++;
    }

    editor_save_recent_files(es);
}
