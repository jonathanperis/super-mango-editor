/*
 * fuzz_level_parse.c — Fuzz the level loader exactly as the game uses it.
 *
 * Input bytes are loaded from memory with level_load_toml_text(), the code
 * every level file goes through after it is read: tomlc17 parse → schema
 * check → section loaders → level_validate_runtime().  Rejecting input is
 * the expected outcome for almost every mutation; the harness only fails
 * on crashes, sanitizer reports, or a broken save/load round trip.
 *
 * Every input is loaded with a listener attached, as the editor does when
 * a file will not open.  That is the only way to reach the code that runs
 * *after* a rejection: the line lookup for each message, and the validator's
 * "report every error" mode, which keeps checking values an earlier rule
 * already refused.  A rejected level is where most inputs end up, so this
 * is most of the search.
 *
 * No input file is written (the bytes are parsed where they are), and the
 * saves of the round trip skip their fsync (serializer_set_durable): a
 * harness that waits for the disk three times per input spends its whole
 * budget waiting.
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

static char first_save[1024];
static char second_save[1024];

/* Normal exit only: after abort() the failing input stays on disk. */
static void remove_temp_files(void)
{
    remove(first_save);
    remove(second_save);
}

/* Per-process names keep parallel fuzzers (-jobs=N) from sharing files. */
static void init_paths(void)
{
    const char *dir;

    if (first_save[0] != '\0') return;
    atexit(remove_temp_files);
    serializer_set_durable(0);   /* no fsync: see the header comment */
    dir = getenv("TMPDIR");
    if (!dir || dir[0] == '\0') dir = "/tmp";
    snprintf(first_save, sizeof(first_save), "%s/mango-fuzz-%ld-a.toml",
             dir, (long)getpid());
    snprintf(second_save, sizeof(second_save), "%s/mango-fuzz-%ld-b.toml",
             dir, (long)getpid());
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
    fprintf(stderr, "fuzz_level_parse: accepted level failed round trip at %s\n", step);
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

/* The listener: every reason is produced, then dropped. */
static void ignore_problem(void *context, const char *message, int line)
{
    (void)context;
    (void)message;
    (void)line;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    /* LevelDef is large; static storage keeps it off the fuzzer's stack. */
    static LevelDef loaded;
    static LevelDef reloaded;

    init_paths();
    check_quoted_level_refs(data, size);

    /* With a listener, so a rejected input still runs the explaining code
     * (line lookups, and every-error validation). */
    if (level_load_toml_text((const char *)data, size, "fuzz input", &loaded,
                             ignore_problem, NULL) != 0)
        return 0;  /* rejected */

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
