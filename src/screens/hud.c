/*
 * hud.c — HUD rendering: hearts, lives counter, and score display.
 *
 * The HUD is drawn as the topmost layer each frame, after fog and before
 * SDL_RenderPresent.  Text is rendered with TTF_RenderText_Solid (no
 * anti-aliasing) to maintain the crisp pixel-art aesthetic.
 */

#include <SDL_image.h>  /* IMG_LoadTexture */
#include <stdio.h>      /* snprintf, fprintf */
#include <stdlib.h>     /* exit, EXIT_FAILURE */

#include "hud.h"
#include "../game.h"    /* GAME_W */

/* ------------------------------------------------------------------ */

/*
 * hud_init — Load the font and heart icon texture.
 *
 * The font (Round9x13.ttf) is opened at size 13, its native bitmap height.
 * Stars_Ui.png is loaded as the heart indicator icon.  Both are fatal if
 * missing — the HUD is essential for gameplay feedback.
 */
void hud_init(Hud *hud, SDL_Renderer *renderer)
{
    /*
     * TTF_OpenFont — load a TrueType font file and set its point size.
     * 13 matches the font's designed pixel height (9×13 character cells),
     * giving the crispest rendering without fractional scaling.
     */
    hud->font = TTF_OpenFont("assets/round9x13.ttf", 13);
    if (!hud->font) {
        fprintf(stderr, "Failed to load Round9x13.ttf: %s\n", TTF_GetError());
        exit(EXIT_FAILURE);
    }

    /*
     * IMG_LoadTexture — decode Stars_Ui.png and upload it to GPU memory.
     * This single texture is drawn once per current heart in hud_render.
     */
    hud->star_tex = IMG_LoadTexture(renderer, "assets/yellow_star.png");
    if (!hud->star_tex) {
        fprintf(stderr, "Failed to load Stars_Ui.png: %s\n", IMG_GetError());
        TTF_CloseFont(hud->font);
        hud->font = NULL;
        exit(EXIT_FAILURE);
    }

    /*
     * Load the coin icon for the score display.
     * Non-fatal: the score still shows without the icon.
     */
    hud->coin_icon = IMG_LoadTexture(renderer, "assets/hud_coins.png");
    if (!hud->coin_icon) {
        fprintf(stderr, "Warning: Failed to load Coins_Ui.png: %s\n", IMG_GetError());
    }

    /*
     * Load the player sprite sheet so the HUD can crop a small icon
     * for the lives counter.  Non-fatal: lives text still renders.
     */
    hud->player_icon = IMG_LoadTexture(renderer, "assets/player.png");
    if (!hud->player_icon) {
        fprintf(stderr, "Warning: Failed to load Player.png (HUD icon): %s\n", IMG_GetError());
    }
}

/* ------------------------------------------------------------------ */

/*
 * render_text — Helper: draw a string at (x, y) using the HUD font.
 *
 * Creates a temporary SDL_Surface via TTF_RenderText_Solid (no anti-aliasing,
 * ideal for pixel art), uploads it to a GPU texture, blits it, then frees
 * both.  The per-frame allocation is negligible for HUD-sized strings.
 */
static void render_text(TTF_Font *font, SDL_Renderer *renderer,
                        const char *text, int x, int y)
{
    SDL_Color white = {255, 255, 255, 255};

    /*
     * TTF_RenderText_Solid — rasterise the text into an 8-bit palettised
     * surface with transparent background.  Fast and sharp, no blending.
     */
    SDL_Surface *surf = TTF_RenderText_Solid(font, text, white);
    if (!surf) return;

    /*
     * SDL_CreateTextureFromSurface — upload the surface pixels to the GPU
     * so we can draw the text with SDL_RenderCopy.
     */
    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
    if (tex) {
        SDL_Rect dst = {x, y, surf->w, surf->h};
        SDL_RenderCopy(renderer, tex, NULL, &dst);
        SDL_DestroyTexture(tex);
    }
    SDL_FreeSurface(surf);
}

/* ------------------------------------------------------------------ */

/*
 * hud_render — Draw hearts, player icon + lives, and score.
 *
 * Layout in logical 400×300 space:
 *
 *   [★][★][★]  [P] x3                        SCORE: 0
 *    4,4        50,3                       right-aligned
 *
 * Hearts are repeated `hearts` times (0–MAX_HEARTS).
 * The player icon uses the first idle frame (row 0, col 0, 48×48 px).
 * Score is right-aligned with HUD_MARGIN from the right edge.
 */
