#pragma once

#include <SDL.h>
#include <stddef.h>

#define PROFILE_ACTION_COUNT 6
#define PROFILE_LEVEL_COUNT 128
#define PROFILE_LEVEL_PATH 256
#define PROFILE_PATH_MAX 1024
#define PROFILE_TEXT_MAX 131072

typedef enum {
    BIND_LEFT, BIND_RIGHT, BIND_UP, BIND_DOWN, BIND_JUMP, BIND_RUN
} GameBindingAction;

typedef struct {
    int music_volume, effects_volume, muted, dead_zone, window_scale;
    int high_contrast, reduced_motion;
    SDL_Scancode keys[PROFILE_ACTION_COUNT];
    SDL_GameControllerButton buttons[PROFILE_ACTION_COUNT];
} GameSettings;

#define GAME_SETTINGS_DEFAULTS {128,128,0,8000,2,0,0, \
    {SDL_SCANCODE_A,SDL_SCANCODE_D,SDL_SCANCODE_W,SDL_SCANCODE_S,SDL_SCANCODE_SPACE,SDL_SCANCODE_LSHIFT}, \
    {SDL_CONTROLLER_BUTTON_DPAD_LEFT,SDL_CONTROLLER_BUTTON_DPAD_RIGHT,SDL_CONTROLLER_BUTTON_DPAD_UP, \
     SDL_CONTROLLER_BUTTON_DPAD_DOWN,SDL_CONTROLLER_BUTTON_A,SDL_CONTROLLER_BUTTON_RIGHTSHOULDER}}

typedef struct {
    char path[PROFILE_LEVEL_PATH];
    int best_score, best_coins;
    float best_time;
} GameProgress;

typedef struct {
    GameSettings settings;
    char last_level[PROFILE_LEVEL_PATH];
    GameProgress levels[PROFILE_LEVEL_COUNT];
    int count;
} GameProfileData;

typedef struct GameProfile {
    GameProfileData data;
    char path[PROFILE_PATH_MAX];
    char status[160];
    char *baseline; /* exact loaded/saved bytes; NULL means no saved profile */
    char *pending_text; /* owned snapshot until save completion/cancellation */
    unsigned int pending_revision;
    int enabled, writable, dirty, error;
    unsigned int revision;
} GameProfile;

enum { PROFILE_SAVE_ERROR = -1, PROFILE_SAVE_OK = 0, PROFILE_SAVE_PENDING = 1 };

void game_profile_init(GameProfile *profile);
void game_profile_close(GameProfile *profile);
int game_profile_key_valid(const char *path);
int game_settings_key_allowed(SDL_Scancode key);
int game_settings_button_allowed(SDL_GameControllerButton button);
int game_settings_valid(const GameSettings *settings);
int game_profile_decode(GameProfileData *out, const char *text);
int game_profile_encode(const GameProfileData *data, char *text, size_t capacity);
/* Explicit path overrides native prefs. NULL uses SDL prefs / web localStorage. */
int game_profile_open(GameProfile *profile, const char *path);
int game_profile_save(GameProfile *profile);
/* Poll asynchronous browser writes; native writes complete synchronously. */
int game_profile_poll(GameProfile *profile);
/* Complete the owned snapshot, preserving dirty state for newer edits. */
int game_profile_finish_save(GameProfile *profile, int result);
void game_profile_select(GameProfile *profile, const char *key);
int game_profile_record(GameProfile *profile, const char *key, int score, int coins, float elapsed);
const GameProgress *game_profile_result(const GameProfile *profile, const char *key);
