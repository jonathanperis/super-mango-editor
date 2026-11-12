/* web/boot.js on its own: the boot sequence both Web pages share.
 * A small hand-made document and window stand in for the browser; nothing
 * is downloaded. tests/web_host_test.cjs covers how each page uses it. */
const assert = require('node:assert/strict');
const boot = require('../web/boot.js');

function target() {
    const handlers = new Map();
    return {
        handlers, attributes: {}, children: [], style: {},
        addEventListener(name, fn, capture) { handlers.set(name, { fn, capture }); },
        removeEventListener(name, fn) { if (handlers.get(name)?.fn === fn) handlers.delete(name); },
        setAttribute(name, value) { this.attributes[name] = value; },
        appendChild(child) { this.children.push(child); child.parent = this; },
        remove() { this.parent.children = this.parent.children.filter(child => child !== this); this.removed = true; },
        focus(options) { this.focused = options; },
        querySelector(selector) { return this.children.find(child => selector.includes(child.href)) || null; },
    };
}

/* One fake page. `timers` holds what the boot script scheduled, so a test
   chooses when main() runs. */
function page(options = {}) {
    const canvas = target(), body = target(), head = target();
    const document = Object.assign(target(), { body, head, activeElement: null,
        createElement() { return target(); } });
    const touch = { enabled: false, clears: 0, destroyed: false,
        setEnabled(value) { this.enabled = value; }, clear() { this.clears++; },
        destroy() { this.destroyed = true; } };
    const timers = [], sent = [];
    const window = { document, setTimeout(fn) { timers.push(fn); },
        SuperMangoTouch: { mount(root, onCanvas, send) { touch.send = send; touch.root = root; return touch; } } };
    const events = [];
    const module = boot.start(Object.assign({
        window, document, canvas, touchRoot: 'touch-root',
        onStatus(text) { events.push(['status', text]); },
        onRunning() { events.push(['running']); },
        onFailure(message, needsReload) { events.push(['failure', message, needsReload]); },
        onTouch(action, pressed) { events.push(['touch', action, pressed]); },
    }, options));
    module._game_web_input_touch = (action, pressed) => { sent.push([action, pressed]); return 1; };
    module.callMain = args => { module.mainArguments = args.slice(); return options.mainResult || 0; };
    const runTimers = () => { while (timers.length) timers.shift()(); };
    return { canvas, document, body, head, touch, window, module, events, sent, timers, runTimers };
}

const failures = events => events.filter(event => event[0] === 'failure');

// ── Addresses: every game file carries the same version ───────────────────
assert.equal(boot.assetUrl('', 'super-mango.js', ''), 'super-mango.js');
assert.equal(boot.assetUrl('/play/', 'super-mango.wasm', '?v=1a2b'), '/play/super-mango.wasm?v=1a2b');
assert.equal(boot.statusText('Downloading data... (12/40)'), 'Downloading data...');
assert.equal(boot.statusText(null), '');
{
    const p = page({ assetBase: '/', assetQuery: '?v=1a2b', injectScript: true });
    assert.equal(p.window.Module, p.module, 'Emscripten reads its settings from window.Module');
    assert.equal(p.body.children.length, 1);
    assert.equal(p.body.children[0].src, '/super-mango.js?v=1a2b');
    // Emscripten passes the folder super-mango.js came from.
    assert.equal(p.module.locateFile('super-mango.wasm', '/'), '/super-mango.wasm?v=1a2b');
    assert.equal(p.module.locateFile('super-mango.data', ''), 'super-mango.data?v=1a2b');
    boot.preload(p.document, '/', '?v=1a2b');
    boot.preload(p.document, '/', '?v=1a2b');
    assert.deepEqual(p.head.children.map(link => [link.rel, link.as, link.crossOrigin, link.href]), [
        ['preload', 'fetch', 'anonymous', '/super-mango.wasm?v=1a2b'],
        ['preload', 'fetch', 'anonymous', '/super-mango.data?v=1a2b'],
    ], 'preload addresses must equal locateFile addresses, once each');
}

