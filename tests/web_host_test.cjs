/* Pure host-contract tests using Node's VM. No browser or network is used. */
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

// Both pages paste web/boot.js into an inline script in front of their own.
// Here it is run first in the same sandbox, as the browser would.
const bootSource = fs.readFileSync(path.join(__dirname, '../web/boot.js'), 'utf8');

function host(file, pattern, options = {}) {
    const nodes = new Map();
    const listeners = new Map();
    let reloads = 0, storageReads = 0;
    function element(id = '') {
        const handlers = new Map();
        const node = { id, style: {}, dataset: {}, children: [], disabled: false,
            textContent: '', handlers, classes: new Set(),
            classList: { add(name) { node.classes.add(name); } },
            addEventListener(name, fn) { handlers.set(name, fn); },
            removeEventListener(name) { handlers.delete(name); },
            appendChild(child) { this.children.push(child); child.parent = this; },
            remove() { this.parent.children = this.parent.children.filter(child => child !== this); },
            focus(options) { this.focusOptions = options; document.activeElement = this; },
            setAttribute(name, value) { this[name] = value; },
            scrollIntoView(options) { this.scrollOptions = options; },
            querySelector(selector) { return nodes.get(selector); },
            getAttribute(name) { return name === 'data-start-game' ? (id === 'debug-btn' ? 'debug' : 'play') : null; },
            requestFullscreen() { this.fullscreenRequests = (this.fullscreenRequests || 0) + 1; return Promise.resolve(); },
        };
        Object.defineProperty(node, 'innerHTML', {
            set() { this.children = []; }, get() { return ''; },
        });
        return node;
    }
    for (const id of ['canvas', 'game-status', 'play-btn', 'debug-btn', 'play',
                       'status', 'status-text', 'touch-controls', 'game-viewport', '.cabinet-standby',
                       '.cabinet-standby-note', 'canvas-container', 'fullscreen-btn']) nodes.set(id, element(id));
    nodes.get('fullscreen-btn').hidden = true;
    Object.assign(nodes.get('play').dataset, options.dataset || {});
    const status = nodes.get('game-status');
    for (const id of ['.cabinet-standby', '.cabinet-standby-note', 'play-btn', 'debug-btn']) status.appendChild(nodes.get(id));
    const document = {
        activeElement: null, body: element('body'), head: element('head'),
        fullscreenEnabled: options.fullscreen !== false,
        getElementById(id) { return nodes.get(id); },
        createElement() { return element(); },
        querySelectorAll() { return [nodes.get('play-btn'), nodes.get('debug-btn')]; },
        addEventListener(name, fn, capture) { fn.capture = capture; listeners.set(name, fn); },
        removeEventListener(name, fn, capture) {
            if (listeners.get(name) === fn && fn.capture === capture) listeners.delete(name);
        },
    };
    const window = {
        SuperMangoTouch: { mount() { return touch; } },
        // Replay restarts in place; the hosts no longer read a boot intent.
        sessionStorage: { getItem() { storageReads++; return null; }, removeItem() { storageReads++; } },
        setTimeout(fn) { fn(); }, location: { reload() { reloads++; } },
        document,
    };
    const touch = { enabled: false, clears: 0,
        setEnabled(value) { this.enabled = value; }, clear() { this.clears++; }, destroy() { this.enabled = false; } };
    const context = vm.createContext({ document, window, sessionStorage: window.sessionStorage,
        setTimeout: window.setTimeout, console: { log() {}, error() {} } });
    vm.runInContext(bootSource, context);
    // tools/web_shell.py fills these two markers when the standalone page is
    // built; the boot script is already loaded above, and a local build has
    // no version.
    const source = fs.readFileSync(path.join(__dirname, '..', file), 'utf8').match(pattern)[1]
        .replace('/*MANGO_BOOT_JS*/', '').replace(/@MANGO_ASSET_QUERY@/g, options.query || '');
    assert(!source.includes('MANGO_'), `${file}: an unfilled build marker reached the script`);
    vm.runInContext(source, context);
    return { nodes, document, window, listeners, context, touch, reloads: () => reloads,
        storageReads: () => storageReads };
}

