/*
 * render_overlay.c — UI overlay rendering implementation.
 *
 * Handles level complete screen and future UI overlays.
 */

#include "game_render.h"

#include <SDL_ttf.h>
#include <stdio.h>  /* snprintf */

/* ------------------------------------------------------------------ */
/* Level complete overlay                                             */
/* ------------------------------------------------------------------ */

void render_level_complete_overlay(GameState *gs)
{
    /* Semi-transparent black overlay */
    SDL_SetRenderDrawBlendMode(gs->renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(gs->renderer, 0, 0, 0, 180);
    SDL_Rect overlay = { 0, 0, GAME_W, GAME_H };
    SDL_RenderFillRect(gs->renderer, &overlay);
    SDL_SetRenderDrawBlendMode(gs->renderer, SDL_BLENDMODE_NONE);

    /* Level Complete title */
    if (gs->hud.font) {
        SDL_Color gold = { 255, 215, 0, 255 };
        SDL_Surface *title_surf = TTF_RenderText_Solid(gs->hud.font, "Level Complete!", gold);
        if (title_surf) {
            SDL_Texture *title_tex = SDL_CreateTextureFromSurface(gs->renderer, title_surf);
            if (title_tex) {
                int tw, th;
                SDL_QueryTexture(title_tex, NULL, NULL, &tw, &th);
                SDL_Rect dst = { (GAME_W - tw) / 2, GAME_H / 2 - 40, tw, th };
                SDL_RenderCopy(gs->renderer, title_tex, NULL, &dst);
                SDL_DestroyTexture(title_tex);
            }
            SDL_FreeSurface(title_surf);
        }

        /* Score display */
        char score_text[64];
        snprintf(score_text, sizeof(score_text), "Score: %d", gs->score);
        SDL_Color white = { 255, 255, 255, 255 };
        SDL_Surface *score_surf = TTF_RenderText_Solid(gs->hud.font, score_text, white);
        if (score_surf) {
            SDL_Texture *score_tex = SDL_CreateTextureFromSurface(gs->renderer, score_surf);
            if (score_tex) {
                int sw, sh;
                SDL_QueryTexture(score_tex, NULL, NULL, &sw, &sh);
                SDL_Rect dst = { (GAME_W - sw) / 2, GAME_H / 2 + 10, sw, sh };
                SDL_RenderCopy(gs->renderer, score_tex, NULL, &dst);
                SDL_DestroyTexture(score_tex);
            }
            SDL_FreeSurface(score_surf);
        }

        /* Exit hint */
        SDL_Surface *hint_surf = TTF_RenderText_Solid(gs->hud.font, "Press ESC or Start to exit", white);
        if (hint_surf) {
            SDL_Texture *hint_tex = SDL_CreateTextureFromSurface(gs->renderer, hint_surf);
            if (hint_tex) {
                int hw, hh;
                SDL_QueryTexture(hint_tex, NULL, NULL, &hw, &hh);
                SDL_Rect dst = { (GAME_W - hw) / 2, GAME_H / 2 + 50, hw, hh };
                SDL_RenderCopy(gs->renderer, hint_tex, NULL, &dst);
                SDL_DestroyTexture(hint_tex);
            }
            SDL_FreeSurface(hint_surf);
        }
    }
}
