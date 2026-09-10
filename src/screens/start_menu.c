/* Start menu screen. Frame-driven; AppSession owns transitions and cleanup. */

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "start_menu.h"
#include "settings_menu.h"

#define MENU_GAME_W 400
#define MENU_GAME_H 300
#define LOGO_DISPLAY_W 128
#define LOGO_DISPLAY_H 96
#define BTN_W 120
#define BTN_H 28
#define BTN_X ((MENU_GAME_W - BTN_W) / 2)
#define BTN_Y 170

static int point_in_rect(int px, int py, int rx, int ry, int rw, int rh)
{
    return px >= rx && px < rx + rw && py >= ry && py < ry + rh;
}

static void draw_text_centered(SDL_Renderer *renderer, TTF_Font *font,
                               const char *text, int cx, int y,
                               SDL_Color color)
{
    SDL_Surface *surface;
    SDL_Texture *texture;

    if (!font) return;
    surface = TTF_RenderUTF8_Solid(font, text, color);
    if (!surface) return;
    texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture) {
        SDL_Rect dst = {cx - surface->w / 2, y, surface->w, surface->h};
        SDL_RenderCopy(renderer, texture, NULL, &dst);
        SDL_DestroyTexture(texture);
    }
    SDL_FreeSurface(surface);
}

static void start_menu_set_selected_level(StartMenu *menu, int index)
{
    if (!menu || !menu->catalog || menu->catalog->count == 0) return;
    if (index < 0) index = (int)menu->catalog->count - 1;
    if ((size_t)index >= menu->catalog->count) index = 0;
    menu->selected_level = index;
    strncpy(menu->selected_level_path, menu->catalog->levels[index].path,
            sizeof(menu->selected_level_path) - 1);
    menu->selected_level_path[sizeof(menu->selected_level_path) - 1] = '\0';
}

static int start_menu_confirm_is_held(const StartMenu *menu)
{
    GameInputPhysicalState state;

    start_menu_get_input_state(menu, &state);
    return (state.keyboard_mask & GAME_INPUT_CONFIRM) != 0 ||
           (state.controller_mask & GAME_INPUT_CONFIRM) != 0;
}

static int start_menu_accept_confirm(StartMenu *menu)
{
    if (menu->confirm_release_required) {
        if (start_menu_confirm_is_held(menu)) return 0;
        menu->confirm_release_required = 0;
    }
    return 1;
}

static void start_menu_play(StartMenu *menu)
{
    if (menu->route != MENU_ROUTE_NONE || !start_menu_accept_confirm(menu)) return;
    if (menu->snd_confirm) Mix_PlayChannel(-1, menu->snd_confirm, 0);
    menu->route = MENU_ROUTE_PLAY;
}

static int start_menu_init_resources(StartMenu *menu,
                                     SDL_Window *window,
                                     SDL_Renderer *renderer)
{
    menu->window = window;
    menu->renderer = renderer;
    menu->route = MENU_ROUTE_NONE;
    menu->confirm_release_required = 0;
    menu->error_message[0] = '\0';
    start_menu_set_selected_level(menu, 0);

    /* AppSession publishes readiness after this screen exists. */
    menu->controller_ready = 0;

    menu->font = TTF_OpenFont("assets/fonts/round9x13.ttf", 13);
    if (!menu->font) {
        fprintf(stderr, "Failed to load Round9x13.ttf: %s\n", TTF_GetError());
        return -1;
    }

    menu->logo_tex = IMG_LoadTexture(renderer,
                                     "assets/sprites/screens/start_menu_logo.png");
    if (!menu->logo_tex) {
        fprintf(stderr, "Warning: Failed to load start_menu_logo.png: %s\n",
                IMG_GetError());
    }

    menu->snd_confirm = Mix_LoadWAV("assets/sounds/screens/confirm_ui.wav");
    if (!menu->snd_confirm) {
        fprintf(stderr, "Warning: Failed to load confirm_ui.wav: %s\n",
                Mix_GetError());
    }
    return 0;
}

int start_menu_init(StartMenu *menu, SDL_Window *window, SDL_Renderer *renderer)
{
    return start_menu_init_resources(menu, window, renderer);
}

StartMenu *start_menu_create(const CampaignCatalog *catalog)
{
    StartMenu *menu = calloc(1, sizeof(*menu));
    SDL_Window *window;
    SDL_Renderer *renderer;

    if (!menu || !catalog || !catalog->levels || catalog->count == 0) {
        free(menu);
        return NULL;
    }
    menu->catalog = catalog;
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    window = SDL_CreateWindow("Super Mango",
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              800, 600, SDL_WINDOW_SHOWN);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow error: %s\n", SDL_GetError());
        free(menu);
        return NULL;
    }
    renderer = SDL_CreateRenderer(window, -1,
                                  SDL_RENDERER_ACCELERATED |
                                  SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer || SDL_RenderSetLogicalSize(renderer, MENU_GAME_W, MENU_GAME_H) < 0) {
        fprintf(stderr, "Start menu renderer error: %s\n", SDL_GetError());
        if (renderer) SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        free(menu);
        return NULL;
    }
    if (start_menu_init_resources(menu, window, renderer) != 0) {
        start_menu_close(&menu);
        return NULL;
    }
    return menu;
}

