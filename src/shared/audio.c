#include "audio.h"
#include <stdlib.h>

/* The original mixer used its default eight effect channels. Each alias owns
 * independent playback state while sharing its owner's decoded sample. */
#define EFFECT_VOICES 8
typedef struct { Sound alias; SoundEffect *owner; float volume; } EffectVoice;
static EffectVoice voices[EFFECT_VOICES];
static MusicTrack *current_music;

int audio_open(void)
{
    InitAudioDevice();
    return IsAudioDeviceReady() ? 0 : -1;
}

static void voice_clear(EffectVoice *voice)
{
    if (!voice->owner) return;
    StopSound(voice->alias);
    UnloadSoundAlias(voice->alias);
    *voice = (EffectVoice){0};
}

void sound_stop_all(void)
{
    for (int i = 0; i < EFFECT_VOICES; i++) voice_clear(&voices[i]);
}

void audio_close(void)
{
    sound_stop_all();
    music_stop();
    if (IsAudioDeviceReady()) CloseAudioDevice();
}

SoundEffect *sound_load(const char *path)
{
    if (!IsAudioDeviceReady()) return NULL;
    Sound sample = LoadSound(path);
    if (!IsSoundValid(sample)) return NULL;
    SoundEffect *sound = malloc(sizeof(*sound));
    if (!sound) { UnloadSound(sample); return NULL; }
    *sound = (SoundEffect){sample, 1.0f};
    return sound;
}

void sound_unload(SoundEffect *sound)
{
    if (!sound) return;
    for (int i = 0; i < EFFECT_VOICES; i++)
        if (voices[i].owner == sound) voice_clear(&voices[i]);
    UnloadSound(sound->sample);
    free(sound);
}

void sound_play(SoundEffect *sound, int volume)
{
    if (!sound) return;
    for (int i = 0; i < EFFECT_VOICES; i++) {
        EffectVoice *voice = &voices[i];
        if (voice->owner && IsSoundPlaying(voice->alias)) continue;
        voice_clear(voice);
        Sound alias = LoadSoundAlias(sound->sample);
        if (!IsSoundValid(alias)) return;
        *voice = (EffectVoice){alias, sound, volume / 128.0f};
        SetSoundVolume(alias, sound->volume * voice->volume);
        PlaySound(alias);
        return;
    }
}

void sound_set_volume(SoundEffect *sound, int volume)
{
    if (!sound) return;
    sound->volume = volume / 128.0f;
    for (int i = 0; i < EFFECT_VOICES; i++)
        if (voices[i].owner == sound)
            SetSoundVolume(voices[i].alias, sound->volume * voices[i].volume);
}

MusicTrack *music_load(const char *path)
{
    if (!IsAudioDeviceReady()) return NULL;
    Music stream = LoadMusicStream(path);
    if (!IsMusicValid(stream)) return NULL;
    MusicTrack *music = malloc(sizeof(*music));
    if (!music) { UnloadMusicStream(stream); return NULL; }
    stream.looping = true;
    music->stream = stream;
    return music;
}

void music_stop(void)
{
    if (current_music) StopMusicStream(current_music->stream);
    current_music = NULL;
}

void music_unload(MusicTrack *music)
{
    if (!music) return;
    if (current_music == music) music_stop();
    UnloadMusicStream(music->stream);
    free(music);
}

void music_play(MusicTrack *music)
{
    music_stop();
    current_music = music;
    if (music) PlayMusicStream(music->stream);
}

void music_pause(void) { if (current_music) PauseMusicStream(current_music->stream); }
void music_resume(void) { if (current_music) ResumeMusicStream(current_music->stream); }
void music_update(void) { if (current_music) UpdateMusicStream(current_music->stream); }
void music_set_volume(int volume)
{
    if (current_music) SetMusicVolume(current_music->stream, volume / 128.0f);
}
