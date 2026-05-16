#include <stdio.h>

#include <SDL.h>

#include "core/game_overlay.h"
#include "input/game_events.h"

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

    if (expect_int("Back exits game-over", gs.running, 0) != 0) return 1;
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

    if (expect_int("Back exits completion", gs.running, 0) != 0) return 1;
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

    if (expect_int("Start exits completion without next phase", gs.running, 0) != 0) return 1;
    if (expect_int("Start did not restart lower-priority game-over", restart_calls, 0) != 0) return 1;
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

int main(void)
{
    if (SDL_Init(SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER) != 0) {
        fprintf(stderr, "game_events_test: SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    if (controller_back_exits_game_over_overlay() != 0) return 1;
    if (controller_back_exits_completion_overlay() != 0) return 1;
    if (controller_start_respects_completion_priority_over_game_over() != 0) return 1;
    if (keyboard_escape_toggles_pause_in_active_gameplay() != 0) return 1;

    SDL_Quit();
    puts("game_events_test: ok");
    return 0;
}