const page = host('docs/src/components/home/Dashboard.astro', /<script is:inline>([\s\S]*?)<\/script>/);
vm.runInContext('startGame(false)', page.context);
assert(page.nodes.get('game-status').children.includes(page.nodes.get('play-btn')), 'boot removed retry controls');
let prevented = false;
const event = { code: 'ArrowDown', preventDefault() { prevented = true; } };
page.listeners.get('keydown')(event);
assert.equal(prevented, false, 'unfocused game captured page keyboard');
page.document.activeElement = page.nodes.get('canvas');
page.listeners.get('keydown')(event);
assert.equal(prevented, true, 'focused game did not capture movement');
prevented = false;
page.listeners.get('keydown')({code:'KeyJ',preventDefault(){prevented=true;}});
assert.equal(prevented,true,'remapped gameplay key escaped focus handling');
prevented = false;
let stopped = 0;
page.listeners.get('keydown')({code:'Tab',preventDefault(){prevented=true;},stopImmediatePropagation(){stopped++;}});
page.listeners.get('keydown')({code:'KeyL',ctrlKey:true,preventDefault(){prevented=true;},stopImmediatePropagation(){stopped++;}});
assert.equal(prevented,false,'browser navigation was trapped');
assert.equal(stopped,2,'reserved navigation reached backend target listeners');
assert.equal(page.listeners.get('keydown').capture,true,'navigation guard was not registered before the canvas backend');
assert.equal(page.document.body.children[0].src, './super-mango.js');
page.document.body.children[0].onerror();
assert.match(page.nodes.get('.cabinet-standby').textContent, /Failed to load game files/);
assert.equal(page.nodes.get('play-btn').textContent, 'Retry normal run', 'a failed download must be retryable');
assert.equal(page.nodes.get('play-btn').disabled, false);
assert.equal(page.nodes.get('play-btn').style.display, 'inline-block');
assert.equal(page.listeners.has('keydown'), false);
vm.runInContext('startGame(false)', page.context);
let calls = 0;
page.window.Module.callMain = () => { calls++; return 0; };
page.window.Module.onRuntimeInitialized();
page.window.Module.onRuntimeInitialized();
assert.equal(calls, 1);
assert.equal(page.touch.enabled, true);
assert.equal(page.nodes.get('canvas').focusOptions.preventScroll, true);
assert.equal(page.nodes.get('game-viewport').scrollOptions.block, 'start');
assert.match(page.nodes.get('canvas')['aria-label'], /running; F1/);
page.window.Module.clearTouchInput();
assert.equal(page.touch.clears, 1);
page.window.Module.onGameEnded(0, 1);
assert.equal(page.touch.enabled, false);
assert.match(page.nodes.get('canvas')['aria-label'], /not running/);
assert.match(page.nodes.get('.cabinet-standby').textContent, /not saved/);
vm.runInContext('startGame(false)', page.context);
assert.equal(page.reloads(), 1, 'ended runtime should restart via reload');
assert.equal(page.storageReads(), 0, 'Dashboard read a replay boot intent');

// A lost WebGL context on the home page stops the game and asks for a reload.
const lost = host('docs/src/components/home/Dashboard.astro', /<script is:inline>([\s\S]*?)<\/script>/);
vm.runInContext('startGame(true)', lost.context);
assert(lost.window.Module.arguments.includes('--debug'), 'the debug button did not pass --debug');
let lossPrevented = false;
lost.nodes.get('canvas').handlers.get('webglcontextlost')({ preventDefault() { lossPrevented = true; } });
assert.equal(lossPrevented, true);
assert.match(lost.nodes.get('.cabinet-standby').textContent, /WebGL context lost/);
assert.equal(lost.nodes.get('play-btn').textContent, 'Reload game');
// The page versions every game file with the commit it was built from.
const versionedPage = host('docs/src/components/home/Dashboard.astro', /<script is:inline>([\s\S]*?)<\/script>/,
    { dataset: { assetBase: '/', assetQuery: '?v=abc123' } });
vm.runInContext('requestGameStart(false)', versionedPage.context);
assert.deepEqual(versionedPage.document.head.children.map(link => link.href),
    ['/super-mango.wasm?v=abc123', '/super-mango.data?v=abc123']);
assert.equal(versionedPage.document.body.children[0].src, '/super-mango.js?v=abc123');
assert.equal(versionedPage.window.Module.locateFile('super-mango.wasm', '/'), '/super-mango.wasm?v=abc123');

const shell = host('web/shell.html', /<script>([\s\S]*?)<\/script\b[^>]*>/i);
let args;
// The debug page inlines web/debug-boot.js after the shell's boot script.
vm.runInContext(fs.readFileSync(path.join(__dirname, '../web/debug-boot.js'), 'utf8'), shell.context);
assert.equal(shell.window.Module.__superMangoDebug, true);
shell.window.Module.callMain = value => { args = value; return 0; };
shell.window.Module.onRuntimeInitialized();
assert(args.includes('--debug'), 'standalone debug payload omitted debug argument');
assert.equal(JSON.stringify(args.slice(0, 2)), JSON.stringify(['--level', 'levels/00_sandbox_01.toml']));
assert.equal(shell.storageReads(), 0, 'standalone shell read a replay boot intent');
assert.match(shell.nodes.get('canvas')['aria-label'], /running; F1/);
shell.window.Module.onAbort();
assert.equal(shell.touch.enabled, false);
assert.equal(shell.nodes.get('status').style.pointerEvents, 'auto');
assert.match(shell.nodes.get('status-text').textContent, /Game startup failed/);
shell.nodes.get('status').children[0].onclick();
assert.equal(shell.reloads(), 1);

