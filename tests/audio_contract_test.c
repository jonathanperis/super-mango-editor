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

/* Writes a big-endian integer of `bytes` bytes, as QOA headers store them. */
static unsigned char *qoa_put(unsigned char *at, unsigned long long value, int bytes)
{
    for (int i = bytes - 1; i >= 0; i--) *at++ = (unsigned char)(value >> (8 * i));
    return at;
}

/* Crafted QOA files whose headers disagree with their contents. raylib's
 * bundled qoa.h trusted them before the raylib PR #6162 bounds checks, which
 * the build applies through vendor/raylib/patches.json:
 * - a nine-channel file wrote decoder state past qoa_desc's eight-channel
 *   array (a stack write), and is now refused;
 * - a file header promising one sample, followed by a frame of twenty, wrote
 *   twenty samples into a one-sample buffer (a heap write). The buffer now
 *   covers whole frames, so the frame decodes inside it.
 * The first case fails in every build: an unbounded decoder accepts the file.
 * The second only differs in where the samples land, so only the sanitizer
 * build (the CI Sanitizers job runs this test under ASan) can see the
 * out-of-bounds write; elsewhere it pins the decoded result. */
int qoa_bounds_contract_test(void)
{
    unsigned char nine[8 + 8 + 9 * 16] = {0}, *at = nine;
    at = qoa_put(at, 0x716f6166, 4);                 /* "qoaf" */
    at = qoa_put(at, 1, 4);                          /* one sample per channel */
    at = qoa_put(at, (9ull << 56) | (44100ull << 32) | (0ull << 16) | (sizeof(nine) - 8), 8);
    Wave refused = LoadWaveFromMemory(".qoa", nine, (int)sizeof(nine));
    int failed = refused.data != NULL;
    UnloadWave(refused);

    unsigned char longer[8 + 8 + 16 + 8] = {0};
    at = qoa_put(longer, 0x716f6166, 4);
    at = qoa_put(at, 1, 4);
    at = qoa_put(at, (1ull << 56) | (44100ull << 32) | (20ull << 16) | (sizeof(longer) - 8), 8);
    Wave bounded = LoadWaveFromMemory(".qoa", longer, (int)sizeof(longer));
    /* The frame's twenty samples decode into the whole-frame buffer and
     * are what raylib reports. */
    failed |= bounded.data == NULL || bounded.frameCount != 20 || bounded.channels != 1;
    UnloadWave(bounded);
    if (failed) fprintf(stderr, "audio: a crafted QOA header was trusted past its buffer\n");
    return failed;
}
