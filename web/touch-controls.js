/* Shared semantic input controls. No keyboard binding assumptions or timers. */
(function(root) {
    'use strict';
    const actions = [
        ['Left', 0], ['Right', 1], ['Up', 2], ['Down', 3],
        ['Jump / confirm', 4], ['Run', 5], ['Pause / back', 6], ['Settings', 7]
    ];
    function mount(container, canvas, send) {
        const doc = container.ownerDocument;
        const view = doc.defaultView;
        if (!doc.getElementById('mango-touch-style')) {
            const style = doc.createElement('style');
            style.id = 'mango-touch-style';
            style.textContent = `
                .mango-touch-controls {display:grid;grid-template-columns:repeat(4,minmax(0,1fr));gap:6px;padding:8px;background:#151b26;max-width:800px;box-sizing:border-box}
                .mango-touch-controls button {min-height:48px;border:2px solid #8894a6;border-radius:6px;background:#243247;color:#fff;font:600 14px system-ui;touch-action:none;user-select:none;-webkit-user-select:none}
                .mango-touch-controls button[aria-pressed="true"] {background:#765900;border-color:#ffd65c}
                .mango-touch-controls button:focus-visible {outline:3px solid #ffd65c;outline-offset:2px}
                .mango-touch-controls button:disabled {opacity:.45}
                @media (max-width:420px) {.mango-touch-controls {gap:4px;padding:6px}.mango-touch-controls button {font-size:12px}}
            `;
            doc.head.appendChild(style);
        }
        const panel = doc.createElement('div');
        panel.className = 'mango-touch-controls';
        panel.setAttribute('role', 'group');
        panel.setAttribute('aria-label', 'Game touch controls');
        const listeners = [];
        const sources = new Map();
        const counts = Array(actions.length).fill(0);
        const buttons = [];
        let enabled = false, destroyed = false;
        function listen(target, event, callback) {
            target.addEventListener(event, callback);
            listeners.push([target, event, callback]);
        }
        function press(source, action, button, pointerId, keyCode) {
            if (!enabled || destroyed || sources.has(source)) return false;
            if (!counts[action] && !send(action, 1)) return false;
            sources.set(source, {action, button, pointerId, keyCode});
            counts[action]++;
            button.setAttribute('aria-pressed', 'true');
            return true;
        }
        function release(source) {
            const held = sources.get(source);
            if (!held) return;
            sources.delete(source);
            if (--counts[held.action] === 0) {
                send(held.action, 0);
                held.button.setAttribute('aria-pressed', 'false');
            }
        }
        function clear() {
            const held = [...sources.values()];
            for (const source of [...sources.keys()]) release(source);
            for (const item of held) {
                if (item.pointerId !== undefined && item.button.hasPointerCapture(item.pointerId))
                    item.button.releasePointerCapture(item.pointerId);
            }
        }
        for (const [label, action] of actions) {
            const button = doc.createElement('button');
            button.type = 'button';
            button.textContent = label;
            button.dataset.action = String(action);
            button.setAttribute('aria-label', label);
            button.setAttribute('aria-pressed', 'false');
            button.disabled = true;
            buttons.push(button);
            panel.appendChild(button);
            listen(button, 'pointerdown', event => {
                if (!enabled || sources.has('pointer:' + event.pointerId) ||
                    (event.pointerType === 'mouse' && event.button !== 0)) return;
                event.preventDefault();
                canvas.focus({preventScroll: true});
                button.setPointerCapture(event.pointerId);
                if (!press('pointer:' + event.pointerId, action, button, event.pointerId))
                    button.releasePointerCapture(event.pointerId);
            });
            for (const eventName of ['pointerup', 'pointercancel', 'lostpointercapture']) {
                listen(button, eventName, event => {
                    release('pointer:' + event.pointerId);
                    if (eventName !== 'lostpointercapture' && button.hasPointerCapture(event.pointerId))
                        button.releasePointerCapture(event.pointerId);
                });
            }
            listen(button, 'keydown', event => {
                if (!enabled || (event.code !== 'Space' && event.code !== 'Enter')) return;
                event.preventDefault();
                if (!event.repeat) {
                    canvas.focus({preventScroll: true});
                    press('key:' + action, action, button, undefined, event.code);
                }
            });
            listen(button, 'keyup', event => {
                if (event.code !== 'Space' && event.code !== 'Enter') return;
                event.preventDefault();
                release('key:' + action);
            });
            listen(button, 'blur', () => release('key:' + action));
            listen(button, 'click', event => {
                // Assistive activation can produce a click without pointer or
                // keyboard events. Pointer clicks already have a hold owner.
                if (!enabled || event.detail !== 0) return;
                event.preventDefault();
                canvas.focus({preventScroll: true});
                press('click:' + action, action, button);
                release('click:' + action);
            });
            listen(button, 'contextmenu', event => event.preventDefault());
        }
        listen(view, 'blur', clear);
        listen(view, 'keyup', event => {
            for (const [source, held] of sources)
                if (held.keyCode !== undefined && held.keyCode === event.code) release(source);
        });
        listen(canvas, 'blur', clear);
        listen(doc, 'visibilitychange', () => { if (doc.hidden) clear(); });
        container.appendChild(panel);
        return {
            clear,
            setEnabled(value) {
                if (!value) clear();
                enabled = !!value && !destroyed;
                for (const button of buttons) button.disabled = !enabled;
            },
            destroy() {
                clear();
                enabled = false;
                destroyed = true;
                for (const [target, event, callback] of listeners) target.removeEventListener(event, callback);
                panel.remove();
            }
        };
    }
    const api = {mount};
    root.SuperMangoTouch = api;
    if (typeof module === 'object' && module.exports) module.exports = api;
})(typeof window === 'object' ? window : globalThis);
