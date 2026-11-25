/*
 * check.h — The whole test "framework" of the native tests, in one header.
 *
 * It does two jobs and nothing more:
 *
 *   1. CHECK(condition) — the assertion. When the condition is false it
 *      prints where it failed and the condition's own text, marks the case
 *      as failed and jumps to the case's clean-up code.
 *   2. test_run_cases() — the runner for a table of named cases. It reads
 *      the command line, so every program built on it takes the same flags:
 *
 *        --list            print the case names, one per line, and stop
 *        --case NAME       run only this case (repeat to run several)
 *        --shuffle[=SEED]  run the cases in a random order; the seed is
 *                          printed, so a failing order can be run again
 *
 *      MANGO_TEST_SHUFFLE=SEED in the environment does what --shuffle=SEED
 *      does (MANGO_TEST_SHUFFLE=random picks a seed), which shuffles every
 *      program of `make test` at once.
 *
 * Why a case looks the way it does
 * --------------------------------
 * C has no exceptions and no destructors, so a case that opened a level,
 * allocated memory or created files has to undo that itself on every path.
 * The pattern every case here follows keeps that in one place:
 *
 *     static int saves_and_reloads(void)
 *     {
 *         int failed = 0;              // CHECK sets this to 1
 *         char *text = NULL;
 *         CHECK(load("a.toml", &text) == 0);
 *         CHECK(strstr(text, "name") != NULL);
 *     done:                            // CHECK jumps here on failure
 *         free(text);                  // clean-up runs on both paths
 *         return failed;               // 0 = pass, nonzero = fail
 *     }
 *
 * So CHECK needs a local `int failed` and a `done:` label in the function
 * that uses it; the compiler says so if either is missing. CHECK_GOTO(c, l)
 * is the same check for functions that jump to another label and return
 * their own failure value there.
 *
 * Why shuffle
 * -----------
 * The cases of one program share one process: one raylib window, the input
 * queues, file-scope statics, scratch files. A case that leaves any of that
 * changed can make a later case pass (or fail) only because of the order in
 * the table. Running the table in another order exposes that; the fix is to
 * reset the shared state in the runner's after_each hook (or in the case),
 * not to put the cases back in an order that happens to work.
 *
 * Everything here is `static inline`, so each test file that includes this
 * header gets its own copy and no extra object file is needed.
 */
#ifndef MANGO_TESTS_CHECK_H
#define MANGO_TESTS_CHECK_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Print "file:line: condition" for a failed check. */
static inline void check_report(const char *file, int line, const char *text)
{
    fprintf(stderr, "%s:%d: CHECK failed: %s\n", file, line, text);
}

/* Fail the case and jump to its `done:` label (see the comment above). */
#define CHECK(condition) do { if (!(condition)) { \
    check_report(__FILE__, __LINE__, #condition); \
    failed = 1; goto done; } } while (0)

/* Report a failure and jump to `label`; that code returns the failure. */
#define CHECK_GOTO(condition, label) do { if (!(condition)) { \
    check_report(__FILE__, __LINE__, #condition); \
    goto label; } } while (0)

/* One row of a case table: the name the flags use, and the function. */
typedef struct TestCase {
    const char *name;
    int (*run)(void);  /* 0 = pass */
} TestCase;

/* Build a table row from a function name: TEST_CASE(f) is {"f", f}. */
#define TEST_CASE(fn) {#fn, fn}

/* Optional work around each case. before_each returning nonzero stops the
 * run (the program cannot test anything without, say, its window);
 * after_each resets whatever state the cases share. Either may be NULL. */
typedef struct TestHooks {
    int (*before_each)(void);
    void (*after_each)(void);
} TestHooks;

/* A small, portable random generator (xorshift32), so the same seed gives
 * the same order on every platform and C library. */
static inline uint32_t check_next_random(uint32_t *state)
{
    uint32_t x = *state ? *state : 0x9E3779B9u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return *state = x;
}

