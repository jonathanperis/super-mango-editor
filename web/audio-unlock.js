/* Browsers keep an AudioContext suspended until the player interacts with
 * the page. raylib's miniaudio backend resumes its contexts only on 'click'
 * and 'touchend', so a player who starts the game from the keyboard heard
 * nothing until they clicked. This listener also resumes on a key press or a
 * mouse button press. It never calls miniaudio.unlock(): that removes
 * miniaudio's own listeners on its first call, even when no context could
 * resume yet. It only resumes contexts of devices miniaudio has started,
 * and stays installed until every one of them is running. */
(function(root) {
    'use strict';
    const EVENTS = ['keydown', 'pointerdown'];

    // The started devices' AudioContexts, or null before raylib has one.
    function startedContexts(miniaudio) {
        if (!miniaudio || !Array.isArray(miniaudio.devices) || !miniaudio.device_state) return null;
        const contexts = miniaudio.devices
            .filter(device => device && device.webaudio &&
                    device.state === miniaudio.device_state.started)
            .map(device => device.webaudio);
        return contexts.length ? contexts : null;
    }

    function install(view) {
        function remove() {
            for (const type of EVENTS) view.removeEventListener(type, onInput, true);
        }
        function finishIfRunning() {
            const contexts = startedContexts(view.miniaudio);
            if (contexts && contexts.every(context => context.state === 'running')) remove();
        }
        function onInput(event) {
            // Escape is not a user activation, so it cannot resume audio.
            if (event.type === 'keydown' && event.key === 'Escape') return;
            const contexts = startedContexts(view.miniaudio);
            if (!contexts) return;
            for (const context of contexts) {
                if (context.state === 'running') continue;
                context.resume().then(finishIfRunning, () => {});
            }
            finishIfRunning();
        }
        for (const type of EVENTS) view.addEventListener(type, onInput, true);
        return remove;
    }

    const api = {install};
    root.SuperMangoAudioUnlock = api;
    if (typeof module === 'object' && module.exports) module.exports = api;
    else if (typeof window === 'object') install(window);
})(typeof window === 'object' ? window : globalThis);
