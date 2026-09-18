#pragma once

#include <raylib.h>

typedef struct { Sound sample; float volume; } SoundEffect;
typedef struct { Music stream; } MusicTrack;

int audio_open(void);
void audio_close(void);
SoundEffect *sound_load(const char *path);
void sound_unload(SoundEffect *sound);
void sound_play(SoundEffect *sound, int volume);
void sound_set_volume(SoundEffect *sound, int volume);
void sound_stop_all(void);
MusicTrack *music_load(const char *path);
void music_unload(MusicTrack *music);
void music_play(MusicTrack *music);
void music_stop(void);
void music_pause(void);
void music_resume(void);
void music_update(void);
void music_set_volume(int volume);
