/* Emscripten 6.0.9's GLFW port installs keyboard handlers on window. Scope
 * those exact handlers to the game canvas so page forms and Tab retain their
 * normal behavior. The C input owner installs/removes this scope with raylib. */
(function(root) {
    'use strict';
    function bind(view, canvas, backend) {
        const handlers = [
            ['keydown', backend.onKeydown],
            ['keypress', backend.onKeyPress],
            ['keyup', backend.onKeyup]
        ];
        for (const [type, handler] of handlers) {
            view.removeEventListener(type, handler, {capture: true});
            canvas.addEventListener(type, handler, {capture: true});
        }
        canvas.addEventListener('blur', backend.onBlur, {capture: true});
        return function clear() {
            backend.onBlur();
            for (const [type, handler] of handlers)
                canvas.removeEventListener(type, handler, {capture: true});
            canvas.removeEventListener('blur', backend.onBlur, {capture: true});
        };
    }
    const api = {bind};
    root.SuperMangoKeyboard = api;
    if (typeof module === 'object' && module.exports) module.exports = api;
})(typeof window === 'object' ? window : globalThis);
