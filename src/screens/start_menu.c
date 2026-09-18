/* Start menu screen. AppSession owns the window, frame loop and transitions. */
#include "start_menu.h"
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

static void centered(TextFont *font, const char *text, int cx, int y, Color color)
{
    int width = 0;
    if (font_measure(font, text, &width, NULL)) return;
    font_draw(font, text, cx-width/2, y, color);
}

static void select_level(StartMenu *menu, int index)
{
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

static void play(StartMenu *menu)
{
    if (menu->route != MENU_ROUTE_NONE) return;
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
    menu->font = font_load("assets/fonts/round9x13.ttf", 13);
    if (!menu->font) return -1;
    menu->logo_tex = texture_load("assets/sprites/screens/start_menu_logo.png");
    menu->snd_confirm = sound_load("assets/sounds/screens/confirm_ui.wav");
    select_level(menu, 0);
    return 0;
}

StartMenu *start_menu_create(const CampaignCatalog *catalog)
{
    if (!catalog || !catalog->levels || !catalog->count || !IsWindowReady()) return NULL;
    StartMenu *menu = calloc(1, sizeof(*menu));
    if (!menu) return NULL;
    menu->catalog = catalog;
    if (start_menu_init(menu)) { start_menu_close(&menu); return NULL; }
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
    InputEvent event;
    while (input_poll(&event)) {
        if (event.type == INPUT_QUIT) {
            if (menu->route == MENU_ROUTE_NONE) menu->route = MENU_ROUTE_EXIT;
            continue;
        }
        if (menu->route != MENU_ROUTE_NONE) continue;
        if (settings_menu_event(menu->settings_menu, menu->profile, &event, PAD_Y)) continue;
        if (event.type == INPUT_MOUSE_DOWN && event.button == MOUSE_BUTTON_LEFT) {
            if (menu->settings_menu && point_in_rect(event.x,event.y,125,270,150,24)) {
                menu->settings_menu->open = 1;
                menu->settings_menu->page = menu->settings_menu->selected = menu->settings_menu->capture = 0;
            } else if (point_in_rect(event.x,event.y,BTN_X,BTN_Y,BTN_W,BTN_H)) play(menu);
        } else if ((event.type == INPUT_KEY_DOWN || event.type == INPUT_PAD_DOWN) && !event.repeat) {
            int key = event.type == INPUT_KEY_DOWN ? event.key : KEY_NULL;
            int button = event.type == INPUT_PAD_DOWN ? event.button : -1;
            if (key == KEY_ESCAPE || button == PAD_B || button == PAD_BACK) menu->route = MENU_ROUTE_EXIT;
            else if (key == KEY_LEFT || key == KEY_A || key == KEY_UP || button == PAD_LEFT || button == PAD_UP)
                select_level(menu, menu->selected_level-1);
            else if (key == KEY_RIGHT || key == KEY_D || key == KEY_DOWN || button == PAD_RIGHT || button == PAD_DOWN)
                select_level(menu, menu->selected_level+1);
            else if (key == KEY_ENTER || key == KEY_KP_ENTER || key == KEY_SPACE || button == PAD_A || button == PAD_START)
                play(menu);
        }
    }
    if (menu->route != MENU_ROUTE_NONE && !menu->route_waiting_render) return 0;
    if (menu->confirm_release_required && !confirm_held(menu)) menu->confirm_release_required = 0;
    BeginDrawing();
    BeginTextureMode(menu->frame_target);
    ClearBackground(BLACK);
    IntRect logo = {(MENU_GAME_W-LOGO_DISPLAY_W)/2,20,LOGO_DISPLAY_W,LOGO_DISPLAY_H};
    sprite_draw(menu->logo_tex,NULL,&logo,0,SPRITE_NORMAL,WHITE);
    Vector2 mouse = input_mouse();
    int hovering = point_in_rect((int)mouse.x,(int)mouse.y,BTN_X,BTN_Y,BTN_W,BTN_H);
    Color color = hovering ? (Color){74,144,217,255} : (Color){77,77,77,255};
    DrawRectangle(BTN_X,BTN_Y,BTN_W,BTN_H,color);
    DrawRectangleLines(BTN_X,BTN_Y,BTN_W,BTN_H,(Color){224,224,224,255});
    centered(menu->font,"Play",BTN_X+BTN_W/2,BTN_Y+7,WHITE);
    char text[160];
    Color grey = {120,120,120,255};
    snprintf(text,sizeof(text),"Level: < %s >",menu->catalog->levels[menu->selected_level].display_name);
    centered(menu->font,text,MENU_GAME_W/2,214,grey);
    if (menu->error_message[0]) centered(menu->font,menu->error_message,MENU_GAME_W/2,232,(Color){220,120,120,255});
    const GameProgress *best = game_profile_result(menu->profile,menu->selected_level_path);
    if (best && !menu->error_message[0]) {
        snprintf(text,sizeof(text),"Best: %d pts / %.2fs / %d coins",best->best_score,best->best_time,best->best_coins);
        centered(menu->font,text,MENU_GAME_W/2,232,grey);
    }
    centered(menu->font,"Arrows/D-pad: level  Enter/A: play  Esc: exit",MENU_GAME_W/2,250,grey);
    if (menu->settings_menu) {
        DrawRectangle(125,270,150,24,(Color){50,65,85,255});
        centered(menu->font,"Settings (F1 / Y)",MENU_GAME_W/2,274,WHITE);
    }
    settings_menu_render(menu->settings_menu,menu->profile,menu->font);
    display_present(menu->frame_target);
    return 1;
}

void start_menu_cleanup(StartMenu *menu)
{
    if (!menu) return;
    sound_unload(menu->snd_confirm); menu->snd_confirm = NULL;
    texture_unload(menu->logo_tex); menu->logo_tex = NULL;
    font_unload(menu->font); menu->font = NULL;
    if (IsRenderTextureValid(menu->frame_target)) UnloadRenderTexture(menu->frame_target);
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