// The standalone page used to handle neither of these and kept its spinner
// turning forever: emcc writes its script tag, so the boot script watches
// the document for that tag's "error" event, and for a lost WebGL context.
const offline = host('web/shell.html', /<script>([\s\S]*?)<\/script\b[^>]*>/i, { query: '?v=abc123' });
assert.equal(offline.window.Module.locateFile('super-mango.data', ''), 'super-mango.data?v=abc123');
assert.equal(offline.document.body.children.length, 0, 'the standalone page added a second script tag');
const resourceError = offline.listeners.get('error');
assert.equal(resourceError.capture, true, 'script errors do not bubble; the listener must capture');
resourceError({ target: { tagName: 'IMG', src: 'x.png' } });
assert.equal(offline.nodes.get('status').children.length, 0, 'an unrelated resource error stopped the game');
resourceError({ target: { tagName: 'SCRIPT', src: 'http://localhost/super-mango.js?v=abc123' } });
assert.match(offline.nodes.get('status-text').textContent, /Failed to load game files/);
assert.equal(offline.nodes.get('status').children.length, 1, 'no Reload button after a failed download');
const glLost = host('web/shell.html', /<script>([\s\S]*?)<\/script\b[^>]*>/i);
glLost.nodes.get('canvas').handlers.get('webglcontextlost')({ preventDefault() {} });
assert.match(glLost.nodes.get('status-text').textContent, /WebGL context lost/);
// Progress text reaches the page; an empty one hides the status box.
const loading = host('web/shell.html', /<script>([\s\S]*?)<\/script\b[^>]*>/i);
loading.window.Module.setStatus('Downloading data... (1/3)');
assert.equal(loading.nodes.get('status-text').textContent, 'Downloading data...');
loading.window.Module.setStatus('');
assert.equal(loading.nodes.get('status').style.display, 'none');

// The page's Fullscreen button (the game cannot request it from its own
// frame callback) takes the whole container, then hands the keyboard back.
const button = shell.nodes.get('fullscreen-btn');
assert.equal(button.hidden, false, 'fullscreen button hidden where fullscreen works');
shell.document.activeElement = button;
button.handlers.get('click')();
assert.equal(shell.nodes.get('canvas-container').fullscreenRequests, 1);
assert.equal(shell.nodes.get('canvas').fullscreenRequests, undefined, 'canvas went fullscreen without its container');
assert.equal(shell.document.activeElement, shell.nodes.get('canvas'), 'keyboard stayed on the button');
const noFullscreen = host('web/shell.html', /<script>([\s\S]*?)<\/script\b[^>]*>/i, { fullscreen: false });
assert.equal(noFullscreen.nodes.get('fullscreen-btn').hidden, true, 'fullscreen button shown where it cannot work');
// The game keeps room for the button only where the button is shown.
assert(shell.nodes.get('canvas-container').classes.has('with-fullscreen-button'),
    'no room kept for the shown fullscreen button');
assert(!noFullscreen.nodes.get('canvas-container').classes.has('with-fullscreen-button'),
    'room kept for a hidden fullscreen button');
const shellHtml = fs.readFileSync(path.join(__dirname, '../web/shell.html'), 'utf8');
const shellCss = shellHtml.match(/<style>([\s\S]*?)<\/style>/)[1];
// Accessibility and search metadata of the standalone page.
assert.match(shellHtml, /id="status-text" role="status" aria-live="polite"/, 'status text is not a live region');
assert.match(shellCss, /@media \(prefers-reduced-motion: reduce\)\s*\{\s*\.spinner\s*\{\s*animation:\s*none/,
    'the spinner ignores prefers-reduced-motion');
assert.match(shellHtml, /<meta name="description" content="[^"]{50,}">/, 'standalone page has no description');
assert.match(shellHtml, /<meta name="robots" content="noindex">/, 'standalone page should not be indexed');
const dashboardHtml = fs.readFileSync(path.join(__dirname, '../docs/src/components/home/Dashboard.astro'), 'utf8');
assert.match(dashboardHtml, /class="cabinet-standby" role="status" aria-live="polite"/,
    'home page status text is not a live region');
const containerWidth = selector => {
    const rule = shellCss.match(new RegExp(selector + '\\s*\\{([^}]*)\\}'));
    assert(rule, `missing CSS rule ${selector}`);
    return rule[1].match(/width:\s*([^;]*);/)[1];
};
assert.match(containerWidth('#canvas-container'), /100svh - 132px/, 'hidden button still reserves its row');
assert.match(containerWidth('#canvas-container\\.with-fullscreen-button'), /100svh - 176px/,
    'shown button lost its row');
console.log('web_host_test: ok');
