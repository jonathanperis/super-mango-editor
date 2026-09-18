#include "shared/audio.h"
#include <stdio.h>
#include <stdlib.h>

static int observing, alias_count, alias_frees, bad_order;
static Sound aliases[2];
static float volumes[2];

Sound test_LoadSoundAlias(Sound source)
{
    Sound alias = LoadSoundAlias(source);
    if (observing && alias_count < 2) aliases[alias_count++] = alias;
    return alias;
}

void test_SetSoundVolume(Sound sound, float volume)
{
    if (observing) for (int i = 0; i < alias_count; i++)
        if (aliases[i].stream.buffer == sound.stream.buffer) volumes[i] = volume;
    SetSoundVolume(sound, volume);
}

void test_UnloadSoundAlias(Sound alias)
{
    if (observing) alias_frees++;
    UnloadSoundAlias(alias);
}

void test_UnloadSound(Sound sound)
{
    if (observing && alias_frees != alias_count) bad_order = 1;
    UnloadSound(sound);
}

int audio_contract_test(void)
{
    /* A long silent sample keeps the overlap check independent of scheduling,
     * speakers and real content. Calls still go through raylib/miniaudio. */
    Wave wave = {.frameCount=48000*5,.sampleRate=48000,.sampleSize=32,.channels=1};
    wave.data = calloc(wave.frameCount, sizeof(float));
    SoundEffect *effect = calloc(1, sizeof(*effect));
    if (!wave.data || !effect) { free(wave.data); free(effect); return 1; }
    effect->sample = LoadSoundFromWave(wave);
    effect->volume = 1;
    UnloadWave(wave);
    if (!IsSoundValid(effect->sample)) { free(effect); return 1; }
    observing = 1;
    alias_count = alias_frees = bad_order = 0;
    sound_set_volume(effect, 64);
    sound_play(effect, 128);
    sound_play(effect, 32);
    int failed = alias_count != 2;
    if (!failed) {
        failed = aliases[0].stream.buffer == aliases[1].stream.buffer ||
                 !IsSoundPlaying(aliases[0]) || !IsSoundPlaying(aliases[1]) ||
                 volumes[0] != 0.5f || volumes[1] != 0.125f;
        sound_set_volume(effect, 32);
        failed |= volumes[0] != 0.25f || volumes[1] != 0.0625f;
    }
    sound_unload(effect);
    failed |= alias_frees != 2 || bad_order;
    observing = 0;
    if (failed) fprintf(stderr, "audio: overlap, per-voice volume or sample ownership failed\n");
    return failed;
}
