/* Experiments record simulation steps, not wall-clock frames. Pauses therefore
 * do not consume tape; each step carries its own dt, semantic input and tuning.
 * Replays require unchanged level bytes and the same engine version. */
#include "game_experiment.h"
#include "game_random.h"
#include "game_completion.h"
#include "../levels/level_loader.h"
#include "../levels/level_resources.h"
#include "../shared/serializer_emit.h"
#include "tomlc17.h"
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
EM_JS(void, experiment_download, (const char *path), {
    const name = UTF8ToString(path);
    const url = URL.createObjectURL(new Blob([FS.readFile(name)], {type: 'application/toml'}));
    const link = document.createElement('a');
    link.href = url; link.download = name; document.body.appendChild(link);
    link.click(); link.remove(); setTimeout(() => URL.revokeObjectURL(url), 1000);
    FS.unlink(name);
});
#endif

void game_experiment_cleanup(GameState *gs)
{
    if (!gs->experiment) return;
    free(gs->experiment->frames);
    free(gs->experiment);
    gs->experiment = NULL;
}

static void restart(GameState *gs, unsigned int seed)
{
    game_random_seed(seed);
    /* The active definition has already passed validation. Repeat initialization
     * in the same order so random enemy timers and fog consume the same stream. */
    (void)level_load(gs, gs->runtime.current_level);
    level_resources_apply(gs, gs->runtime.current_level);
    player_reset(&gs->player);
    game_completion_reset_summary(gs);
    gs->completion.complete = gs->game_over = 0;
    gs->loop.fp_prev_riding = -1;
    gs->camera.x = 0;
    gs->level_score_start = 0;
    gs->profile_completion_recorded = 1; /* Experiments never become best results. */
    gs->inspector.frozen = gs->inspector.step_requested = 0;
    gs->loop.prev_ticks = SDL_GetTicks64();
}

int game_experiment_begin(GameState *gs)
{
    GameExperiment *tape = calloc(1, sizeof(*tape));
    if (!tape) return -1;
    tape->frames = calloc(EXPERIMENT_MAX_FRAMES, sizeof(*tape->frames));
    if (!tape->frames || serializer_fingerprint_utf8(gs->level_path, &tape->fingerprint) != 1 ||
        tape->fingerprint.content_hash != gs->source_level_hash) {
        free(tape->frames); free(tape); return -1;
    }
    SDL_strlcpy(tape->level_path, gs->level_path, sizeof(tape->level_path));
    tape->seed = gs->random_seed;
    tape->recording = 1;
    game_experiment_cleanup(gs);
    gs->experiment = tape;
    restart(gs, tape->seed);
    debug_log(&gs->debug, "Recording restarted (seed %u)", tape->seed);
    return 0;
}

float game_experiment_dt(const GameState *gs, float dt)
{
    const GameExperiment *tape = gs->experiment;
    if (tape && tape->replaying)
        return tape->cursor < tape->count ? tape->frames[tape->cursor].dt : 0;
    /* Recording starts from a known state and uses reproducible steps. Slow
     * mode/stepping still choose the actual captured duration. */
    return dt;
}

unsigned int game_experiment_input(GameState *gs, float dt, unsigned int input)
{
    GameExperiment *tape = gs->experiment;
    if (!tape) return input;
    if (tape->replaying) {
        if (tape->cursor >= tape->count) return 0;
        ExperimentFrame *frame = &tape->frames[tape->cursor++];
        game_inspector_physics(&gs->player, frame->physics, 1);
        if (tape->cursor == tape->count) gs->inspector.frozen = 1;
        return frame->input;
    }
    if (tape->recording) {
        ExperimentFrame *frame = &tape->frames[tape->count++];
        frame->dt = dt; frame->input = input & 63u;
        game_inspector_physics(&gs->player, frame->physics, 0);
        if (tape->count == EXPERIMENT_MAX_FRAMES) {
            tape->recording = 0;
            debug_log(&gs->debug, "Recording full; F9 exports %d steps", tape->count);
        }
    }
    return input;
}

int game_experiment_save(GameState *gs, const char *path)
{
    GameExperiment *tape = gs->experiment;
    if (!tape || !tape->count) return -1;
    char temporary[SERIALIZER_IO_PATH_MAX];
    FILE *fp = serializer_open_temp(path, temporary, sizeof(temporary));
    if (!fp) return -1;
    fprintf(fp, "# Super Mango simulation experiment. Replay with the same engine revision.\nformat_version = 1\n");
    write_toml_key_string(fp, "level_path", tape->level_path);
    fprintf(fp, "seed = %u\nlevel_hash = \"%016llx\"\nframes = [\n", tape->seed,
            (unsigned long long)tape->fingerprint.content_hash);
    for (int i = 0; i < tape->count; i++) {
        const ExperimentFrame *frame = &tape->frames[i];
        fprintf(fp, "  [%.9g, %u", (double)frame->dt, frame->input);
        for (int j = 0; j < INSPECTOR_PHYSICS_COUNT; j++) fprintf(fp, ", %.9g", (double)frame->physics[j]);
        fprintf(fp, "],\n");
    }
    fprintf(fp, "]\n");
    int failed = serializer_flush(fp) != 0;
    if (fclose(fp) != 0) failed = 1;
    if (!failed) failed = serializer_create_file(temporary, path) != 0;
    serializer_remove_temp(temporary);
    if (!failed) tape->recording = 0;
    return failed ? -1 : 0;
}

