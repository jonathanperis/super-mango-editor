/*
 * game_ghost_file.c — The ghost run's text format, and where it is kept.
 *
 * A ghost is TOML, like levels and the profile, so a curious player can
 * open one in a text editor:
 *
 *   format_version = 1
 *   level = "levels/01_lugio_01.toml"
 *   level_hash = "0123456789abcdef"     # the level file it was run on
 *   time = 45.25                        # completion time, seconds
 *   steps = 2715                        # samples, one per 1/60 s step
 *   frames = [
 *     "0450011c000450011c00...",        # one second (60 samples) per line
 *   ]
 *
 * Each sample is 10 hex digits: x + 1024 (4 digits), y + 1024 (4 digits)
 * and the sprite cell (2 digits); see GhostSample in game_ghost.h.
 *
 * The stored text is untrusted (a hand edit, another program, a damaged
 * disk), so decoding is strict and rejects the whole ghost on any fault.
 * level_hash ties a ghost to one version of its level: the session ignores
 * a ghost whose hash differs from the loaded level's, because a ghost
 * running through walls that moved would be misleading.
 *
 * Storage: natively a file next to the profile (game_ghost_file_path),
 * written through a temporary file like the profile itself; in a browser a
 * localStorage entry per level. Ghosts are not shared between profiles.
 */
#include "game_ghost.h"
#include "../game.h"  /* GameState: this file reads its fields */

#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game_timing.h"  /* GAME_FIXED_STEP */
#include "tomlc17.h"
#include "../shared/platform.h"       /* str_copy */
#include "../shared/printf_format.h"
#include "../shared/serializer_io.h"

/* Samples per line of the frames array: one second of play. */
#define GHOST_SAMPLES_PER_LINE TARGET_FPS
/* Hex digits per sample: 4 for x, 4 for y, 2 for the cell. */
#define GHOST_SAMPLE_DIGITS 10
/* A ghost's time can be at most its steps plus a little rounding. */
#define GHOST_TIME_MAX ((float)GHOST_MAX_STEPS / TARGET_FPS + 1.0f)

/*
 * ghost_time_matches_steps — Is `time` the time a run of `steps` steps
 * shows?
 *
 * The session keeps the run with the smaller time, so a time that does not
 * belong to its samples (a hand edit to `time = 0`) would make a ghost no
 * real run could ever beat. The level timer starts at 0 and adds
 * GAME_FIXED_STEP once per step, in float (game_update.c); after 18000
 * steps that sum is about 0.03 s short of steps / 60, more than one step.
 * So repeat the same float sum here, and accept a time within one step of
 * it.
 */
static int ghost_time_matches_steps(double time, int steps)
{
    float expected = 0.0f;
    for (int i = 0; i < steps; i++) expected += GAME_FIXED_STEP;
    return fabs(time - (double)expected) <= (double)GAME_FIXED_STEP;
}

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
/* Browser storage: one localStorage entry per level, keyed by its profile
 * key. Values are copied to and from C memory right away; JavaScript keeps
 * no pointer into it. A full or denied storage makes the write return 0. */
EM_JS(int, ghost_browser_read, (const char *key, char *out, int capacity), {
    try {
        const text = localStorage.getItem('super-mango-ghost-v1:' + UTF8ToString(key));
        if (text === null) return 0;
        if (text.includes('\0')) return -1;
        const size = lengthBytesUTF8(text);
        if (size >= capacity) return -1;
        stringToUTF8(text, out, capacity);
        return size + 1;
    } catch (_) { return -1; }
});
EM_JS(int, ghost_browser_write, (const char *key, const char *text), {
    try {
        localStorage.setItem('super-mango-ghost-v1:' + UTF8ToString(key), UTF8ToString(text));
        return 1;
    } catch (_) { return 0; }
});
#endif

/* ---- Decoding ------------------------------------------------------ */

