/* Browser storage contracts, executed in isolated VMs without a browser. */
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const source = fs.readFileSync(path.join(__dirname, '../src/core/game_profile.c'), 'utf8');
const currentKey = 'super-mango-profile-v2';
const legacyKey = 'super-mango-profile-v1';

function environment(initial = {}) {
    const values = new Map(Object.entries(initial));
    const requests = [];
    const timers = new Map();
    let nextTimer = 0, now = 0;
    const storage = { getItem: key => values.has(key) ? values.get(key) : null,
        setItem: (key, value) => values.set(key, value) };
    const locks = { request(name, options, callback) {
        assert.equal(name, currentKey);
        return new Promise((resolve, reject) => {
            const request = { callback, resolve, reject, cancelled: false };
            requests.push(request);
            options.signal.addEventListener('abort', () => {
                request.cancelled = true;
                reject(new Error('aborted'));
            }, { once: true });
        });
    } };
    return { values, requests, storage, locks, timers,
        tick: () => { now = 6000; },
        expire: () => { for (const callback of [...timers.values()]) callback(); },
        run() {
            while (requests.length) {
                const request = requests.shift();
                if (request.cancelled) continue;
                try { request.callback(); request.resolve(); } catch (error) { request.reject(error); }
            }
        },
        context() {
            const Module = {};
            const context = vm.createContext({ Module, localStorage: storage, navigator: { locks }, AbortController,
                Date: { now: () => now },
                UTF8ToString: value => typeof value === 'string' ? value : value.text,
                lengthBytesUTF8: text => Buffer.byteLength(text),
                stringToUTF8: (text, out) => { out.text = text; },
                setTimeout: (callback, delay) => { assert.equal(delay, 5000); timers.set(++nextTimer, callback); return nextTimer; },
                clearTimeout: id => timers.delete(id) });
            function method(name, args) {
                const body = source.match(new RegExp(`EM_JS\\((?:int|void), ${name},[\\s\\S]*?\\{([\\s\\S]*?)\\n\\}\\);`))[1];
                return vm.runInContext(`(function(${args}) {${body}})`, context);
            }
            return { Module, context,
                read: method('profile_browser_read', 'out, capacity'),
                begin: method('profile_browser_begin_write', 'text, baseline'),
                poll: method('profile_browser_poll_write', ''),
                cancel: method('profile_browser_cancel_write', '') };
        }
    };
}

const settle = () => new Promise(resolve => setImmediate(resolve));
async function main() {
    const shared = environment({ [legacyKey]: 'café' });
    const a = shared.context(), b = shared.context(), destination = {};
    assert.equal(a.read(destination, 5), -1);
    assert.equal(a.read(destination, 6), 6);
    assert.equal(destination.text, 'café');
    const bytes = { text: 'first snapshot' };
    assert.equal(a.begin(bytes, 'café'), 1);
    bytes.text = 'changed after C returned';
    assert.equal(b.begin('conflicting snapshot', 'café'), 1);
    assert.equal(a.poll(), 1, 'queued is not committed');
    shared.run();
    // A deadline firing after setItem must not turn success into failure.
    shared.expire();
    await settle();
    assert.equal(a.poll(), 2);
    assert.equal(b.poll(), -1);
    assert.equal(shared.values.get(currentKey), 'first snapshot');
    assert.equal(shared.values.get(legacyKey), 'café', 'migration changed legacy data');
    assert.equal(b.read(destination, 100), Buffer.byteLength('first snapshot') + 1);
    assert.equal(destination.text, 'first snapshot');

    for (const boundary of ['cancel', 'timeout', 'late grant', 'quota', 'unsupported']) {
        const env = environment(), tab = env.context();
        if (boundary === 'unsupported') {
            vm.runInContext('navigator.locks = undefined', tab.context);
            assert.equal(tab.begin('new', null), -1);
        } else {
            assert.equal(tab.begin('new', null), 1);
            if (boundary === 'cancel') tab.cancel();
            if (boundary === 'timeout') env.expire();
            if (boundary === 'late grant') env.tick();
            if (boundary === 'quota') env.storage.setItem = () => { throw Error('quota'); };
            env.run();
            await settle();
            assert.equal(tab.poll(), -1, boundary);
        }
        assert.equal(env.values.has(currentKey), false, boundary + ' wrote data');
    }

    const restarted = environment(), tab = restarted.context();
    tab.begin('cancelled', null);
    tab.cancel();
    tab.begin('replacement', null);
    restarted.run();
    await settle();
    assert.equal(tab.poll(), 2, 'old cancellation affected a new request');
    assert.equal(restarted.values.get(currentKey), 'replacement');
    assert.equal(restarted.timers.size, 0);
    const invalid = environment({ [currentKey]: 'valid-prefix\0hidden-suffix' });
    assert.equal(invalid.context().read({}, 100), -1, 'embedded NUL was accepted');
    invalid.storage.getItem = () => { throw Error('storage denied'); };
    assert.equal(invalid.context().read({}, 100), -1, 'read denial was treated as absence');
    console.log('profile_storage_test: ok');
}
main().catch(error => { console.error(error); process.exitCode = 1; });
