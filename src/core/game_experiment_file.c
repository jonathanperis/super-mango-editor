/*
 * game_experiment_file.c — Decode an experiment capture from text.
 *
 * A capture is a TOML file a player exported with F9 and may have edited
 * or received from someone else, so it is untrusted input. This file turns
 * its bytes into frames and touches nothing else: no GameState, no level,
 * no files. game_experiment.c reads the file and applies the result; the
 * fuzz harness (tests/fuzz_campaign_experiment.c) calls this directly.
 */
#include "game_experiment.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tomlc17.h"
#include "../levels/level.h"  /* MAX_PLAYER_SPEED, MAX_PLAYER_ACCEL */

/*
 * The largest value of each physics column of a row, in the order
 * game_inspector.c records them: the two top speeds, then seven
 * accelerations and frictions. These are the caps a level's [physics]
 * overrides have (MAX_PLAYER_SPEED and MAX_PLAYER_ACCEL in level.h), so a
 * capture cannot replay a player faster than any level could make it.
 */
static const float s_physics_max[INSPECTOR_PHYSICS_COUNT] = {
    MAX_PLAYER_SPEED, MAX_PLAYER_SPEED,                    /* walk, run top speed */
    MAX_PLAYER_ACCEL, MAX_PLAYER_ACCEL, MAX_PLAYER_ACCEL,  /* ground accel, friction */
    MAX_PLAYER_ACCEL, MAX_PLAYER_ACCEL, MAX_PLAYER_ACCEL,  /* counter, air accel */
    MAX_PLAYER_ACCEL                                        /* air friction */
};

float game_experiment_physics_max(int column)
{
    if (column < 0 || column >= INSPECTOR_PHYSICS_COUNT) return 0.0f;
    return s_physics_max[column];
}

/* One physics value: a finite number from 0 to max. */
static int number(toml_datum_t datum, float max, float *out)
{
    double value;
    if (datum.type == TOML_FP64) value = datum.u.fp64;
    else if (datum.type == TOML_INT64) value = (double)datum.u.int64;
    else return -1;
    if (!isfinite(value) || value < 0 || value > max) return -1;
    *out = (float)value;
    return 0;
}

int game_experiment_decode(const char *text, size_t size, const char *label,
                           uint64_t level_hash, GameExperiment *tape)
{
    int result = -1;

    if (!text || !tape || size >= EXPERIMENT_TEXT_MAX) return -1;
    if (!label) label = "(memory)";
    memset(tape, 0, sizeof(*tape));

    toml_result_t parsed = toml_parse(text, (int)size);
    if (!parsed.ok) goto done;
    toml_datum_t top = parsed.toptab;
    /* Exactly the five keys below, none of them hiding a NUL byte. */
    if (top.type != TOML_TABLE || top.u.tab.size != 5) goto done;
    for (int i = 0; i < top.u.tab.size; i++)
        if (strlen(top.u.tab.key[i]) != (size_t)top.u.tab.len[i]) goto done;
    toml_datum_t version = toml_get(top, "format_version"), seed = toml_get(top, "seed");
    toml_datum_t level = toml_get(top, "level_path"), hash = toml_get(top, "level_hash");
    toml_datum_t frames = toml_get(top, "frames");
    if (version.type == TOML_INT64 && version.u.int64 == 1) {
        /* Recorded by the variable-timestep engine: its rows hold frame
         * durations that the fixed-step engine cannot reproduce. */
        fprintf(stderr, "Error: %s was recorded before fixed-step simulation "
                        "(format_version 1); record it again\n", label);
        goto done;
    }
    if (version.type == TOML_INT64 && version.u.int64 == 2) {
        /* Recorded while the camera still panned in from x = 0 and eased
         * back after a lost life. Spike blocks wait until the camera shows
         * them, so with the camera snapping now they can start on other
         * steps and the run would quietly go another way. */
        fprintf(stderr, "Error: %s was recorded before the camera snapped to the level "
                        "start and respawn point (format_version 2); record it again\n", label);
        goto done;
    }
    if (version.type == TOML_INT64 && version.u.int64 == 3) {
        /* Recorded while a waiting spike block started from the camera's
         * position (which the Reduced motion setting changes) and while
         * jump input could replace or shorten a bouncepad launch. */
        fprintf(stderr, "Error: %s was recorded before spike blocks and bouncepads "
                        "stopped depending on settings and jump input "
                        "(format_version 3); record it again\n", label);
        goto done;
    }
    if (version.type != TOML_INT64 || version.u.int64 != EXPERIMENT_FORMAT_VERSION ||
        seed.type != TOML_INT64 || seed.u.int64 < 0 || seed.u.int64 > UINT_MAX ||
        level.type != TOML_STRING || level.u.str.len <= 0 || level.u.str.len >= GAME_LEVEL_PATH_MAX ||
        strlen(level.u.str.ptr) != (size_t)level.u.str.len ||
        hash.type != TOML_STRING || hash.u.str.len != 16 || strlen(hash.u.str.ptr) != 16 ||
        frames.type != TOML_ARRAY || frames.u.arr.size <= 0 || frames.u.arr.size > EXPERIMENT_MAX_FRAMES) goto done;

    /* The capture names the level bytes it was recorded on, as 16 hex
     * digits; they must be the bytes of the level that is loaded now. */
    char expected[17];
    snprintf(expected, sizeof(expected), "%016llx", (unsigned long long)level_hash);
    if (strcmp(expected, hash.u.str.ptr) != 0) goto done;

    tape->count = frames.u.arr.size;
    tape->seed = (unsigned int)seed.u.int64;
    tape->frames = calloc((size_t)tape->count, sizeof(*tape->frames));
    if (!tape->frames) goto done;
    for (int i = 0; i < tape->count; i++) {
        toml_datum_t row = frames.u.arr.elem[i];
        /* Row: [input bits, physics values...] for one fixed step. */
        if (row.type != TOML_ARRAY || row.u.arr.size != INSPECTOR_PHYSICS_COUNT + 1) goto done;
        toml_datum_t *values = row.u.arr.elem;
        ExperimentFrame *frame = &tape->frames[i];
        if (values[0].type != TOML_INT64 || values[0].u.int64 < 0 || values[0].u.int64 > 63) goto done;
        frame->input = (unsigned int)values[0].u.int64;
        for (int j = 0; j < INSPECTOR_PHYSICS_COUNT; j++)
            if (number(values[j + 1], s_physics_max[j], &frame->physics[j])) goto done;
    }
    result = 0;
done:
    if (result) {
        free(tape->frames);
        memset(tape, 0, sizeof(*tape));
    }
    toml_free(parsed);
    return result;
}
