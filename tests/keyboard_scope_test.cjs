const assert = require('node:assert/strict');
const {bind} = require('../web/keyboard-scope.js');
const view = new EventTarget();
const canvas = new EventTarget();
let held = false, calls = 0, released = 0, pageKeys = 0;
const backend = {
    onKeydown() { calls++; held = true; },
    onKeyPress() { calls++; },
    onKeyup() { calls++; held = false; },
    onBlur() { released++; held = false; }
};
for (const [type, handler] of [['keydown', backend.onKeydown], ['keypress', backend.onKeyPress], ['keyup', backend.onKeyup]])
    view.addEventListener(type, handler, {capture: true});
view.addEventListener('keydown', () => pageKeys++);
const clear = bind(view, canvas, backend);
for (const type of ['keydown', 'keypress', 'keyup']) view.dispatchEvent(new Event(type));
assert.equal(calls, 0, 'page typing reached the game backend');
assert.equal(pageKeys, 1, 'page keyboard handling was intercepted');
for (const type of ['keydown', 'keypress', 'keyup']) canvas.dispatchEvent(new Event(type));
assert.equal(calls, 3);
canvas.dispatchEvent(new Event('keydown'));
assert.equal(held, true);
canvas.dispatchEvent(new Event('blur'));
assert.equal(held, false, 'canvas focus loss retained a held key');
clear();
assert.equal(released, 2);
for (const type of ['keydown', 'keypress', 'keyup', 'blur']) canvas.dispatchEvent(new Event(type));
assert.equal(calls, 4, 'shutdown retained backend keyboard handlers');
assert.equal(released, 2);
console.log('keyboard_scope_test: ok');
