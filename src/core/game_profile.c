/* Versioned player preferences/results. Persistence belongs to AppSession,
 * never to a render frame, entity, or smoke/replay run. */
#include "game_profile.h"
#include "../collectibles/coin.h"  /* MAX_COINS: the most coins a level holds */
#include "../game_constants.h"      /* MAX_CHECKPOINTS */
#include "../levels/level_ref.h"
#include "../shared/platform.h"
#include "../shared/printf_format.h"
#include "../shared/serializer_io.h"
#include "tomlc17.h"
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef __EMSCRIPTEN__
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#include <windows.h>
typedef HANDLE ProfileLock;
#define PROFILE_LOCK_INVALID INVALID_HANDLE_VALUE
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
typedef int ProfileLock;
#define PROFILE_LOCK_INVALID (-1)
#endif
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
EM_JS(int, profile_browser_read, (char *out, int capacity), {
    try {
        const current = localStorage.getItem('super-mango-profile-v2');
        const text = current === null ? localStorage.getItem('super-mango-profile-v1') : current;
        if (text === null) return 0;
        if (text.includes('\0')) return -1;
        const size = lengthBytesUTF8(text);
        if (size >= capacity) return -1;
        stringToUTF8(text, out, capacity);
        return size + 1;
    } catch (_) { return -1; }
});
EM_JS(int, profile_browser_begin_write, (const char *text, const char *baseline), {
    if (typeof navigator === 'undefined' || !navigator.locks ||
        typeof navigator.locks.request !== 'function' || typeof AbortController === 'undefined') return -1;
    if (Module.__mangoProfileWrite && Module.__mangoProfileWrite.status === 1) return -1;
    // Copy bytes now. Asynchronous work must never retain pointers into C memory.
    const value = UTF8ToString(text);
    const expected = baseline ? UTF8ToString(baseline) : null;
    const operation = { status: 1, controller: new AbortController(), deadline: Date.now() + 5000 };
    Module.__mangoProfileWrite = operation;
    const timer = setTimeout(function() {
        if (operation.status === 1) {
            operation.status = -1;
            operation.controller.abort();
        }
    }, 5000);
    try {
        navigator.locks.request('super-mango-profile-v2', { signal: operation.controller.signal }, function() {
            if (operation.status !== 1) return;
            if (Date.now() >= operation.deadline) { operation.status = -1; return; }
            const saved = localStorage.getItem('super-mango-profile-v2');
            const current = saved === null ? localStorage.getItem('super-mango-profile-v1') : saved;
            if (current !== expected) { operation.status = -1; return; }
            try { localStorage.setItem('super-mango-profile-v2', value); } catch (full) {
                // Storage is full. Results and settings matter more than any
                // time-trial ghost (game_ghost_file.c), so delete ghosts,
                // least recently written first, until the profile fits.
                const prefix = 'super-mango-ghost-v1:', orderKey = 'super-mango-ghost-order-v1';
                let order = [];
                try { order = JSON.parse(localStorage.getItem(orderKey) || '[]'); } catch (_) { order = []; }
                if (!Array.isArray(order)) order = [];
                const ghosts = [];
                for (let i = 0; i < localStorage.length; i++) {
                    const k = localStorage.key(i);
                    if (k && k.startsWith(prefix)) ghosts.push(k);
                }
                const listed = order.filter(k => ghosts.includes(k));
                const doomed = ghosts.filter(k => !listed.includes(k)).concat(listed);
                if (!doomed.length) throw full;
                localStorage.removeItem(orderKey);
                for (;;) {
                    localStorage.removeItem(doomed.shift());
                    try { localStorage.setItem('super-mango-profile-v2', value); break; } catch (again) {
                        if (!doomed.length) throw again;
                    }
                }
            }
            operation.status = 2; // confirmed commit, not merely a queued request
        }).catch(function() {
            if (operation.status === 1) operation.status = -1;
        }).finally(function() { clearTimeout(timer); });
        return 1;
    } catch (_) {
        clearTimeout(timer);
        operation.status = -1;
        return -1;
    }
});
EM_JS(int, profile_browser_poll_write, (void), {
    const operation = Module.__mangoProfileWrite;
    if (!operation) return -1;
    const status = operation.status;
    if (status !== 1) Module.__mangoProfileWrite = null;
    return status;
});
EM_JS(void, profile_browser_cancel_write, (void), {
    const operation = Module.__mangoProfileWrite;
    if (operation && operation.status === 1) {
        operation.status = -1;
        operation.controller.abort();
    }
    Module.__mangoProfileWrite = null;
});
#endif

