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
#include "../core/game_timing.h"
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
    if (!gs || gs->screen.route != GAME_ROUTE_NONE) return;
    if (action == GAME_TERMINAL_ACTION_RETRY) {
        game_restart_after_game_over(gs);
        game_music_sync(gs);
        game_timing_restart_clock(gs);
        game_input_arm_release_latch(gs, NULL);
    } else
        gs->screen.route = game_terminal_action_route(action);
}

static void sync_pause_music(GameState *gs)
{
    /* game_music_should_play also honours the settings panel: regaining
     * window focus while settings are open must keep the music paused. */
    game_music_sync(gs);
    if (game_overlay_state(gs) != GAME_OVERLAY_PAUSED)
        game_timing_restart_clock(gs);
}

void game_handle_events(GameState *gs)
{
    InputEvent event;
    while (input_poll(&event)) {
        /* Preserve priority: settings/capture first, then the debug inspector,
         * then normal screen commands. Key-repeat must not toggle pause twice. */
        int was_open = gs->screen.settings_menu && gs->screen.settings_menu->open;
        /* Settings consume every key while open, including the inspector's
         * F2-F10/-/= (which game_settings_key_allowed refuses as bindings). */
        if (gs->screen.route == GAME_ROUTE_NONE && settings_menu_event(gs->screen.settings_menu, gs->screen.profile, &event,
                                                               terminal_overlay(gs) ? -1 : PAD_BACK)) {
            if (was_open && !gs->screen.settings_menu->open) {
                GameInputPhysicalState inherited = {0};
                if (event.type == INPUT_PAD_DOWN && (event.button == PAD_A || event.button == PAD_START))
                    inherited.controller_mask = GAME_INPUT_CONFIRM;
                game_input_arm_release_latch(gs, &inherited);
            }
            continue;
        }
        if (game_inspector_event(gs, &event)) continue;
        if (event.type == INPUT_QUIT) {
            if (gs->screen.route == GAME_ROUTE_NONE) gs->screen.route = GAME_ROUTE_EXIT;
        } else if (event.type == INPUT_FOCUS) {
            game_overlay_set_pause_reason(gs, GAME_PAUSE_REASON_FOCUS, !event.focused);
            sync_pause_music(gs);
        } else if (event.type == INPUT_PAD_ADDED) {
            gamepad_adopt_controller(gs, event.device);
        } else if (event.type == INPUT_PAD_REMOVED) {
            if (gs->screen.controller == event.device) {
                gs->screen.controller = 0;
                game_input_clear_controller_latch(gs);
            }
        } else if (event.type == INPUT_KEY_DOWN || event.type == INPUT_PAD_DOWN) {
            if (event.repeat) continue;
            int key = event.type == INPUT_KEY_DOWN ? event.key : KEY_NULL;
            int button = event.type == INPUT_PAD_DOWN ? event.button : -1;
            int confirm = game_input_event_confirms(&event);
            if (terminal_overlay(gs)) {
                if (gs->screen.route != GAME_ROUTE_NONE) continue;
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
                /* Space and A are also the default Jump bindings. Without a
                 * latch, the key that resumed would still be held on the
                 * first gameplay step and the player would jump. */
                GameInputPhysicalState inherited = {0};
                if (button == PAD_A || button == PAD_START)
                    inherited.controller_mask = GAME_INPUT_CONFIRM;
                game_input_arm_release_latch(gs, &inherited);
            }
        }
    }
}