// ── A normal start ────────────────────────────────────────────────────────
{
    const p = page({ injectScript: true });
    assert.deepEqual(p.module.arguments, ['--level', 'levels/00_sandbox_01.toml']);
    assert.equal(p.document.handlers.get('keydown').capture, true);
    p.module.setStatus('Downloading data... (1/9)');
    p.module.setStatus('Downloading data... (2/9)');
    p.module.monitorRunDependencies(3);
    p.module.monitorRunDependencies(0);
    assert.deepEqual(p.events, [['status', 'Downloading data...'], ['status', 'Launching run...']],
        'each progress stage is reported once');
    p.module.onRuntimeInitialized();
    p.module.onRuntimeInitialized();
    assert.equal(p.touch.enabled, false, 'touch buttons were live before main() started');
    assert.equal(p.touch.root, 'touch-root');
    assert.equal(p.canvas.tabIndex, 0);
    assert.equal(p.canvas.focused.preventScroll, true);
    p.runTimers();
    assert.deepEqual(p.module.mainArguments, ['--level', 'levels/00_sandbox_01.toml']);
    assert.equal(p.module.__superMangoMainStarted, true);
    assert.equal(p.events.filter(event => event[0] === 'running').length, 1, 'main() must start exactly once');
    assert.equal(p.touch.enabled, true);
    assert.match(p.canvas.attributes['aria-label'], /running; F1 opens settings/);
    assert.deepEqual(p.events[2], ['status', ''], 'the page is told to hide its loading text');
    // A touch press reaches the page first, then the game.
    assert.equal(p.touch.send(4, 1), 1);
    assert.deepEqual(p.sent, [[4, 1]]);
    assert.deepEqual(p.events.at(-1), ['touch', 4, 1]);
    p.module.clearTouchInput();
    assert.equal(p.touch.clears, 1);
    assert.equal(failures(p.events).length, 0);
}

// ── Debug: the home page's option and the debug page's marker ─────────────
{
    const option = page({ debug: true });
    assert.deepEqual(option.module.arguments.slice(-1), ['--debug']);
    const marker = page();
    marker.module.__superMangoDebug = true;       // web/debug-boot.js
    marker.module.onRuntimeInitialized();
    marker.runTimers();
    assert.deepEqual(marker.module.mainArguments, ['--level', 'levels/00_sandbox_01.toml', '--debug']);
    const both = page({ debug: true });
    both.module.__superMangoDebug = true;
    both.module.onRuntimeInitialized();
    both.runTimers();
    assert.equal(both.module.mainArguments.filter(arg => arg === '--debug').length, 1);
}

// ── Keyboard scope ────────────────────────────────────────────────────────
{
    const p = page();
    const keydown = p.document.handlers.get('keydown').fn;
    const press = fields => {
        const event = Object.assign({ prevented: false, stopped: false,
            preventDefault() { this.prevented = true; },
            stopImmediatePropagation() { this.stopped = true; } }, fields);
        keydown(event);
        return event;
    };
    assert.equal(press({ code: 'ArrowDown' }).prevented, false, 'the page lost its keys while the game was unfocused');
    p.document.activeElement = p.canvas;
    assert.equal(press({ code: 'ArrowDown' }).prevented, true);
    const tab = press({ code: 'Tab' });
    assert.deepEqual([tab.prevented, tab.stopped], [false, true], 'Tab must leave the game');
    const shortcut = press({ code: 'KeyL', ctrlKey: true });
    assert.deepEqual([shortcut.prevented, shortcut.stopped], [false, true], 'browser shortcuts must keep working');
    const modifier = press({ code: 'ControlLeft', ctrlKey: true });
    assert.deepEqual([modifier.prevented, modifier.stopped], [false, false], 'a bare modifier still reaches the game');
}

