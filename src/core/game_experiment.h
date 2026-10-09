/* Bounded, explicit experiment capture. No automatic filesystem writes. */
#pragma once
#include "../game.h"
#include "game_inspector.h"
#include "../shared/serializer_io.h"

#define EXPERIMENT_MAX_FRAMES 36000

/*
 * Capture file format. Version 2 records one row per fixed simulation step
 * (GAME_FIXED_STEP seconds each), so rows hold no duration. Version 1 came
 * from the variable-timestep engine and cannot be replayed faithfully.
 */
#define EXPERIMENT_FORMAT_VERSION 2

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
