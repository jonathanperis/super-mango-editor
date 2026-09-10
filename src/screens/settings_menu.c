#include "settings_menu.h"
#include <stdio.h>
#include <string.h>

static const char *const actions[PROFILE_ACTION_COUNT] = {"Left", "Right", "Up", "Down", "Jump", "Run"};

static void changed(GameProfile *profile)
{
    profile->dirty = 1;
    profile->revision++;
}

static void adjust(SettingsMenu *menu, GameProfile *profile, int direction)
{
    GameSettings *s = &profile->data.settings;
    int *value = NULL, step = 1, low = 0, high = 1;
    switch (menu->selected) {
        case 0: value = &s->music_volume; high = 128; step = 8; break;
        case 1: value = &s->effects_volume; high = 128; step = 8; break;
        case 2: value = &s->muted; break;
        case 3: value = &s->dead_zone; high = 28000; step = 1000; break;
        case 4: value = &s->window_scale; low = 1; high = 4; break;
        case 5: value = &s->high_contrast; break;
        case 6: value = &s->reduced_motion; break;
        default: return;
    }
    int updated = *value + step * direction;
    if (high == 1) updated = !*value;
    if (updated < low) updated = low;
    if (updated > high) updated = high;
    if (*value != updated) { *value = updated; changed(profile); }
}

static void activate(SettingsMenu *menu, GameProfile *profile)
{
    menu->message[0] = '\0';
    if (menu->page) {
        if (menu->selected == 12) { menu->page = 0; menu->selected = 7; }
        else menu->capture = menu->selected < 6 ? 1 : 2;
    } else if (menu->selected == 7) { menu->page = 1; menu->selected = 0; }
    else if (menu->selected == 8) {
        profile->data.settings = (GameSettings)GAME_SETTINGS_DEFAULTS;
        changed(profile);
    } else if (menu->selected == 9) menu->open = 0;
    else adjust(menu, profile, 1);
}

int settings_menu_event(SettingsMenu *menu, GameProfile *profile,
                         const SDL_Event *event, int open_button)
{
    if (!menu || !profile) return 0;
    int keyboard = event->type == SDL_KEYDOWN;
    int controller = event->type == SDL_CONTROLLERBUTTONDOWN;
    if (event->type == SDL_QUIT || event->type == SDL_WINDOWEVENT ||
        event->type == SDL_CONTROLLERDEVICEADDED || event->type == SDL_CONTROLLERDEVICEREMOVED) return 0;
    if (keyboard && event->key.repeat) return menu->open;
    SDL_Keycode key = keyboard ? event->key.keysym.sym : SDLK_UNKNOWN;
    int button = controller ? event->cbutton.button : -1;
    if (!menu->open) {
        if (key != SDLK_F1 && !(controller && button == open_button)) return 0;
        menu->open = 1; menu->page = menu->selected = menu->capture = 0;
        menu->message[0] = '\0';
        return 1;
    }
    if (key == SDLK_ESCAPE || (controller && (button == SDL_CONTROLLER_BUTTON_B || button == SDL_CONTROLLER_BUTTON_BACK))) {
        if (menu->capture) menu->capture = 0;
        else menu->open = 0;
        return 1;
    }
    if (menu->capture) {
        GameSettings candidate = profile->data.settings;
        int action = menu->selected % PROFILE_ACTION_COUNT;
        if (menu->capture == 1 && keyboard) candidate.keys[action] = event->key.keysym.scancode;
        else if (menu->capture == 2 && controller) candidate.buttons[action] = (SDL_GameControllerButton)button;
        else return 1;
        if (game_settings_valid(&candidate)) {
            profile->data.settings = candidate; changed(profile); menu->capture = 0;
            menu->message[0] = '\0';
        } else snprintf(menu->message, sizeof(menu->message), "Reserved/duplicate binding. Choose another; Esc cancels.");
        return 1;
    }
    int rows = menu->page ? 13 : 10;
    if (key == SDLK_UP || button == SDL_CONTROLLER_BUTTON_DPAD_UP) menu->selected = (menu->selected + rows - 1) % rows;
    if (key == SDLK_DOWN || button == SDL_CONTROLLER_BUTTON_DPAD_DOWN) menu->selected = (menu->selected + 1) % rows;
    if (!menu->page && (key == SDLK_LEFT || button == SDL_CONTROLLER_BUTTON_DPAD_LEFT)) adjust(menu, profile, -1);
    if (!menu->page && (key == SDLK_RIGHT || button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) adjust(menu, profile, 1);
    if (key == SDLK_RETURN || key == SDLK_KP_ENTER || key == SDLK_SPACE ||
        button == SDL_CONTROLLER_BUTTON_A || button == SDL_CONTROLLER_BUTTON_START) activate(menu, profile);
    if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT) {
        int row = (event->button.y - 36) / 16;
        if (event->button.x >= 18 && event->button.x < 382 && event->button.y >= 36 && row < rows) {
            menu->selected = row; activate(menu, profile);
        }
    }
    return 1;
}

