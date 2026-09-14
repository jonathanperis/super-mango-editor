const assert = require('node:assert/strict');
const controls = require('../web/touch-controls.js');

function environment() {
    const nodes = [];
    let document;
    function element() {
        const listeners = new Map(), captures = new Set(), attributes = new Map();
        const node = { children: [], dataset: {}, ownerDocument: document,
            addEventListener(type, listener) {
                if (!listeners.has(type)) listeners.set(type, new Set());
                listeners.get(type).add(listener);
            },
            removeEventListener(type, listener) { listeners.get(type)?.delete(listener); },
            emit(type, data = {}) {
                const event = { button: 0, pointerType: 'touch', preventDefault() { this.prevented = true; }, ...data };
                if (type === 'lostpointercapture') captures.delete(event.pointerId);
                for (const listener of [...(listeners.get(type) || [])]) listener(event);
                return event;
            },
            appendChild(child) { this.children.push(child); child.parent = this; },
            remove() { if (this.parent) this.parent.children = this.parent.children.filter(child => child !== this); },
            setAttribute(key, value) { attributes.set(key, value); },
            getAttribute(key) { return attributes.get(key); },
            setPointerCapture(id) { captures.add(id); },
            hasPointerCapture(id) { return captures.has(id); },
            releasePointerCapture(id) { captures.delete(id); this.emit('lostpointercapture', {pointerId: id}); },
            focus() {
                if (document.activeElement !== this) document.activeElement?.emit('blur');
                document.activeElement = this;
            },
            listenerCount() { return [...listeners.values()].reduce((sum, group) => sum + group.size, 0); }
        };
        nodes.push(node);
        return node;
    }
    const view = element();
    document = element();
    document.defaultView = view;
    document.createElement = element;
    document.getElementById = id => nodes.find(node => node.id === id);
    document.head = element();
    const container = element(), canvas = element(), sent = [];
    const ui = controls.mount(container, canvas, (action, pressed) => { sent.push([action, pressed]); return 1; });
    return { document, view, container, canvas, sent, ui, buttons: container.children[0].children };
}

const env = environment();
const [left, right, up, down, jump, run, pause] = env.buttons;
right.emit('pointerdown', {pointerId: 1});
assert.equal(env.sent.length, 0, 'input accepted before startup');
env.ui.setEnabled(true);
right.emit('pointerdown', {pointerId: 1});
right.emit('pointerdown', {pointerId: 1});
jump.emit('pointerdown', {pointerId: 2});
assert.deepEqual(env.sent, [[1,1],[4,1]]);
assert.equal(right.hasPointerCapture(1), true);
right.emit('pointerleave', {pointerId: 1});
assert.equal(env.sent.length, 2, 'leaving a captured button released too early');
jump.emit('pointercancel', {pointerId: 2});
right.emit('pointerup', {pointerId: 1, clientX: -100});
assert.deepEqual(env.sent.slice(-2), [[4,0],[1,0]]);

env.sent.length = 0;
left.emit('pointerdown', {pointerId: 3});
left.emit('pointerdown', {pointerId: 4});
left.emit('pointerup', {pointerId: 3});
assert.deepEqual(env.sent, [[0,1]]);
assert.equal(left.getAttribute('aria-pressed'), 'true');
left.emit('lostpointercapture', {pointerId: 4});
assert.deepEqual(env.sent, [[0,1],[0,0]]);
assert.equal(left.getAttribute('aria-pressed'), 'false');

for (const boundary of ['blur', 'hidden', 'route', 'disabled']) {
    env.ui.setEnabled(true);
    up.emit('pointerdown', {pointerId: 5});
    run.emit('pointerdown', {pointerId: 6});
    if (boundary === 'blur') env.view.emit('blur');
    if (boundary === 'hidden') { env.document.hidden = true; env.document.emit('visibilitychange'); env.document.hidden = false; }
    if (boundary === 'route') env.ui.clear();
    if (boundary === 'disabled') env.ui.setEnabled(false);
    assert.deepEqual(env.sent.slice(-2), [[2,0],[5,0]], boundary);
}

env.ui.setEnabled(true);
env.sent.length = 0;
jump.focus();
jump.emit('keydown', {code: 'Space', repeat: false});
assert.equal(env.document.activeElement, env.canvas);
env.view.emit('keyup', {code: 'Space'});
assert.deepEqual(env.sent, [[4,1],[4,0]], 'keyboard activation got stuck after focus moved');
pause.emit('click', {detail: 0});
assert.deepEqual(env.sent.slice(-2), [[6,1],[6,0]], 'assistive click did not activate');
pause.emit('click', {detail: 1});
assert.equal(env.sent.length, 4, 'pointer click double-fired');

down.emit('pointerdown', {pointerId: 7});
env.ui.destroy();
assert.deepEqual(env.sent.at(-1), [3,0]);
assert.equal(env.container.children.length, 0);
assert.equal(env.view.listenerCount(), 0);
assert.equal(env.document.listenerCount(), 0);
assert.equal(env.canvas.listenerCount(), 0);
const count = env.sent.length;
down.emit('pointerdown', {pointerId: 8});
assert.equal(env.sent.length, count);
env.ui.destroy();
console.log('touch_controls_test: ok');
