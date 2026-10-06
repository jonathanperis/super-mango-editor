/*
 * fuzz_profile_decode.c — Fuzz the player profile decoder.
 *
 * The profile is plain TOML in the user's preference directory (native) or
 * localStorage (browser), so anyone can hand-edit it.  game_profile_decode()
 * must reject bad text without crashing, and anything it accepts must encode
 * and decode back to the same text: otherwise a valid profile could become
 * unsaveable or change on every launch.
 *
 * See fuzz_replay_main.c for the standalone driver.  POSIX only.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/game_profile.h"

static void round_trip_failed(const char *step)
{
    fprintf(stderr, "fuzz_profile_decode: accepted profile failed round trip at %s\n",
            step);
    abort();
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    /* GameProfileData holds 128 results; keep it and the text off the stack. */
    static GameProfileData decoded;
    static GameProfileData redecoded;
    static char first[PROFILE_TEXT_MAX];
    static char second[PROFILE_TEXT_MAX];
    char *text;

    /* The decoder reads a C string, like the native and browser loaders. */
    text = malloc(size + 1);
    if (!text) return 0;
    memcpy(text, data, size);
    text[size] = '\0';

    if (game_profile_decode(&decoded, text) == 0) {
        if (game_profile_encode(&decoded, first, sizeof(first)) != 0)
            round_trip_failed("first encode");
        if (game_profile_decode(&redecoded, first) != 0)
            round_trip_failed("decode of encoded text");
        if (game_profile_encode(&redecoded, second, sizeof(second)) != 0)
            round_trip_failed("second encode");
        if (strcmp(first, second) != 0)
            round_trip_failed("text comparison");
    }
    free(text);
    return 0;
}
