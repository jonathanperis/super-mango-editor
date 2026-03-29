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
#include "game.h"       /* GAME_W */

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
    hud->font = TTF_OpenFont("assets/Round9x13.ttf", 13);
    if (!hud->font) {
        fprintf(stderr, "Failed to load Round9x13.ttf: %s\n", TTF_GetError());
        exit(EXIT_FAILURE);
    }

    /*
     * IMG_LoadTexture — decode Stars_Ui.png and upload it to GPU memory.
     * This single texture is drawn once per current heart in hud_render.
     */
    hud->star_tex = IMG_LoadTexture(renderer, "assets/Stars_Ui.png");
    if (!hud->star_tex) {
        fprintf(stderr, "Failed to load Stars_Ui.png: %s\n", IMG_GetError());
        TTF_CloseFont(hud->font);
        hud->font = NULL;
        exit(EXIT_FAILURE);
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
                SDL_Texture *player_tex,
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
            hud_row_y + (HUD_ICON_SIZE - HUD_HEART_SIZE) / 2,
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
     * Source rect: first frame of Player.png (row 0, col 0, 48×48 px).
     * This is the idle standing pose — recognisable as the player character.
     */
    SDL_Rect icon_src = {0, 0, 48, 48};
    SDL_Rect icon_dst = {icon_x, hud_row_y, HUD_ICON_SIZE, HUD_ICON_SIZE};
    SDL_RenderCopy(renderer, player_tex, &icon_src, &icon_dst);

    /*
     * "x3" — lives counter text, rendered right next to the player icon.
     * snprintf formats the number; render_text draws it in white.
     */
    char lives_buf[8];
    snprintf(lives_buf, sizeof(lives_buf), "x%d", lives);
    /* Centre text vertically within the icon row; font is 13 px tall by design */
    render_text(hud->font, renderer, lives_buf,
                icon_x + HUD_ICON_SIZE + 4,
                hud_row_y + (HUD_ICON_SIZE - 13) / 2);

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
    /* Centre text vertically within the icon row; font is 13 px tall by design */
    render_text(hud->font, renderer, score_buf,
                GAME_W - HUD_MARGIN - text_w,
                hud_row_y + (HUD_ICON_SIZE - 13) / 2);
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
    if (hud->star_tex) {
        SDL_DestroyTexture(hud->star_tex);
        hud->star_tex = NULL;
    }
    if (hud->font) {
        TTF_CloseFont(hud->font);
        hud->font = NULL;
    }
}