void start_menu_set_error(StartMenu *menu, const char *message)
{
    if (!menu) return;
    strncpy(menu->error_message, message ? message : "",
            sizeof(menu->error_message) - 1);
    menu->error_message[sizeof(menu->error_message) - 1] = '\0';
    menu->confirm_release_required = 1;
}

void start_menu_get_input_state(const StartMenu *menu,
                                GameInputPhysicalState *state)
{
    if (!state) return;
    game_input_read_physical(menu ? menu->controller : NULL, state);
}

void start_menu_refresh_controller(StartMenu *menu)
{
    if (!menu || !menu->controller_ready || menu->controller ||
        SDL_WasInit(SDL_INIT_GAMECONTROLLER) == 0)
        return;
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (SDL_IsGameController(i)) {
            menu->controller = SDL_GameControllerOpen(i);
            if (menu->controller) break;
        }
    }
}

void start_menu_set_controller_ready(StartMenu *menu, int ready)
{
    if (menu) menu->controller_ready = ready != 0;
}

int start_menu_frame(StartMenu *menu)
{
    SDL_Event event;

    if (!menu || !menu->catalog || menu->catalog->count == 0) return 0;
    if (menu->route != MENU_ROUTE_NONE && !menu->route_waiting_render) return 0;
    if (menu->route == MENU_ROUTE_NONE && menu->controller_ready)
        start_menu_refresh_controller(menu);
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            if (menu->route == MENU_ROUTE_NONE) menu->route = MENU_ROUTE_EXIT;
            continue;
        }
        if (menu->route == MENU_ROUTE_NONE &&
            settings_menu_event(menu->settings_menu, menu->profile, &event, SDL_CONTROLLER_BUTTON_Y)) continue;
        if (menu->settings_menu && menu->route == MENU_ROUTE_NONE && event.type == SDL_MOUSEBUTTONDOWN &&
            event.button.button == SDL_BUTTON_LEFT && point_in_rect(event.button.x,event.button.y,125,270,150,24)) {
            menu->settings_menu->open = 1;
            menu->settings_menu->page = menu->settings_menu->selected = menu->settings_menu->capture = 0;
            continue;
        }
        if (event.type == SDL_KEYDOWN) {
            if (menu->route != MENU_ROUTE_NONE || event.key.repeat) continue;
            switch (event.key.keysym.sym) {
            case SDLK_ESCAPE:
                menu->route = MENU_ROUTE_EXIT;
                break;
            case SDLK_LEFT:
            case SDLK_a:
            case SDLK_UP:
                start_menu_set_selected_level(menu, menu->selected_level - 1);
                break;
            case SDLK_RIGHT:
            case SDLK_d:
            case SDLK_DOWN:
                start_menu_set_selected_level(menu, menu->selected_level + 1);
                break;
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
            case SDLK_SPACE:
                start_menu_play(menu);
                break;
            default:
                break;
            }
        } else if (event.type == SDL_MOUSEBUTTONDOWN &&
                   event.button.button == SDL_BUTTON_LEFT) {
            /* SDL_RenderSetLogicalSize has already converted button events. */
            if (point_in_rect(event.button.x, event.button.y, BTN_X, BTN_Y, BTN_W, BTN_H)) {
                start_menu_play(menu);
            }
        } else if (event.type == SDL_CONTROLLERDEVICEADDED && menu->route == MENU_ROUTE_NONE &&
                   menu->controller_ready &&
                   !menu->controller && SDL_WasInit(SDL_INIT_GAMECONTROLLER) != 0 &&
                   SDL_IsGameController(event.cdevice.which)) {
            menu->controller = SDL_GameControllerOpen(event.cdevice.which);
        } else if (event.type == SDL_CONTROLLERDEVICEREMOVED && menu->controller) {
            SDL_Joystick *joy = SDL_GameControllerGetJoystick(menu->controller);
            if (SDL_JoystickInstanceID(joy) == event.cdevice.which) {
                SDL_GameControllerClose(menu->controller);
                menu->controller = NULL;
            }
        } else if (event.type == SDL_CONTROLLERBUTTONDOWN &&
                   menu->route == MENU_ROUTE_NONE) {
            switch (event.cbutton.button) {
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
            case SDL_CONTROLLER_BUTTON_DPAD_UP:
                start_menu_set_selected_level(menu, menu->selected_level - 1);
                break;
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                start_menu_set_selected_level(menu, menu->selected_level + 1);
                break;
            case SDL_CONTROLLER_BUTTON_A:
            case SDL_CONTROLLER_BUTTON_START:
                start_menu_play(menu);
                break;
            case SDL_CONTROLLER_BUTTON_B:
            case SDL_CONTROLLER_BUTTON_BACK:
                menu->route = MENU_ROUTE_EXIT;
                break;
            default:
                break;
            }
        }
    }

    if (menu->route != MENU_ROUTE_NONE && !menu->route_waiting_render) return 0;
    if (menu->route == MENU_ROUTE_NONE && menu->confirm_release_required &&
        !start_menu_confirm_is_held(menu)) {
        menu->confirm_release_required = 0;
    }

    SDL_SetRenderDrawColor(menu->renderer, 0, 0, 0, 255);
    if (SDL_RenderClear(menu->renderer) != 0) {
        SDL_Log("start_menu_frame: SDL_RenderClear failed: %s", SDL_GetError());
        menu->route = MENU_ROUTE_FATAL;
        return 0;
    }
    if (menu->logo_tex) {
        SDL_Rect dst = {(MENU_GAME_W - LOGO_DISPLAY_W) / 2, 20,
                        LOGO_DISPLAY_W, LOGO_DISPLAY_H};
        SDL_RenderCopy(menu->renderer, menu->logo_tex, NULL, &dst);
    }

    {
        int mx, my;
        float sx, sy;
        int hovering;
        SDL_Color button_color;
        SDL_Rect rect = {BTN_X, BTN_Y, BTN_W, BTN_H};

        SDL_GetMouseState(&mx, &my);
        SDL_RenderGetScale(menu->renderer, &sx, &sy);
        hovering = point_in_rect((int)(mx / sx), (int)(my / sy),
                                 BTN_X, BTN_Y, BTN_W, BTN_H);
        button_color = hovering ? (SDL_Color){74, 144, 217, 255}
                                : (SDL_Color){77, 77, 77, 255};
        SDL_SetRenderDrawColor(menu->renderer, button_color.r, button_color.g,
                               button_color.b, button_color.a);
        SDL_RenderFillRect(menu->renderer, &rect);
        SDL_SetRenderDrawColor(menu->renderer, 224, 224, 224, 255);
        SDL_RenderDrawRect(menu->renderer, &rect);
        draw_text_centered(menu->renderer, menu->font, "Play",
                           BTN_X + BTN_W / 2, BTN_Y + 7,
                           (SDL_Color){255, 255, 255, 255});
    }

    {
        char level_text[96];
        SDL_Color grey = {120, 120, 120, 255};
        snprintf(level_text, sizeof(level_text), "Level: < %s >",
                 menu->catalog->levels[menu->selected_level].display_name);
        draw_text_centered(menu->renderer, menu->font, level_text,
                           MENU_GAME_W / 2, 214, grey);
        if (menu->error_message[0] != '\0') {
            draw_text_centered(menu->renderer, menu->font, menu->error_message,
                               MENU_GAME_W / 2, 232,
                               (SDL_Color){220, 120, 120, 255});
        }
        const GameProgress *best = game_profile_result(menu->profile, menu->selected_level_path);
        if (best && !menu->error_message[0]) {
            char summary[96];
            snprintf(summary, sizeof(summary), "Best: %d pts / %.2fs / %d coins", best->best_score, best->best_time, best->best_coins);
            draw_text_centered(menu->renderer,menu->font,summary,MENU_GAME_W/2,232,grey);
        }
        draw_text_centered(menu->renderer, menu->font, "Arrows/D-pad: level  Enter/A: play  Esc: exit", MENU_GAME_W/2,250,grey);
        if (menu->settings_menu) {
            SDL_Rect settings = {125,270,150,24};
            SDL_SetRenderDrawColor(menu->renderer,50,65,85,255); SDL_RenderFillRect(menu->renderer,&settings);
            draw_text_centered(menu->renderer,menu->font,"Settings (F1 / Y)",MENU_GAME_W/2,274,(SDL_Color){255,255,255,255});
        }
    }
    settings_menu_render(menu->settings_menu, menu->profile, menu->renderer, menu->font);
    SDL_RenderPresent(menu->renderer);
    return 1;
}

void start_menu_cleanup(StartMenu *menu)
{
    if (!menu) return;
    Mix_HaltChannel(-1);
    if (menu->snd_confirm) {
        Mix_FreeChunk(menu->snd_confirm);
        menu->snd_confirm = NULL;
    }
    if (menu->logo_tex) {
        SDL_DestroyTexture(menu->logo_tex);
        menu->logo_tex = NULL;
    }
    if (menu->font) {
        TTF_CloseFont(menu->font);
        menu->font = NULL;
    }
    if (menu->controller) {
        SDL_GameControllerClose(menu->controller);
        menu->controller = NULL;
    }
}

void start_menu_close(StartMenu **menu)
{
    StartMenu *owned;

    if (!menu || !*menu) return;
    owned = *menu;
    *menu = NULL;
    start_menu_cleanup(owned);
    if (owned->renderer) {
        SDL_DestroyRenderer(owned->renderer);
        owned->renderer = NULL;
    }
    if (owned->window) {
        SDL_DestroyWindow(owned->window);
        owned->window = NULL;
    }
    free(owned);
}
