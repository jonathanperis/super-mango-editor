/*
 * render_overlay.c — UI overlay rendering implementation.
 *
 * Handles level complete screen and future UI overlays.
 */

#include "game_render.h"
#include "../levels/level.h"  /* LevelDef for next_phase check */

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

    /* Determine if this is the final level (no next_phase set) */
    const LevelDef *def = (const LevelDef *)gs->current_level;
    int is_final_level = (def && def->next_phase[0] == '\0');

    /* Level Complete title - show "Game Complete!" for final level */
    if (gs->hud.font) {
        SDL_Color gold = { 255, 215, 0, 255 };
        const char *title_text = is_final_level ? "Game Complete!" : "Level Complete!";
        SDL_Surface *title_surf = TTF_RenderText_Solid(gs->hud.font, title_text, gold);
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

        /* Congratulations message for final level */
        if (is_final_level) {
            SDL_Color green = { 100, 255, 100, 255 };
            SDL_Surface *congrats_surf = TTF_RenderText_Solid(gs->hud.font, "Congratulations!", green);
            if (congrats_surf) {
                SDL_Texture *congrats_tex = SDL_CreateTextureFromSurface(gs->renderer, congrats_surf);
                if (congrats_tex) {
                    int cw, ch;
                    SDL_QueryTexture(congrats_tex, NULL, NULL, &cw, &ch);
                    SDL_Rect dst = { (GAME_W - cw) / 2, GAME_H / 2 + 32, cw, ch };
                    SDL_RenderCopy(gs->renderer, congrats_tex, NULL, &dst);
                    SDL_DestroyTexture(congrats_tex);
                }
                SDL_FreeSurface(congrats_surf);
            }
        }

        /* Exit hint */
        const char *exit_text = is_final_level ? "Press ESC or Start to return to menu" : "Press ESC or Start to exit";
        SDL_Surface *hint_surf = TTF_RenderText_Solid(gs->hud.font, exit_text, white);
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
