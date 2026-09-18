/*
 * main.c — Turn command-line arguments into one application session.
 *
 * Read this file first, then core/app_session.c. The entry point chooses how
 * to start; AppSession owns the raylib window/audio, active screen and loop.
 * Keeping those jobs separate lets native and browser builds share the game.
 */
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game.h"
#include "core/app_session.h"
#include "core/game_random.h"

static const char *argument(int argc, char **argv, int *index)
{
    /* argv strings belong to the C runtime. Return a borrowed pointer and
     * advance the caller's index so the value is not parsed as another flag. */
    if (*index + 1 >= argc || !argv[*index + 1][0] || argv[*index + 1][0] == '-')
        return NULL;
    return argv[++*index];
}

static int unsigned_argument(const char *text, unsigned int *result)
{
    if (!text || text[0] == '-') return -1;
    char *end;
    /* Unlike atoi, strtoul lets us reject overflow and trailing characters.
     * Check the range before narrowing unsigned long to unsigned int. */
    errno = 0;
    unsigned long value = strtoul(text, &end, 10);
    if (errno || end == text || *end || value > UINT_MAX) return -1;
    *result = (unsigned int)value;
    return 0;
}

int main(int argc, char **argv)
{
    /* The designated initializer enables saving and zero-initializes the
     * remaining members. A NULL level path selects the campaign menu. */
    AppSessionConfig config = {.profile_enabled = 1};
    int seed_set = 0;
    for (int i = 1; i < argc; i++) {
        const char *option = argv[i];
        if (!strcmp(option, "--debug"))
            config.debug_mode = 1;
        else if (!strcmp(option, "--sandbox"))
            config.level_path = "levels/00_sandbox_01.toml";
        else if (!strcmp(option, "--no-save"))
            config.profile_enabled = 0;
        else if (!strcmp(option, "--continue"))
            config.continue_last = 1;
        else if (!strcmp(option, "--level") || !strcmp(option, "--profile") ||
                 !strcmp(option, "--replay-script") || !strcmp(option, "--experiment")) {
            const char *value = argument(argc, argv, &i);
            if (!value) {
                fprintf(stderr, "Error: %s requires a path\n", option);
                return EXIT_FAILURE;
            }
            if (!strcmp(option, "--level"))
                config.level_path = value;
            else if (!strcmp(option, "--profile"))
                config.profile_path = value;
            else if (!strcmp(option, "--replay-script"))
                config.replay_script_path = value;
            else {
                config.experiment_path = value;
                config.debug_mode = 1;
                config.profile_enabled = 0;
            }
        } else if (!strcmp(option, "--seed") || !strcmp(option, "--smoke-test-frames")) {
            unsigned int value;
            const char *text = argument(argc, argv, &i);
            if (unsigned_argument(text, &value)) {
                fprintf(stderr, "Error: %s requires an unsigned integer\n", option);
                return EXIT_FAILURE;
            }
            if (!strcmp(option, "--seed")) {
                config.random_seed = value;
                seed_set = 1;
            }
            else {
                if (!value || value > INT_MAX) {
                    fprintf(stderr, "Error: --smoke-test-frames requires a positive integer\n");
                    return EXIT_FAILURE;
                }
                config.smoke_test_frames = (int)value;
            }
        } else if (!strcmp(option, "--help")) {
            puts("Super Mango: --level PATH | --continue | --sandbox\n"
                 "  --profile PATH   explicit native player profile\n"
                 "  --no-save        memory-only settings/results\n"
                 "  --experiment PATH --level PATH   replay a captured experiment\n"
                 "Debug: F2 freeze, F3 step, F4 slow, F6 field, -/+ tune, F7 reset, F8 record, F9 export.\n"
                 "  --debug --seed N --smoke-test-frames N --replay-script NAME\n"
                 "F1: settings (menu: gamepad Y; gameplay: Back).\n"
                 "Continue opens the last stage, not a mid-level save.\n"
                 "Profiles: OS preference directory on native; localStorage on web.\n"
                 "Smoke/replay never reads or writes personal profiles.");
            return EXIT_SUCCESS;
        } else if (option[0] == '-') {
            fprintf(stderr, "Error: unknown option '%s'\n", option);
            return EXIT_FAILURE;
        }
    }
    if (config.experiment_path && (!config.level_path || config.replay_script_path)) {
        fprintf(stderr, "Error: --experiment requires --level and cannot combine with --replay-script\n");
        return EXIT_FAILURE;
    }
    if (config.smoke_test_frames > 0 && !config.level_path)
        config.level_path = "levels/00_sandbox_01.toml";

    /* A fixed seed makes enemy/decorative randomness reproducible. A normal
     * run without --seed varies its starting state using a monotonic clock. */
    if (!seed_set) config.random_seed = (unsigned int)clock_millis();
    game_random_seed(config.random_seed);
    /* session_create unwinds partially initialized resources on failure. */
    AppSession *session = session_create(&config);
    if (!session) return EXIT_FAILURE;
    int result = session_run(session);
#ifndef __EMSCRIPTEN__
    /* Native session_run blocks until exit, so main can destroy its session.
     * The browser call returns after registering a callback; that callback
     * retains the session and eventually releases it during terminal cleanup. */
    session_destroy(&session);
#endif
    return result;
}
