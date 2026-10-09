/*
 * start_menu.c — A small screen with input, drawing and explicit ownership.
 *
 * The menu borrows the session's catalog/profile/settings. It owns its logical
 * render target, font, logo and confirmation sound. Choosing Play sets a route;
 * AppSession performs the transition after this frame rather than nesting loops.
 */
#include "start_menu.h"
#include "../shared/platform.h"  /* str_copy */
#include "settings_menu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MENU_GAME_W 400
#define MENU_GAME_H 300
#define LOGO_DISPLAY_W 128
#define LOGO_DISPLAY_H 96
#define BTN_W 120
#define BTN_H 28
#define BTN_X ((MENU_GAME_W-BTN_W)/2)
#define BTN_Y 170

static int point_in_rect(int x, int y, int rx, int ry, int w, int h)
{
    return x >= rx && x < rx+w && y >= ry && y < ry+h;
}

static void select_level(StartMenu *menu, int index)
{
    /* Wrap at the catalog ends; the manifest, not a directory scan, owns the
     * level order. Copy the path because later transitions outlive this menu. */
    if (!menu->catalog || !menu->catalog->count) return;
    if (index < 0) index = (int)menu->catalog->count-1;
    if ((size_t)index >= menu->catalog->count) index = 0;
    menu->selected_level = index;
    str_copy(menu->selected_level_path, menu->catalog->levels[index].path, sizeof(menu->selected_level_path));
}

void start_menu_get_input_state(const StartMenu *menu, GameInputPhysicalState *state)
{
    game_input_read_physical(menu ? menu->controller : 0, state);
}

static int confirm_held(const StartMenu *menu)
{
    GameInputPhysicalState state;
    start_menu_get_input_state(menu, &state);
    return ((state.keyboard_mask | state.controller_mask) & GAME_INPUT_CONFIRM) != 0;
}

static const CampaignLevel *selected_entry(const StartMenu *menu)
{
    return &menu->catalog->levels[menu->selected_level];
}

static void play(StartMenu *menu)
{
    /* A confirm carried from a prior screen must be released before this
     * menu can act on another press. Sound playback accepts a missing slot.
     * An unavailable level is listed so the player can see it is broken,
     * but it cannot start; the menu already shows why. */
    if (menu->route != MENU_ROUTE_NONE) return;
    if (!selected_entry(menu)->available) return;
    if (menu->confirm_release_required && confirm_held(menu)) return;
    menu->confirm_release_required = 0;
    sound_play(menu->snd_confirm, 128);
    menu->route = MENU_ROUTE_PLAY;
}

int start_menu_init(StartMenu *menu)
{
    menu->frame_target = LoadRenderTexture(MENU_GAME_W, MENU_GAME_H);
    if (!IsRenderTextureValid(menu->frame_target)) return -1;
    SetTextureFilter(menu->frame_target.texture, TEXTURE_FILTER_POINT);
    menu->font = font_load();
    if (!menu->font) return -1;
    menu->logo_tex = texture_load("assets/sprites/screens/start_menu_logo.png");
    menu->snd_confirm = sound_load("assets/sounds/screens/confirm_ui.wav");
    /* Start on the first playable entry; a loaded catalog always has one. */
    int first = campaign_first_available(menu->catalog);
    select_level(menu, first >= 0 ? first : 0);
    return 0;
}

StartMenu *start_menu_create(const CampaignCatalog *catalog)
{
    if (!catalog || !catalog->levels || !catalog->count || !IsWindowReady()) return NULL;
    StartMenu *menu = calloc(1, sizeof(*menu));
    if (!menu) return NULL;
    menu->catalog = catalog;
    if (start_menu_init(menu)) {
        start_menu_close(&menu);
        return NULL;
    }
    return menu;
}

void start_menu_set_error(StartMenu *menu, const char *message)
{
    if (!menu) return;
    str_copy(menu->error_message, message ? message : "", sizeof(menu->error_message));
    menu->confirm_release_required = 1;
}

void start_menu_refresh_controller(StartMenu *menu)
{
    if (menu) menu->controller = input_first_gamepad();
}

