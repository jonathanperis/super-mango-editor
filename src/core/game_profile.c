/* Versioned player preferences/results. Persistence belongs to AppSession,
 * never to a render frame, entity, or smoke/replay run. */
#include "game_profile.h"
#include "../shared/platform.h"
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
            localStorage.setItem('super-mango-profile-v2', value);
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

int game_profile_key_valid(const char *path)
{
    if (!path) return 0;
    size_t size = strlen(path);
    if (size < 13 || size >= PROFILE_LEVEL_PATH || strncmp(path, "levels/", 7) ||
        strcmp(path + size - 5, ".toml")) return 0;
    for (size_t i = 7; i < size; i++) {
        unsigned char c = (unsigned char)path[i];
        if (c < 32 || c == 127 || c == '/' || c == '\\' || c == ':') return 0;
    }
    return 1;
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

static int decode_result(toml_datum_t table, GameProgress *result)
{
    if (table.type != TOML_TABLE || table.u.tab.size != 4) return -1;
    int mask = 0;
    for (int i = 0; i < table.u.tab.size; i++) {
        const char *key = table.u.tab.key[i];
        toml_datum_t value = table.u.tab.value[i];
        if (strlen(key) != (size_t)table.u.tab.len[i]) return -1;
        if (!strcmp(key, "path")) {
            if (string(value, result->path, sizeof(result->path)) || !game_profile_key_valid(result->path)) return -1;
            mask |= 1;
        } else if (!strcmp(key, "score")) {
            if (integer(value, &result->best_score)) return -1;
            mask |= 2;
        } else if (!strcmp(key, "coins")) {
            if (integer(value, &result->best_coins) || result->best_coins > 64) return -1;
            mask |= 4;
        } else if (!strcmp(key, "time")) {
            double time = value.type == TOML_FP64 ? value.u.fp64 :
                          value.type == TOML_INT64 ? (double)value.u.int64 : -1;
            if (!isfinite(time) || time < 0 || time > 1e9) return -1;
            result->best_time = (float)time;
            mask |= 8;
        } else return -1;
    }
    return mask == 15 ? 0 : -1;
}

int game_profile_decode(GameProfileData *out, const char *text)
{
    if (!out || !text || strlen(text) >= PROFILE_TEXT_MAX) return -1;
    toml_result_t parsed = toml_parse(text, (int)strlen(text));
    if (!parsed.ok) { toml_free(parsed); return -1; }
    GameProfileData *data = calloc(1, sizeof(*data));
    int ok = 0, version = 0;
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
        else if (!strcmp(key, "format_version")) { if (integer(value, &version) || version != 1) goto done; }
        else if (!strcmp(key, "keys")) { if (bindings(value, &data->settings, 1)) goto done; }
        else if (!strcmp(key, "buttons")) { if (bindings(value, &data->settings, 0)) goto done; }
        else if (!strcmp(key, "last_level")) {
            if (string(value, data->last_level, sizeof(data->last_level)) ||
                (data->last_level[0] && !game_profile_key_valid(data->last_level))) goto done;
        } else if (!strcmp(key, "levels")) {
            if (value.type != TOML_ARRAY || value.u.arr.size > PROFILE_LEVEL_COUNT) goto done;
            data->count = value.u.arr.size;
            for (int j = 0; j < data->count; j++) {
                if (decode_result(value.u.arr.elem[j], &data->levels[j])) goto done;
                for (int k = 0; k < j; k++) if (!strcmp(data->levels[k].path, data->levels[j].path)) goto done;
            }
        } else goto done;
#undef FIELD
    }
    if (version != 1 || !game_settings_valid(&data->settings)) goto done;
    *out = *data;
    ok = 1;
done:
    free(data);
    toml_free(parsed);
    return ok ? 0 : -1;
}

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
        (data->last_level[0] && !game_profile_key_valid(data->last_level))) return -1;
    size_t used = 0;
    const GameSettings *s = &data->settings;
    if (append(text, capacity, &used,
               "format_version = 1\nmusic_volume = %d\neffects_volume = %d\nmuted = %d\n"
               "dead_zone = %d\nwindow_scale = %d\nhigh_contrast = %d\nreduced_motion = %d\nlast_level = ",
               s->music_volume,s->effects_volume,s->muted,s->dead_zone,s->window_scale,s->high_contrast,s->reduced_motion) ||
        quoted(text, capacity, &used, data->last_level)) return -1;
    for (int kind = 0; kind < 2; kind++) {
        if (append(text, capacity, &used, "\n%s = [", kind ? "buttons" : "keys")) return -1;
        for (int i = 0; i < PROFILE_ACTION_COUNT; i++)
            if (append(text, capacity, &used, "%s%d", i ? ", " : "", kind ? (int)s->buttons[i] : (int)s->keys[i])) return -1;
        if (append(text, capacity, &used, "]\n")) return -1;
    }
    for (int i = 0; i < data->count; i++) {
        const GameProgress *p = &data->levels[i];
        if (!game_profile_key_valid(p->path) || p->best_score < 0 || p->best_coins < 0 ||
            p->best_coins > 64 || !isfinite(p->best_time) || p->best_time < 0 || p->best_time > 1e9f) return -1;
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
    if (!result) {
        char *current = NULL;
        int found = read_native(profile->path, &current);
        if (found < 0 || (profile->baseline ? found != 1 || strcmp(profile->baseline, current) : found != 0)) result = -1;
        free(current);
        if (!result) result = profile->baseline ? serializer_replace_file(temporary, profile->path)
                                               : serializer_create_file(temporary, profile->path);
    }
    serializer_remove_temp(temporary);
    unlock_profile(lock);
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
    if (!game_profile_key_valid(key) || score < 0 || coins < 0 || coins > 64 ||
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
