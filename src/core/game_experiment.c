/* Experiments record simulation steps, not wall-clock frames. Pauses therefore
 * do not consume tape; each row holds one fixed GAME_FIXED_STEP step's semantic
 * input and movement tuning (format_version 3). Older captures are refused with
 * a "record it again" error (EXPERIMENT_FORMAT_VERSION says why for each).
 * Replays require unchanged level bytes and the same engine version. */
#include "game_experiment.h"
#include "../shared/platform.h"  /* str_copy */
#include "game_random.h"
#include "game_camera.h"
#include "game_completion.h"
#include "game_timing.h"
#include "../levels/level_loader.h"
#include "../levels/level_resources.h"
#include "../shared/serializer_emit.h"
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>    /* time: calendar seconds for export names */
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
    if (!gs->screen.experiment) return;
    free(gs->screen.experiment->frames);
    free(gs->screen.experiment);
    gs->screen.experiment = NULL;
}

static void restart(GameState *gs, unsigned int seed)
{
    game_random_seed(seed);
    /* The active definition has already passed validation, so apply it
     * again without re-checking. Repeat initialization in the same order so
     * random enemy timers and fog consume the same stream. */
    level_apply(gs, gs->world.runtime.current_level);
    /* This restarts the music at the player's mute and volume settings
     * (game_music_volume), so F8 cannot unmute the game. */
    level_resources_apply(gs, gs->world.runtime.current_level);
    player_reset(&gs->world.player);
    game_completion_reset_summary(gs);
    gs->screen.completion.complete = gs->screen.game_over = 0;
    gs->screen.loop.fp_prev_riding = -1;
    game_camera_snap(gs);  /* the restarted level's start, not x = 0 */
    gs->world.level_score_start = 0;
    gs->screen.profile_completion_recorded = 1; /* Experiments never become best results. */
    gs->screen.inspector.frozen = gs->screen.inspector.step_requested = 0;
    game_timing_restart_clock(gs);
}

int game_experiment_begin(GameState *gs)
{
    GameExperiment *tape = calloc(1, sizeof(*tape));
    if (!tape) return -1;
    tape->frames = calloc(EXPERIMENT_MAX_FRAMES, sizeof(*tape->frames));
    if (!tape->frames || serializer_fingerprint_utf8(gs->world.level_path, &tape->fingerprint) != 1 ||
        tape->fingerprint.content_hash != gs->world.source_level_hash) {
        free(tape->frames); free(tape); return -1;
    }
    str_copy(tape->level_path, gs->world.level_path, sizeof(tape->level_path));
    tape->seed = gs->screen.random_seed;
    tape->recording = 1;
    game_experiment_cleanup(gs);
    gs->screen.experiment = tape;
    restart(gs, tape->seed);
    debug_log(&gs->screen.debug, "Recording restarted (seed %u)", tape->seed);
    return 0;
}

float game_experiment_dt(const GameState *gs, float dt)
{
    const GameExperiment *tape = gs->screen.experiment;
    /* Every step, live or replayed, is the fixed step; slow mode and F3 only
     * change how many steps run per rendered frame. A finished replay stops. */
    if (tape && tape->replaying && tape->cursor >= tape->count) return 0;
    return dt;
}

unsigned int game_experiment_input(GameState *gs, unsigned int input)
{
    GameExperiment *tape = gs->screen.experiment;
    if (!tape) return input;
    if (tape->replaying) {
        if (tape->cursor >= tape->count) return 0;
        ExperimentFrame *frame = &tape->frames[tape->cursor++];
        game_inspector_physics(&gs->world.player, frame->physics, 1);
        if (tape->cursor == tape->count) gs->screen.inspector.frozen = 1;
        return frame->input;
    }
    if (tape->recording) {
        ExperimentFrame *frame = &tape->frames[tape->count++];
        frame->input = input & 63u;
        game_inspector_physics(&gs->world.player, frame->physics, 0);
        if (tape->count == EXPERIMENT_MAX_FRAMES) {
            tape->recording = 0;
            debug_log(&gs->screen.debug, "Recording full; F9 exports %d steps", tape->count);
        }
    }
    return input;
}

