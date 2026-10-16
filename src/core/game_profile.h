#pragma once

#include "../input/input_backend.h"
#include <stddef.h>
#include <stdint.h>  /* uint64_t: the level hash and coin mask in GameResume */

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
    int ghost;      /* 1 = draw the time-trial ghost of the best run (format 2) */
    int keys[PROFILE_ACTION_COUNT]; /* version-1 wire IDs, translated at input */
    int buttons[PROFILE_ACTION_COUNT];
} GameSettings;

#define GAME_SETTINGS_DEFAULTS {128,128,0,8000,2,0,0,1, \
    {BINDING_KEY_A,BINDING_KEY_D,BINDING_KEY_W,BINDING_KEY_S,BINDING_KEY_SPACE,BINDING_KEY_LEFT_SHIFT}, \
    {PAD_LEFT,PAD_RIGHT,PAD_UP,PAD_DOWN,PAD_A,PAD_RIGHT_SHOULDER}}

typedef struct {
    char path[PROFILE_LEVEL_PATH];
    int best_score, best_coins;
    float best_time;
} GameProgress;

/*
 * The profile file's format_version. Version 2 added the optional [resume]
 * table (the Continue point) and the `ghost` setting. A version-1 profile
 * still loads: it has no Continue point, the ghost setting takes its
 * default (on), and the next save writes it as version 2.
 */
#define PROFILE_FORMAT_VERSION 2

/*
 * GameResume — where Continue picks a level up again (one slot per profile).
 *
 * The session records it when the player reaches a checkpoint, pauses, or
 * leaves a level part-way, and clears it when that level is finished or
 * lost. Resuming puts the player back on the saved respawn point exactly as
 * a lost life would, with the saved score, lives and collected coins.
 * level_hash ties it to the level file's exact bytes (the hash in
 * GameState.world.source_level_hash): an edited level no longer matches, so its
 * old Continue point is dropped instead of placing the player somewhere
 * the new layout never meant.
 */
typedef struct {
    char path[PROFILE_LEVEL_PATH]; /* profile key; "" = no Continue point   */
    uint64_t level_hash;           /* content hash of the level when saved  */
    uint64_t coins;                /* bit i set = coin i already collected  */
    int checkpoint;                /* authored checkpoint index, -1 = none  */
    int legacy_screen;             /* furthest automatic screen checkpoint  */
    int score, level_score_start;  /* run score, and its value at level start */
    int score_life_next;           /* next bonus-life threshold, 0 = none left */
    int lives;
    float respawn_x, respawn_y;    /* where the player reappears            */
    float elapsed;                 /* level timer when saved, seconds       */
} GameResume;

typedef struct {
    GameSettings settings;
    char last_level[PROFILE_LEVEL_PATH];
    GameProgress levels[PROFILE_LEVEL_COUNT];
    int count;
    GameResume resume;
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
int game_settings_key_allowed(int key);
/* F2-F10, '-' and '=': the debug inspector's keys, never a control. */
int game_settings_key_debug_reserved(int key);
int game_settings_button_allowed(int button);
int game_settings_has_unavailable_binding(const GameSettings *settings);
int game_settings_valid(const GameSettings *settings);
int game_profile_decode(GameProfileData *out, const char *text);
int game_profile_encode(const GameProfileData *data, char *text, size_t capacity);
/*
 * Native only: take the lock that cooperating game instances hold while
 * they compare a file next to this profile with what they expect and then
 * replace it (the empty "<profile path>.lock" file). game_profile_save
 * takes it internally; the ghost save takes it the same way. It does not
 * wait: NULL means another instance holds it right now, or it failed.
 */
typedef struct GameProfileLock GameProfileLock;
GameProfileLock *game_profile_lock(const GameProfile *profile);
void game_profile_unlock(GameProfileLock *lock);
/* Explicit path overrides native prefs. NULL uses OS prefs / web localStorage. */
int game_profile_open(GameProfile *profile, const char *path);
int game_profile_save(GameProfile *profile);
/* Poll asynchronous browser writes; native writes complete synchronously. */
int game_profile_poll(GameProfile *profile);
/* Complete the owned snapshot, preserving dirty state for newer edits. */
int game_profile_finish_save(GameProfile *profile, int result);
void game_profile_select(GameProfile *profile, const char *key);
int game_profile_record(GameProfile *profile, const char *key, int score, int coins, float elapsed);
const GameProgress *game_profile_result(const GameProfile *profile, const char *key);
/* Replace the Continue point. Returns -1 (and keeps the old one) when the
 * record is out of range; marks the profile changed only when it differs. */
int game_profile_set_resume(GameProfile *profile, const GameResume *resume);
/* Forget the Continue point, if there is one. */
void game_profile_clear_resume(GameProfile *profile);
/* The Continue point saved for key, or NULL when there is none. */
const GameResume *game_profile_resume(const GameProfile *profile, const char *key);
