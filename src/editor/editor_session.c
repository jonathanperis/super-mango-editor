/*
 * editor_session.c — Editor session state helpers.
 */

#include "editor_session.h"

#include <SDL.h>          /* SDL_ShowMessageBox, SDL_GetError, SDL_SetWindowTitle */
#include <stdarg.h>       /* va_list */
#include <stdio.h>        /* fprintf, snprintf, stderr, vsnprintf */
#include <string.h>       /* memset, strncpy */

#include "editor_validation.h" /* editor_validate_level */
#include "undo.h"              /* undo_clear */

void editor_set_status(EditorState *es, const char *fmt, ...)
{
    va_list ap;

    if (!es || !fmt) return;
    va_start(ap, fmt);
    vsnprintf(es->status_message, sizeof(es->status_message), fmt, ap);
    va_end(ap);
}

void editor_level_init_defaults(LevelDef *level)
{
    if (!level) return;

    level_def_init_defaults(level);
    strncpy(level->name, "Untitled", sizeof(level->name) - 1);
    level->screen_count = 4;
    level->player_start_x = 48.0f;
    level->player_start_y = 205.0f;
    level->last_star.x = 145.0f;
    level->last_star.y = 167.0f;
    strncpy(level->floor_tile_path, "assets/sprites/levels/grass_tileset.png",
            sizeof(level->floor_tile_path) - 1);
}

void editor_update_window_title(EditorState *es)
{
    char title[300];

    if (es->file_path[0] != '\0') {
        snprintf(title, sizeof(title), "Super Mango Editor - %s%s",
                 es->file_path, es->modified ? " *" : "");
    } else {
        snprintf(title, sizeof(title), "Super Mango Editor%s",
                 es->modified ? " *" : "");
    }
    SDL_SetWindowTitle(es->window, title);
}

void editor_reset_new_level(EditorState *es)
{
    editor_level_init_defaults(&es->level);
    es->file_path[0] = '\0';
    undo_clear(es->undo);
    es->modified        = 0;
    es->selection.index = -1;
    editor_set_status(es, "New level");
    editor_update_window_title(es);
}

int editor_can_persist(EditorState *es, const char *action)
{
    editor_validate_level(&es->level, &es->validation_report);
    if (es->validation_report.error_count > 0) {
        const char *msg = es->validation_report.message_count > 0
                        ? es->validation_report.messages[0]
                        : "validation failed";
        editor_set_status(es, "%s blocked: %s", action, msg);
        fprintf(stderr, "%s blocked: %s\n", action, msg);
        return 0;
    }
    return 1;
}

int editor_confirm_discard_changes(EditorState *es, const char *action)
{
    SDL_MessageBoxButtonData buttons[2] = {
        { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Cancel" },
        { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Discard" },
    };
    SDL_MessageBoxData data;
    char message[256];
    int button_id = 0;

    if (!es || !es->modified) return 1;

    snprintf(message, sizeof(message),
             "Unsaved changes will be lost. Discard changes and %s?", action);

    memset(&data, 0, sizeof(data));
    data.flags = SDL_MESSAGEBOX_WARNING;
    data.window = es->window;
    data.title = "Unsaved Changes";
    data.message = message;
    data.numbuttons = 2;
    data.buttons = buttons;

    if (SDL_ShowMessageBox(&data, &button_id) != 0) {
        fprintf(stderr, "SDL_ShowMessageBox error: %s\n", SDL_GetError());
        editor_set_status(es, "%s cancelled: confirmation failed", action);
        return 0;
    }

    if (button_id == 1) return 1;

    editor_set_status(es, "%s cancelled", action);
    return 0;
}