void hud_render(const Hud *hud, SDL_Renderer *renderer,
                int hearts, int lives, int score)
{
    int hud_row_y = HUD_MARGIN;

    /* ---- Hearts (top-left) ---------------------------------------- */
    /*
     * Draw one star icon per current heart.  Missing hearts are simply
     * not drawn, so the number of visible stars equals the player's HP.
     */
    for (int i = 0; i < hearts; i++) {
        SDL_Rect dst = {
            HUD_MARGIN + i * (HUD_HEART_SIZE + HUD_HEART_GAP),
            hud_row_y + (HUD_ROW_H - HUD_HEART_SIZE) / 2,
            HUD_HEART_SIZE,
            HUD_HEART_SIZE
        };
        SDL_RenderCopy(renderer, hud->star_tex, NULL, &dst);
    }

    /* ---- Player icon + lives counter ------------------------------ */
    /*
     * Position the player icon after the last possible heart slot so it
     * doesn't shift when hearts are lost.
     */
    int icon_x = HUD_MARGIN + MAX_HEARTS * (HUD_HEART_SIZE + HUD_HEART_GAP) + 6;

    /*
     * Source rect: crop to just the visible character art within the
     * 48×48 idle frame.  The art occupies x=16..31, y=19..31 (16×13 px);
     * the rest is transparent padding.  Displaying at HUD_ICON_W × HUD_ICON_H
     * gives a 1:1 pixel-crisp icon that aligns with the hearts and text.
     */
    SDL_Rect icon_src = {16, 19, 16, 13};
    SDL_Rect icon_dst = {
        icon_x,
        hud_row_y + (HUD_ROW_H - HUD_ICON_H) / 2,
        HUD_ICON_W,
        HUD_ICON_H
    };
    if (hud->player_icon) {
        SDL_RenderCopy(renderer, hud->player_icon, &icon_src, &icon_dst);
    }

    /*
     * "x3" — lives counter text, rendered right next to the player icon.
     * snprintf formats the number; render_text draws it in white.
     * Aligned to HUD_ROW_H so it sits level with hearts and score.
     */
    char lives_buf[8];
    snprintf(lives_buf, sizeof(lives_buf), "x%d", lives);
    render_text(hud->font, renderer, lives_buf,
                icon_x + HUD_ICON_W + 4,
                hud_row_y + (HUD_ROW_H - 13) / 2);

    /* ---- Score (top-right) ---------------------------------------- */
    /*
     * Format the score and right-align it.  TTF_SizeText returns the pixel
     * width of the rendered string so we can position the left edge correctly.
     */
    char score_buf[32];
    snprintf(score_buf, sizeof(score_buf), "SCORE: %d", score);

    int text_w = 0;
    if (TTF_SizeText(hud->font, score_buf, &text_w, NULL) != 0) {
        fprintf(stderr, "TTF_SizeText failed for score text: %s\n", TTF_GetError());
        text_w = 0;  /* Fallback: draw at GAME_W - HUD_MARGIN, as before */
    }
    /*
     * Compute score text x position, leaving room for the coin icon after it.
     * Layout: [SCORE: 123] [coin_icon]
     * The coin icon sits immediately to the right of the score text.
     */
    int coin_gap = 3;  /* px gap between score text and coin icon */
    int total_score_w = text_w + coin_gap + HUD_COIN_ICON_SIZE;
    int score_x = GAME_W - HUD_MARGIN - total_score_w;

    render_text(hud->font, renderer, score_buf,
                score_x,
                hud_row_y + (HUD_ROW_H - 13) / 2);

    /* Draw the coin icon right after the score text */
    if (hud->coin_icon) {
        SDL_Rect coin_dst = {
            score_x + text_w + coin_gap,
            hud_row_y + (HUD_ROW_H - HUD_COIN_ICON_SIZE) / 2,
            HUD_COIN_ICON_SIZE,
            HUD_COIN_ICON_SIZE
        };
        SDL_RenderCopy(renderer, hud->coin_icon, NULL, &coin_dst);
    }
}

/* ------------------------------------------------------------------ */

/*
 * hud_cleanup — Release the font and heart texture.
 *
 * TTF_CloseFont releases FreeType memory; SDL_DestroyTexture releases
 * GPU memory.  Both are set to NULL for double-free safety.
 */
void hud_cleanup(Hud *hud)
{
    if (hud->player_icon) {
        SDL_DestroyTexture(hud->player_icon);
        hud->player_icon = NULL;
    }
    if (hud->coin_icon) {
        SDL_DestroyTexture(hud->coin_icon);
        hud->coin_icon = NULL;
    }
    if (hud->star_tex) {
        SDL_DestroyTexture(hud->star_tex);
        hud->star_tex = NULL;
    }
    if (hud->font) {
        TTF_CloseFont(hud->font);
        hud->font = NULL;
    }
}