// ── Failures: one message each, and nothing after it ──────────────────────
{
    // The injected script does not download: retry is possible.
    const p = page({ injectScript: true });
    const script = p.body.children[0];
    script.onerror();
    assert.deepEqual(p.events, [['failure', 'Failed to load game files. Retry the download.', false]]);
    assert.equal(script.removed, true, 'the dead script tag stayed in the page');
    assert.equal(p.document.handlers.has('keydown'), false, 'the keyboard stayed captured after a failure');
    assert.equal(p.canvas.handlers.has('webglcontextlost'), false);
    assert.match(p.canvas.attributes['aria-label'], /not running/);
    // Emscripten may still deliver callbacks; none may undo the message.
    p.module.setStatus('');
    p.module.onAbort();
    p.module.onRuntimeInitialized();
    p.runTimers();
    assert.equal(p.events.length, 1, 'something was reported after the failure');
    assert.equal(p.module.mainArguments, undefined, 'main() ran after the boot had failed');
}
{
    // The page's own script tag (written by emcc) does not download.
    const p = page({ injectScript: false });
    assert.equal(p.body.children.length, 0, 'injectScript: false must not add a script tag');
    const listener = p.document.handlers.get('error');
    assert.equal(listener.capture, true, 'resource errors do not bubble');
    listener.fn({ target: { tagName: 'LINK', src: '' } });
    listener.fn({ target: { tagName: 'SCRIPT', src: 'https://example.test/analytics.js' } });
    assert.equal(failures(p.events).length, 0, 'an unrelated resource error stopped the game');
    listener.fn({ target: { tagName: 'script', src: 'https://example.test/super-mango.js?v=1' } });
    assert.deepEqual(p.events, [['failure', 'Failed to load game files. Reload to retry.', true]]);
    assert.equal(p.document.handlers.has('error'), false);
}
{
    // The browser takes the WebGL context away while the game runs.
    const p = page();
    p.module.onRuntimeInitialized();
    p.runTimers();
    let prevented = false;
    p.canvas.handlers.get('webglcontextlost').fn({ preventDefault() { prevented = true; } });
    assert.equal(prevented, true);
    assert.deepEqual(p.events.at(-1), ['failure', 'WebGL context lost. Reload to continue.', true]);
    assert.equal(p.touch.enabled, false);
    // A second loss, or the game ending afterwards, adds nothing.
    p.module.onGameEnded(1, 0);
    assert.equal(failures(p.events).length, 1);
}
{
    // main() returns non-zero, or throws.
    const refused = page({ mainResult: 3 });
    refused.module.onRuntimeInitialized();
    refused.runTimers();
    assert.deepEqual(refused.events.at(-1), ['failure', 'Game startup failed. Reload to retry.', true]);
    assert.equal(refused.touch.enabled, false);
    const thrown = page({ logPrefix: '[test] ' });
    thrown.module.callMain = () => { throw new Error('boom'); };
    const realError = console.error;
    const logged = [];
    console.error = (...args) => logged.push(args[0]);
    try {
        thrown.module.onRuntimeInitialized();
        thrown.runTimers();
        thrown.module.printErr('stderr line');
    } finally {
        console.error = realError;
    }
    assert.deepEqual(thrown.events.at(-1), ['failure', 'Game startup failed. Reload to retry.', true]);
    assert.deepEqual(logged, ['[test] startup failed', '[test] stderr line']);
}
{
    // The game ends: the three messages a page can show.
    const messages = [[1, 0], [0, 1], [0, 0]].map(([fatal, profileError]) => {
        const p = page();
        p.module.onGameEnded(fatal, profileError);
        return p.events.at(-1)[1];
    });
    assert.deepEqual(messages, ['Game ended with an error.',
        'Run ended. Profile changes were not saved.', 'Run ended.']);
}
{
    // A failure between "runtime ready" and main() cancels main().
    const p = page();
    p.module.onRuntimeInitialized();
    p.module.onAbort();
    p.runTimers();
    assert.equal(p.module.mainArguments, undefined);
    assert.equal(failures(p.events).length, 1);
}
console.log('boot_test: ok');
