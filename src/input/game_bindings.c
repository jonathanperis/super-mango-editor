#include "../core/game_profile.h"

int game_settings_key_allowed(SDL_Scancode key)
{
    if (key <= SDL_SCANCODE_UNKNOWN || key >= SDL_NUM_SCANCODES || !SDL_GetScancodeName(key)[0]) return 0;
    return key != SDL_SCANCODE_ESCAPE && key != SDL_SCANCODE_F1 && key != SDL_SCANCODE_TAB &&
           key != SDL_SCANCODE_RETURN && key != SDL_SCANCODE_KP_ENTER &&
           key != SDL_SCANCODE_LEFT && key != SDL_SCANCODE_RIGHT &&
           key != SDL_SCANCODE_UP && key != SDL_SCANCODE_DOWN && key != SDL_SCANCODE_RSHIFT;
}

int game_settings_button_allowed(SDL_GameControllerButton button)
{
    return button >= 0 && button < SDL_CONTROLLER_BUTTON_MAX &&
           button != SDL_CONTROLLER_BUTTON_BACK && button != SDL_CONTROLLER_BUTTON_START &&
           button != SDL_CONTROLLER_BUTTON_GUIDE && button != SDL_CONTROLLER_BUTTON_B;
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
