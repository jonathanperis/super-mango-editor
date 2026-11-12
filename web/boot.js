/* The one boot sequence for the browser game.
 *
 * Two pages host the WebAssembly build: the standalone page (web/shell.html,
 * which becomes super-mango.html) and the website's home page
 * (docs/src/components/home/Dashboard.astro). Each used to carry its own
 * copy of this logic and the copies drifted apart: the home page handled a
 * lost WebGL context and a failed download, the standalone page handled
 * neither and showed its spinner forever. Now both pages paste this file
 * into their inline script (tools/web_shell.py for the standalone page, a
 * ?raw import for the home page) and only describe their own buttons and
 * status text. tools/check_web_boot_contract.py fails the build if a host
 * grows boot logic of its own again.
 *
 * What happens, in order:
 *   1. start() builds the settings object Emscripten reads (window.Module)
 *      and, on the home page, adds the script tag for super-mango.js. The
 *      standalone page already has that tag: emcc writes it.
 *   2. super-mango.js downloads super-mango.wasm and super-mango.data. Their
 *      addresses come from locateFile below, which appends "?v=<commit>" so
 *      a browser that cached an older deploy never pairs an old file with a
 *      new one.
 *   3. Emscripten calls onRuntimeInitialized. The touch buttons are mounted,
 *      the canvas takes the keyboard, and main() is called exactly once.
 *   4. Any failure (the script or a data file does not load, main() fails,
 *      the browser takes the WebGL context away, the game ends) goes through
 *      one function, fail(), which tells the page what to show.
 *
 * Written in plain ES5 (var, function) on purpose: emcc minifies the
 * standalone page's inline script, and the older syntax is the safe subset.
 *
 * This file is pasted inside an HTML script element, so it must never contain
 * the text of a script tag or an HTML comment opener, not even in a comment
 * (tools/web_shell.py checks).
 */
