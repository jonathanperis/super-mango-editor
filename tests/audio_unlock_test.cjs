const assert = require('node:assert/strict');
const {install} = require('../web/audio-unlock.js');

const STOPPED = 2, STARTED = 3;
function context(state) {
    return {
        state, resumes: 0,
        resume() {
            this.resumes++;
            if (this.allow) this.state = 'running';
            return Promise.resolve();
        }
    };
}
function key(view, keyName) {
    const event = new Event('keydown');
    event.key = keyName;
    view.dispatchEvent(event);
}
const tick = () => new Promise(resolve => setImmediate(resolve));

(async () => {
    const view = new EventTarget();
    let miniaudioListeners = 0;
    install(view);

    // Before raylib opens its audio device there is nothing to resume.
    key(view, 'a');

    const playback = context('suspended'), stopped = context('suspended');
    view.miniaudio = {
        device_state: {stopped: STOPPED, started: STARTED},
        devices: [null, {webaudio: stopped, state: STOPPED}, {webaudio: playback, state: STARTED}],
        unlock() { miniaudioListeners++; }
    };
    key(view, 'Escape');
    assert.equal(playback.resumes, 0, 'Escape is not a user activation');

    // A key press without activation leaves the context suspended; the
    // listener must stay for the next input instead of giving up.
    key(view, 'ArrowRight');
    await tick();
    assert.equal(playback.resumes, 1);
    assert.equal(stopped.resumes, 0, 'resumed a device miniaudio had not started');

    playback.allow = true;
    view.dispatchEvent(new Event('pointerdown'));
    await tick();
    assert.equal(playback.resumes, 2);
    assert.equal(playback.state, 'running');

    key(view, 'a');
    view.dispatchEvent(new Event('pointerdown'));
    await tick();
    assert.equal(playback.resumes, 2, 'listener stayed installed after audio ran');
    assert.equal(miniaudioListeners, 0, 'called miniaudio.unlock, which drops its own listeners');
    console.log('audio_unlock_test: ok');
})().catch(error => { console.error(error); process.exit(1); });