/* Value of one hex digit, or -1. Lowercase only, as encode writes it. */
static int hex_digit(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

/* Read `digits` hex digits from text into *out; -1 on a bad digit. */
static int hex_value(const char *text, int digits, uint64_t *out)
{
    uint64_t value = 0;
    for (int i = 0; i < digits; i++) {
        int digit = hex_digit(text[i]);
        if (digit < 0) return -1;
        value = (value << 4) | (uint64_t)digit;
    }
    *out = value;
    return 0;
}

/* Decode the frames array into out->samples (already sized to `steps`). */
static int decode_frames(toml_datum_t frames, GameGhostTrack *out, int steps)
{
    int count = 0;
    if (frames.type != TOML_ARRAY) return -1;
    for (int line = 0; line < frames.u.arr.size; line++) {
        toml_datum_t text = frames.u.arr.elem[line];
        if (text.type != TOML_STRING || text.u.str.len <= 0 ||
            text.u.str.len % GHOST_SAMPLE_DIGITS != 0 ||
            text.u.str.len > GHOST_SAMPLES_PER_LINE * GHOST_SAMPLE_DIGITS) return -1;
        for (int at = 0; at < text.u.str.len; at += GHOST_SAMPLE_DIGITS) {
            uint64_t x, y, cell;
            const char *sample = text.u.str.ptr + at;
            if (count == steps || hex_value(sample, 4, &x) || hex_value(sample + 4, 4, &y) ||
                hex_value(sample + 8, 2, &cell) || cell > GHOST_CELL_MAX) return -1;
            out->samples[count].x = (uint16_t)x;
            out->samples[count].y = (uint16_t)y;
            out->samples[count].cell = (uint8_t)cell;
            count++;
        }
    }
    return count == steps ? 0 : -1;  /* exactly `steps` samples */
}

int game_ghost_decode(GameGhostTrack *out, const char *text)
{
    memset(out, 0, sizeof(*out));
    if (!text) return -1;
    size_t size = strlen(text);
    if (size >= GHOST_TEXT_MAX) return -1;
    toml_result_t parsed = toml_parse(text, (int)size);
    if (!parsed.ok) { toml_free(parsed); return -1; }

    toml_datum_t top = parsed.toptab;
    toml_datum_t frames = {0};
    int version = 0, steps = 0, seen = 0, failed = 0;
    for (int i = 0; i < top.u.tab.size && !failed; i++) {
        const char *key = top.u.tab.key[i];
        toml_datum_t value = top.u.tab.value[i];
        if (strlen(key) != (size_t)top.u.tab.len[i]) { failed = 1; break; }
        if (!strcmp(key, "format_version")) {
            failed = value.type != TOML_INT64 || value.u.int64 != GHOST_FORMAT_VERSION;
            version = 1; seen |= 1;
        } else if (!strcmp(key, "level")) {
            failed = value.type != TOML_STRING || value.u.str.len < 0 ||
                     (size_t)value.u.str.len >= sizeof(out->level) ||
                     strlen(value.u.str.ptr) != (size_t)value.u.str.len;
            if (!failed) {
                memcpy(out->level, value.u.str.ptr, (size_t)value.u.str.len + 1);
                failed = !game_profile_key_valid(out->level);
            }
            seen |= 2;
        } else if (!strcmp(key, "level_hash")) {
            failed = value.type != TOML_STRING || value.u.str.len != 16 ||
                     hex_value(value.u.str.ptr, 16, &out->level_hash);
            seen |= 4;
        } else if (!strcmp(key, "time")) {
            double time = value.type == TOML_FP64 ? value.u.fp64 :
                          value.type == TOML_INT64 ? (double)value.u.int64 : -1.0;
            failed = !isfinite(time) || time < 0.0 || time > GHOST_TIME_MAX;
            out->time = (float)time;
            seen |= 8;
        } else if (!strcmp(key, "steps")) {
            failed = value.type != TOML_INT64 || value.u.int64 < 1 || value.u.int64 > GHOST_MAX_STEPS;
            if (!failed) steps = (int)value.u.int64;
            seen |= 16;
        } else if (!strcmp(key, "frames")) {
            frames = value;
            seen |= 32;
        } else failed = 1;  /* unknown key */
    }
    /* Samples are decoded last, once `steps` says how many to expect. */
    if (!failed && version && seen == 63 && ghost_time_matches_steps(out->time, steps)) {
        out->samples = malloc((size_t)steps * sizeof(*out->samples));
        failed = !out->samples || decode_frames(frames, out, steps);
        out->count = steps;
    } else failed = 1;
    toml_free(parsed);
    if (failed) {
        game_ghost_track_free(out);
        return -1;
    }
    return 0;
}

/* ---- Encoding ------------------------------------------------------ */

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

int game_ghost_encode(const GameGhostTrack *track, char *text, size_t capacity)
{
    size_t used = 0;
    if (!track || !text || !capacity || !game_profile_key_valid(track->level) ||
        track->count < 1 || track->count > GHOST_MAX_STEPS || !track->samples ||
        !isfinite(track->time) || track->time < 0.0f || track->time > GHOST_TIME_MAX ||
        !ghost_time_matches_steps(track->time, track->count)) return -1;
    /* The key passed game_profile_key_valid, which refuses quotes,
     * backslashes and control characters, so it needs no TOML escaping. */
    if (append(text, capacity, &used,
               "# Super Mango ghost run: one sample per fixed 1/60 s step.\n"
               "# A sample is 10 hex digits: x + %d, y + %d, sprite cell.\n"
               "format_version = %d\nlevel = \"%s\"\nlevel_hash = \"%016llx\"\n"
               "time = %.9g\nsteps = %d\nframes = [\n",
               GHOST_COORD_OFFSET, GHOST_COORD_OFFSET, GHOST_FORMAT_VERSION, track->level,
               (unsigned long long)track->level_hash, (double)track->time, track->count)) return -1;
    for (int i = 0; i < track->count; i++) {
        const GhostSample *s = &track->samples[i];
        int line_start = i % GHOST_SAMPLES_PER_LINE == 0;
        int line_end = i % GHOST_SAMPLES_PER_LINE == GHOST_SAMPLES_PER_LINE - 1 || i == track->count - 1;
        if (s->cell > GHOST_CELL_MAX ||
            append(text, capacity, &used, "%s%04x%04x%02x%s", line_start ? "  \"" : "",
                   (unsigned)s->x, (unsigned)s->y, (unsigned)s->cell, line_end ? "\",\n" : "")) return -1;
    }
    return append(text, capacity, &used, "]\n");
}

/* ---- Storage ------------------------------------------------------- */

int game_ghost_file_path(const char *profile_path, const char *level_key,
                         char *out, size_t size)
{
    static const char prefix[] = "levels/", suffix[] = ".toml";
    if (!profile_path || !profile_path[0] || !game_profile_key_valid(level_key)) return -1;
    /* The level's own name: "levels/01_lugio_01.toml" -> "01_lugio_01". */
    size_t key_size = strlen(level_key);
    const char *stem = level_key + sizeof(prefix) - 1;
    int stem_size = (int)(key_size - (sizeof(prefix) - 1) - (sizeof(suffix) - 1));
    /* The profile's path without its own .toml ending, if it has one. */
    size_t base_size = strlen(profile_path);
    if (base_size >= sizeof(suffix) - 1 &&
        !strcmp(profile_path + base_size - (sizeof(suffix) - 1), suffix))
        base_size -= sizeof(suffix) - 1;
    int written = snprintf(out, size, "%.*s-ghost-%.*s.toml", (int)base_size, profile_path, stem_size, stem);
    return written < 0 || (size_t)written >= size ? -1 : 0;
}

#ifndef __EMSCRIPTEN__
/* Read a whole ghost file. 1 read, 0 missing, -1 unreadable or too big. */
static int read_ghost_file(const char *path, char **out)
{
    *out = NULL;
    FILE *fp = serializer_fopen_utf8(path, "rb");
    if (!fp) return errno == ENOENT ? 0 : -1;
    char *text = malloc(GHOST_TEXT_MAX);
    if (!text) { fclose(fp); return -1; }
    size_t size = fread(text, 1, GHOST_TEXT_MAX, fp);
    int failed = ferror(fp) || size >= GHOST_TEXT_MAX || memchr(text, '\0', size) != NULL;
    if (fclose(fp) != 0) failed = 1;
    if (failed) { free(text); return -1; }
    text[size] = '\0';
    *out = text;
    return 1;
}
#endif

int game_ghost_load(const GameProfile *profile, const char *level_key, GameGhostTrack *out)
{
    char *text = NULL;
    int found;
    memset(out, 0, sizeof(*out));
    if (!profile || !profile->enabled || !game_profile_key_valid(level_key)) return -1;
#ifdef __EMSCRIPTEN__
    text = malloc(GHOST_TEXT_MAX);
    found = text ? ghost_browser_read(level_key, text, GHOST_TEXT_MAX) : -1;
    if (found > 0) found = 1;
#else
    char path[SERIALIZER_IO_PATH_MAX];
    if (game_ghost_file_path(profile->path, level_key, path, sizeof(path))) return -1;
    found = read_ghost_file(path, &text);
#endif
    /* A ghost stored under one level's name must be for that level. */
    if (found == 1 && (game_ghost_decode(out, text) || strcmp(out->level, level_key))) {
        game_ghost_track_free(out);
        found = -1;
    }
    free(text);
    return found;
}

int game_ghost_save(const GameProfile *profile, const GameGhostTrack *track)
{
    if (!profile || !profile->enabled || !track) return -1;
    char *text = malloc(GHOST_TEXT_MAX);
    if (!text) return -1;
    int result = game_ghost_encode(track, text, GHOST_TEXT_MAX);
#ifdef __EMSCRIPTEN__
    if (!result && !ghost_browser_write(track->level, text)) result = -1;
#else
    char path[SERIALIZER_IO_PATH_MAX], temporary[SERIALIZER_IO_PATH_MAX] = "";
    if (!result && game_ghost_file_path(profile->path, track->level, path, sizeof(path))) result = -1;
    FILE *fp = result ? NULL : serializer_open_temp(path, temporary, sizeof(temporary));
    if (!result && !fp) result = -1;
    if (fp) {
        /* Write the whole file to a temporary sibling first, then move it
         * over the old ghost, so a crash never leaves half a ghost. */
        int failed = fputs(text, fp) == EOF || serializer_stream_has_error(fp) || serializer_flush(fp);
        if (fclose(fp) != 0) failed = 1;
        if (failed) {
            serializer_remove_temp(temporary);
            result = -1;
        } else {
            result = serializer_install_temp(temporary, path, 0);
            if (result == SERIALIZER_REPLACE_TEMP_KEPT)
                fprintf(stderr, "Ghost save incomplete; the ghost is kept in '%s'\n", temporary);
        }
    }
#endif
    free(text);
    return result ? -1 : 0;
}
