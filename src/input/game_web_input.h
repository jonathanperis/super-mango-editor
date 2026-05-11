/*
 * game_web_input.h — WebAssembly browser input helpers.
 *
 * Native builds include this header too, but the functions become no-ops
 * outside Emscripten so callers do not need platform-specific branches.
 */
#pragma once

/* Flush stale browser key state before the WebAssembly main loop starts. */
void game_web_input_flush_stale_keys(void);
