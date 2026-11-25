/**
 * @fileoverview Names the Closure Compiler must leave alone in super-mango.js.
 *
 * `make web` links with --closure 1: Closure shortens names inside the
 * JavaScript emcc writes (the runtime, raylib's library code, the --pre-js
 * files in web/ and the EM_JS bodies in src/), which cuts super-mango.js
 * to about half. It renames every property it does not know is shared with
 * code it never sees. This file is that list: what web/boot.js (the page's
 * own script, which Closure does not compile) and the game hand each
 * other, and the browser features Closure's own list lacks. A property
 * named in any object below is never renamed anywhere. A name missing
 * here fails quietly at run time (an undefined property), so add every
 * new one; tools/check_web_boot_contract.py checks the ones it can find.
 *
 * @externs
 */

/** @type {!Storage} */
var localStorage;

/* The Web Locks API (game_profile.c): navigator.locks.request(name,
 * {signal}, callback). */
/** @type {?} */
var __superMangoWebLocks = {locks: null, request: null, signal: null};

/* What web/boot.js puts on Module for the game to call (app_session.c,
 * game_web_input.c), and what the game keeps on Module between calls. */
/** @type {?} */
var __superMangoModule = {onGameEnded: null, clearTouchInput: null, __superMangoDebug: null,
                          __mangoKeyboardCleanup: null, __mangoProfileWrite: null};

/* The --pre-js host objects (web/touch-controls.js, web/keyboard-scope.js,
 * web/audio-unlock.js) on window, and their methods. */
/** @type {?} */
var SuperMangoTouch;
/** @type {?} */
var SuperMangoKeyboard;
/** @type {?} */
var SuperMangoAudioUnlock;
/** @type {?} */
var __superMangoHostApi = {mount: null, clear: null, setEnabled: null, destroy: null,
                           bind: null, install: null};

/* raylib's miniaudio state, which web/audio-unlock.js reads, and the GLFW
 * keyboard handlers web/keyboard-scope.js moves to the canvas. */
/** @type {?} */
var __superMangoRaylibState = {miniaudio: null, devices: null, device_state: null, started: null,
                               webaudio: null, state: null, onKeydown: null, onKeyPress: null,
                               onKeyup: null, onBlur: null};
