/* Browser storage contracts, executed in isolated VMs without a browser. */
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const source = fs.readFileSync(path.join(__dirname, '../src/core/game_profile.c'), 'utf8');
const currentKey = 'super-mango-profile-v2';
const legacyKey = 'super-mango-profile-v1';

/* localStorage double: getItem/setItem/removeItem/key/length, and an
 * optional quota in characters (keys plus values), as browsers count it. */
function makeStorage(values, quota = Infinity) {
    const used = () => [...values].reduce((sum, [k, v]) => sum + k.length + v.length, 0);
    return {
        getItem: key => values.has(key) ? values.get(key) : null,
        setItem(key, value) {
            const old = values.has(key) ? key.length + values.get(key).length : 0;
            if (used() - old + key.length + value.length > quota) throw Error('QuotaExceededError');
            values.set(key, value);
        },
        removeItem: key => { values.delete(key); },
        key: index => [...values.keys()][index] ?? null,
        get length() { return values.size; }
    };
}

function environment(initial = {}, quota = Infinity) {
    const values = new Map(Object.entries(initial));
    const requests = [];
    const timers = new Map();
    let nextTimer = 0, now = 0;
    const storage = makeStorage(values, quota);
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

/* Time-trial ghosts: one localStorage entry per level, synchronous, and a
 * full or denied storage is a failed write, never an exception into C. */
function ghostStorage() {
    const ghostSource = fs.readFileSync(path.join(__dirname, '../src/core/game_ghost_file.c'), 'utf8');
    const values = new Map();
    const storage = makeStorage(values);
    const context = vm.createContext({ localStorage: storage,
        UTF8ToString: value => typeof value === 'string' ? value : value.text,
        lengthBytesUTF8: text => Buffer.byteLength(text),
        stringToUTF8: (text, out) => { out.text = text; } });
    const method = (name, args) => {
        const body = ghostSource.match(new RegExp(`EM_JS\\((?:int|void), ${name},[\\s\\S]*?\\{([\\s\\S]*?)\\n\\}\\);`))[1];
        return vm.runInContext(`(function(${args}) {${body}})`, context);
    };
    const read = method('ghost_browser_read', 'key, out, capacity');
    const write = method('ghost_browser_write', 'key, text, budget');
    const level = 'levels/01_lugio_01.toml', key = 'super-mango-ghost-v1:' + level, out = {};
    assert.equal(read(level, out, 100), 0, 'missing ghost was not reported as absent');
    assert.equal(write(level, 'format_version = 1', 1e6), 1);
    assert.equal(values.get(key), 'format_version = 1', 'ghost stored under the wrong key');
    assert.equal(read(level, out, 100), Buffer.byteLength('format_version = 1') + 1);
    assert.equal(out.text, 'format_version = 1');
    assert.equal(read(level, out, 5), -1, 'oversized ghost was accepted');
    values.set(key, 'a\0b');
    assert.equal(read(level, out, 100), -1, 'embedded NUL was accepted');
    storage.setItem = () => { throw Error('QuotaExceededError'); };
    assert.equal(write(level, 'too big', 1e6), 0, 'quota failure was not reported');
    storage.getItem = () => { throw Error('storage denied'); };
    assert.equal(read(level, out, 100), -1, 'read denial was treated as absence');
    ghostBudget();
}

/* All ghosts together stay within GHOST_WEB_BUDGET: past it, the ghost
 * written longest ago goes first, and a ghost never writes over the room a
 * profile save needs. */
function ghostBudget() {
    const ghostSource = fs.readFileSync(path.join(__dirname, '../src/core/game_ghost_file.c'), 'utf8');
    const budgetMatch = ghostSource.match(/#define GHOST_WEB_BUDGET \((\d+) \* (\d+)\)/);
    assert.ok(budgetMatch, 'GHOST_WEB_BUDGET not found');
    assert.ok(Number(budgetMatch[1]) * Number(budgetMatch[2]) <= 1024 * 1024,
              'ghost budget grew past a fifth of a typical 5M-character quota');
    const prefix = 'super-mango-ghost-v1:', order = 'super-mango-ghost-order-v1';
    const setup = (initial, quota) => {
        const values = new Map(Object.entries(initial));
        const storage = makeStorage(values, quota);
        const context = vm.createContext({ localStorage: storage,
            UTF8ToString: value => typeof value === 'string' ? value : value.text });
        const body = ghostSource.match(/EM_JS\(int, ghost_browser_write,[\s\S]*?\{([\s\S]*?)\n\}\);/)[1];
        return { values, storage, write: vm.runInContext(`(function(key, text, budget) {${body}})`, context) };
    };
    const ghost = 'g'.repeat(50);
    const size = name => (prefix + name).length + ghost.length;
    const a = 'levels/a.toml', b = 'levels/b.toml', c = 'levels/c.toml', d = 'levels/d.toml';
    const budget = 2 * size(a) + 10;  /* room for two ghosts, not three */
    const env = setup({ 'other-site': 'x'.repeat(30) });
    assert.equal(env.write(a, ghost, budget), 1);
    assert.equal(env.write(b, ghost, budget), 1);
    assert.equal(env.write(c, ghost, budget), 1);
    assert.equal(env.values.has(prefix + a), false, 'oldest ghost was not evicted');
    assert.ok(env.values.has(prefix + b) && env.values.has(prefix + c));
    assert.deepEqual(JSON.parse(env.values.get(order)), [prefix + b, prefix + c]);
    assert.equal(env.values.get('other-site'), 'x'.repeat(30), 'non-ghost entry was touched');
    /* Rewriting b makes it the newest, so d now evicts c. */
    assert.equal(env.write(b, ghost, budget), 1);
    assert.equal(env.write(d, ghost, budget), 1);
    assert.equal(env.values.has(prefix + c), false, 'rewritten ghost was evicted first');
    assert.ok(env.values.has(prefix + b) && env.values.has(prefix + d));
    /* A ghost bigger than the whole budget is refused without evicting. */
    assert.equal(env.write(a, 'g'.repeat(budget), budget), 0);
    assert.ok(env.values.has(prefix + b) && env.values.has(prefix + d));
    /* Unlisted ghosts (an older build, a lost order list) count as oldest. */
    const legacy = setup({ [prefix + a]: ghost, [prefix + b]: ghost, [order]: JSON.stringify([prefix + b]) });
    assert.equal(legacy.write(c, ghost, budget), 1);
    assert.equal(legacy.values.has(prefix + a), false, 'unlisted ghost was kept over a listed one');
    assert.ok(legacy.values.has(prefix + b));
    /* A browser quota fuller than the budget allows: evict until it fits. */
    const full = setup({ [prefix + a]: ghost, [prefix + b]: ghost, 'other-site': 'x'.repeat(100) },
                       2 * size(a) + 100 + 'other-site'.length + order.length + 2);
    assert.equal(full.write(c, ghost, 1e6), 1);
    assert.ok(full.values.has(prefix + c), 'quota full: new ghost not written');
    assert.equal(full.values.get('other-site').length, 100);
}

/* A profile save that meets a full storage deletes ghosts (oldest first)
 * until the profile fits, instead of failing to save the player's results. */
async function profileBeatsGhosts() {
    const prefix = 'super-mango-ghost-v1:', order = 'super-mango-ghost-order-v1';
    const ghost = 'g'.repeat(200);
    const initial = { [prefix + 'levels/old.toml']: ghost, [prefix + 'levels/new.toml']: ghost,
                      [order]: JSON.stringify([prefix + 'levels/old.toml', prefix + 'levels/new.toml']),
                      'other-site': 'x'.repeat(100) };
    const base = Object.entries(initial).reduce((sum, [k, v]) => sum + k.length + v.length, 0);
    const profile = 'p'.repeat(150);
    /* Room for the profile only once one ghost (and the order list) is gone. */
    const quota = base - (prefix + 'levels/old.toml').length - ghost.length + currentKey.length + profile.length;
    const env = environment(initial, quota), tab = env.context();
    assert.equal(tab.begin(profile, null), 1);
    env.run();
    await settle();
    assert.equal(tab.poll(), 2, 'profile save failed although ghosts could make room');
    assert.equal(env.values.get(currentKey), profile);
    assert.equal(env.values.has(prefix + 'levels/old.toml'), false, 'oldest ghost kept');
    assert.equal(env.values.get('other-site'), 'x'.repeat(100), 'non-ghost entry was deleted');
    /* No ghosts left to drop: the save fails as before, nothing is lost. */
    const bare = environment({ 'other-site': 'x'.repeat(100) }, 120), lone = bare.context();
    assert.equal(lone.begin(profile, null), 1);
    bare.run();
    await settle();
    assert.equal(lone.poll(), -1);
    assert.equal(bare.values.has(currentKey), false);
}
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
    ghostStorage();
    await profileBeatsGhosts();
    console.log('profile_storage_test: ok');
}
main().catch(error => { console.error(error); process.exitCode = 1; });