void game_profile_init(GameProfile *profile)
{
    memset(profile, 0, sizeof(*profile));
    profile->data.settings = (GameSettings)GAME_SETTINGS_DEFAULTS;
}

void game_profile_close(GameProfile *profile)
{
#ifdef __EMSCRIPTEN__
    if (profile->pending_text) profile_browser_cancel_write();
#endif
    free(profile->pending_text);
    profile->pending_text = NULL;
    free(profile->baseline);
    profile->baseline = NULL;
}

/* Profile keys use the same levels/<name>.toml rule as next_phase and the
 * campaign manifest (level_ref.h), plus this file's fixed key capacity. */
int game_profile_key_valid(const char *path)
{
    if (!path) return 0;
    size_t size = strlen(path);
    return size < PROFILE_LEVEL_PATH && level_ref_valid(path, size);
}

static int integer(toml_datum_t value, int *out)
{
    if (value.type != TOML_INT64 || value.u.int64 < 0 || value.u.int64 > INT_MAX) return -1;
    *out = (int)value.u.int64;
    return 0;
}

static int string(toml_datum_t value, char *out, size_t capacity)
{
    if (value.type != TOML_STRING || value.u.str.len < 0 ||
        (size_t)value.u.str.len >= capacity || strlen(value.u.str.ptr) != (size_t)value.u.str.len) return -1;
    memcpy(out, value.u.str.ptr, (size_t)value.u.str.len + 1);
    return 0;
}

/* A whole number between low and high; unlike integer(), it may be negative. */
static int integer_in(toml_datum_t value, int low, int high, int *out)
{
    if (value.type != TOML_INT64 || value.u.int64 < low || value.u.int64 > high) return -1;
    *out = (int)value.u.int64;
    return 0;
}

/* Any finite number; TOML writes 900.0 as a float but a hand edit may say 900. */
static int number(toml_datum_t value, float *out)
{
    double parsed = value.type == TOML_FP64 ? value.u.fp64 :
                    value.type == TOML_INT64 ? (double)value.u.int64 : NAN;
    if (!isfinite(parsed) || parsed < -1e9 || parsed > 1e9) return -1;
    *out = (float)parsed;
    return 0;
}

/* Exactly 16 lowercase hex digits, the way encode writes a 64-bit value.
 * TOML integers are signed 64-bit, so a hash or bit mask that uses the top
 * bit could not be stored as one; a string can hold all 64 bits. */
static int hex64(toml_datum_t value, uint64_t *out)
{
    if (value.type != TOML_STRING || value.u.str.len != 16) return -1;
    uint64_t result = 0;
    for (int i = 0; i < 16; i++) {
        char c = value.u.str.ptr[i];
        int digit = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
        if (digit < 0) return -1;
        result = (result << 4) | (uint64_t)digit;
    }
    *out = result;
    return 0;
}

/*
 * resume_valid — The checks every Continue point passes, whether it is
 * decoded, encoded or recorded. They only say the numbers are sane; whether
 * they fit the level (a checkpoint it has, a coin it places) is checked when
 * the point is applied to that level (game_resume_apply).
 */
_Static_assert(MAX_COINS <= 64, "GameResume.coins holds one bit per coin in 64 bits");
static int resume_valid(const GameResume *r)
{
    if (!game_profile_key_valid(r->path) ||
        r->checkpoint < -1 || r->checkpoint >= MAX_CHECKPOINTS ||
        r->legacy_screen < 0 || r->legacy_screen > 1000 ||
        r->score < 0 || r->level_score_start < 0 || r->level_score_start > r->score ||
        r->score_life_next < 0 || r->lives < 0 ||
        !isfinite(r->respawn_x) || !isfinite(r->respawn_y) ||
        r->respawn_x < -1e6f || r->respawn_x > 1e6f || r->respawn_y < -1e6f || r->respawn_y > 1e6f ||
        !isfinite(r->elapsed) || r->elapsed < 0 || r->elapsed > 1e9f) return 0;
    /* No coin beyond the most a level can place. The mask has 64 bits and
     * MAX_COINS is 64 today, so there is no bit to refuse; the check is
     * compiled only if the limit is ever lowered. */
#if MAX_COINS < 64
    for (int i = MAX_COINS; i < 64; i++)
        if (r->coins & ((uint64_t)1 << i)) return 0;
#endif
    return 1;
}

