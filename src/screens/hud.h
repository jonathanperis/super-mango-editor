/*
 * hud.h — Public interface for the heads-up display module.
 *
 * The HUD renders at the top of the screen as an overlay:
 *   Left  : borrowed yellow-star icons showing remaining hit points.
 *   Center: player icon + "x{lives}" showing remaining lives.
 *   Right : "SCORE: {n}" showing the player's current score.
 *
 * All rendering uses the 400×300 logical render target, scaled on presentation.
 */
#pragma once

#include "../shared/text.h"

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
 * font     : owned round9x13.ttf font loaded at 13 logical pixels.
 * star_tex : borrowed star_yellow.png used as the health indicator.
 * coin_icon: owned hud_coins.png texture beside the score.
 * player_icon: borrowed player sprite sheet, cropped for the lives counter.
 */
/*
 * HUD_COIN_ICON_SIZE — display size of the coin icon next to the score.
 */
#define HUD_COIN_ICON_SIZE  12

typedef struct {
    TextFont *font;           /* owned HUD font */
    Texture2D *star_tex;      /* borrowed heart indicator */
    Texture2D *coin_icon;     /* owned coin icon */
    Texture2D *player_icon;   /* borrowed player sprite sheet */
} Hud;

/* Load the font; accept shared textures from GameState to avoid duplicates. */
int hud_init(Hud *hud, Texture2D *star_tex, Texture2D *player_tex);

/*
 * hud_render — Draw the full HUD overlay.
 *
 * hearts     : current hit points (0–MAX_HEARTS).
 * lives      : remaining extra lives.
 * score      : current score to display.
 * checkpoint_index is zero-based; temporary feedback labels display index+1.
 * feedback_until/now use the same wrapping millisecond clock.
 */
void hud_render(const Hud *hud,
                int hearts, int lives, int score,
                int checkpoint_index, int feedback_kind, uint32_t feedback_until,
                uint32_t now);

int hud_checkpoint_feedback_visible(int feedback_kind, uint32_t deadline, uint32_t now);
const char *hud_checkpoint_feedback_label(int feedback_kind, int checkpoint_index);

/* Release the owned font/coin icon; leave borrowed star/player textures alive. */
void hud_cleanup(Hud *hud);
