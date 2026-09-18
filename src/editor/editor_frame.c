/*
 * editor_frame.c — One input/update/render iteration of the editor.
 *
 * Widgets are immediate-mode: call them every frame after collecting input.
 * Document state and an unfinished field edit survive between these calls.
 */
#include "editor_frame.h"
#include "canvas.h"
#include "editor_chrome.h"
#include "editor_events.h"
#include "editor_files.h"
#include "editor_panels.h"
#include "editor_playtest.h"
#include "editor_validation.h"
#include "editor_session.h"

void editor_run_frame(EditorState *es)
{
    uint32_t now = (uint32_t)clock_millis();
    /* Clear one-shot flags before draining commands. Otherwise the previous
     * click or Return key would be applied again on the following frame. */
    ui_begin_frame(&es->ui);
    input_collect();
    InputEvent event;
    while (input_poll(&event))
        editor_handle_event(es, &event);

    /* Hover uses the latest sampled pointer. Commands above carry their own
     * event-time coordinates, so a quick click is not moved by later motion. */
    Vector2 mouse = input_mouse();
    es->mouse_x = es->ui.mouse_x = (int)mouse.x;
    es->mouse_y = es->ui.mouse_y = (int)mouse.y;
    if (es->playing
#ifndef _WIN32
        || es->play_pid > 0
#endif
    )
        editor_check_play_status(es);

    /* Validate edits immediately, but recheck unchanged external asset files
     * only once per second. Save/playtest still validate synchronously. */
    uint64_t hash = editor_document_hash(&es->level);
    if (!es->validation_cache_valid || hash != es->validated_document_hash ||
        now - es->last_validation_ms >= 1000u) {
        editor_validate_level(&es->level, &es->validation_report);
        es->validated_document_hash = hash;
        es->last_validation_ms = now;
        es->validation_cache_valid = 1;
    }
    editor_maybe_autosave(es);
    /* BeginDrawing resets raylib's frame state; BeginTextureMode redirects
     * subsequent draw calls to our logical editor canvas. Draw panels after
     * the world canvas so they appear above it (the painter's algorithm). */
    BeginDrawing();
    BeginTextureMode(es->frame_target);
    ClearBackground((Color){0x1a, 0x1a, 0x1a, 255});
    if (es->playing)
        editor_render_play_overlay(es);
    else {
        canvas_render(es);
        editor_render_toolbar(es);
        editor_render_side_panels(es);
        editor_render_status_bar(es);
    }
    /* Ends texture mode, presents the scaled target, then calls EndDrawing.
     * EndDrawing polls the next frame's input and owns frame pacing; adding
     * another event poll or sleep here would perform that job twice. */
    display_present(es->frame_target);
}