/* The [resume] table: every field present exactly once, each in range. */
static int decode_resume(toml_datum_t table, GameResume *resume)
{
    static const char *const names[] = {"path", "level_hash", "coins", "checkpoint", "legacy_screen",
        "score", "level_score_start", "score_life_next", "lives", "respawn_x", "respawn_y", "elapsed"};
    enum { RESUME_FIELDS = sizeof(names) / sizeof(names[0]) };
    int seen = 0;
    if (table.type != TOML_TABLE || table.u.tab.size != RESUME_FIELDS) return -1;
    memset(resume, 0, sizeof(*resume));
    for (int i = 0; i < table.u.tab.size; i++) {
        const char *key = table.u.tab.key[i];
        toml_datum_t value = table.u.tab.value[i];
        int field = 0;
        if (strlen(key) != (size_t)table.u.tab.len[i]) return -1;
        while (field < RESUME_FIELDS && strcmp(key, names[field])) field++;
        int failed;
        switch (field) {
        case 0: failed = string(value, resume->path, sizeof(resume->path)); break;
        case 1: failed = hex64(value, &resume->level_hash); break;
        case 2: failed = hex64(value, &resume->coins); break;
        case 3: failed = integer_in(value, -1, MAX_CHECKPOINTS - 1, &resume->checkpoint); break;
        case 4: failed = integer(value, &resume->legacy_screen); break;
        case 5: failed = integer(value, &resume->score); break;
        case 6: failed = integer(value, &resume->level_score_start); break;
        case 7: failed = integer(value, &resume->score_life_next); break;
        case 8: failed = integer(value, &resume->lives); break;
        case 9: failed = number(value, &resume->respawn_x); break;
        case 10: failed = number(value, &resume->respawn_y); break;
        case 11: failed = number(value, &resume->elapsed); break;
        default: return -1;  /* unknown key */
        }
        if (failed) return -1;
        seen |= 1 << field;
    }
    /* size matched and every key was known, so a repeat would leave a gap. */
    return seen == (1 << RESUME_FIELDS) - 1 && resume_valid(resume) ? 0 : -1;
}

static int bindings(toml_datum_t value, GameSettings *settings, int keyboard)
{
    if (value.type != TOML_ARRAY || value.u.arr.size != PROFILE_ACTION_COUNT) return -1;
    for (int i = 0; i < PROFILE_ACTION_COUNT; i++) {
        int binding;
        if (integer(value.u.arr.elem[i], &binding)) return -1;
        if (keyboard) settings->keys[i] = binding;
        else settings->buttons[i] = binding;
    }
    return 0;
}

/* decode_result outcomes: a usable entry, a well-formed entry to drop, or a
 * malformed one that rejects the whole profile.
 *
 * A saved coin count can never exceed MAX_COINS, the most coins one level
 * can place. The file stores the plain number, so raising MAX_COINS keeps
 * every existing profile readable; lowering it would reject old profiles
 * that recorded more coins than the new limit. */
enum { RESULT_KEEP = 0, RESULT_DROP = 1, RESULT_INVALID = -1 };

/*
 * The level-reference rule (level_ref.h) became stricter after profiles were
 * already being written, so an older profile can hold a now-refused key
 * such as "levels/con.toml".  Rejecting the whole file for that would load
 * no progress and, with the profile read-only, never save again.  So an
 * entry whose only fault is its key is dropped (with a warning) and the
 * rest of the profile still loads.  Everything else stays strict.
 */
