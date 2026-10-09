#pragma once
#include "../core/game_profile.h"
#include "../shared/ui.h"

typedef struct SettingsMenu {
    int open, page, selected, capture; /* capture: 0 none, 1 keyboard, 2 gamepad */
    char message[128];
    UIState ui;
} SettingsMenu;

/* Open on the first row of the main page with no leftover message. The
 * start menu's Settings button and the F1/Y/Back keys both use this. */
void settings_menu_open(SettingsMenu *menu);

/* Returns 1 only for events consumed by settings; quit/hot-plug still propagate. */
int settings_menu_event(SettingsMenu *menu, GameProfile *profile,
                         const InputEvent *event, int open_button);
void settings_menu_render(SettingsMenu *menu, const GameProfile *profile,
                           TextFont *font);
void settings_menu_cleanup(SettingsMenu *menu);
