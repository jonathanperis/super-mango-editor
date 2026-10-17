#include "../core/game_profile.h"

int game_settings_key_debug_reserved(int key)
{
    /* F2-F10, '-' and '=' drive the --debug inspector. One profile serves
     * normal and debug runs, so a key bound in normal play (Jump on F2)
     * would both jump and freeze the game in --debug: refuse them always. */
    return (key >= BINDING_KEY_F2 && key <= BINDING_KEY_F10) ||
           key == BINDING_KEY_MINUS || key == BINDING_KEY_EQUAL;
}

int game_settings_key_allowed(int key)
{
    if (!input_binding_known(key) || game_settings_key_debug_reserved(key)) return 0;
    /* Menu, pause and settings keys, the fixed arrow alternatives and the
     * fixed Run key stay what they are, so a remap can never strand them. */
    return key != BINDING_KEY_ESCAPE && key != BINDING_KEY_F1 && key != BINDING_KEY_TAB &&
           key != BINDING_KEY_ENTER && key != BINDING_KEY_KP_ENTER &&
           key != BINDING_KEY_LEFT && key != BINDING_KEY_RIGHT &&
           key != BINDING_KEY_UP && key != BINDING_KEY_DOWN &&
           key != BINDING_KEY_RIGHT_SHIFT;
}

int game_settings_button_allowed(int button)
{
    return button >= 0 && button < PAD_COUNT && button != PAD_BACK &&
           button != PAD_START && button != PAD_GUIDE && button != PAD_B;
}

int game_settings_has_unavailable_binding(const GameSettings *settings)
{
    for (int i = 0; i < PROFILE_ACTION_COUNT; i++)
        if (!input_key_from_binding(settings->keys[i]) || !input_pad_button(settings->buttons[i])) return 1;
    return 0;
}

static int in_range(int value, int min, int max) { return value >= min && value <= max; }

int game_settings_valid(const GameSettings *s)
{
    /* One check per setting: numbers inside their ranges, switches 0 or 1. */
    if (!in_range(s->music_volume, 0, 128) || !in_range(s->effects_volume, 0, 128) ||
        !in_range(s->dead_zone, 0, 28000) || !in_range(s->window_scale, 1, 4)) return 0;
    const int switches[] = {s->muted, s->high_contrast, s->reduced_motion, s->ghost, s->fullscreen};
    for (size_t i = 0; i < sizeof(switches) / sizeof(switches[0]); i++)
        if (!in_range(switches[i], 0, 1)) return 0;
    for (int i = 0; i < PROFILE_ACTION_COUNT; i++) {
        if (!game_settings_key_allowed(s->keys[i]) || !game_settings_button_allowed(s->buttons[i])) return 0;
        for (int j = 0; j < i; j++)
            if (s->keys[i] == s->keys[j] || s->buttons[i] == s->buttons[j]) return 0;
    }
    return 1;
}