void game_experiment_export(GameState *gs)
{
    char path[128];
    snprintf(path, sizeof(path), "mango-experiment-%llu.toml", (unsigned long long)SDL_GetPerformanceCounter());
    if (game_experiment_save(gs, path)) { debug_log(&gs->debug, "Export failed; F8 starts a recording"); return; }
#ifdef __EMSCRIPTEN__
    experiment_download(path);
#endif
    debug_log(&gs->debug, "Exported %s", path);
    SDL_Log("Experiment exported: %s", path);
}

static int number(toml_datum_t datum, float *out)
{
    double value;
    if (datum.type == TOML_FP64) value = datum.u.fp64;
    else if (datum.type == TOML_INT64) value = (double)datum.u.int64;
    else return -1;
    if (!isfinite(value) || value < 0 || value > MAX_LEVEL_MOTION) return -1;
    *out = (float)value;
    return 0;
}

int game_experiment_load(GameState *gs, const char *path)
{
    FILE *fp = serializer_fopen_utf8(path, "rb");
    if (!fp) {
        fprintf(stderr, "Error: cannot open experiment: %s\n", path);
        return -1;
    }
    toml_result_t parsed = toml_parse_file(fp);
    fclose(fp);
    GameExperiment *tape = calloc(1, sizeof(*tape));
    int result = -1;
    if (!tape || !parsed.ok) goto done;
    toml_datum_t top = parsed.toptab;
    if (top.type != TOML_TABLE || top.u.tab.size != 5) goto done;
    for (int i = 0; i < top.u.tab.size; i++)
        if (strlen(top.u.tab.key[i]) != (size_t)top.u.tab.len[i]) goto done;
    toml_datum_t version = toml_get(top, "format_version"), seed = toml_get(top, "seed");
    toml_datum_t level = toml_get(top, "level_path"), hash = toml_get(top, "level_hash");
    toml_datum_t frames = toml_get(top, "frames");
    if (version.type != TOML_INT64 || version.u.int64 != 1 ||
        seed.type != TOML_INT64 || seed.u.int64 < 0 || seed.u.int64 > UINT_MAX ||
        level.type != TOML_STRING || level.u.str.len <= 0 || level.u.str.len >= GAME_LEVEL_PATH_MAX ||
        strlen(level.u.str.ptr) != (size_t)level.u.str.len ||
        hash.type != TOML_STRING || hash.u.str.len != 16 || strlen(hash.u.str.ptr) != 16 ||
        frames.type != TOML_ARRAY || frames.u.arr.size <= 0 || frames.u.arr.size > EXPERIMENT_MAX_FRAMES) goto done;
    if (serializer_fingerprint_utf8(gs->level_path, &tape->fingerprint) != 1 ||
        tape->fingerprint.content_hash != gs->source_level_hash) goto done;
    char expected[17];
    snprintf(expected, sizeof(expected), "%016llx", (unsigned long long)tape->fingerprint.content_hash);
    if (strcmp(expected, hash.u.str.ptr)) goto done;
    tape->count = frames.u.arr.size;
    tape->seed = (unsigned int)seed.u.int64;
    SDL_strlcpy(tape->level_path, gs->level_path, sizeof(tape->level_path));
    tape->frames = calloc((size_t)tape->count, sizeof(*tape->frames));
    if (!tape->frames) goto done;
    for (int i = 0; i < tape->count; i++) {
        toml_datum_t row = frames.u.arr.elem[i];
        if (row.type != TOML_ARRAY || row.u.arr.size != INSPECTOR_PHYSICS_COUNT + 2) goto done;
        toml_datum_t *values = row.u.arr.elem;
        ExperimentFrame *frame = &tape->frames[i];
        if (number(values[0], &frame->dt) || frame->dt <= 0 || frame->dt > 0.1f ||
            values[1].type != TOML_INT64 || values[1].u.int64 < 0 || values[1].u.int64 > 63) goto done;
        frame->input = (unsigned int)values[1].u.int64;
        for (int j = 0; j < INSPECTOR_PHYSICS_COUNT; j++)
            if (number(values[j + 2], &frame->physics[j])) goto done;
    }
    tape->replaying = 1;
    game_experiment_cleanup(gs);
    gs->experiment = tape;
    restart(gs, tape->seed);
    gs->random_seed = tape->seed;
    result = 0;
done:
    if (result && tape) { free(tape->frames); free(tape); }
    toml_free(parsed);
    if (result) fprintf(stderr, "Error: experiment invalid or level bytes changed: %s\n", path);
    return result;
}
