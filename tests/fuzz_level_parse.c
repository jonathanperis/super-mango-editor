/*
 * fuzz_level_parse.c — Fuzz the level loader exactly as the game uses it.
 *
 * Input bytes are written to a temporary .toml file and loaded with
 * level_load_toml(): tomlc17 parse → schema check → section loaders →
 * level_validate_runtime().  Rejecting input is the expected outcome for
 * almost every mutation; the harness only fails on crashes, sanitizer
 * reports, or a broken save/load round trip.
 *
 * Round trip: any level the loader accepts must also save, load again, and
 * save to the same bytes.  This is how a raw DEL in a saved name (accepted
 * on load, rejected on reload) shows up as a fuzz failure.  An accepted
 * level's next_phase must also pass level_ref_valid, and quoted strings in
 * the raw input are fed to level_ref_valid directly.
 *
 * See fuzz_replay_main.c for the standalone driver and the Makefile's
 * fuzz / fuzz-corpus targets for how it is built.  POSIX only.
 */

#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>  /* getpid */

#include "shared/serializer.h"
#include "levels/level.h"
#include "levels/level_ref.h"

static char input_path[1024];
static char first_save[1024];
static char second_save[1024];

/* Normal exit only: after abort() the failing input stays on disk. */
static void remove_temp_files(void)
{
    remove(input_path);
    remove(first_save);
    remove(second_save);
}

/* Per-process names keep parallel fuzzers (-jobs=N) from sharing files. */
static void init_paths(void)
{
    const char *dir;

    if (input_path[0] != '\0') return;
    atexit(remove_temp_files);
    dir = getenv("TMPDIR");
    if (!dir || dir[0] == '\0') dir = "/tmp";
    snprintf(input_path, sizeof(input_path), "%s/mango-fuzz-%ld-in.toml",
             dir, (long)getpid());
    snprintf(first_save, sizeof(first_save), "%s/mango-fuzz-%ld-a.toml",
             dir, (long)getpid());
    snprintf(second_save, sizeof(second_save), "%s/mango-fuzz-%ld-b.toml",
             dir, (long)getpid());
}

static int write_bytes(const char *path, const uint8_t *data, size_t size)
{
    FILE *fp = fopen(path, "wb");
    size_t written;

    if (!fp) return -1;
    written = fwrite(data, 1, size, fp);
    if (fclose(fp) != 0 || written != size) return -1;
    return 0;
}

/* Return 1 when two files hold identical bytes. */
static int same_bytes(const char *a_path, const char *b_path)
{
    FILE *a = fopen(a_path, "rb");
    FILE *b = fopen(b_path, "rb");
    int same = a && b;

    while (same) {
        int ca = fgetc(a);
        int cb = fgetc(b);
        if (ca != cb) same = 0;
        if (ca == EOF || cb == EOF) break;
    }
    if (a) fclose(a);
    if (b) fclose(b);
    return same;
}

static void round_trip_failed(const char *step)
{
    fprintf(stderr, "fuzz_level_parse: accepted level failed round trip at %s "
                    "(input kept at %s)\n", step, input_path);
    abort();
}

/*
 * Feed every double-quoted run in the input (the shape of a next_phase
 * value) straight to level_ref_valid.  The rule reads raw bytes with an
 * explicit length, so mutated text exercises it even when the TOML around
 * it is broken.  Only crashes and sanitizer reports matter here.
 */
static void check_quoted_level_refs(const uint8_t *data, size_t size)
{
    size_t start = 0;
    int inside = 0;

    for (size_t i = 0; i < size; i++) {
        if (data[i] == '\n') inside = 0;
        if (data[i] != '"') continue;
        if (inside) (void)level_ref_valid((const char *)data + start, i - start);
        inside = !inside;
        start = i + 1;
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    /* LevelDef is large; static storage keeps it off the fuzzer's stack. */
    static LevelDef loaded;
    static LevelDef reloaded;

    init_paths();
    check_quoted_level_refs(data, size);
    if (write_bytes(input_path, data, size) != 0) return 0;

    if (level_load_toml(input_path, &loaded) != 0) return 0;  /* rejected */

    /* An accepted level may only chain to a valid level reference. */
    if (loaded.next_phase[0] != '\0' &&
        !level_ref_valid(loaded.next_phase, strlen(loaded.next_phase)))
        round_trip_failed("accepted next_phase");

    if (level_save_toml(&loaded, first_save) != 0) round_trip_failed("first save");
    if (level_load_toml(first_save, &reloaded) != 0) round_trip_failed("reload");
    if (level_save_toml(&reloaded, second_save) != 0) round_trip_failed("second save");
    if (!same_bytes(first_save, second_save)) round_trip_failed("byte comparison");
    return 0;
}
