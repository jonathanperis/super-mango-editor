/*
 * game_web_input.c — Browser-specific input repair for WebAssembly builds.
 */

#include "game_web_input.h"

#include <SDL.h>  /* SDL_FlushEvents — discard synthetic keyup events */

#ifdef __EMSCRIPTEN__
#include <emscripten.h>  /* emscripten_run_script — run JavaScript on canvas */
#endif

/*
 * game_web_input_flush_stale_keys — Clear stuck movement keys on startup.
 *
 * SDL2's Emscripten backend registers JavaScript keydown/keyup listeners on
 * #canvas during SDL_Init.  If a browser focus/navigation event creates a
 * keydown before the game loop starts, the matching keyup may never arrive and
 * SDL's internal keystate[] stays pressed forever.  Dispatching synthetic
 * keyup events on the canvas resets those scancodes before frame 1.
 */
void game_web_input_flush_stale_keys(void)
{
#ifdef __EMSCRIPTEN__
    /*
     * emscripten_run_script_int evaluates this JavaScript synchronously and
     * returns 1 only when #canvas existed and synthetic keyups were dispatched.
     * EM_ASM would require GNU C extensions; this keeps -std=c11 compatibility.
     */
    int repaired = emscripten_run_script_int(
        "(function(){"
        "  var K=['ArrowLeft','ArrowRight','ArrowUp','ArrowDown',"
        "    'Space','KeyA','KeyD','KeyW','KeyS','ShiftLeft','ShiftRight'];"
        "  var c=document.getElementById('canvas');"
        "  if(!c)return 0;"
        "  K.forEach(function(code){"
        "    c.dispatchEvent(new KeyboardEvent('keyup',{"
        "      code:code,bubbles:true,cancelable:true"
        "    }));"
        "  });"
        "  return 1;"
        "})()"
    );

    /*
     * SDL_FlushEvents removes only queued keyboard events after the repair.
     * The keystate[] reset already happened synchronously in SDL's browser
     * listener, so unrelated startup events must stay in the queue.
     */
    if (repaired) {
        SDL_FlushEvents(SDL_KEYDOWN, SDL_KEYUP);
    }
#else
    /* Native builds never see browser key state, so this helper is inert. */
#endif
}
