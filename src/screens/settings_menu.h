#pragma once
#include "../core/game_profile.h"
#include "../shared/ui.h"

typedef struct SettingsMenu {
    int open, page, selected, capture; /* capture: 0 none, 1 keyboard, 2 gamepad */
    char message[128];
    float wheel_rest; /* wheel notches not yet turned into a step */
    int   wheel_row;  /* the row wheel_rest was gathered on; -1 = none */
    UIState ui;
} SettingsMenu;

/* Open on the first row of the main page with no leftover message. The
 * start menu's Settings button and the F1/Y/Back keys both use this. */
void settings_menu_open(SettingsMenu *menu);

/* Returns 1 only for events consumed by settings; quit/hot-plug still propagate. */
int settings_menu_event(SettingsMenu *menu, GameProfile *profile,
                         const InputEvent *event, int open_button);
/* controller is the screen's adopted gamepad (device index + 1, 0 = none);
 * the controls page names it. */
void settings_menu_render(SettingsMenu *menu, const GameProfile *profile,
                           TextFont *font, int controller);
void settings_menu_cleanup(SettingsMenu *menu);

/* The style the game's menus draw with: their own palette, or
 * UI_STYLE_HIGH_CONTRAST when that setting is on. */
UIStyle settings_menu_style(int high_contrast);
