/* Bounded, explicit experiment capture. No automatic filesystem writes. */
#pragma once
#include "../game.h"
#include "game_inspector.h"
#include "../shared/serializer_io.h"

#define EXPERIMENT_MAX_FRAMES 36000

typedef struct {
    float dt;
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
void game_experiment_export(GameState *gs);
float game_experiment_dt(const GameState *gs, float dt);
unsigned int game_experiment_input(GameState *gs, float dt, unsigned int input);
void game_experiment_cleanup(GameState *gs);
