/*
 * hud.c — Screen-space feedback on the 400x300 logical canvas.
 *
 * World sprites subtract camera X; HUD elements do not move with the world.
 * The HUD owns its font and coin icon, but borrows the star/player textures.
 * Text is measured before right alignment so wider scores keep their margin.
 */
#include "hud.h"
#include "../game.h"
#include <stdio.h>

int hud_checkpoint_feedback_visible(int kind, uint32_t deadline, uint32_t now)
{
    /* Unsigned subtraction followed by a signed comparison handles the
     * 32-bit millisecond clock wrapping during these short-lived notices. */
    return kind != CHECKPOINT_FEEDBACK_NONE && (int32_t)(now - deadline) < 0;
}

const char *hud_checkpoint_feedback_label(int kind, int index)
{
    (void)index;
    if (kind == CHECKPOINT_FEEDBACK_RESPAWN) return "RESPAWN";
    if (kind == CHECKPOINT_FEEDBACK_SAVED) return "CHECKPOINT";
    return "CP";
}

int hud_init(Hud *hud, Texture2D *star, Texture2D *player)
{
    hud->font = font_load("assets/fonts/round9x13.ttf", 13);
    if (!hud->font) {
        fprintf(stderr, "Failed to load assets/fonts/round9x13.ttf\n");
        return -1;
    }
    hud->star_tex = star;
    hud->player_icon = player;
    /* Borrowing lets gameplay and HUD share images. Cleanup below must not
     * unload these two textures: their owners are outside this Hud. */
    hud->coin_icon = texture_load("assets/sprites/screens/hud_coins.png");
    return 0;
}

void hud_render(const Hud *hud, int hearts, int lives, int score,
                int checkpoint_index, int feedback_kind, uint32_t feedback_until,
                uint32_t now)
{
    /* One icon per heart; reserve the full MAX_HEARTS width for the lives
     * counter so losing health does not shift the rest of the HUD. */
    for (int i = 0; i < hearts; i++) {
        IntRect dst = {HUD_MARGIN + i*(HUD_HEART_SIZE + HUD_HEART_GAP),
                       HUD_MARGIN, HUD_HEART_SIZE, HUD_HEART_SIZE};
        sprite_draw(hud->star_tex, NULL, &dst, 0, SPRITE_NORMAL, WHITE);
    }
    int icon_x = HUD_MARGIN + MAX_HEARTS*(HUD_HEART_SIZE + HUD_HEART_GAP) + 6;
    /* Crop visible player art out of its larger animation-sheet frame. */
    IntRect src = {16, 19, 16, 13};
    IntRect dst = {icon_x, HUD_MARGIN + (HUD_ROW_H-HUD_ICON_H)/2, HUD_ICON_W, HUD_ICON_H};
    sprite_draw(hud->player_icon, &src, &dst, 0, SPRITE_NORMAL, WHITE);
    char text[32];
    snprintf(text, sizeof(text), "x%d", lives);
    int text_y = HUD_MARGIN + (HUD_ROW_H-13)/2;
    font_draw(hud->font, text, icon_x+HUD_ICON_W+4, text_y, WHITE);
    snprintf(text, sizeof(text), "SCORE: %d", score);
    /* Subtract the text width, gap and icon width from the right margin. */
    int width = 0;
    font_measure(hud->font, text, &width, NULL);
    int score_x = GAME_W-HUD_MARGIN-width-3-HUD_COIN_ICON_SIZE;
    font_draw(hud->font, text, score_x, text_y, WHITE);
    dst = (IntRect){score_x+width+3, HUD_MARGIN+(HUD_ROW_H-HUD_COIN_ICON_SIZE)/2,
                    HUD_COIN_ICON_SIZE, HUD_COIN_ICON_SIZE};
    sprite_draw(hud->coin_icon, NULL, &dst, 0, SPRITE_NORMAL, WHITE);
    if (hud_checkpoint_feedback_visible(feedback_kind, feedback_until, now)) {
        const char *label = hud_checkpoint_feedback_label(feedback_kind, checkpoint_index);
        if (checkpoint_index >= 0)
            snprintf(text, sizeof(text), "%s CP %d", label, checkpoint_index + 1);
        else
            snprintf(text, sizeof(text), "%s", label);
        font_draw(hud->font, text, HUD_MARGIN, GAME_H-HUD_MARGIN-13, WHITE);
    }
}

void hud_cleanup(Hud *hud)
{
    texture_unload(hud->coin_icon);
    font_unload(hud->font);
    *hud = (Hud){0};
}
