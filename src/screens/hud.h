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

#include "../shared/asset_cache.h"  /* AssetCache — hud_init asks it for the font and icon */
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
 * font     : raylib's built-in font at TEXT_FONT_SIZE px, held from the
 *            asset cache (one reference).
 * star_tex : borrowed star_yellow.png used as the health indicator.
 * coin_icon: hud_coins.png beside the score, held from the asset cache.
 * player_icon: borrowed player sprite sheet, cropped for the lives counter.
 *
 * "Held" and "borrowed" differ in who must act at cleanup: hud_cleanup
 * releases the two held ones to the cache and leaves the borrowed ones alone.
 */
/*
 * HUD_COIN_ICON_SIZE — display size of the coin icon next to the score.
 */
#define HUD_COIN_ICON_SIZE  12

typedef struct {
    TextFont *font;           /* held from the asset cache */
    Texture2D *star_tex;      /* borrowed heart indicator */
    Texture2D *coin_icon;     /* held from the asset cache */
    Texture2D *player_icon;   /* borrowed player sprite sheet */
} Hud;

/*
 * HudLayout — where each HUD element sits this frame.
 *
 * hud_render draws into these rectangles and the --debug overlay outlines
 * the very same ones, so the two can never disagree. Text rectangles use
 * the measured width of the formatted string, which changes with the score.
 */
typedef struct {
    int     heart_count;          /* hearts to draw, 0..MAX_HEARTS        */
    IntRect hearts[MAX_HEARTS];   /* one star icon per remaining heart    */
    IntRect player_icon;          /* cropped player sprite beside lives   */
    IntRect lives_text;           /* "x3"                                 */
    IntRect score_text;           /* "SCORE: 1200", right-aligned         */
    IntRect coin_icon;            /* coin to the right of the score       */
    char    lives[32];            /* the formatted lives text             */
    char    score[32];            /* the formatted score text             */
} HudLayout;

/* Fill layout for these values; needs hud->font to measure the text. */
void hud_layout(const Hud *hud, int hearts, int lives, int score, HudLayout *layout);

/* Get the font and coin icon from cache (NULL = load them for this Hud
 * alone); accept shared textures from GameState to avoid duplicates. */
int hud_init(Hud *hud, AssetCache *cache, Texture2D *star_tex, Texture2D *player_tex);

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

/* Give the font and coin icon back to the cache hud_init took them from;
 * leave the borrowed star/player textures alone. */
void hud_cleanup(Hud *hud, AssetCache *cache);
