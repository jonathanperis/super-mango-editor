/*
 * fuzz_bindings.c — Fuzz the key and button bindings.
 *
 * A player's controls are six keyboard keys and six controller buttons,
 * saved in the profile as numbers ("version-1 wire IDs", see
 * input_backend.h) and translated to raylib keys only at input time
 * (game_bindings.c, input_backend.c). The profile is a text file anyone can
 * edit, so every number can arrive. Each input is tried two ways:
 *
 *   1. As twelve 16-bit numbers: keys, then buttons. When
 *      game_settings_valid() accepts them,
 *        - no two actions share a key or a button, and no two keys reach
 *          the same physical key (raylib key code) either;
 *        - every key translates there and back to itself, and has a name;
 *        - a binding this backend cannot deliver (a legacy key, a paddle)
 *          is reported by game_settings_has_unavailable_binding();
 *        - saving them in a profile and loading it gives the same bindings.
 *      When it refuses them, a profile holding them must not load with
 *      them: the decoder may repair a debug-inspector key (it puts the
 *      default back), but must never accept a binding the validator
 *      refused.
 *   2. As the text of a `keys = ...` line inside an otherwise valid
 *      profile, which reaches the TOML array reading in game_profile.c. An
 *      accepted profile must hold valid settings and save, load and save
 *      again to the same text.
 *
 * See fuzz_replay_main.c for the standalone driver.  POSIX only.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/game_profile.h"
#include "input/input_backend.h"

static void fail(const char *what)
{
    fprintf(stderr, "fuzz_bindings: %s\n", what);
    abort();
}

static GameProfileData profile_with(const GameSettings *settings)
{
    GameProfileData data;
    memset(&data, 0, sizeof(data));
    data.settings = *settings;
    return data;
}

static void check_valid_settings(const GameSettings *s)
{
    static GameProfileData decoded;
    static char text[PROFILE_TEXT_MAX];

    for (int i = 0; i < PROFILE_ACTION_COUNT; i++) {
        int key = input_key_from_binding(s->keys[i]);
        if (!game_settings_key_allowed(s->keys[i]) || !game_settings_button_allowed(s->buttons[i]))
            fail("valid settings hold a binding that is not allowed");
        if (!input_binding_known(s->keys[i])) fail("valid settings hold an unknown key");
        const char *name = input_binding_name(s->keys[i]);
        if (!name || !name[0]) fail("a bound key has no name");
        /* A legacy binding may have no key on this backend (KEY_NULL), and
         * a paddle or touchpad binding no controller button: both are kept
         * and the settings screen says they are unavailable. A key that
         * does exist must translate back to the same binding. */
        if (key && input_binding_from_key(key) != s->keys[i])
            fail("a key does not translate back to its binding");
        if ((!key || !input_pad_button(s->buttons[i])) &&
            !game_settings_has_unavailable_binding(s))
            fail("a binding with nothing to press is not reported as unavailable");
        for (int j = 0; j < i; j++) {
            if (s->keys[i] == s->keys[j] || s->buttons[i] == s->buttons[j])
                fail("two actions share a binding");
            if (key && key == input_key_from_binding(s->keys[j]))
                fail("two bindings reach the same physical key");
        }
    }

    GameProfileData data = profile_with(s);
    if (game_profile_encode(&data, text, sizeof(text)) != 0) fail("valid settings did not save");
    if (game_profile_decode(&decoded, text) != 0) fail("saved settings did not load");
    if (memcmp(decoded.settings.keys, s->keys, sizeof(s->keys)) != 0 ||
        memcmp(decoded.settings.buttons, s->buttons, sizeof(s->buttons)) != 0)
        fail("bindings changed on the way through the profile");
}

/* An invalid set, written into a profile by hand (encode refuses it). */
static void check_invalid_settings(const GameSettings *s)
{
    static GameProfileData decoded;
    char text[512];
    int used = snprintf(text, sizeof(text), "format_version = 2\nkeys = [");
    for (int kind = 0; kind < 2; kind++) {
        for (int i = 0; i < PROFILE_ACTION_COUNT; i++)
            used += snprintf(text + used, sizeof(text) - (size_t)used, "%s%d", i ? ", " : "",
                             kind ? s->buttons[i] : s->keys[i]);
        used += snprintf(text + used, sizeof(text) - (size_t)used,
                         kind ? "]\n" : "]\nbuttons = [");
    }
    if (game_profile_decode(&decoded, text) != 0) return;   /* refused: good */
    if (!game_settings_valid(&decoded.settings)) fail("a loaded profile holds invalid settings");
    int repaired = 0;
    for (int i = 0; i < PROFILE_ACTION_COUNT; i++)
        repaired |= game_settings_key_debug_reserved(s->keys[i]);
    if (!repaired) fail("the decoder accepted bindings the validator refuses");
    if (memcmp(decoded.settings.buttons, s->buttons, sizeof(s->buttons)) != 0)
        fail("repairing a key changed the buttons");
}

static void check_keys_text(const uint8_t *data, size_t size)
{
    static GameProfileData decoded, again;
    static char text[4096], first[PROFILE_TEXT_MAX], second[PROFILE_TEXT_MAX];
    const char *prefix = "format_version = 2\nkeys = ";
    size_t prefix_len = strlen(prefix);

    if (size > sizeof(text) - prefix_len - 2 || memchr(data, '\0', size)) return;
    memcpy(text, prefix, prefix_len);
    memcpy(text + prefix_len, data, size);
    text[prefix_len + size] = '\n';
    text[prefix_len + size + 1] = '\0';
    if (game_profile_decode(&decoded, text) != 0) return;
    if (!game_settings_valid(&decoded.settings)) fail("accepted keys text gave invalid settings");
    if (game_profile_encode(&decoded, first, sizeof(first)) != 0 ||
        game_profile_decode(&again, first) != 0 ||
        game_profile_encode(&again, second, sizeof(second)) != 0 ||
        strcmp(first, second) != 0) fail("accepted keys text does not round-trip");
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    GameSettings settings = GAME_SETTINGS_DEFAULTS;
    for (int i = 0; i < 2 * PROFILE_ACTION_COUNT && (size_t)(2 * i + 1) < size; i++) {
        int value = (int16_t)(data[2 * i] | (data[2 * i + 1] << 8));
        if (i < PROFILE_ACTION_COUNT) settings.keys[i] = value;
        else settings.buttons[i - PROFILE_ACTION_COUNT] = value;
    }
    if (game_settings_valid(&settings)) check_valid_settings(&settings);
    else check_invalid_settings(&settings);
    check_keys_text(data, size);
    return 0;
}
