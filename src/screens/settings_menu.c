#include "settings_menu.h"
#include "../shared/platform.h"
#include <stdio.h>
#include <string.h>

static const char *const actions[PROFILE_ACTION_COUNT] = {"Left", "Right", "Up", "Down", "Jump", "Run"};

static void shorten_utf8(char *text)
{
    size_t size = strlen(text);
    if (!size) return;
    do { size--; } while (size && ((unsigned char)text[size] & 0xc0) == 0x80);
    text[size] = '\0';
}

static void render_status(UIState *ui, const char *text)
{
    for (int row = 0; row < 2 && *text; row++) {
        char line[160];
        utf8_copy(line, text, sizeof(line));
        while (line[0] && ui_text_width(ui, line) > 364) shorten_utf8(line);
        size_t consumed = strlen(line);
        if (row == 0 && text[consumed]) {
            char *space = strrchr(line, ' ');
            if (space && space != line) { *space = '\0'; consumed = (size_t)(space - line); }
        } else if (row == 1 && text[consumed]) {
            while (line[0] && (strlen(line) + 3 >= sizeof(line) ||
                   ui_text_width(ui, line) + ui_text_width(ui, "...") > 364)) shorten_utf8(line);
            strcat(line, "...");
        }
        ui_label_color(ui, 18, 265 + row * 15, line, (Color){255,190,90,255});
        text += consumed;
        while (*text == ' ') text++;
    }
}

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
                         const InputEvent *event, int open_button)
{
    if (!menu || !profile) return 0;
    int keyboard = event->type == INPUT_KEY_DOWN;
    int controller = event->type == INPUT_PAD_DOWN;
    if (event->type == INPUT_QUIT || event->type == INPUT_FOCUS ||
        event->type == INPUT_PAD_ADDED || event->type == INPUT_PAD_REMOVED) return 0;
    if (keyboard && event->repeat) return menu->open;
    int key = keyboard ? event->key : KEY_NULL;
    int button = controller ? event->button : -1;
    if (!menu->open) {
        if (key != KEY_F1 && !(controller && button == open_button)) return 0;
        menu->open = 1; menu->page = menu->selected = menu->capture = 0;
        menu->message[0] = '\0';
        return 1;
    }
    if (key == KEY_ESCAPE || (controller && (button == PAD_B || button == PAD_BACK))) {
        if (menu->capture) menu->capture = 0;
        else menu->open = 0;
        return 1;
    }
    if (menu->capture) {
        GameSettings candidate = profile->data.settings;
        int action = menu->selected % PROFILE_ACTION_COUNT;
        if (menu->capture == 1 && keyboard && input_key_from_binding(event->binding)) candidate.keys[action] = event->binding;
        else if (menu->capture == 2 && controller && input_pad_button(button)) candidate.buttons[action] = button;
        else return 1;
        if (game_settings_valid(&candidate)) {
            profile->data.settings = candidate; changed(profile); menu->capture = 0;
            menu->message[0] = '\0';
        } else snprintf(menu->message, sizeof(menu->message), "Reserved/duplicate binding. Choose another; Esc cancels.");
        return 1;
    }
    int rows = menu->page ? 13 : 10;
    if (key == KEY_UP || button == PAD_UP) menu->selected = (menu->selected + rows - 1) % rows;
    if (key == KEY_DOWN || button == PAD_DOWN) menu->selected = (menu->selected + 1) % rows;
    if (!menu->page && (key == KEY_LEFT || button == PAD_LEFT)) adjust(menu, profile, -1);
    if (!menu->page && (key == KEY_RIGHT || button == PAD_RIGHT)) adjust(menu, profile, 1);
    if (key == KEY_ENTER || key == KEY_KP_ENTER || key == KEY_SPACE ||
        button == PAD_A || button == PAD_START) activate(menu, profile);
    if (event->type == INPUT_MOUSE_DOWN && event->button == MOUSE_BUTTON_LEFT) {
        int row = (event->y - 36) / 16;
        if (event->x >= 18 && event->x < 382 && event->y >= 36 && row < rows) {
            menu->selected = row; activate(menu, profile);
        }
    }
    return 1;
}

void settings_menu_render(SettingsMenu *menu, const GameProfile *profile,
                           TextFont *font)
{
    if (!menu || !profile) return;
    int unavailable = game_settings_has_unavailable_binding(&profile->data.settings);
    if (!menu->open && !profile->error && !profile->pending_text && !unavailable) return;
    if (menu->ui.font != font) { ui_cleanup(&menu->ui); ui_init(&menu->ui, font); }
    if (!menu->open) {
        DrawRectangle(8, 278, 384, 18, (Color){10,12,18,255});
        ui_label_color(&menu->ui,12,281,profile->error ? "Profile issue - F1: details" :
                       unavailable ? "Binding unavailable - F1: remap" : "Saving profile...",
                       (Color){255,190,90,255});
        return;
    }
    DrawRectangle(8, 4, 384, 292, (Color){10,12,18,255});
    ui_label(&menu->ui, 18, 12, menu->page ? "CONTROLS" : "SETTINGS");
    const GameSettings *s = &profile->data.settings;
    int rows = menu->page ? 13 : 10;
    for (int row = 0; row < rows; row++) {
        char label[160];
        if (menu->page && row < 12) {
            int action = row % PROFILE_ACTION_COUNT;
            const char *binding = row < 6 ? input_binding_name(s->keys[action]) : input_pad_name(s->buttons[action]);
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
            DrawRectangle(16, y-1, 368, 16, (Color){40,70,105,255});
        }
        ui_label(&menu->ui, 20, y, label);
    }
    const char *status = menu->message[0] ? menu->message :
        profile->error ? profile->status :
        unavailable ? "Saved binding unavailable. Configure controls to remap; stored values retained." : profile->status;
    ui_label(&menu->ui, 18, 249, menu->capture ? "Press a new binding. Esc cancels." :
             status[0] ? "Esc/B: close. Arrows: choose. Enter/A: select." : "Arrows/D-pad: choose/change. Enter/A: select.");
    if (status[0]) {
        render_status(&menu->ui, status);
    } else ui_label(&menu->ui, 18, 265, "Esc/B: close. Arrow keys stay available in game.");
}

void settings_menu_cleanup(SettingsMenu *menu)
{
    ui_cleanup(&menu->ui);
    menu->ui.font = NULL;
    menu->open = menu->capture = 0;
}
