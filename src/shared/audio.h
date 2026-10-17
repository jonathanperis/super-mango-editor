/* Project audio owners. raylib's Sound/Music structs contain resource handles;
 * copying those structs does not create another independently owned resource. */
#pragma once

#include <raylib.h>

typedef struct {
    Sound sample;              /* owns decoded sample data */
    float volume;              /* normalized per-sample/user volume, 0..1 */
} SoundEffect;
typedef struct {
    Music stream;              /* owns decoder state and stream buffers */
} MusicTrack;

/* Device first, then assets. Opening returns 0 on success, -1 on failure. */
int audio_open(void);
/* Stop active voices/stream and close the device after asset owners unload. */
void audio_close(void);
/* Loading returns an owned pointer or NULL. An unavailable optional effect
 * can stay NULL; playback and unloading accept that empty slot. */
SoundEffect *sound_load(const char *path);
void sound_unload(SoundEffect *sound);
/* FREE_CHUNK — unload a sound slot and clear it, so a second cleanup of the
 * same slot does nothing (the sound twin of DESTROY_TEX in graphics.h). */
#define FREE_CHUNK(snd) \
    do { sound_unload(snd); (snd) = NULL; } while (0)
/* Volumes use authored/profile units 0..128. Each play gets independent
 * alias state; it shares the sample bytes without taking ownership of them. */
void sound_play(SoundEffect *sound, int volume);
/* sound_play with a stereo position: pan -1 is left, 0 centre, 1 right
 * (raylib 6's range; sound_pan_for_screen_x computes it from the screen). */
void sound_play_at(SoundEffect *sound, int volume, float pan);
void sound_set_volume(SoundEffect *sound, int volume);
void sound_stop_all(void);
/* Silence every voice playing this one sound; the sample stays loaded. */
void sound_stop(SoundEffect *sound);
MusicTrack *music_load(const char *path);
void music_unload(MusicTrack *music);
/* Only one music stream is active. Starting another stops the previous one
 * but does not free it; that remains the asset owner's responsibility. */
void music_play(MusicTrack *music);
void music_stop(void);
/* Stop music only if it is the active stream; another one keeps playing. */
void music_stop_track(MusicTrack *music);
void music_pause(void);
void music_resume(void);
/* Pump the active stream every session frame, including overlay frames. */
void music_update(void);
void music_set_volume(int volume);

#ifdef MANGO_TESTING
/* Test builds only. *_test_loads: files loaded so far. *_test_live: loaded
 * ones not unloaded yet, so a test can check that a session frees each
 * sound and track exactly once. */
int sound_test_loads(void);
int sound_test_live(void);
int music_test_loads(void);
int music_test_live(void);
#endif