static int decode_result(toml_datum_t table, GameProgress *result)
{
    int drop = 0;
    if (table.type != TOML_TABLE || table.u.tab.size != 4) return RESULT_INVALID;
    int mask = 0;
    for (int i = 0; i < table.u.tab.size; i++) {
        const char *key = table.u.tab.key[i];
        toml_datum_t value = table.u.tab.value[i];
        if (strlen(key) != (size_t)table.u.tab.len[i]) return -1;
        if (!strcmp(key, "path")) {
            if (string(value, result->path, sizeof(result->path))) return -1;
            drop = !game_profile_key_valid(result->path);
            mask |= 1;
        } else if (!strcmp(key, "score")) {
            if (integer(value, &result->best_score)) return -1;
            mask |= 2;
        } else if (!strcmp(key, "coins")) {
            if (integer(value, &result->best_coins) || result->best_coins > MAX_COINS) return -1;
            mask |= 4;
        } else if (!strcmp(key, "time")) {
            double time = value.type == TOML_FP64 ? value.u.fp64 :
                          value.type == TOML_INT64 ? (double)value.u.int64 : -1;
            if (!isfinite(time) || time < 0 || time > 1e9) return -1;
            result->best_time = (float)time;
            mask |= 8;
        } else return -1;
    }
    if (mask != 15) return RESULT_INVALID;
    return drop ? RESULT_DROP : RESULT_KEEP;
}

/*
 * Profiles saved before the debug inspector's keys (F2-F10, '-', '=') were
 * reserved may bind one of them. Rejecting the whole file for that would
 * also throw away the player's progress, so put each such action back on
 * its default key, or every key on its default if that default is already
 * taken, and warn. Any other invalid binding still rejects the profile.
 */
static void reset_debug_reserved_keys(GameSettings *settings)
{
    static const GameSettings defaults = GAME_SETTINGS_DEFAULTS;
    int reset = 0;
    for (int i = 0; i < PROFILE_ACTION_COUNT; i++) {
        if (!game_settings_key_debug_reserved(settings->keys[i])) continue;
        settings->keys[i] = defaults.keys[i];
        reset = 1;
    }
    if (!reset) return;
    fprintf(stderr, "Warning: profile bound a debug inspector key (F2-F10, -, =); "
                    "that control is back on its default key\n");
    for (int i = 0; i < PROFILE_ACTION_COUNT; i++)
        for (int j = 0; j < i; j++)
            if (settings->keys[i] == settings->keys[j]) {
                memcpy(settings->keys, defaults.keys, sizeof(settings->keys));
                fprintf(stderr, "Warning: that default key was taken; all keys restored to defaults\n");
                return;
            }
}

int game_profile_decode(GameProfileData *out, const char *text)
{
    if (!out || !text || strlen(text) >= PROFILE_TEXT_MAX) return -1;
    toml_result_t parsed = toml_parse(text, (int)strlen(text));
    if (!parsed.ok) { toml_free(parsed); return -1; }
    GameProfileData *data = calloc(1, sizeof(*data));
    int ok = 0, version = 0, has_version2_field = 0;
    if (!data) goto done;
    data->settings = (GameSettings)GAME_SETTINGS_DEFAULTS;
    for (int i = 0; i < parsed.toptab.u.tab.size; i++) {
        const char *key = parsed.toptab.u.tab.key[i];
        toml_datum_t value = parsed.toptab.u.tab.value[i];
        if (strlen(key) != (size_t)parsed.toptab.u.tab.len[i]) goto done;
#define FIELD(name, member) if (!strcmp(key, name)) { if (integer(value, &data->settings.member)) goto done; }
        FIELD("music_volume", music_volume)
        else FIELD("effects_volume", effects_volume)
        else FIELD("muted", muted)
        else FIELD("dead_zone", dead_zone)
        else FIELD("window_scale", window_scale)
        else FIELD("high_contrast", high_contrast)
        else FIELD("reduced_motion", reduced_motion)
        else if (!strcmp(key, "ghost")) {
            if (integer(value, &data->settings.ghost)) goto done;
            has_version2_field = 1;
        }
        else if (!strcmp(key, "format_version")) {
            /* Version 1 (no Continue point, no ghost setting) still loads;
             * see game_profile.h. */
            if (integer(value, &version) || version < 1 || version > PROFILE_FORMAT_VERSION) goto done;
        }
        else if (!strcmp(key, "resume")) {
            if (decode_resume(value, &data->resume)) goto done;
            has_version2_field = 1;
        }
        else if (!strcmp(key, "keys")) { if (bindings(value, &data->settings, 1)) goto done; }
        else if (!strcmp(key, "buttons")) { if (bindings(value, &data->settings, 0)) goto done; }
        else if (!strcmp(key, "last_level")) {
            if (string(value, data->last_level, sizeof(data->last_level))) goto done;
            if (data->last_level[0] && !game_profile_key_valid(data->last_level)) {
                /* Same legacy case as a progress key: forget the selection. */
                fprintf(stderr, "Warning: profile last_level is no longer a valid "
                        "level path; ignoring it\n");
                data->last_level[0] = '\0';
            }
        } else if (!strcmp(key, "levels")) {
            if (value.type != TOML_ARRAY || value.u.arr.size > PROFILE_LEVEL_COUNT) goto done;
            data->count = 0;
            for (int j = 0; j < value.u.arr.size; j++) {
                GameProgress *entry = &data->levels[data->count];
                int outcome = decode_result(value.u.arr.elem[j], entry);
                if (outcome == RESULT_INVALID) goto done;
                if (outcome == RESULT_DROP) {
                    /* The path is not echoed: it is untrusted text that
                     * may hold terminal control characters. */
                    fprintf(stderr, "Warning: dropping profile progress for a level "
                            "path that is no longer valid\n");
                    memset(entry, 0, sizeof(*entry));
                    continue;
                }
                for (int k = 0; k < data->count; k++) if (!strcmp(data->levels[k].path, entry->path)) goto done;
                data->count++;
            }
        } else goto done;
#undef FIELD
    }
    reset_debug_reserved_keys(&data->settings);
    /* `ghost` and [resume] arrived with version 2; a version-1 file never
     * had them, so finding one there means the file is damaged. A version-1
     * file keeps the version-2 defaults: ghost on, no Continue point. */
    if (version < 1 || (version == 1 && has_version2_field) || !game_settings_valid(&data->settings)) goto done;
    *out = *data;
    ok = 1;
done:
    free(data);
    toml_free(parsed);
    return ok ? 0 : -1;
}

