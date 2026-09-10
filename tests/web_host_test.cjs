/* Pure host-contract tests using Node's VM. No browser or network is used. */
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

function host(file, pattern) {
    const nodes = new Map();
    const listeners = new Map();
    let reloads = 0;
    function element(id = '') {
        const handlers = new Map();
        const node = { id, style: {}, dataset: {}, children: [], disabled: false,
            textContent: '', handlers,
            addEventListener(name, fn) { handlers.set(name, fn); },
            removeEventListener(name) { handlers.delete(name); },
            appendChild(child) { this.children.push(child); child.parent = this; },
            remove() { this.parent.children = this.parent.children.filter(child => child !== this); },
            focus() { document.activeElement = this; },
            scrollIntoView() {},
            querySelector(selector) { return nodes.get(selector); },
            getAttribute(name) { return name === 'data-start-game' ? (id === 'debug-btn' ? 'debug' : 'play') : null; },
        };
        Object.defineProperty(node, 'innerHTML', {
            set() { this.children = []; }, get() { return ''; },
        });
        return node;
    }
    for (const id of ['canvas', 'game-status', 'play-btn', 'debug-btn', 'play',
                       'status', '.cabinet-standby', '.cabinet-standby-note']) nodes.set(id, element(id));
    const status = nodes.get('game-status');
    for (const id of ['.cabinet-standby', '.cabinet-standby-note', 'play-btn', 'debug-btn']) status.appendChild(nodes.get(id));
    const document = {
        activeElement: null, body: element('body'),
        getElementById(id) { return nodes.get(id); },
        createElement() { return element(); },
        querySelectorAll() { return [nodes.get('play-btn'), nodes.get('debug-btn')]; },
        addEventListener(name, fn) { listeners.set(name, fn); },
        removeEventListener(name) { listeners.delete(name); },
    };
    const window = {
        sessionStorage: { getItem() { return null; }, removeItem() {} },
        setTimeout(fn) { fn(); }, location: { reload() { reloads++; } },
    };
    const context = vm.createContext({ document, window, sessionStorage: window.sessionStorage,
        setTimeout: window.setTimeout, console: { log() {}, error() {} } });
    const source = fs.readFileSync(path.join(__dirname, '..', file), 'utf8').match(pattern)[1];
    vm.runInContext(source, context);
    return { nodes, document, window, listeners, context, reloads: () => reloads };
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
page.listeners.get('keydown')({code:'Tab',preventDefault(){prevented=true;}});
page.listeners.get('keydown')({code:'KeyL',ctrlKey:true,preventDefault(){prevented=true;}});
assert.equal(prevented,false,'browser navigation was trapped');
page.document.body.children[0].onerror();
assert.equal(page.nodes.get('play-btn').disabled, false);
assert.equal(page.nodes.get('play-btn').style.display, 'inline-block');
assert.equal(page.listeners.has('keydown'), false);
vm.runInContext('startGame(false)', page.context);
let calls = 0;
page.window.Module.callMain = () => { calls++; return 0; };
page.window.Module.onRuntimeInitialized();
page.window.Module.onRuntimeInitialized();
assert.equal(calls, 1);
page.window.Module.onGameEnded(0);
vm.runInContext('startGame(false)', page.context);
assert.equal(page.reloads(), 1, 'ended runtime should restart via reload');

const shell = host('web/shell.html', /<script>([\s\S]*?)<\/script>/);
let args;
shell.window.Module.__superMangoDebug = true;
shell.window.Module.callMain = value => { args = value; return 0; };
shell.window.Module.onRuntimeInitialized();
assert(args.includes('--debug'), 'standalone debug payload omitted debug argument');
shell.window.Module.onAbort();
assert.equal(shell.nodes.get('status').style.pointerEvents, 'auto');
shell.nodes.get('status').children[0].onclick();
assert.equal(shell.reloads(), 1);
console.log('web_host_test: ok');

/* Exercise the C/JS storage bridge as pure JavaScript, with no browser state. */
const profileSource = fs.readFileSync(path.join(__dirname, '../src/core/game_profile.c'), 'utf8');
let saved = null;
const storage = { getItem: () => saved, setItem: (_, text) => { saved = text; } };
const bridge = vm.createContext({ localStorage: storage,
    UTF8ToString: text => text, lengthBytesUTF8: text => Buffer.byteLength(text),
    stringToUTF8: (text, out) => { out.text = text; } });
const readBody = profileSource.match(/EM_JS\(int, profile_browser_read,[\s\S]*?\{([\s\S]*?)\n\}\);/)[1];
const writeBody = profileSource.match(/EM_JS\(int, profile_browser_write,[\s\S]*?\{([\s\S]*?)\n\}\);/)[1];
const readProfile = vm.runInContext(`(function(out, capacity) {${readBody}})`, bridge);
const writeProfile = vm.runInContext(`(function(text, baseline) {${writeBody}})`, bridge);
const destination = {};
assert.equal(readProfile(destination, 100), 0);
assert.equal(writeProfile('café', null), 1);
assert.equal(readProfile(destination, 5), -1);
assert.equal(readProfile(destination, 6), 6);
assert.equal(destination.text, 'café');
assert.equal(writeProfile('stale', null), 0);
assert.equal(saved, 'café');
assert.equal(writeProfile('next', 'café'), 1);
storage.setItem = () => { throw Error('storage denied'); };
assert.equal(writeProfile('lost', 'next'), 0);
assert.equal(saved, 'next');
console.log('profile storage bridge: ok');