(function(root) {
    'use strict';

    /* The page always boots the sandbox level; Replay restarts a level
       inside the running game, so no page reads a "which level" value. */
    var DEFAULT_ARGUMENTS = ['--level', 'levels/00_sandbox_01.toml'];
    var GAME_SCRIPT = 'super-mango.js';
    /* Most of the download. super-mango.js only asks for them once it has
       loaded and run, so pages start them earlier with preload links. */
    var PRELOADED_FILES = ['super-mango.wasm', 'super-mango.data'];

    var LABEL_RUNNING = 'Super Mango game canvas - running; F1 opens settings';
    var LABEL_STOPPED = 'Super Mango game canvas - not running';

    /* base: where the game files are ('' = next to the page). query: '' or
       '?v=<commit>'. Every address of a game file is built here. */
    function assetUrl(base, name, query) {
        return (base || '') + name + (query || '');
    }

    /* Emscripten reports download progress as "Downloading data... (3/7)".
       Dropping the counter leaves one message per stage, so a screen reader
       announces "Downloading data..." once instead of on every tick. */
    function statusText(text) {
        return String(text || '').replace(/\s*\(\d+\/\d+\)\s*$/, '');
    }

    /* Adds a preload link for the .wasm and .data files. Emscripten
       fetches both in CORS mode with same-origin credentials, which is what
       crossorigin gives a preload, so it reuses these responses instead of
       downloading again. The address must match locateFile's exactly. */
    function preload(doc, base, query) {
        for (var i = 0; i < PRELOADED_FILES.length; i++) {
            var href = assetUrl(base, PRELOADED_FILES[i], query);
            if (doc.head.querySelector('link[rel="preload"][href="' + href + '"]')) continue;
            var link = doc.createElement('link');
            link.rel = 'preload';
            link.as = 'fetch';
            link.crossOrigin = 'anonymous';
            link.href = href;
            doc.head.appendChild(link);
        }
    }

    /* start(options) boots the game and returns Emscripten's Module object.
     *
     *   canvas        the <canvas> the game draws on
     *   touchRoot     the element that receives the touch buttons
     *   assetBase     address prefix of the game files; '' = beside the page
     *   assetQuery    '' or '?v=<commit>' (see locateFile)
     *   debug         true starts the game with --debug
     *   injectScript  true: add the script tag for super-mango.js here
     *                 (home page). false: the page already has it.
     *   logPrefix     put before console lines, e.g. '[Super Mango] '
     *   onStatus(text)             progress text; '' once the game runs
     *   onRunning()                main() started
     *   onFailure(message, needsReload)
     *                 the game is not running. needsReload is false only
     *                 when pressing Play again can work (a failed download
     *                 of super-mango.js on the home page).
     *   onTouch(action, pressed)   optional; called before a touch button
     *                              press is passed to the game
     *   window, document           optional; the tests pass their own
     */
    function start(options) {
        var view = options.window || root;
        var doc = options.document || view.document;
        var canvas = options.canvas;
        var prefix = options.logPrefix || '';
        var base = options.assetBase || '';
        var query = options.assetQuery || '';
        var touchControls = null;
        var script = null;
        var ended = false;
        var lastStatus = null;

        function report(name, a, b) {
            if (typeof options[name] === 'function') options[name](a, b);
        }

        /* While the canvas has the keyboard, keys belong to the game: the
           page must not scroll on arrows or Space. Tab and browser
           shortcuts (Ctrl/Cmd + key) are stopped before raylib's own
           listeners see them, so they keep their normal meaning and the
           keyboard can always leave the game. Registered in the capture
           phase, which runs before the canvas listeners. */
        function handleKeyDown(event) {
            if (doc.activeElement !== canvas) return;
            var modifierOnly = /^(Control|Meta)(Left|Right)$/.test(event.code);
            if (event.code === 'Tab' || ((event.ctrlKey || event.metaKey) && !modifierOnly)) {
                event.stopImmediatePropagation();
                return;
            }
            if (!event.ctrlKey && !event.metaKey) event.preventDefault();
        }

        /* Browsers take the WebGL context away when the GPU resets or too
           many pages use one. The game cannot redraw after that. */
        function handleContextLoss(event) {
            event.preventDefault();
            fail('WebGL context lost. Reload to continue.', true);
        }

        /* A script that fails to download fires "error" on itself. It
           does not bubble, but a capture-phase listener on the document
           still sees it; this is how the standalone page, whose script tag
           emcc writes, learns that super-mango.js never arrived. */
        function handleResourceError(event) {
            var target = event && event.target;
            if (!target || String(target.tagName).toUpperCase() !== 'SCRIPT') return;
            if (String(target.src || '').indexOf(GAME_SCRIPT) < 0) return;
            fail('Failed to load game files. Reload to retry.', true);
        }

        function removeListeners() {
            doc.removeEventListener('keydown', handleKeyDown, true);
            doc.removeEventListener('error', handleResourceError, true);
            canvas.removeEventListener('webglcontextlost', handleContextLoss, false);
        }

        /* The single way out. After it, nothing else is reported: a late
           progress message must not hide the failure text. */
        function fail(message, needsReload) {
            if (ended) return;
            ended = true;
            if (touchControls) touchControls.setEnabled(false);
            canvas.setAttribute('aria-label', LABEL_STOPPED);
            removeListeners();
            report('onFailure', message, needsReload);
        }

        function setStatus(text) {
            var shown = statusText(text);
            if (ended || shown === lastStatus) return;
            lastStatus = shown;
            report('onStatus', shown);
        }

        var moduleConfig = {
            arguments: DEFAULT_ARGUMENTS.concat(options.debug ? ['--debug'] : []),
            canvas: canvas,
            print: function(text) { console.log(prefix + text); },
            printErr: function(text) { console.error(prefix + text); },
            /* path is a bare file name; scriptDirectory is where
               super-mango.js came from. */
            locateFile: function(path, scriptDirectory) {
                return assetUrl(scriptDirectory, path, query);
            },
            onAbort: function() { fail('Game startup failed. Reload to retry.', true); },
            onGameEnded: function(fatal, profileError) {
                fail(fatal ? 'Game ended with an error.' : profileError
                    ? 'Run ended. Profile changes were not saved.' : 'Run ended.', true);
            },
            setStatus: setStatus,
            monitorRunDependencies: function(left) {
                if (left === 0) setStatus('Launching run...');
            },
            onRuntimeInitialized: function() {
                if (ended) return;
                if (!touchControls) touchControls = view.SuperMangoTouch.mount(
                    options.touchRoot, canvas,
                    function(action, pressed) {
                        report('onTouch', action, pressed);
                        return moduleConfig._game_web_input_touch(action, pressed);
                    });
                moduleConfig.clearTouchInput = function() { touchControls.clear(); };
                canvas.tabIndex = 0;
                canvas.focus({preventScroll: true});
                /* The debug page sets this marker after this script (see
                   web/debug-boot.js); the home page passes options.debug. */
                if (moduleConfig.__superMangoDebug && moduleConfig.arguments.indexOf('--debug') < 0)
                    moduleConfig.arguments.push('--debug');
                setStatus('');
                /* main() runs on the next turn of the event loop, once
                   Emscripten has finished its own start-up, and only once
                   even if this callback is ever delivered twice. */
                view.setTimeout(function() {
                    if (ended || moduleConfig.__superMangoMainStarted) return;
                    moduleConfig.__superMangoMainStarted = true;
                    try {
                        var result = moduleConfig.callMain(moduleConfig.arguments || []);
                        if (result) {
                            fail('Game startup failed. Reload to retry.', true);
                            return;
                        }
                    } catch (error) {
                        console.error(prefix + 'startup failed', error);
                        fail('Game startup failed. Reload to retry.', true);
                        return;
                    }
                    touchControls.setEnabled(true);
                    canvas.setAttribute('aria-label', LABEL_RUNNING);
                    report('onRunning');
                }, 0);
            }
        };

        doc.addEventListener('keydown', handleKeyDown, true);
        canvas.addEventListener('webglcontextlost', handleContextLoss, false);
        view.Module = moduleConfig;

        if (options.injectScript) {
            script = doc.createElement('script');
            script.src = assetUrl(base, GAME_SCRIPT, query);
            script.onerror = function() {
                if (touchControls) { touchControls.destroy(); touchControls = null; }
                script.remove();
                fail('Failed to load game files. Retry the download.', false);
            };
            doc.body.appendChild(script);
        } else {
            doc.addEventListener('error', handleResourceError, true);
        }
        return moduleConfig;
    }

    var api = {start: start, preload: preload, assetUrl: assetUrl, statusText: statusText,
        DEFAULT_ARGUMENTS: DEFAULT_ARGUMENTS};
    root.SuperMangoBoot = api;
    if (typeof module === 'object' && module.exports) module.exports = api;
})(typeof window === 'object' ? window : globalThis);