PRINTF_FORMAT(4, 5)
static int append(char *text, size_t capacity, size_t *used, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int size = vsnprintf(text + *used, capacity - *used, format, args);
    va_end(args);
    if (size < 0 || (size_t)size >= capacity - *used) return -1;
    *used += (size_t)size;
    return 0;
}

static int quoted(char *text, size_t capacity, size_t *used, const char *value)
{
    if (append(text, capacity, used, "\"")) return -1;
    for (; *value; value++) {
        if (*value == '"' && append(text, capacity, used, "\\")) return -1;
        if (append(text, capacity, used, "%c", *value)) return -1;
    }
    return append(text, capacity, used, "\"");
}

int game_profile_encode(const GameProfileData *data, char *text, size_t capacity)
{
    if (!data || !text || !capacity || !game_settings_valid(&data->settings) ||
        data->count < 0 || data->count > PROFILE_LEVEL_COUNT ||
        (data->last_level[0] && !game_profile_key_valid(data->last_level)) ||
        (data->resume.path[0] && !resume_valid(&data->resume))) return -1;
    size_t used = 0;
    const GameSettings *s = &data->settings;
    if (append(text, capacity, &used,
               "format_version = %d\nmusic_volume = %d\neffects_volume = %d\nmuted = %d\n"
               "dead_zone = %d\nwindow_scale = %d\nhigh_contrast = %d\nreduced_motion = %d\nghost = %d\nlast_level = ",
               PROFILE_FORMAT_VERSION,
               s->music_volume,s->effects_volume,s->muted,s->dead_zone,s->window_scale,s->high_contrast,s->reduced_motion,
               s->ghost) ||
        quoted(text, capacity, &used, data->last_level)) return -1;
    for (int kind = 0; kind < 2; kind++) {
        if (append(text, capacity, &used, "\n%s = [", kind ? "buttons" : "keys")) return -1;
        for (int i = 0; i < PROFILE_ACTION_COUNT; i++)
            if (append(text, capacity, &used, "%s%d", i ? ", " : "", kind ? (int)s->buttons[i] : (int)s->keys[i])) return -1;
        if (append(text, capacity, &used, "]\n")) return -1;
    }
    const GameResume *r = &data->resume;
    if (r->path[0]) {
        /* A table must follow the plain keys above, and comes before the
         * [[levels]] array so each part of the file stays together. */
        if (append(text, capacity, &used, "\n[resume]\npath = ") || quoted(text, capacity, &used, r->path) ||
            append(text, capacity, &used,
                   "\nlevel_hash = \"%016llx\"\ncoins = \"%016llx\"\ncheckpoint = %d\nlegacy_screen = %d\n"
                   "score = %d\nlevel_score_start = %d\nscore_life_next = %d\nlives = %d\n"
                   "respawn_x = %.9g\nrespawn_y = %.9g\nelapsed = %.9g\n",
                   (unsigned long long)r->level_hash, (unsigned long long)r->coins, r->checkpoint,
                   r->legacy_screen, r->score, r->level_score_start, r->score_life_next, r->lives,
                   (double)r->respawn_x, (double)r->respawn_y, (double)r->elapsed)) return -1;
    }
    for (int i = 0; i < data->count; i++) {
        const GameProgress *p = &data->levels[i];
        if (!game_profile_key_valid(p->path) || p->best_score < 0 || p->best_coins < 0 ||
            p->best_coins > MAX_COINS || !isfinite(p->best_time) || p->best_time < 0 || p->best_time > 1e9f) return -1;
        if (append(text, capacity, &used, "\n[[levels]]\npath = ") || quoted(text, capacity, &used, p->path) ||
            append(text, capacity, &used, "\nscore = %d\ncoins = %d\ntime = %.9g\n",
                   p->best_score, p->best_coins, (double)p->best_time)) return -1;
    }
    return 0;
}

