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
/* Volumes use authored/profile units 0..128. Each play gets independent
 * alias state; it shares the sample bytes without taking ownership of them. */
void sound_play(SoundEffect *sound, int volume);
void sound_set_volume(SoundEffect *sound, int volume);
void sound_stop_all(void);
MusicTrack *music_load(const char *path);
void music_unload(MusicTrack *music);
/* Only one music stream is active. Starting another stops the previous one
 * but does not free it; that remains the asset owner's responsibility. */
void music_play(MusicTrack *music);
void music_stop(void);
void music_pause(void);
void music_resume(void);
/* Pump the active stream every session frame, including overlay frames. */
void music_update(void);
void music_set_volume(int volume);
