/*
 * fuzz_replay_main.c — Standalone driver for the fuzz harnesses.
 *
 * Every harness defines LLVMFuzzerTestOneInput(), the libFuzzer entry point.
 * `make fuzz` links a harness with clang's -fsanitize=fuzzer, which supplies
 * its own main() and searches for new inputs by coverage.  Apple clang ships
 * without libFuzzer, so this file supplies a plain main() instead:
 *
 *     fuzz-level-replay DIR_OR_FILE...            replay every input once
 *     fuzz-level-replay -mutate=N DIR_OR_FILE...  also try N random byte
 *                                                 mutations of each input
 *
 * Built with ASan/UBSan (`make fuzz-corpus`), a replay turns any memory or
 * undefined-behavior bug reached by the seeds into a test failure.  Mutation
 * mode is a blind, deterministic stand-in for real fuzzing: much weaker than
 * coverage guidance, but it needs nothing beyond the normal toolchain.
 * POSIX only (opendir/readdir); the harnesses are not built on Windows.
 */

#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

#define REPLAY_MAX_INPUT (1u << 20)  /* 1 MiB; seeds are a few KiB */

static unsigned long mutations_per_input;
static uint32_t random_state = 0x6d616e67u;  /* "mang": fixed for replays */
static size_t inputs_run;

/* xorshift32: tiny, deterministic and good enough to pick bytes. */
static uint32_t next_random(void)
{
    random_state ^= random_state << 13;
    random_state ^= random_state >> 17;
    random_state ^= random_state << 5;
    return random_state;
}

/*
 * Apply 1-4 edits: overwrite, insert or delete a byte.  Overwrites favor
 * bytes TOML cares about so mutations reach past the tokenizer.
 */
static size_t mutate(uint8_t *data, size_t size, size_t capacity)
{
    static const char interesting[] = "=[]{}\"'\\.,#\n-+0123456789eEu";
    int edits = 1 + (int)(next_random() % 4);

    for (int i = 0; i < edits; i++) {
        uint32_t choice = next_random();
        size_t at = size ? next_random() % size : 0;
        uint8_t byte = (choice & 1)
            ? (uint8_t)interesting[next_random() % (sizeof(interesting) - 1)]
            : (uint8_t)next_random();

        if (choice % 3 == 0 && size < capacity) {
            memmove(data + at + 1, data + at, size - at);
            data[at] = byte;
            size++;
        } else if (choice % 3 == 1 && size > 0) {
            memmove(data + at, data + at + 1, size - at - 1);
            size--;
        } else if (size > 0) {
            data[at] = byte;
        }
    }
    return size;
}

static int run_file(const char *path)
{
    FILE *fp = fopen(path, "rb");
    uint8_t *original;
    uint8_t *work;
    size_t size;

    if (!fp) {
        fprintf(stderr, "fuzz replay: cannot open %s\n", path);
        return -1;
    }
    original = malloc(REPLAY_MAX_INPUT);
    work = malloc(REPLAY_MAX_INPUT);
    if (!original || !work) {
        fclose(fp);
        free(original);
        free(work);
        return -1;
    }
    size = fread(original, 1, REPLAY_MAX_INPUT, fp);
    fclose(fp);

    /* Copy into an exact-size buffer so ASan catches reads past the end. */
    {
        uint8_t *exact = malloc(size ? size : 1);
        if (!exact) { free(original); free(work); return -1; }
        memcpy(exact, original, size);
        LLVMFuzzerTestOneInput(exact, size);
        free(exact);
        inputs_run++;
    }

    for (unsigned long i = 0; i < mutations_per_input; i++) {
        size_t mutated_size;
        uint8_t *exact;

        memcpy(work, original, size);
        mutated_size = mutate(work, size, REPLAY_MAX_INPUT);
        exact = malloc(mutated_size ? mutated_size : 1);
        if (!exact) break;
        memcpy(exact, work, mutated_size);
        LLVMFuzzerTestOneInput(exact, mutated_size);
        free(exact);
        inputs_run++;
    }

    free(original);
    free(work);
    return 0;
}

static int run_path(const char *path)
{
    struct stat st;
    DIR *dir;
    struct dirent *entry;
    int failed = 0;

    if (stat(path, &st) != 0) {
        fprintf(stderr, "fuzz replay: missing %s\n", path);
        return -1;
    }
    if (!S_ISDIR(st.st_mode)) return run_file(path);

    dir = opendir(path);
    if (!dir) return -1;
    while ((entry = readdir(dir)) != NULL) {
        char child[4096];
        struct stat child_st;
        int written;

        if (entry->d_name[0] == '.') continue;  /* ., .., .DS_Store */
        written = snprintf(child, sizeof(child), "%s/%s", path, entry->d_name);
        if (written < 0 || (size_t)written >= sizeof(child)) continue;
        if (stat(child, &child_st) != 0 || !S_ISREG(child_st.st_mode)) continue;
        if (run_file(child) != 0) failed = 1;
    }
    closedir(dir);
    return failed ? -1 : 0;
}

int main(int argc, char **argv)
{
    int failed = 0;
    int paths = 0;

    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "-mutate=", 8) == 0) {
            mutations_per_input = strtoul(argv[i] + 8, NULL, 10);
            continue;
        }
        if (strncmp(argv[i], "-seed=", 6) == 0) {
            random_state = (uint32_t)strtoul(argv[i] + 6, NULL, 10);
            if (random_state == 0) random_state = 1;  /* xorshift needs != 0 */
            continue;
        }
        paths++;
        if (run_path(argv[i]) != 0) failed = 1;
    }
    if (paths == 0) {
        fprintf(stderr, "usage: %s [-mutate=N] [-seed=S] DIR_OR_FILE...\n", argv[0]);
        return 2;
    }
    printf("fuzz replay: %zu inputs ok\n", inputs_run);
    return failed;
}
