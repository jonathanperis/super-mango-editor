/*
 * debug.h — Public interface for the debug overlay module.
 *
 * Defines the DebugOverlay struct and declares functions that draw
 * collision boxes, an FPS counter, and a scrolling event log on screen.
 *
 * This header does NOT include game.h to avoid a circular dependency
 * (game.h includes debug.h for the DebugOverlay field in GameState).
 * Instead, debug_render takes a const void* that debug.c casts to
 * const GameState* internally.
 */
#pragma once

#include <SDL.h>       /* SDL_Renderer, Uint64 */
#include <SDL_ttf.h>   /* TTF_Font             */

/* ------------------------------------------------------------------ */
/* Constants                                                           */
/* ------------------------------------------------------------------ */

/* Maximum number of log messages visible on screen at once. */
#define DEBUG_LOG_MAX_ENTRIES  8

/* Maximum characters per log message (including the null terminator). */
#define DEBUG_LOG_MSG_LEN     64

/*
 * Seconds each log message stays visible before it is considered expired
 * and no longer rendered.
 */
#define DEBUG_LOG_DISPLAY_SEC 4.0f

/*
 * Milliseconds between FPS counter updates.  500 ms gives two samples per
 * second, which is stable enough to read while still being responsive.
 */
#define DEBUG_FPS_SAMPLE_MS   500

/* ------------------------------------------------------------------ */
/* Types                                                               */
/* ------------------------------------------------------------------ */

/*
 * DebugLogEntry — one message in the scrolling debug log.
 *
 * Each entry stores its text and its age in seconds.  When the age
 * exceeds DEBUG_LOG_DISPLAY_SEC the entry is no longer drawn.
 */
typedef struct {
    char  text[DEBUG_LOG_MSG_LEN]; /* the formatted message string         */
    float age;                      /* seconds since this entry was created */
} DebugLogEntry;

/*
 * DebugOverlay — all state for the debug HUD.
 *
 * Contains the FPS measurement counters and the circular buffer of
 * log messages.  Owns no GPU resources — it borrows the Hud font
 * for text rendering.
 */
typedef struct {
    /* FPS measurement */
    Uint64 fps_prev_ticks;    /* SDL_GetTicks64 value at the last sample  */
    int    fps_frame_count;   /* frames rendered since the last sample    */
    int    fps_display;       /* the FPS number currently shown on screen */

    /* Frame time (CPU proxy) */
    float  frame_ms;          /* last frame duration in milliseconds      */
    float  frame_ms_display;  /* smoothed frame time shown on screen      */
    float  cpu_percent;       /* frame_ms / 16.67 as percentage of budget */

    /* Memory usage (updated every sample period) */
    float  mem_mb;            /* resident memory in megabytes             */

    /* Scrolling event log (circular buffer) */
    DebugLogEntry log[DEBUG_LOG_MAX_ENTRIES];
    int           log_head;   /* index of the next slot to write into     */
    int           log_count;  /* number of valid entries (≤ MAX_ENTRIES)  */
} DebugOverlay;

/* ------------------------------------------------------------------ */
/* Function declarations                                               */
/* ------------------------------------------------------------------ */

/* Reset all debug overlay state (FPS counters, log buffer). */
void debug_init(DebugOverlay *dbg);

/*
 * Advance FPS sample timer and age log entries.
 * dt is in seconds (same value used by game physics).
 */
void debug_update(DebugOverlay *dbg, float dt);

/*
 * Push a formatted message into the scrolling debug log.
 * Uses printf-style format string + variadic arguments.
 */
void debug_log(DebugOverlay *dbg, const char *fmt, ...);

/*
 * debug_render — Draw all debug overlays on top of everything else.
 *
 * Draws collision boxes, the FPS counter, and the event log.
 *   font     — the TTF_Font from Hud (borrowed, not owned).
 *   renderer — the SDL renderer.
 *   gs_ptr   — opaque pointer to GameState (cast inside debug.c to
 *              break the circular include between debug.h and game.h).
 *   cam_x    — current camera offset for world-to-screen conversion.
 */
void debug_render(const DebugOverlay *dbg, TTF_Font *font,
                  SDL_Renderer *renderer, const void *gs_ptr, int cam_x);
