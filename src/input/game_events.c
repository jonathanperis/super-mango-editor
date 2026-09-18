/*
 * game_events.c — Decide which UI/game state owns a discrete input command.
 *
 * Movement is sampled elsewhere. This queue handles edges such as pause,
 * settings and terminal confirmation. A command consumed by settings must
 * not also jump or select an item on the screen underneath it.
 */
#include "game_events.h"
#include "game_input.h"
#include "../collision/collision_damage.h"
#include "../core/game_overlay.h"
#include "../core/game_terminal.h"
#include "../core/game_inspector.h"
#include "../screens/settings_menu.h"

static int terminal_overlay(const GameState *gs)
{
    GameOverlayState overlay = game_overlay_state(gs);
    return overlay == GAME_OVERLAY_LEVEL_COMPLETE || overlay == GAME_OVERLAY_GAME_OVER;
}

static void request_terminal_action(GameState *gs, GameTerminalAction action)
{
    /* Retry resets the current level in place. Other terminal choices become
     * routes for AppSession to apply after this frame has finished. */
    if (!gs || gs->route != GAME_ROUTE_NONE) return;
    if (action == GAME_TERMINAL_ACTION_RETRY) {
        game_restart_after_game_over(gs);
        music_resume();
        gs->loop.prev_ticks = clock_millis();
        game_input_arm_release_latch(gs, NULL);
    } else
        gs->route = game_terminal_action_route(action);
}

static void sync_pause_music(GameState *gs)
{
    if (game_overlay_state(gs) == GAME_OVERLAY_PAUSED)
        music_pause();
    else {
        music_resume();
        gs->loop.prev_ticks = clock_millis();
    }
}

void game_handle_events(GameState *gs)
{
    InputEvent event;
    while (input_poll(&event)) {
        /* Preserve priority: settings/capture first, then the debug inspector,
         * then normal screen commands. Key-repeat must not toggle pause twice. */
        int was_open = gs->settings_menu && gs->settings_menu->open;
        if (gs->debug_mode && was_open && gs->settings_menu->capture == 1 && event.type == INPUT_KEY_DOWN &&
            ((event.key >= KEY_F2 && event.key <= KEY_F10) || event.key == KEY_MINUS || event.key == KEY_EQUAL)) {
            str_copy(gs->settings_menu->message, "Reserved for debug inspection; choose another key.", sizeof(gs->settings_menu->message));
            continue;
        }
        if (gs->route == GAME_ROUTE_NONE && settings_menu_event(gs->settings_menu, gs->profile, &event,
                                                               terminal_overlay(gs) ? -1 : PAD_BACK)) {
            if (was_open && !gs->settings_menu->open) {
                GameInputPhysicalState inherited = {0};
                if (event.type == INPUT_PAD_DOWN && (event.button == PAD_A || event.button == PAD_START))
                    inherited.controller_mask = GAME_INPUT_CONFIRM;
                game_input_arm_release_latch(gs, &inherited);
            }
            continue;
        }
        if (game_inspector_event(gs, &event)) continue;
        if (event.type == INPUT_QUIT) {
            if (gs->route == GAME_ROUTE_NONE) gs->route = GAME_ROUTE_EXIT;
        } else if (event.type == INPUT_FOCUS) {
            game_overlay_set_pause_reason(gs, GAME_PAUSE_REASON_FOCUS, !event.focused);
            sync_pause_music(gs);
        } else if (event.type == INPUT_PAD_ADDED) {
            if (!gs->controller) gs->controller = event.device;
        } else if (event.type == INPUT_PAD_REMOVED) {
            if (gs->controller == event.device) {
                gs->controller = 0;
                game_input_clear_controller_latch(gs);
            }
        } else if (event.type == INPUT_KEY_DOWN || event.type == INPUT_PAD_DOWN) {
            if (event.repeat) continue;
            int key = event.type == INPUT_KEY_DOWN ? event.key : KEY_NULL;
            int button = event.type == INPUT_PAD_DOWN ? event.button : -1;
            int confirm = key == KEY_ENTER || key == KEY_KP_ENTER || key == KEY_SPACE || button == PAD_A || button == PAD_START;
            if (terminal_overlay(gs)) {
                if (gs->route != GAME_ROUTE_NONE) continue;
                if (key == KEY_UP || key == KEY_W || button == PAD_UP) game_terminal_move(gs, -1);
                else if (key == KEY_DOWN || key == KEY_S || button == PAD_DOWN) game_terminal_move(gs, 1);
                else if (key == KEY_ESCAPE || button == PAD_BACK || button == PAD_B)
                    request_terminal_action(gs, GAME_TERMINAL_ACTION_EXIT);
                else if (confirm) request_terminal_action(gs, game_terminal_focused_action(gs));
            } else if (key == KEY_ESCAPE || button == PAD_START) {
                game_overlay_toggle_pause(gs);
                sync_pause_music(gs);
            } else if (confirm && game_overlay_state(gs) == GAME_OVERLAY_PAUSED) {
                game_overlay_resume(gs);
                sync_pause_music(gs);
            }
        }
    }
}