#ifndef __EMSCRIPTEN__
/* Serialize cooperating native instances across the compare/replace window.
 * The lock file is persistent and empty; OS ownership releases on process exit. */
static ProfileLock lock_profile(const char *path)
{
    char lock_path[SERIALIZER_IO_PATH_MAX];
    int size = snprintf(lock_path, sizeof(lock_path), "%s.lock", path);
    if (size < 0 || (size_t)size >= sizeof(lock_path)) return PROFILE_LOCK_INVALID;
#ifdef _WIN32
    wchar_t *wide = serializer_utf8_to_wide(lock_path);
    if (!wide) return PROFILE_LOCK_INVALID;
    HANDLE lock = CreateFileW(wide, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    free(wide);
    return lock;
#else
    int lock = open(lock_path, O_RDWR | O_CREAT, 0600);
    if (lock >= 0 && flock(lock, LOCK_EX | LOCK_NB) != 0) { close(lock); lock = -1; }
    return lock;
#endif
}

static void unlock_profile(ProfileLock lock)
{
    if (lock == PROFILE_LOCK_INVALID) return;
#ifdef _WIN32
    CloseHandle(lock);
#else
    close(lock);
#endif
}

/* The lock file outlives every holder, so the opaque handle game_ghost.c
 * gets is only a heap copy of the OS handle. */
struct GameProfileLock {
    ProfileLock handle;
};

GameProfileLock *game_profile_lock(const GameProfile *profile)
{
    if (!profile || !profile->path[0]) return NULL;
    GameProfileLock *lock = malloc(sizeof(*lock));
    if (!lock) return NULL;
    lock->handle = lock_profile(profile->path);
    if (lock->handle == PROFILE_LOCK_INVALID) {
        free(lock);
        return NULL;
    }
    return lock;
}

void game_profile_unlock(GameProfileLock *lock)
{
    if (!lock) return;
    unlock_profile(lock->handle);
    free(lock);
}

/* 1 existing, 0 absent, -1 failed. Never mistake unreadable data for absence. */
static int read_native(const char *path, char **out)
{
    *out = NULL;
    FILE *fp = serializer_fopen_utf8(path, "rb");
    if (!fp) return errno == ENOENT ? 0 : -1;
    char *text = malloc(PROFILE_TEXT_MAX);
    if (!text) { fclose(fp); return -1; }
    size_t size = fread(text, 1, PROFILE_TEXT_MAX, fp);
    int failed = ferror(fp) || size >= PROFILE_TEXT_MAX || memchr(text, '\0', size) != NULL;
    if (fclose(fp) != 0) failed = 1;
    if (failed) { free(text); return -1; }
    text[size] = '\0';
    *out = text;
    return 1;
}
#endif

int game_profile_open(GameProfile *profile, const char *path)
{
    game_profile_close(profile);
    profile->error = 0;
    profile->writable = 0;
    profile->enabled = 1;
    int found;
#ifdef __EMSCRIPTEN__
    (void)path;
    profile->baseline = malloc(PROFILE_TEXT_MAX);
    if (!profile->baseline) found = -1;
    else found = profile_browser_read(profile->baseline, PROFILE_TEXT_MAX);
    if (found == 0) game_profile_close(profile);
#else
    char *base = path ? NULL : preference_path("SuperMango", "SuperMango");
    int size = (path || base) ? snprintf(profile->path, sizeof(profile->path), "%s%s", path ? path : base,
                                        path ? "" : "profile.toml") : -1;
    free(base);
    found = size < 0 || (size_t)size >= sizeof(profile->path) ? -1 : read_native(profile->path, &profile->baseline);
#endif
    if (found < 0 || (found > 0 && game_profile_decode(&profile->data, profile->baseline))) {
        snprintf(profile->status, sizeof(profile->status), "Profile unreadable/invalid; original preserved. Using this run only.");
        profile->writable = 0;
        profile->error = 1;
        return -1;
    }
    profile->writable = 1;
    return 0;
}

int game_profile_save(GameProfile *profile)
{
    if (profile->pending_text) return PROFILE_SAVE_PENDING;
    if (!profile->enabled) return 0;
    if (!profile->writable) return -1;
    char *text = malloc(PROFILE_TEXT_MAX);
    if (!text) {
        profile->error = 1;
        snprintf(profile->status, sizeof(profile->status), "Profile save failed: out of memory. Existing data preserved.");
        return -1;
    }
    int result = game_profile_encode(&profile->data, text, PROFILE_TEXT_MAX);
    profile->pending_text = text;
    profile->pending_revision = profile->revision;
#ifdef __EMSCRIPTEN__
    if (result) return game_profile_finish_save(profile, PROFILE_SAVE_ERROR);
    if (profile_browser_begin_write(text, profile->baseline) < 0) {
        game_profile_finish_save(profile, PROFILE_SAVE_ERROR);
        profile->writable = 0;
        snprintf(profile->status, sizeof(profile->status), "Browser saving requires Web Locks in a secure supported browser.");
        return PROFILE_SAVE_ERROR;
    }
    snprintf(profile->status, sizeof(profile->status), "Saving profile...");
    return PROFILE_SAVE_PENDING;
#else
    char temporary[SERIALIZER_IO_PATH_MAX] = "";
    ProfileLock lock = result ? PROFILE_LOCK_INVALID : lock_profile(profile->path);
    if (lock == PROFILE_LOCK_INVALID) result = -1;
    FILE *fp = result ? NULL : serializer_open_temp(profile->path, temporary, sizeof(temporary));
    if (!fp) result = -1;
    if (fp) {
        if (fputs(text, fp) == EOF || serializer_stream_has_error(fp) || serializer_flush(fp)) result = -1;
        if (fclose(fp)) result = -1;
    }
    int installing = 0;  /* 1 once serializer_install_temp owns the temp file */
    if (!result) {
        char *current = NULL;
        int found = read_native(profile->path, &current);
        if (found < 0 || (profile->baseline ? found != 1 || strcmp(profile->baseline, current) : found != 0)) result = -1;
        free(current);
        if (!result) {
            /* Replace an existing profile; create a first one only while no
             * other instance has created it meanwhile. */
            installing = 1;
            result = serializer_install_temp(temporary, profile->path, profile->baseline == NULL);
        }
    }
    /* Before installing, a failure leaves an unused temp file to delete;
     * serializer_install_temp has already cleaned up after itself. */
    if (!installing) serializer_remove_temp(temporary);
    unlock_profile(lock);
    if (result == SERIALIZER_REPLACE_TEMP_KEPT) {
        /*
         * Windows moved the old profile away but could not move the new one
         * in, so the temp file may now be the only copy of the player's
         * progress. It stays on disk; stop saving for this run so nothing
         * writes over it, and say where it is. Renaming that file to
         * profile.toml restores the profile.
         */
        game_profile_finish_save(profile, PROFILE_SAVE_ERROR);
        profile->writable = 0;
        fprintf(stderr, "Profile save incomplete; the profile is kept in '%s'\n", temporary);
        /* Two copies rather than one snprintf: a long path is cut to fit
         * the status line on purpose (stderr above has all of it). */
        str_copy(profile->status, "Profile save incomplete; your profile is safe in ", sizeof(profile->status));
        size_t used = strlen(profile->status);
        str_copy(profile->status + used, temporary, sizeof(profile->status) - used);
        return PROFILE_SAVE_ERROR;
    }
    return game_profile_finish_save(profile, result);
#endif
}

int game_profile_finish_save(GameProfile *profile, int result)
{
    if (!profile->pending_text) return PROFILE_SAVE_ERROR;
    char *text = profile->pending_text;
    profile->pending_text = NULL;
    if (result != PROFILE_SAVE_OK) {
        free(text);
        profile->error = 1;
        snprintf(profile->status, sizeof(profile->status), "Profile save failed/changed elsewhere; existing file preserved.");
        return -1;
    }
    free(profile->baseline);
    profile->baseline = text;
    profile->dirty = profile->revision != profile->pending_revision;
    profile->error = 0;
    profile->status[0] = '\0';
    return 0;
}

int game_profile_poll(GameProfile *profile)
{
    if (!profile->pending_text) return PROFILE_SAVE_OK;
#ifdef __EMSCRIPTEN__
    int status = profile_browser_poll_write();
    if (status != 1)
        return game_profile_finish_save(profile, status == 2 ? PROFILE_SAVE_OK : PROFILE_SAVE_ERROR);
#endif
    return PROFILE_SAVE_PENDING;
}

void game_profile_select(GameProfile *profile, const char *key)
{
    if (!game_profile_key_valid(key) || !strcmp(profile->data.last_level, key)) return;
    str_copy(profile->data.last_level, key, sizeof(profile->data.last_level));
    profile->dirty = 1;
    profile->revision++;
}

const GameProgress *game_profile_result(const GameProfile *profile, const char *key)
{
    if (!profile) return NULL;
    for (int i = 0; i < profile->data.count; i++) if (!strcmp(profile->data.levels[i].path, key)) return &profile->data.levels[i];
    return NULL;
}

int game_profile_record(GameProfile *profile, const char *key, int score, int coins, float elapsed)
{
    if (!game_profile_key_valid(key) || score < 0 || coins < 0 || coins > MAX_COINS ||
        !isfinite(elapsed) || elapsed < 0 || elapsed > 1e9f) return -1;
    int index = 0;
    while (index < profile->data.count && strcmp(profile->data.levels[index].path, key)) index++;
    if (index == PROFILE_LEVEL_COUNT) return -1;
    GameProgress *result = &profile->data.levels[index];
    if (index == profile->data.count) {
        str_copy(result->path, key, sizeof(result->path));
        result->best_time = elapsed;
        profile->data.count++;
    }
    if (score > result->best_score) result->best_score = score;
    if (coins > result->best_coins) result->best_coins = coins;
    if (elapsed < result->best_time) result->best_time = elapsed;
    profile->dirty = 1;
    profile->revision++;
    return 0;
}

/* Field by field: struct padding makes memcmp unreliable for this. */
static int resume_equal(const GameResume *a, const GameResume *b)
{
    return !strcmp(a->path, b->path) && a->level_hash == b->level_hash && a->coins == b->coins &&
           a->checkpoint == b->checkpoint && a->legacy_screen == b->legacy_screen &&
           a->score == b->score && a->level_score_start == b->level_score_start &&
           a->score_life_next == b->score_life_next && a->lives == b->lives &&
           a->respawn_x == b->respawn_x && a->respawn_y == b->respawn_y && a->elapsed == b->elapsed;
}

int game_profile_set_resume(GameProfile *profile, const GameResume *resume)
{
    if (!profile || !resume || !resume_valid(resume)) return -1;
    if (resume_equal(&profile->data.resume, resume)) return 0;
    profile->data.resume = *resume;
    profile->dirty = 1;
    profile->revision++;
    return 0;
}

void game_profile_clear_resume(GameProfile *profile)
{
    if (!profile || !profile->data.resume.path[0]) return;
    memset(&profile->data.resume, 0, sizeof(profile->data.resume));
    profile->dirty = 1;
    profile->revision++;
}

const GameResume *game_profile_resume(const GameProfile *profile, const char *key)
{
    if (!profile || !key || !profile->data.resume.path[0] || strcmp(profile->data.resume.path, key)) return NULL;
    return &profile->data.resume;
}