int start_menu_frame(StartMenu *menu)
{
    if (!menu || !menu->catalog || !menu->catalog->count) return 0;
    if (menu->route != MENU_ROUTE_NONE && !menu->route_waiting_render) return 0;
    start_menu_refresh_controller(menu);
    /* The settings panel gets first refusal. A consumed command must not
     * also change the level selector or start a game underneath that panel. */
    InputEvent event;
    while (input_poll(&event)) {
        if (event.type == INPUT_QUIT) {
            if (menu->route == MENU_ROUTE_NONE) menu->route = MENU_ROUTE_EXIT;
            continue;
        }
        if (menu->route != MENU_ROUTE_NONE) continue;
        if (settings_menu_event(menu->settings_menu, menu->profile, &event, PAD_Y)) continue;
        if (event.type == INPUT_MOUSE_DOWN && event.button == MOUSE_BUTTON_LEFT) {
            if (menu->settings_menu && point_in_rect(event.x,event.y,125,270,150,24))
                settings_menu_open(menu->settings_menu);
            else if (point_in_rect(event.x, event.y, BTN_X, BTN_Y, BTN_W, BTN_H))
                play(menu);
        } else if ((event.type == INPUT_KEY_DOWN || event.type == INPUT_PAD_DOWN) && !event.repeat) {
            int key = event.type == INPUT_KEY_DOWN ? event.key : KEY_NULL;
            int button = event.type == INPUT_PAD_DOWN ? event.button : -1;
            if (key == KEY_ESCAPE || button == PAD_B || button == PAD_BACK) menu->route = MENU_ROUTE_EXIT;
            else if (key == KEY_LEFT || key == KEY_A || key == KEY_UP || button == PAD_LEFT || button == PAD_UP)
                select_level(menu, menu->selected_level-1);
            else if (key == KEY_RIGHT || key == KEY_D || key == KEY_DOWN || button == PAD_RIGHT || button == PAD_DOWN)
                select_level(menu, menu->selected_level+1);
            else if (game_input_event_confirms(&event))
                play(menu);
        }
    }
    if (menu->route != MENU_ROUTE_NONE && !menu->route_waiting_render) return 0;
    if (menu->confirm_release_required && !confirm_held(menu)) menu->confirm_release_required = 0;
    /* Draw in logical 400x300 coordinates. Window scaling happens only when
     * display_present copies the completed target to the OS framebuffer. */
    BeginDrawing();
    BeginTextureMode(menu->frame_target);
    ClearBackground(BLACK);
    IntRect logo = {(MENU_GAME_W - LOGO_DISPLAY_W) / 2, 20, LOGO_DISPLAY_W, LOGO_DISPLAY_H};
    sprite_draw(menu->logo_tex, NULL, &logo, 0, SPRITE_NORMAL, WHITE);
    Vector2 mouse = input_mouse();
    const CampaignLevel *entry = selected_entry(menu);
    int hovering = point_in_rect((int)mouse.x,(int)mouse.y,BTN_X,BTN_Y,BTN_W,BTN_H);
    /* A disabled Play button is darker and never highlights on hover. */
    Color color = !entry->available ? (Color){45,45,45,255} :
                  hovering ? (Color){74,144,217,255} : (Color){77,77,77,255};
    Color red = {220,120,120,255};
    DrawRectangle(BTN_X, BTN_Y, BTN_W, BTN_H, color);
    DrawRectangleLines(BTN_X, BTN_Y, BTN_W, BTN_H, (Color){224, 224, 224, 255});
    font_draw_centered(menu->font, "Play", BTN_X + BTN_W/2, BTN_Y + (BTN_H-TEXT_FONT_SIZE)/2,
                       entry->available ? WHITE : (Color){110,110,110,255});
    char text[160];
    Color grey = {120,120,120,255};
    snprintf(text,sizeof(text),"Level: < %s >",entry->display_name);
    font_draw_centered(menu->font,text,MENU_GAME_W/2,214,entry->available ? grey : (Color){80,80,80,255});
    if (menu->error_message[0])
        font_draw_centered(menu->font, menu->error_message, MENU_GAME_W/2, 232, red);
    else if (!entry->available) {
        snprintf(text, sizeof(text), "Unavailable: %s", entry->problem);
        font_draw_centered(menu->font, text, MENU_GAME_W/2, 232, red);
    }
    const GameProgress *best = game_profile_result(menu->profile,menu->selected_level_path);
    if (best && entry->available && !menu->error_message[0]) {
        snprintf(text, sizeof(text), "Best: %d pts / %.2fs / %d coins",
                 best->best_score, best->best_time, best->best_coins);
        font_draw_centered(menu->font,text,MENU_GAME_W/2,232,grey);
    }
    font_draw_centered(menu->font,"Arrows/D-pad: level  Enter/A: play  Esc: exit",MENU_GAME_W/2,250,grey);
    if (menu->settings_menu) {
        DrawRectangle(125,270,150,24,(Color){50,65,85,255});
        font_draw_centered(menu->font,"Settings (F1 / Y)",MENU_GAME_W/2,270+(24-TEXT_FONT_SIZE)/2,WHITE);
    }
    settings_menu_render(menu->settings_menu,menu->profile,menu->font);
    display_present(menu->frame_target);
    return 1;
}

void start_menu_cleanup(StartMenu *menu)
{
    if (!menu) return;
    /* Free owned screen resources while the shared context is still alive.
     * Clearing slots prevents another cleanup from reusing released handles. */
    sound_unload(menu->snd_confirm);
    menu->snd_confirm = NULL;
    texture_unload(menu->logo_tex);
    menu->logo_tex = NULL;
    font_unload(menu->font);
    menu->font = NULL;
    if (IsRenderTextureValid(menu->frame_target))
        UnloadRenderTexture(menu->frame_target);
    menu->frame_target = (RenderTexture2D){0};
    menu->controller = 0;
}

void start_menu_close(StartMenu **menu)
{
    if (!menu || !*menu) return;
    start_menu_cleanup(*menu);
    free(*menu);
    *menu = NULL;
}
