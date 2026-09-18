/*
 * editor.c — Build and release the standalone editor's owned resources.
 *
 * The editor runs in a separate process with its own window. It opens no
 * audio device. editor_frame.c shows the input → update → draw frame order;
 * editor_events.c translates commands into document/history operations.
 */
#include "editor.h"
#include "editor_frame.h"
#include "editor_files.h"
#include "editor_session.h"
#include "editor_textures.h"
#include <stdio.h>

int editor_init(EditorState *es, int hidden)
{
    if (display_open(EDITOR_W, EDITOR_H, "Super Mango Editor", hidden)) return -1;
    input_open(EDITOR_W, EDITOR_H);
    /* The window/context must exist before GPU resources are loaded. Draw
     * editor panels into this fixed-size target, then scale it for display. */
    es->frame_target = LoadRenderTexture(EDITOR_W, EDITOR_H);
    if (!IsRenderTextureValid(es->frame_target)) goto fail;
    SetTextureFilter(es->frame_target.texture, TEXTURE_FILTER_POINT);
    es->font = font_load("assets/fonts/round9x13.ttf", 13);
    if (!es->font) goto fail;

    /* Camera coordinates describe the level, while toolbar/panel coordinates
     * describe the editor canvas. A -1 selection means no entity is selected. */
    es->camera.x = 0;
    es->camera.zoom = 2;
    es->tool = TOOL_SELECT;
    es->selection.index = -1;
    es->palette_type = ENT_COIN;
    es->show_grid = es->panel_open = es->config_open = es->palette_open = 1;
    es->undo = undo_create();
    if (!es->undo) goto fail;
    /* Widgets borrow the font and call back before a document command so an
     * unfinished field edit can be applied, discarded or left active. */
    ui_init(&es->ui, es->font);
    es->ui.before_command = editor_before_command;
    es->ui.before_command_context = es;
    if (!hidden) {
        /* Render smoke uses task-owned data and must not discover the user's
         * recent documents or recovery files just to exercise drawing. */
        (void)editor_init_persistence_paths(es);
        editor_load_recent_files(es);
    }
    editor_textures_load(es);
    editor_level_init_defaults(&es->level);
    editor_set_document_save_point(es);
    es->running = 1;
    es->last_autosave_ms = (uint32_t)clock_millis();
    editor_set_status(es, es->recovery_entry_count > 1 ? "Recovery copies found: Ctrl+R to choose" :
        es->recovery_entry_count == 1 ? "Recovery copy found: Ctrl+R to recover" : "Ready");
    return 0;
fail:
    /* The caller supplies a zero-initialized EditorState. Cleanup accepts
     * empty slots as well as resources acquired before the failing step. */
    fprintf(stderr, "Editor startup failed: required graphics/font/history resource unavailable\n");
    editor_cleanup(es);
    return -1;
}

void editor_loop(EditorState *es)
{
    while (es->running) editor_run_frame(es);
}

void editor_cleanup(EditorState *es)
{
    /* Release cached labels and textures while their GPU context is alive.
     * UIState borrows the font; it must not be used after font_unload. */
    ui_cleanup(&es->ui);
    editor_textures_cleanup(es);
    font_unload(es->font);
    es->font = NULL;
    undo_destroy(es->undo);
    es->undo = NULL;
    if (IsRenderTextureValid(es->frame_target))
        UnloadRenderTexture(es->frame_target);
    es->frame_target = (RenderTexture2D){0};
    /* Restore backend callbacks before destroying the window they belong to. */
    input_close();
    if (IsWindowReady()) CloseWindow();
}