int game_experiment_save(GameState *gs, const char *path)
{
    GameExperiment *tape = gs->screen.experiment;
    if (!tape || !tape->count) return -1;
    char temporary[SERIALIZER_IO_PATH_MAX];
    FILE *fp = serializer_open_temp(path, temporary, sizeof(temporary));
    if (!fp) return -1;
    fprintf(fp, "# Super Mango simulation experiment: one row per fixed %.0f Hz step.\n"
                "# Replay with the same engine revision.\nformat_version = %d\n",
            (double)TARGET_FPS, EXPERIMENT_FORMAT_VERSION);
    write_toml_key_string(fp, "level_path", tape->level_path);
    fprintf(fp, "seed = %u\nlevel_hash = \"%016llx\"\nframes = [\n", tape->seed,
            (unsigned long long)tape->fingerprint.content_hash);
    for (int i = 0; i < tape->count; i++) {
        const ExperimentFrame *frame = &tape->frames[i];
        fprintf(fp, "  [%u", frame->input);
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

/*
 * The file name used to come from clock_millis(), the time since the
 * computer started. After a reboot that count starts again, so a new export
 * could ask for the name of an old one; the create-only save then refused
 * it and the log blamed a missing recording. The name now uses the calendar
 * time, and a taken name moves on to -2, -3, ... instead of failing.
 */
ExperimentExportResult game_experiment_export_at(GameState *gs, const char *folder,
                                                 long long stamp, char *path, size_t size)
{
    const GameExperiment *tape = gs->screen.experiment;
    if (!tape || !tape->count) return EXPERIMENT_EXPORT_NOTHING;
    for (int attempt = 1; attempt <= EXPERIMENT_EXPORT_ATTEMPTS; attempt++) {
        int length = attempt == 1
            ? snprintf(path, size, "%smango-experiment-%lld.toml", folder, stamp)
            : snprintf(path, size, "%smango-experiment-%lld-%d.toml", folder, stamp, attempt);
        if (length < 0 || (size_t)length >= size) return EXPERIMENT_EXPORT_WRITE_FAILED;
        SerializerPathStatus status = serializer_probe_path_utf8(path);
        if (status == SERIALIZER_PATH_EXISTING) continue;
        if (status == SERIALIZER_PATH_ERROR) return EXPERIMENT_EXPORT_WRITE_FAILED;
        if (game_experiment_save(gs, path) == 0) return EXPERIMENT_EXPORT_OK;
        /* Another program took the name between the check and the save. */
        if (serializer_probe_path_utf8(path) == SERIALIZER_PATH_EXISTING) continue;
        return EXPERIMENT_EXPORT_WRITE_FAILED;
    }
    return EXPERIMENT_EXPORT_NAME_TAKEN;
}

void game_experiment_export(GameState *gs)
{
    char path[160];
    switch (game_experiment_export_at(gs, "", (long long)time(NULL), path, sizeof(path))) {
    case EXPERIMENT_EXPORT_OK:
#ifdef __EMSCRIPTEN__
        experiment_download(path);
#endif
        debug_log(&gs->screen.debug, "Exported %s", path);
        TraceLog(LOG_INFO, "Experiment exported: %s", path);
        break;
    case EXPERIMENT_EXPORT_NOTHING:
        debug_log(&gs->screen.debug, "Nothing recorded; F8 starts a recording");
        break;
    case EXPERIMENT_EXPORT_NAME_TAKEN:
        debug_log(&gs->screen.debug, "Export failed: file names already taken");
        TraceLog(LOG_WARNING, "Experiment export: %s and earlier names already exist", path);
        break;
    default:
        debug_log(&gs->screen.debug, "Export failed: could not write the file");
        TraceLog(LOG_WARNING, "Experiment export: could not write %s", path);
        break;
    }
}

int game_experiment_load(GameState *gs, const char *path)
{
    /* A capture is somebody else's file: read it with a size limit before
     * parsing any of it (serializer_read_bytes). */
    char *text = NULL;
    size_t size = 0;
    int too_large = 0;
    if (serializer_read_bytes(path, EXPERIMENT_TEXT_MAX, &text, &size, &too_large) !=
        SERIALIZER_PATH_EXISTING) {
        if (too_large)
            fprintf(stderr, "Error: experiment is larger than %d bytes: %s\n",
                    EXPERIMENT_TEXT_MAX - 1, path);
        else
            fprintf(stderr, "Error: cannot open experiment: %s\n", path);
        return -1;
    }
    /* The level file must still hold the bytes this game loaded: a capture
     * replayed on a changed level would quietly go another way. */
    GameExperiment *tape = calloc(1, sizeof(*tape));
    SerializerFileFingerprint fingerprint;
    int result = -1;
    if (tape &&
        serializer_fingerprint_utf8(gs->world.level_path, &fingerprint) == 1 &&
        fingerprint.content_hash == gs->world.source_level_hash &&
        game_experiment_decode(text, size, path, fingerprint.content_hash, tape) == 0) {
        tape->fingerprint = fingerprint;
        str_copy(tape->level_path, gs->world.level_path, sizeof(tape->level_path));
        tape->replaying = 1;
        game_experiment_cleanup(gs);
        gs->screen.experiment = tape;
        restart(gs, tape->seed);
        gs->screen.random_seed = tape->seed;
        result = 0;
    }
    free(text);
    if (result) {
        free(tape);   /* a failed decode left it with no frames */
        fprintf(stderr, "Error: experiment invalid or level bytes changed: %s\n", path);
    }
    return result;
}
