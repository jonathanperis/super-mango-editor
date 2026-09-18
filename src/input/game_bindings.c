#include "../core/game_profile.h"

int game_settings_key_allowed(int key)
{
    if (!input_binding_known(key)) return 0;
    return key != 41 && key != 58 && key != 43 && key != 40 && key != 88 &&
           key != 80 && key != 79 && key != 82 && key != 81 && key != 229;
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

int game_settings_valid(const GameSettings *s)
{
    if (s->music_volume < 0 || s->music_volume > 128 || s->effects_volume < 0 || s->effects_volume > 128 ||
        s->dead_zone < 0 || s->dead_zone > 28000 || s->window_scale < 1 || s->window_scale > 4 ||
        (s->muted != 0 && s->muted != 1) || (s->high_contrast != 0 && s->high_contrast != 1) ||
        (s->reduced_motion != 0 && s->reduced_motion != 1)) return 0;
    for (int i = 0; i < PROFILE_ACTION_COUNT; i++) {
        if (!game_settings_key_allowed(s->keys[i]) || !game_settings_button_allowed(s->buttons[i])) return 0;
        for (int j = 0; j < i; j++)
            if (s->keys[i] == s->keys[j] || s->buttons[i] == s->buttons[j]) return 0;
    }
    return 1;
}
