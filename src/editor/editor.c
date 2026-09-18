/* Editor startup/teardown. Widget input and drawing live in editor_frame.c. */
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
    es->frame_target = LoadRenderTexture(EDITOR_W, EDITOR_H);
    if (!IsRenderTextureValid(es->frame_target)) goto fail;
    SetTextureFilter(es->frame_target.texture, TEXTURE_FILTER_POINT);
    es->font = font_load("assets/fonts/round9x13.ttf", 13);
    if (!es->font) goto fail;
    es->camera.x = 0;
    es->camera.zoom = 2;
    es->tool = TOOL_SELECT;
    es->selection.index = -1;
    es->palette_type = ENT_COIN;
    es->show_grid = es->panel_open = es->config_open = es->palette_open = 1;
    es->undo = undo_create();
    if (!es->undo) goto fail;
    ui_init(&es->ui, es->font);
    es->ui.before_command = editor_before_command;
    es->ui.before_command_context = es;
    if (!hidden) {
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
    ui_cleanup(&es->ui);
    editor_textures_cleanup(es);
    font_unload(es->font);
    es->font = NULL;
    undo_destroy(es->undo);
    es->undo = NULL;
    if (IsRenderTextureValid(es->frame_target)) UnloadRenderTexture(es->frame_target);
    es->frame_target = (RenderTexture2D){0};
    input_close();
    if (IsWindowReady()) CloseWindow();
}
