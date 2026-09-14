#include <stdio.h>

#include <SDL.h>

#include "core/game_overlay.h"
#include "core/game_terminal.h"
#include "input/game_events.h"
#include "input/game_input.h"
#include "input/game_web_input.h"

static int restart_calls;
static int load_next_phase_calls;

int game_load_next_phase(GameState *gs)
{
    (void)gs;
    load_next_phase_calls++;
    return 0;
}

void game_restart_after_game_over(GameState *gs)
{
    restart_calls++;
    gs->game_over = 0;
}

static int expect_int(const char *name, int actual, int expected)
{
    if (actual != expected) {
        fprintf(stderr, "game_events_test: %s got %d expected %d\n",
                name, actual, expected);
        return 1;
    }
    return 0;
}

static void reset_counters(void)
{
    restart_calls = 0;
    load_next_phase_calls = 0;
}

static int push_controller_button(Uint8 button)
{
    SDL_Event event;
    SDL_zero(event);
    event.type = SDL_CONTROLLERBUTTONDOWN;
    event.cbutton.button = button;
    return SDL_PushEvent(&event) == 1 ? 0 : 1;
}

static int push_key(SDL_Keycode key)
{
    SDL_Event event;
    SDL_zero(event);
    event.type = SDL_KEYDOWN;
    event.key.keysym.sym = key;
    return SDL_PushEvent(&event) == 1 ? 0 : 1;
}

static int controller_back_exits_game_over_overlay(void)
{
    GameState gs = {0};
    gs.running = 1;
    gs.game_over = 1;

    reset_counters();
    if (push_controller_button(SDL_CONTROLLER_BUTTON_BACK) != 0) return 1;
    game_handle_events(&gs);

    if (expect_int("Back routes game-over exit", gs.route, GAME_ROUTE_EXIT) != 0) return 1;
    if (expect_int("Back does not restart game-over", restart_calls, 0) != 0) return 1;
    return 0;
}

static int controller_back_exits_completion_overlay(void)
{
    GameState gs = {0};
    gs.running = 1;
    gs.completion.complete = 1;

    reset_counters();
    if (push_controller_button(SDL_CONTROLLER_BUTTON_BACK) != 0) return 1;
    game_handle_events(&gs);

    if (expect_int("Back routes completion exit", gs.route, GAME_ROUTE_EXIT) != 0) return 1;
    if (expect_int("Back does not load next phase", load_next_phase_calls, 0) != 0) return 1;
    return 0;
}

static int controller_start_respects_completion_priority_over_game_over(void)
{
    GameState gs = {0};
    gs.running = 1;
    gs.game_over = 1;
    gs.completion.complete = 1;

    reset_counters();
    if (push_controller_button(SDL_CONTROLLER_BUTTON_START) != 0) return 1;
    game_handle_events(&gs);

    if (expect_int("Start confirms replay", gs.route, GAME_ROUTE_REPLAY) != 0) return 1;
    if (expect_int("Start did not restart lower-priority game-over", restart_calls, 0) != 0) return 1;
    return 0;
}

static int completion_navigation_and_first_route_wins(void)
{
    GameState gs = {0};
    gs.completion.complete = 1;
    gs.completion.pending_next_phase = 1;

    if (push_key(SDLK_DOWN) != 0) return 1;
    game_handle_events(&gs);
    if (expect_int("down focuses replay", gs.terminal_action_index, 1) != 0) return 1;

    if (push_key(SDLK_RETURN) != 0) return 1;
    if (push_key(SDLK_ESCAPE) != 0) return 1;
    game_handle_events(&gs);
    if (expect_int("first confirm route wins", gs.route, GAME_ROUTE_REPLAY) != 0) return 1;
    return 0;
}

static int failed_next_level_keeps_completion_overlay(void)
{
    GameState gs = {0};
    gs.completion.complete = 1;
    gs.completion.pending_next_phase = 1;
    gs.terminal_action_index = 0;

    if (push_key(SDLK_RETURN) != 0) return 1;
    game_handle_events(&gs);
    if (expect_int("next routes explicitly", gs.route, GAME_ROUTE_NEXT_LEVEL) != 0) return 1;
    if (expect_int("completion remains after request",
                   game_overlay_state(&gs), GAME_OVERLAY_LEVEL_COMPLETE) != 0) return 1;
    return 0;
}

