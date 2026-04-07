/*
 * hud.h — Public interface for the heads-up display module.
 *
 * The HUD renders at the top of the screen as an overlay:
 *   Left  : heart icons (Stars_Ui.png) showing remaining hit points.
 *   Center: player icon + "x{lives}" showing remaining lives.
 *   Right : "SCORE: {n}" showing the player's current score.
 *
 * All rendering uses the 400×300 logical coordinate space; SDL's logical
 * size scaling handles the 2× magnification automatically.
 */
#pragma once

#include <SDL.h>       /* SDL_Renderer, SDL_Texture, SDL_Rect */
#include <SDL_ttf.h>   /* TTF_Font for score and lives text   */

#define MAX_HEARTS      3     /* maximum hearts the player can have        */
#define DEFAULT_LIVES   3     /* lives the player starts with              */
#define HUD_MARGIN      4     /* pixel margin from screen edges            */
#define HUD_HEART_SIZE 16     /* display size of each heart icon (px)      */
#define HUD_HEART_GAP   2     /* horizontal gap between heart icons (px)   */
#define HUD_ICON_W     16     /* display width  of the player icon (px)    */
#define HUD_ICON_H     13     /* display height of the player icon (px)    */
#define HUD_ROW_H      16     /* row height for text alignment (font px)   */

/*
 * Hud — resources needed by the HUD renderer.
 *
 * font     : Round9x13.ttf loaded at its native bitmap size.
 * star_tex : Stars_Ui.png used as a heart/health indicator icon.
 */
/*
 * HUD_COIN_ICON_SIZE — display size of the coin icon next to the score.
 */
#define HUD_COIN_ICON_SIZE  12

typedef struct {
    TTF_Font    *font;        /* bitmap font for HUD text                  */
    SDL_Texture *star_tex;    /* heart indicator icon (Star_Yellow.png)    */
    SDL_Texture *coin_icon;   /* coin icon next to score (Coins_Ui.png)    */
    SDL_Texture *player_icon; /* small player icon for lives (Player.png)  */
} Hud;

/* Load the font; accept shared textures from GameState to avoid duplicates. */
void hud_init(Hud *hud, SDL_Renderer *renderer,
              SDL_Texture *star_tex, SDL_Texture *player_tex);

/*
 * hud_render — Draw the full HUD overlay.
 *
 * player_tex : the player's sprite sheet; first frame is used as the icon.
 * hearts     : current hit points (0–MAX_HEARTS).
 * lives      : remaining extra lives.
 * score      : current score to display.
 */
void hud_render(const Hud *hud, SDL_Renderer *renderer,
                int hearts, int lives, int score);

/* Release the font and heart texture. */
void hud_cleanup(Hud *hud);
