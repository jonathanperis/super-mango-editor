/* Terminal overlay rendering. Backdrop and HUD remain owned by game render. */

#include "game_render.h"

#include <SDL_ttf.h>
#include <stdio.h>

#include "../core/game_terminal.h"

static void render_overlay_backdrop(GameState *gs, Uint8 alpha)
{
    SDL_SetRenderDrawBlendMode(gs->renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(gs->renderer, 0, 0, 0, alpha);
    SDL_Rect overlay = {0, 0, GAME_W, GAME_H};
    SDL_RenderFillRect(gs->renderer, &overlay);
    SDL_SetRenderDrawBlendMode(gs->renderer, SDL_BLENDMODE_NONE);
}

static void render_centered_text(GameState *gs, const char *text,
                                 SDL_Color color, int y)
{
    SDL_Surface *surface;
    SDL_Texture *texture;

    if (!gs->hud.font) return;
    surface = TTF_RenderUTF8_Solid(gs->hud.font, text, color);
    if (!surface) return;
    texture = SDL_CreateTextureFromSurface(gs->renderer, surface);
    if (texture) {
        int width, height;
        SDL_QueryTexture(texture, NULL, NULL, &width, &height);
        SDL_Rect dst = {(GAME_W - width) / 2, y, width, height};
        SDL_RenderCopy(gs->renderer, texture, NULL, &dst);
        SDL_DestroyTexture(texture);
    }
    SDL_FreeSurface(surface);
}

static void render_terminal_actions(GameState *gs, int first_y)
{
    GameTerminalActionList list;
    int focused;

    game_terminal_actions(gs, &list);
    focused = gs->terminal_action_index;
    if (focused < 0 || focused >= list.count) focused = 0;

    for (int i = 0; i < list.count; i++) {
        SDL_Color color = i == focused
            ? (SDL_Color){255, 215, 0, 255}
            : (SDL_Color){150, 150, 150, 255};
        char label[64];
        snprintf(label, sizeof(label), "%c %s %c",
                 i == focused ? '>' : ' ',
                 game_terminal_action_label(list.items[i]),
                 i == focused ? '<' : ' ');
        render_centered_text(gs, label, color, first_y + i * 16);
    }
}

void render_pause_overlay(GameState *gs)
{
    render_overlay_backdrop(gs, 150);
    if (gs->hud.font) {
        render_centered_text(gs, "Paused", (SDL_Color){255, 215, 0, 255}, 92);
        render_centered_text(gs, "Enter/Space/Esc/Start: resume",
                             (SDL_Color){255, 255, 255, 255}, 134);
        render_centered_text(gs, "Close window to quit",
                             (SDL_Color){190, 190, 190, 255}, 160);
        if (gs->settings_menu) render_centered_text(gs, "F1 / Back: settings",
                              (SDL_Color){190, 190, 190, 255}, 186);
    }
}

void render_game_over_overlay(GameState *gs)
{
    render_overlay_backdrop(gs, 190);
    if (gs->hud.font) {
        char line[96];
        render_centered_text(gs, "Game Over", (SDL_Color){255, 90, 90, 255}, 66);
        snprintf(line, sizeof(line), "Final Score: %d", gs->score);
        render_centered_text(gs, line, (SDL_Color){255, 255, 255, 255}, 104);
        render_terminal_actions(gs, 136);
        render_centered_text(gs, "Up/Down or D-pad: Select",
                             (SDL_Color){255, 255, 255, 255}, 192);
        render_centered_text(gs, "Enter/Space/A/Start: Confirm",
                             (SDL_Color){255, 255, 255, 255}, 208);
        render_centered_text(gs, "Esc/B/Back: Exit",
                             (SDL_Color){255, 255, 255, 255}, 224);
    }
}

void render_level_complete_overlay(GameState *gs)
{
    const int has_next_level = gs->completion.pending_next_phase;

    render_overlay_backdrop(gs, 180);
    if (gs->hud.font) {
        char line[96];
        int elapsed = (int)(gs->completion.elapsed + 0.5f);
        int minutes = elapsed / 60;
        int seconds = elapsed % 60;

        render_centered_text(gs,
                             has_next_level ? "Level Complete!" : "Game Complete!",
                             (SDL_Color){255, 215, 0, 255}, 54);
        snprintf(line, sizeof(line), "Score: %d", gs->score);
        render_centered_text(gs, line, (SDL_Color){255, 255, 255, 255}, 88);
        snprintf(line, sizeof(line), "Coins: %d/%d",
                 gs->completion.coins_collected,
                 gs->completion.coin_total);
        render_centered_text(gs, line, (SDL_Color){255, 255, 255, 255}, 104);
        snprintf(line, sizeof(line), "Lives: %d", gs->lives);
        render_centered_text(gs, line, (SDL_Color){255, 255, 255, 255}, 120);
        snprintf(line, sizeof(line), "Time: %02d:%02d", minutes, seconds);
        render_centered_text(gs, line, (SDL_Color){255, 255, 255, 255}, 136);
        if (!has_next_level) {
            render_centered_text(gs, "Congratulations!",
                                 (SDL_Color){100, 255, 100, 255}, 154);
        }
        render_terminal_actions(gs, 168);
        render_centered_text(gs, "Up/Down or D-pad: Select",
                             (SDL_Color){255, 255, 255, 255}, 232);
        render_centered_text(gs, "Enter/Space/A/Start: Confirm",
                             (SDL_Color){255, 255, 255, 255}, 248);
        render_centered_text(gs, "Esc/B/Back: Exit",
                             (SDL_Color){255, 255, 255, 255}, 264);
    }
}
