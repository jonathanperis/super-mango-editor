/*
 * fuzz_profile_decode.c — Fuzz the player profile and ghost-run decoders.
 *
 * The profile is plain TOML in the user's preference directory (native) or
 * localStorage (browser), so anyone can hand-edit it; the time-trial ghost
 * runs stored beside it are the same kind of untrusted text. Each input is
 * tried as both. game_profile_decode() and game_ghost_decode() must reject
 * bad text without crashing, and anything they accept must encode and
 * decode back to the same text: otherwise a valid file could become
 * unsaveable or change on every launch.
 *
 * See fuzz_replay_main.c for the standalone driver.  POSIX only.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/game_ghost.h"
#include "core/game_profile.h"

static void round_trip_failed(const char *what, const char *step)
{
    fprintf(stderr, "fuzz_profile_decode: accepted %s failed round trip at %s\n",
            what, step);
    abort();
}

static void check_profile(const char *text)
{
    /* GameProfileData holds 128 results; keep it and the text off the stack. */
    static GameProfileData decoded;
    static GameProfileData redecoded;
    static char first[PROFILE_TEXT_MAX];
    static char second[PROFILE_TEXT_MAX];

    if (game_profile_decode(&decoded, text) != 0) return;
    if (game_profile_encode(&decoded, first, sizeof(first)) != 0)
        round_trip_failed("profile", "first encode");
    if (game_profile_decode(&redecoded, first) != 0)
        round_trip_failed("profile", "decode of encoded text");
    if (game_profile_encode(&redecoded, second, sizeof(second)) != 0)
        round_trip_failed("profile", "second encode");
    if (strcmp(first, second) != 0)
        round_trip_failed("profile", "text comparison");
}

static void check_ghost(const char *text)
{
    static char first[GHOST_TEXT_MAX];
    static char second[GHOST_TEXT_MAX];
    GameGhostTrack decoded, redecoded;

    if (game_ghost_decode(&decoded, text) != 0) return;
    if (game_ghost_encode(&decoded, first, sizeof(first)) != 0)
        round_trip_failed("ghost", "first encode");
    if (game_ghost_decode(&redecoded, first) != 0)
        round_trip_failed("ghost", "decode of encoded text");
    if (game_ghost_encode(&redecoded, second, sizeof(second)) != 0)
        round_trip_failed("ghost", "second encode");
    if (strcmp(first, second) != 0)
        round_trip_failed("ghost", "text comparison");
    game_ghost_track_free(&decoded);
    game_ghost_track_free(&redecoded);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    /* The decoders read a C string, like the native and browser loaders. */
    char *text = malloc(size + 1);
    if (!text) return 0;
    memcpy(text, data, size);
    text[size] = '\0';
    check_profile(text);
    check_ghost(text);
    free(text);
    return 0;
}
