/*
 * editor_frame.c — One-frame update/render step for the editor.
 *
 * Extracted from editor_loop so startup/teardown code stays separate from
 * frame dispatch, validation, autosave, rendering, and frame pacing.
 */

#include <SDL.h>      /* SDL_Event, SDL_GetTicks, SDL_RenderPresent */
#include <stdio.h>    /* fprintf, stderr */

#include "editor_frame.h"

#include "canvas.h"            /* canvas_render */
#include "editor_chrome.h"     /* editor toolbar/status rendering helpers */
#include "editor_events.h"     /* editor_handle_event */
#include "editor_files.h"      /* editor_maybe_autosave */
#include "editor_panels.h"     /* editor_render_side_panels */
#include "editor_playtest.h"   /* editor_check_play_status */
#include "editor_validation.h" /* editor_validate_level */
#include "ui.h"                /* ui_begin_frame */

/*
 * editor_run_frame — Run one editor event/update/render frame.
 *
 * Called once per loop iteration while EditorState.running is true.
 */
void editor_run_frame(EditorState *es) {
    SDL_Event event;

    /*
     * SDL_GetTicks — milliseconds since SDL_Init.
     *
     * We record the start time so we can compute elapsed time at the
     * end of the frame and sleep if we finished early.  This provides
     * a manual frame-rate cap as a fallback in case VSync is not
     * supported by the driver.
     */
    Uint32 frame_start = SDL_GetTicks();

    /* ---- Reset per-frame input ---------------------------------- */
    /*
     * ui_begin_frame clears the one-shot input flags (mouse_clicked,
     * key_backspace, key_return, key_escape, text_input) so that only
     * events from THIS frame are seen by widget functions.  Without
     * this reset a single click would register on multiple frames.
     */
    ui_begin_frame(&es->ui);

    /* ---- Event polling ------------------------------------------ */
    /*
     * SDL_PollEvent — dequeue one event from SDL's internal queue.
     *
     * Returns 1 if an event was available (and fills `event`), or 0
     * when the queue is empty.  We loop until the queue is drained
     * so that every input within a single frame is processed before
     * rendering.  This prevents input lag from accumulating.
     */
    while (SDL_PollEvent(&event)) {
        editor_handle_event(es, &event);
    }

    /* ---- Update mouse state for the UI -------------------------- */
    /*
     * SDL_GetMouseState — query the current mouse position.
     *
     * Updates es->mouse_x/y with the cursor's window-pixel coordinates.
     * We also copy them into the UIState so that immediate-mode widgets
     * (buttons, dropdowns, input fields) can do hit testing this frame.
     */
    SDL_GetMouseState(&es->mouse_x, &es->mouse_y);
    es->ui.mouse_x = es->mouse_x;
    es->ui.mouse_y = es->mouse_y;

    /* ---- Check if game child process exited --------------------- */
    /*
     * When the editor is in play-test mode, check each frame whether
     * the game process has exited.  If it has, return to editor mode.
     */
    if (es->playing
#ifndef _WIN32
        || es->play_pid > 0
#endif
    ) {
        editor_check_play_status(es);
    }

    editor_validate_level(&es->level, &es->validation_report);
    editor_maybe_autosave(es);

    /* ---- Clear the screen --------------------------------------- */
    if (SDL_SetRenderDrawColor(es->renderer, 0x1A, 0x1A, 0x1A, 0xFF) != 0) {
        fprintf(stderr, "SDL_SetRenderDrawColor failed: %s\n", SDL_GetError());
        es->running = 0;
        return;
    }
    if (SDL_RenderClear(es->renderer) != 0) {
        fprintf(stderr, "SDL_RenderClear failed: %s\n", SDL_GetError());
        es->running = 0;
        return;
    }

    if (es->playing) {
        editor_render_play_overlay(es);
    } else {
        /* ---- Normal editor rendering ----------------------------- */
        canvas_render(es);
        editor_render_toolbar(es);
        editor_render_side_panels(es);
        editor_render_status_bar(es);
    }

    /* ---- Present the frame -------------------------------------- */
    /*
     * SDL_RenderPresent — swap the back buffer to the screen.
     *
     * Everything drawn since SDL_RenderClear becomes visible at once.
     * If VSync is active this call blocks until the next monitor refresh
     * (typically 16.67 ms at 60 Hz), which naturally caps the frame rate.
     */
    SDL_RenderPresent(es->renderer);

    /* ---- Manual frame-rate cap ---------------------------------- */
    /*
     * If the frame completed in less than 16 ms (~60 FPS), sleep for
     * the remainder.  This is a fallback for systems where VSync is
     * unavailable or disabled — without it the editor would spin the
     * CPU at 100% and waste power.
     *
     * SDL_Delay yields the thread to the OS scheduler.  The actual
     * sleep may be slightly longer than requested, but for an editor
     * tool (not a twitchy game) this imprecision is fine.
     */
    Uint32 elapsed = SDL_GetTicks() - frame_start;
    if (elapsed < 16) {
        SDL_Delay(16 - elapsed);
    }
}
