/*
 * render_overlay.c — UI overlay rendering implementation.
 *
 * Handles level complete screen and future UI overlays.
 */

#include "game_render.h"

#include <SDL_ttf.h>
#include <stdio.h>  /* snprintf */

/* ------------------------------------------------------------------ */
/* Shared overlay drawing                                             */
/* ------------------------------------------------------------------ */

static void render_overlay_backdrop(GameState *gs, Uint8 alpha)
{
    SDL_SetRenderDrawBlendMode(gs->renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(gs->renderer, 0, 0, 0, alpha);
    SDL_Rect overlay = { 0, 0, GAME_W, GAME_H };
    SDL_RenderFillRect(gs->renderer, &overlay);
    SDL_SetRenderDrawBlendMode(gs->renderer, SDL_BLENDMODE_NONE);
}

static void render_centered_text(GameState *gs, const char *text,
                                 SDL_Color color, int y)
{
    SDL_Surface *surf = TTF_RenderText_Solid(gs->hud.font, text, color);
    if (surf) {
        SDL_Texture *tex = SDL_CreateTextureFromSurface(gs->renderer, surf);
        if (tex) {
            int tw, th;
            SDL_QueryTexture(tex, NULL, NULL, &tw, &th);
            SDL_Rect dst = { (GAME_W - tw) / 2, y, tw, th };
            SDL_RenderCopy(gs->renderer, tex, NULL, &dst);
            SDL_DestroyTexture(tex);
        }
        SDL_FreeSurface(surf);
    }
}

void render_pause_overlay(GameState *gs)
{
    render_overlay_backdrop(gs, 150);

    if (gs->hud.font) {
        SDL_Color gold = { 255, 215, 0, 255 };
        SDL_Color white = { 255, 255, 255, 255 };
        SDL_Color dim = { 190, 190, 190, 255 };

        render_centered_text(gs, "Paused", gold, 92);
        render_centered_text(gs, "Enter/Space/Esc/Start: resume", white, 134);
        render_centered_text(gs, "Close window to quit", dim, 160);
    }
}

void render_game_over_overlay(GameState *gs)
{
    render_overlay_backdrop(gs, 190);

    if (gs->hud.font) {
        SDL_Color red = { 255, 90, 90, 255 };
        SDL_Color white = { 255, 255, 255, 255 };
        SDL_Color dim = { 190, 190, 190, 255 };
        char line[96];

        render_centered_text(gs, "Game Over", red, 82);

        snprintf(line, sizeof(line), "Final Score: %d", gs->score);
        render_centered_text(gs, line, white, 124);

        render_centered_text(gs, "Enter/Space/Start: restart", dim, 164);
        render_centered_text(gs, "Esc/Back: exit", dim, 184);
    }
}

/* ------------------------------------------------------------------ */
/* Level complete overlay                                             */
/* ------------------------------------------------------------------ */

void render_level_complete_overlay(GameState *gs)
{
    render_overlay_backdrop(gs, 180);

    const int has_next_level = gs->completion.pending_next_phase;

    /* Level Complete title - show "Game Complete!" for final level */
    if (gs->hud.font) {
        SDL_Color gold = { 255, 215, 0, 255 };
        SDL_Color white = { 255, 255, 255, 255 };
        SDL_Color green = { 100, 255, 100, 255 };
        SDL_Color dim = { 190, 190, 190, 255 };
        const char *title_text = has_next_level ? "Level Complete!" : "Game Complete!";
        char line[96];
        int elapsed = (int)(gs->completion.elapsed + 0.5f);
        int minutes = elapsed / 60;
        int seconds = elapsed % 60;

        render_centered_text(gs, title_text, gold, 70);

        snprintf(line, sizeof(line), "Score: %d", gs->score);
        render_centered_text(gs, line, white, 112);

        snprintf(line, sizeof(line), "Coins: %d/%d",
                 gs->completion.coins_collected,
                 gs->completion.coin_total);
        render_centered_text(gs, line, white, 132);

        snprintf(line, sizeof(line), "Lives: %d", gs->lives);
        render_centered_text(gs, line, white, 152);

        snprintf(line, sizeof(line), "Time: %02d:%02d", minutes, seconds);
        render_centered_text(gs, line, white, 172);

        if (!has_next_level) {
            render_centered_text(gs, "Congratulations!", green, 198);
        }

        /* Exit hint */
        render_centered_text(gs,
                             has_next_level
                                 ? "Enter/Space/Start: next level"
                                 : "Enter/Space/Start: finish",
                             dim, 228);
        render_centered_text(gs, "Esc/Back: exit", dim, 246);
    }
}
