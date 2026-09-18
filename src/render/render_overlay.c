/* Terminal overlay rendering. Backdrop and HUD remain owned by game render. */

#include "game_render.h"

#include <stdio.h>

#include "../core/game_terminal.h"

static void render_overlay_backdrop(GameState *gs, uint8_t alpha)
{
    (void)gs;
    DrawRectangle(0, 0, GAME_W, GAME_H, (Color){0,0,0,alpha});
}

static void render_centered_text(GameState *gs, const char *text,
                                 Color color, int y)
{
    int width = 0;
    if (font_measure(gs->hud.font, text, &width, NULL)) return;
    font_draw(gs->hud.font, text, (GAME_W-width)/2, y, color);
}

static void render_terminal_actions(GameState *gs, int first_y)
{
    GameTerminalActionList list;
    int focused;

    game_terminal_actions(gs, &list);
    focused = gs->terminal_action_index;
    if (focused < 0 || focused >= list.count) focused = 0;

    for (int i = 0; i < list.count; i++) {
        Color color = i == focused
            ? (Color){255, 215, 0, 255}
            : (Color){150, 150, 150, 255};
        char label[64];
        snprintf(label, sizeof(label), "%c %s %c",
                 i == focused ? '>' : ' ',
                 game_terminal_action_label(list.items[i]),
                 i == focused ? '<' : ' ');
        render_centered_text(gs, label, color, first_y + i * 16);
    }
}

void render_pause_overlay(GameState *gs)
{
    render_overlay_backdrop(gs, 150);
    if (gs->hud.font) {
        render_centered_text(gs, "Paused", (Color){255, 215, 0, 255}, 92);
        render_centered_text(gs, "Enter/Space/Esc/Start: resume",
                             (Color){255, 255, 255, 255}, 134);
        render_centered_text(gs, "Close window to quit",
                             (Color){190, 190, 190, 255}, 160);
        if (gs->settings_menu) render_centered_text(gs, "F1 / Back: settings",
                              (Color){190, 190, 190, 255}, 186);
    }
}

void render_game_over_overlay(GameState *gs)
{
    render_overlay_backdrop(gs, 190);
    if (gs->hud.font) {
        char line[96];
        render_centered_text(gs, "Game Over", (Color){255, 90, 90, 255}, 66);
        snprintf(line, sizeof(line), "Final Score: %d", gs->score);
        render_centered_text(gs, line, (Color){255, 255, 255, 255}, 104);
        render_terminal_actions(gs, 136);
        render_centered_text(gs, "Up/Down or D-pad: Select",
                             (Color){255, 255, 255, 255}, 192);
        render_centered_text(gs, "Enter/Space/A/Start: Confirm",
                             (Color){255, 255, 255, 255}, 208);
        render_centered_text(gs, "Esc/B/Back: Exit",
                             (Color){255, 255, 255, 255}, 224);
    }
}

void render_level_complete_overlay(GameState *gs)
{
    const int has_next_level = gs->completion.pending_next_phase;

    render_overlay_backdrop(gs, 180);
    if (gs->hud.font) {
        char line[96];
        int elapsed = (int)(gs->completion.elapsed + 0.5f);
        int minutes = elapsed / 60;
        int seconds = elapsed % 60;

        render_centered_text(gs,
                             has_next_level ? "Level Complete!" : "Game Complete!",
                             (Color){255, 215, 0, 255}, 54);
        snprintf(line, sizeof(line), "Score: %d", gs->score);
        render_centered_text(gs, line, (Color){255, 255, 255, 255}, 88);
        snprintf(line, sizeof(line), "Coins: %d/%d",
                 gs->completion.coins_collected,
                 gs->completion.coin_total);
        render_centered_text(gs, line, (Color){255, 255, 255, 255}, 104);
        snprintf(line, sizeof(line), "Lives: %d", gs->lives);
        render_centered_text(gs, line, (Color){255, 255, 255, 255}, 120);
        snprintf(line, sizeof(line), "Time: %02d:%02d", minutes, seconds);
        render_centered_text(gs, line, (Color){255, 255, 255, 255}, 136);
        if (!has_next_level) {
            render_centered_text(gs, "Congratulations!",
                                 (Color){100, 255, 100, 255}, 154);
        }
        render_terminal_actions(gs, 168);
        render_centered_text(gs, "Up/Down or D-pad: Select",
                             (Color){255, 255, 255, 255}, 232);
        render_centered_text(gs, "Enter/Space/A/Start: Confirm",
                             (Color){255, 255, 255, 255}, 248);
        render_centered_text(gs, "Esc/B/Back: Exit",
                             (Color){255, 255, 255, 255}, 264);
    }
}
