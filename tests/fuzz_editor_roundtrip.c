/*
 * fuzz_editor_roundtrip.c — Fuzz the editor's save → load round trip.
 *
 * The editor keeps a level in memory as a LevelDef, saves it with the
 * serializer and later opens the file again.  A designer expects to get
 * back exactly the level they saved.  This harness checks that promise:
 *
 *   1. Input bytes go through the normal parser (level_load_toml).  Most
 *      mutations are rejected there, which is fine.
 *   2. An accepted level is saved the way the editor saves a new file
 *      (level_save_toml_with_policy, create-only) and loaded back.  The
 *      reloaded LevelDef must equal the first one, byte for byte.
 *   3. The same level is saved as a crash-recovery snapshot, with the
 *      level's (fuzzed) name as the destination path, and loaded back.  It
 *      must equal the first one too, and level_read_recovery_path must give
 *      the destination back unless it holds a control byte, which the
 *      reader refuses by design.
 *
 * fuzz_level_parse.c already checks that two saves give the same bytes.
 * Comparing the LevelDefs themselves catches what that cannot: a field
 * that is loaded but never saved is lost the same way on both saves, so
 * the two files still match while the designer's level has changed.
 *
 * Comparing with memcmp is safe here because every LevelDef in the test
 * comes from level_load_toml, which starts from level_def_init_defaults
 * (a memset) and so leaves unused slots, string tails and padding at zero.
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

static char input_path[1024];
static char saved_path[1024];
static char recovery_path[1024];

/* Normal exit only: after abort() the failing input stays on disk. */
static void remove_temp_files(void)
{
    remove(input_path);
    remove(saved_path);
    remove(recovery_path);
}

/* Per-process names keep parallel fuzzers (-jobs=N) from sharing files. */
static void init_paths(void)
{
    const char *dir;

    if (input_path[0] != '\0') return;
    atexit(remove_temp_files);
    dir = getenv("TMPDIR");
    if (!dir || dir[0] == '\0') dir = "/tmp";
    snprintf(input_path, sizeof(input_path), "%s/mango-editor-fuzz-%ld-in.toml",
             dir, (long)getpid());
    snprintf(saved_path, sizeof(saved_path), "%s/mango-editor-fuzz-%ld-save.toml",
             dir, (long)getpid());
    snprintf(recovery_path, sizeof(recovery_path),
             "%s/mango-editor-fuzz-%ld-recovery.toml", dir, (long)getpid());
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

static void round_trip_failed(const char *step)
{
    fprintf(stderr, "fuzz_editor_roundtrip: saved level did not come back at %s "
                    "(input kept at %s)\n", step, input_path);
    abort();
}

/* Abort, naming the first byte offset where the two levels differ. */
static void expect_same_level(const char *step, const LevelDef *want,
                              const LevelDef *got)
{
    const unsigned char *a = (const unsigned char *)want;
    const unsigned char *b = (const unsigned char *)got;

    for (size_t i = 0; i < sizeof(*want); i++) {
        if (a[i] != b[i]) {
            fprintf(stderr, "fuzz_editor_roundtrip: LevelDef differs at byte "
                            "%zu of %zu\n", i, sizeof(*want));
            round_trip_failed(step);
        }
    }
}

/* 1 when text holds a byte the recovery reader refuses: U+0000..U+001F, DEL. */
static int has_control_byte(const char *text)
{
    for (const unsigned char *p = (const unsigned char *)text; *p; p++)
        if (*p < 0x20 || *p == 0x7F) return 1;
    return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    /* LevelDef is large; static storage keeps it off the fuzzer's stack. */
    static LevelDef opened;
    static LevelDef reopened;
    char destination[LEVEL_NAME_CAPACITY + 1];
    int found;

    init_paths();
    if (write_bytes(input_path, data, size) != 0) return 0;
    if (level_load_toml(input_path, &opened) != 0) return 0;  /* rejected */

    /* The one rewrite saving makes on purpose: a level with an empty name is
     * written as "Untitled" (serializer_save.c). Expect that, not the input. */
    if (opened.name[0] == '\0')
        snprintf(opened.name, sizeof(opened.name), "%s", "Untitled");

    /* Save As to a new file, then open it again. */
    remove(saved_path);
    if (level_save_toml_with_policy(&opened, saved_path,
                                    SERIALIZER_SAVE_CREATE_ONLY) != 0)
        round_trip_failed("save");
    if (level_load_toml(saved_path, &reopened) != 0) round_trip_failed("reopen");
    expect_same_level("reopen", &opened, &reopened);

    /* Autosave snapshot, with the name standing in for a fuzzed path. */
    if (level_save_toml_recovery(&opened, recovery_path, opened.name) != 0)
        round_trip_failed("recovery save");
    if (level_load_toml(recovery_path, &reopened) != 0)
        round_trip_failed("recovery reopen");
    expect_same_level("recovery reopen", &opened, &reopened);

    found = level_read_recovery_path(recovery_path, destination, sizeof(destination));
    if (has_control_byte(opened.name)) {
        if (found != -1) round_trip_failed("recovery path with a control byte");
    } else if (found != 1 || strcmp(destination, opened.name) != 0) {
        round_trip_failed("recovery path");
    }
    return 0;
}
