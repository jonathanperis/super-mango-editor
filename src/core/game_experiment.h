/* Bounded, explicit experiment capture. No automatic filesystem writes. */
#pragma once
#include "../game.h"
#include "game_inspector.h"
#include "../shared/serializer_io.h"

#define EXPERIMENT_MAX_FRAMES 36000

/*
 * Capture file format. Rows are one fixed simulation step each
 * (GAME_FIXED_STEP seconds), so they hold no duration. The version names
 * the engine behaviour a capture depends on, and older ones are refused
 * with "record it again" because they could replay differently:
 *   1  variable-timestep engine: its rows hold frame durations.
 *   2  fixed step, but the camera panned from x = 0 at a level start and
 *      eased back after a lost life. The camera now snaps to the start and
 *      to the respawn point, and waiting spike blocks start when the
 *      camera reveals them, so they can start on other steps.
 *   3  the current engine.
 */
#define EXPERIMENT_FORMAT_VERSION 3

/* One recorded simulation step: the input and tuning in effect. */
typedef struct {
    unsigned int input;
    float physics[INSPECTOR_PHYSICS_COUNT];
} ExperimentFrame;

typedef struct GameExperiment {
    ExperimentFrame *frames;
    int count, cursor, recording, replaying;
    unsigned int seed;
    SerializerFileFingerprint fingerprint;
    char level_path[GAME_LEVEL_PATH_MAX];
} GameExperiment;

int game_experiment_begin(GameState *gs);
int game_experiment_load(GameState *gs, const char *path);
int game_experiment_save(GameState *gs, const char *path);

/* What an F9 export did, so the debug log can say what actually went wrong. */
typedef enum {
    EXPERIMENT_EXPORT_OK = 0,
    EXPERIMENT_EXPORT_NOTHING,      /* no recorded steps to export          */
    EXPERIMENT_EXPORT_NAME_TAKEN,   /* every candidate file name exists     */
    EXPERIMENT_EXPORT_WRITE_FAILED  /* the file could not be written        */
} ExperimentExportResult;

/* mango-experiment-<stamp>.toml, then -2 ... -EXPERIMENT_EXPORT_ATTEMPTS. */
#define EXPERIMENT_EXPORT_ATTEMPTS 10

/* Export into folder ("" = working folder; else ending in a separator),
 * named after stamp, seconds since 1970. path receives the name used. */
ExperimentExportResult game_experiment_export_at(GameState *gs, const char *folder,
                                                 long long stamp, char *path, size_t size);
/* F9: export into the working folder, stamped with the current date/time. */
void game_experiment_export(GameState *gs);
/* Step length for the next update: dt normally; 0 once a replay has ended. */
float game_experiment_dt(const GameState *gs, float dt);
unsigned int game_experiment_input(GameState *gs, unsigned int input);
void game_experiment_cleanup(GameState *gs);