void settings_menu_render(SettingsMenu *menu, const GameProfile *profile,
                           SDL_Renderer *renderer, TTF_Font *font)
{
    if (!menu || !profile || (!menu->open && !profile->error)) return;
    if (menu->ui.renderer != renderer || menu->ui.font != font) ui_init(&menu->ui, renderer, font);
    if (!menu->open) {
        SDL_Rect warning = {8,278,384,18};
        SDL_SetRenderDrawColor(renderer, 10, 12, 18, 255); SDL_RenderFillRect(renderer,&warning);
        char message[64]; SDL_utf8strlcpy(message,profile->status,sizeof(message));
        ui_label_color(&menu->ui,12,281,message,(SDL_Color){255,190,90,255});
        return;
    }
    SDL_SetRenderDrawColor(renderer, 10, 12, 18, 255);
    SDL_Rect panel = {8, 4, 384, 292};
    SDL_RenderFillRect(renderer, &panel);
    ui_label(&menu->ui, 18, 12, menu->page ? "CONTROLS" : "SETTINGS");
    const GameSettings *s = &profile->data.settings;
    int rows = menu->page ? 13 : 10;
    for (int row = 0; row < rows; row++) {
        char label[160];
        if (menu->page && row < 12) {
            int action = row % PROFILE_ACTION_COUNT;
            const char *binding = row < 6 ? SDL_GetScancodeName(s->keys[action]) : SDL_GameControllerGetStringForButton(s->buttons[action]);
            snprintf(label, sizeof(label), "%s %-6s : %s", row < 6 ? "Key" : "Pad", actions[action], binding ? binding : "?");
        } else if (menu->page) snprintf(label, sizeof(label), "Back to settings");
        else switch (row) {
            case 0: snprintf(label,sizeof(label),"Music volume: %d%%",s->music_volume*100/128); break;
            case 1: snprintf(label,sizeof(label),"Effects volume: %d%%",s->effects_volume*100/128); break;
            case 2: snprintf(label,sizeof(label),"Mute: %s",s->muted?"On":"Off"); break;
            case 3: snprintf(label,sizeof(label),"Stick dead zone: %d",s->dead_zone); break;
            case 4: snprintf(label,sizeof(label),"Window scale (native): %dx",s->window_scale); break;
            case 5: snprintf(label,sizeof(label),"High-contrast outlines: %s",s->high_contrast?"On":"Off"); break;
            case 6: snprintf(label,sizeof(label),"Reduced motion: %s",s->reduced_motion?"On":"Off"); break;
            case 7: snprintf(label,sizeof(label),"Configure controls..."); break;
            case 8: snprintf(label,sizeof(label),"Restore default settings"); break;
            default: snprintf(label,sizeof(label),"Save and close"); break;
        }
        int y = 36 + row*16;
        if (row == menu->selected) {
            SDL_SetRenderDrawColor(renderer, 40, 70, 105, 255);
            SDL_Rect highlight = {16,y-1,368,16}; SDL_RenderFillRect(renderer,&highlight);
        }
        ui_label(&menu->ui, 20, y, label);
    }
    ui_label(&menu->ui, 18, 249, menu->capture ? "Press a new binding. Esc cancels." : "Arrows/D-pad: choose/change. Enter/A: select.");
    ui_label(&menu->ui, 18, 265, "Esc/B: close. Arrow keys stay available in game.");
    const char *status = menu->message[0] ? menu->message : profile->status;
    if (status[0]) {
        char clipped[64]; SDL_utf8strlcpy(clipped, status, sizeof(clipped));
        ui_label_color(&menu->ui, 18, 280, clipped, (SDL_Color){255,190,90,255});
    }
}

void settings_menu_cleanup(SettingsMenu *menu)
{
    ui_cleanup(&menu->ui);
    menu->ui.renderer = NULL; menu->ui.font = NULL;
    menu->open = menu->capture = 0;
}