static int game_over_retry_stays_in_place(void)
{
    GameState gs = {0};
    gs.game_over = 1;

    reset_counters();
    if (push_key(SDLK_RETURN) != 0) return 1;
    game_handle_events(&gs);
    if (expect_int("retry calls existing restart", restart_calls, 1) != 0) return 1;
    if (expect_int("retry clears game over", gs.game_over, 0) != 0) return 1;
    if (expect_int("retry has no cross-screen route", gs.route, GAME_ROUTE_NONE) != 0) return 1;
    return 0;
}

static int keyboard_escape_toggles_pause_in_active_gameplay(void)
{
    GameState gs = {0};
    gs.running = 1;

    if (push_key(SDLK_ESCAPE) != 0) return 1;
    game_handle_events(&gs);

    if (expect_int("Escape pauses active gameplay", gs.paused, 1) != 0) return 1;
    if (expect_int("Escape sets player pause reason",
                   game_overlay_pause_reasons(&gs), GAME_PAUSE_REASON_PLAYER) != 0) return 1;
    return 0;
}

static int semantic_touch_input(void)
{
    GameState gs = {0};
    if (game_web_input_touch(-1, 1) || game_web_input_touch(GAME_TOUCH_COUNT, 1) ||
        game_web_input_touch(GAME_TOUCH_LEFT, 2)) return 1;
    if (!game_web_input_touch(GAME_TOUCH_RIGHT, 1) || !game_web_input_touch(GAME_TOUCH_JUMP, 1)) return 1;
    if (expect_int("two held touch actions", game_web_input_take_touch_mask(), PLAYER_INPUT_RIGHT | PLAYER_INPUT_JUMP)) return 1;
    if (!game_web_input_touch(GAME_TOUCH_JUMP, 0) || !game_web_input_touch(GAME_TOUCH_RIGHT, 0)) return 1;
    if (expect_int("released touch actions", game_web_input_take_touch_mask(), 0)) return 1;
    game_web_input_touch(GAME_TOUCH_JUMP, 1);
    game_web_input_touch(GAME_TOUCH_JUMP, 0);
    if (expect_int("quick tap is buffered", game_web_input_take_touch_mask(), PLAYER_INPUT_JUMP) ||
        expect_int("quick tap consumed once", game_web_input_take_touch_mask(), 0)) return 1;
    game_web_input_touch(GAME_TOUCH_LEFT, 1);
    game_input_arm_release_latch(&gs, NULL);
    if (expect_int("route latch clears touch", game_web_input_take_touch_mask(), 0)) return 1;
    SDL_FlushEvents(SDL_KEYDOWN, SDL_KEYUP);
    game_web_input_touch(GAME_TOUCH_PAUSE, 1);
    game_web_input_touch(GAME_TOUCH_PAUSE, 0);
    if (expect_int("pause is not a movement bit", game_web_input_take_touch_mask(), 0)) return 1;
    gs.running = 1;
    game_handle_events(&gs);
    if (expect_int("touch pause dispatches SDL action", gs.paused, 1)) return 1;
    game_web_input_clear_touch();
    SDL_FlushEvents(SDL_KEYDOWN, SDL_KEYUP);
    return 0;
}

int main(void)
{
    if (game_web_input_touch(GAME_TOUCH_JUMP, 1) != 0) return 1;
    if (SDL_Init(SDL_INIT_EVENTS) != 0) {
        fprintf(stderr, "game_events_test: SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    if (semantic_touch_input() != 0) return 1;
    if (controller_back_exits_game_over_overlay() != 0) return 1;
    if (controller_back_exits_completion_overlay() != 0) return 1;
    if (controller_start_respects_completion_priority_over_game_over() != 0) return 1;
    if (completion_navigation_and_first_route_wins() != 0) return 1;
    if (failed_next_level_keeps_completion_overlay() != 0) return 1;
    if (game_over_retry_stays_in_place() != 0) return 1;
    if (keyboard_escape_toggles_pause_in_active_gameplay() != 0) return 1;

    SDL_Quit();
    if (game_web_input_touch(GAME_TOUCH_JUMP, 1) != 0) return 1;
    puts("game_events_test: ok");
    return 0;
}