/* Parse a seed; "random" (or nothing) picks one from the clock. */
static inline uint32_t check_parse_seed(const char *text)
{
    if (!text || !*text || strcmp(text, "random") == 0)
        return (uint32_t)time(NULL) ^ (uint32_t)clock();
    return (uint32_t)strtoul(text, NULL, 10);
}

/*
 * Run the cases the command line selects, print one PASS/FAIL line each
 * ("<suite>: <name> PASS") and a total, and return the exit status: 0 when
 * every selected case passed, 1 otherwise (or on a bad flag). Nonzero from
 * a case counts as a failure; the remaining cases still run.
 */
static inline int test_run_cases(const char *suite, const TestCase *cases, size_t count,
                                 TestHooks hooks, int argc, char **argv)
{
    int list = 0, shuffle = 0, failures = 0, ran = 0, status = 1;
    uint32_t seed = 0;
    const char *env = getenv("MANGO_TEST_SHUFFLE");
    size_t *order = malloc((count ? count : 1) * sizeof(*order));
    unsigned char *wanted = calloc(count ? count : 1, 1);
    size_t wanted_count = 0;

    if (!order || !wanted) goto out;
    if (env && *env) { shuffle = 1; seed = check_parse_seed(env); }
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (strcmp(arg, "--list") == 0) {
            list = 1;
        } else if (strcmp(arg, "--shuffle") == 0) {
            shuffle = 1; seed = check_parse_seed(NULL);
        } else if (strncmp(arg, "--shuffle=", 10) == 0) {
            shuffle = 1; seed = check_parse_seed(arg + 10);
        } else if (strcmp(arg, "--case") == 0 && i + 1 < argc) {
            const char *name = argv[++i];
            size_t c = 0;
            while (c < count && strcmp(cases[c].name, name) != 0) c++;
            if (c == count) {
                fprintf(stderr, "%s: no case named '%s' (--list shows them)\n", suite, name);
                goto out;
            }
            if (!wanted[c]) wanted_count++;
            wanted[c] = 1;
        } else {
            fprintf(stderr, "%s: unknown argument '%s'\n"
                    "usage: [--list] [--case NAME]... [--shuffle[=SEED]]\n", suite, arg);
            goto out;
        }
    }
    if (list) {
        for (size_t c = 0; c < count; c++) printf("%s\n", cases[c].name);
        status = 0;
        goto out;
    }

    for (size_t c = 0; c < count; c++) order[c] = c;
    if (shuffle) {
        uint32_t state = seed;
        /* Fisher-Yates: swap each slot with a random one at or before it. */
        for (size_t c = count; c > 1; c--) {
            size_t pick = check_next_random(&state) % c;
            size_t keep = order[c - 1];
            order[c - 1] = order[pick];
            order[pick] = keep;
        }
        printf("%s: shuffled order, seed %lu (run again with --shuffle=%lu)\n",
               suite, (unsigned long)seed, (unsigned long)seed);
    }

    for (size_t i = 0; i < count; i++) {
        const TestCase *test = &cases[order[i]];
        if (wanted_count && !wanted[order[i]]) continue;
        if (hooks.before_each && hooks.before_each() != 0) {
            fprintf(stderr, "%s: setup before %s failed\n", suite, test->name);
            failures++;
            break;
        }
        int result = test->run();
        if (hooks.after_each) hooks.after_each();
        printf("%s: %s %s\n", suite, test->name, result ? "FAIL" : "PASS");
        failures += result != 0;
        ran++;
    }
    printf("%s: %d of %d cases failing\n", suite, failures, ran);
    if (failures && shuffle)
        printf("%s: the order came from seed %lu; repeat it with --shuffle=%lu\n",
               suite, (unsigned long)seed, (unsigned long)seed);
    status = failures ? 1 : 0;
out:
    free(order);
    free(wanted);
    return status;
}

#endif
