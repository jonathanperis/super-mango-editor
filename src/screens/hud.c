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
    hud->font = font_load();
    if (!hud->font) {
        fprintf(stderr, "Failed to prepare raylib's default font\n");
        return -1;
    }
    hud->star_tex = star;
    hud->player_icon = player;
    /* Borrowing lets gameplay and HUD share images. Cleanup below must not
     * unload these two textures: their owners are outside this Hud. */
    hud->coin_icon = texture_load("assets/sprites/screens/hud_coins.png");
    return 0;
}

void hud_layout(const Hud *hud, int hearts, int lives, int score, HudLayout *layout)
{
    /* One icon per heart; reserve the full MAX_HEARTS width for the lives
     * counter so losing health does not shift the rest of the HUD. */
    layout->heart_count = hearts < 0 ? 0 : hearts > MAX_HEARTS ? MAX_HEARTS : hearts;
    for (int i = 0; i < layout->heart_count; i++)
        layout->hearts[i] = (IntRect){HUD_MARGIN + i*(HUD_HEART_SIZE + HUD_HEART_GAP),
                                      HUD_MARGIN, HUD_HEART_SIZE, HUD_HEART_SIZE};
    int icon_x = HUD_MARGIN + MAX_HEARTS*(HUD_HEART_SIZE + HUD_HEART_GAP) + 6;
    int text_y = HUD_MARGIN + (HUD_ROW_H-TEXT_FONT_SIZE)/2;
    layout->player_icon = (IntRect){icon_x, HUD_MARGIN + (HUD_ROW_H-HUD_ICON_H)/2,
                                    HUD_ICON_W, HUD_ICON_H};

    int width = 0;
    snprintf(layout->lives, sizeof(layout->lives), "x%d", lives);
    font_measure(hud->font, layout->lives, &width, NULL);
    layout->lives_text = (IntRect){icon_x+HUD_ICON_W+4, text_y, width, TEXT_FONT_SIZE};

    /* Right-align: subtract the text width, a 3 px gap and the coin icon
     * from the right margin. */
    snprintf(layout->score, sizeof(layout->score), "SCORE: %d", score);
    font_measure(hud->font, layout->score, &width, NULL);
    int score_x = GAME_W-HUD_MARGIN-width-3-HUD_COIN_ICON_SIZE;
    layout->score_text = (IntRect){score_x, text_y, width, TEXT_FONT_SIZE};
    layout->coin_icon = (IntRect){score_x+width+3, HUD_MARGIN+(HUD_ROW_H-HUD_COIN_ICON_SIZE)/2,
                                  HUD_COIN_ICON_SIZE, HUD_COIN_ICON_SIZE};
}

void hud_render(const Hud *hud, int hearts, int lives, int score,
                int checkpoint_index, int feedback_kind, uint32_t feedback_until,
                uint32_t now)
{
    HudLayout layout;
    hud_layout(hud, hearts, lives, score, &layout);
    for (int i = 0; i < layout.heart_count; i++)
        sprite_draw(hud->star_tex, NULL, &layout.hearts[i], 0, SPRITE_NORMAL, WHITE);
    /* Crop visible player art out of its larger animation-sheet frame. */
    IntRect src = {16, 19, 16, 13};
    sprite_draw(hud->player_icon, &src, &layout.player_icon, 0, SPRITE_NORMAL, WHITE);
    font_draw(hud->font, layout.lives, layout.lives_text.x, layout.lives_text.y, WHITE);
    font_draw(hud->font, layout.score, layout.score_text.x, layout.score_text.y, WHITE);
    sprite_draw(hud->coin_icon, NULL, &layout.coin_icon, 0, SPRITE_NORMAL, WHITE);
    char text[32];
    if (hud_checkpoint_feedback_visible(feedback_kind, feedback_until, now)) {
        const char *label = hud_checkpoint_feedback_label(feedback_kind, checkpoint_index);
        if (checkpoint_index >= 0)
            snprintf(text, sizeof(text), "%s CP %d", label, checkpoint_index + 1);
        else
            snprintf(text, sizeof(text), "%s", label);
        font_draw(hud->font, text, HUD_MARGIN, GAME_H-HUD_MARGIN-TEXT_FONT_SIZE, WHITE);
    }
}

void hud_cleanup(Hud *hud)
{
    texture_unload(hud->coin_icon);
    font_unload(hud->font);
    *hud = (Hud){0};
}
