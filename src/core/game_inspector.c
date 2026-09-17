#include "game_inspector.h"
#include "game_experiment.h"
#include "game_overlay.h"
#include "../levels/level_physics.h"
#include "../screens/settings_menu.h"
#include "../player/player_internal.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

typedef struct { const char *name; size_t offset; } PhysicsField;
#define FIELD(name) {#name, offsetof(Player, name)}
static const PhysicsField fields[] = {
    FIELD(walk_max_speed), FIELD(run_max_speed), FIELD(walk_ground_accel),
    FIELD(run_ground_accel), FIELD(ground_friction), FIELD(ground_counter_accel),
    FIELD(air_accel_walk), FIELD(air_accel_run), FIELD(air_friction)
};
#undef FIELD
_Static_assert(sizeof(fields) / sizeof(fields[0]) == INSPECTOR_PHYSICS_COUNT, "physics tape layout");

const char *game_inspector_physics_name(int index) { return fields[index].name; }

void game_inspector_physics(Player *player, float *values, int apply)
{
    for (int i = 0; i < INSPECTOR_PHYSICS_COUNT; i++) {
        float *field = (float *)((char *)player + fields[i].offset);
        if (apply) *field = values[i];
        else values[i] = *field;
    }
}

void game_inspector_reset_physics(GameState *gs)
{
    level_apply_player_physics(&gs->player, gs->runtime.current_level);
}

int game_inspector_event(GameState *gs, const SDL_Event *event)
{
    if (!gs->debug_mode || event->type != SDL_KEYDOWN || event->key.repeat ||
        (gs->settings_menu && gs->settings_menu->open)) return 0;
    SDL_Keycode key = event->key.keysym.sym;
    /* Export remains available on completion/game-over screens. */
    if (key == SDLK_F9) { game_experiment_export(gs); return 1; }
    if (game_overlay_blocks_update(gs) || gs->route != GAME_ROUTE_NONE) return 0;
    switch (key) {
    case SDLK_F2:
        gs->inspector.frozen = !gs->inspector.frozen;
        gs->inspector.step_requested = 0;
        return 1;
    case SDLK_F3:
        gs->inspector.frozen = 1;
        gs->inspector.step_requested = 1;
        return 1;
    case SDLK_F4:
        if (gs->experiment && gs->experiment->replaying) {
            debug_log(&gs->debug, "Replay uses recorded step durations");
            return 1;
        }
        gs->inspector.slow_mode = (gs->inspector.slow_mode + 1) % 3;
        return 1;
    case SDLK_F6:
        gs->inspector.physics_field = (gs->inspector.physics_field + 1) % INSPECTOR_PHYSICS_COUNT;
        return 1;
    case SDLK_F10:
        gs->inspector.entity_index = (gs->inspector.entity_index + 1) %
            (1 + gs->fish_count + gs->float_platform_count + gs->circular_saw_count);
        return 1;
    case SDLK_F7:
        if (!gs->experiment || !gs->experiment->replaying) game_inspector_reset_physics(gs);
        return 1;
    case SDLK_F8:
        if (game_experiment_begin(gs)) debug_log(&gs->debug, "Cannot start experiment");
        return 1;
    case SDLK_MINUS:
    case SDLK_EQUALS: {
        if (gs->experiment && gs->experiment->replaying) return 1;
        float *value = (float *)((char *)&gs->player + fields[gs->inspector.physics_field].offset);
        float next = *value + (key == SDLK_EQUALS ? 25.0f : -25.0f);
        if (next >= 0 && next <= MAX_LEVEL_MOTION) *value = next;
        return 1;
    }
    default: return 0;
    }
}

float game_inspector_step(GameState *gs, float elapsed)
{
    if (game_overlay_blocks_update(gs) || (gs->settings_menu && gs->settings_menu->open) ||
        !gs->running || gs->route != GAME_ROUTE_NONE) {
        gs->inspector.step_requested = 0;
        return 0;
    }
    if (!gs->debug_mode) return elapsed;
    int step = gs->inspector.step_requested;
    gs->inspector.step_requested = 0;
    if (gs->inspector.frozen && !step) return 0;
    static const float speeds[] = {1.0f, 0.25f, 0.1f};
    float dt = step ? 1.0f / TARGET_FPS : elapsed * speeds[gs->inspector.slow_mode];
    return game_experiment_dt(gs, dt);
}

void game_inspector_render(GameState *gs)
{
    if (!gs->debug_mode || !gs->hud.font) return;
    char text[192];
    float values[INSPECTOR_PHYSICS_COUNT];
    game_inspector_physics(&gs->player, values, 0);
    static const float speeds[] = {1.0f, 0.25f, 0.1f};
    snprintf(text, sizeof(text), "%s %.2fx | tape %d steps", gs->inspector.frozen ? "FROZEN" : "LIVE",
             speeds[gs->inspector.slow_mode], gs->experiment ? gs->experiment->count : 0);
    const char *lines[6] = {text, "F2 freeze F3 step F4 speed", "F6 field -/+ tune F7 reset", "F8 record F9 export F10 inspect", NULL, NULL};
    char tuning[128];
    snprintf(tuning, sizeof(tuning), "%s %.0f | support %d CP %d",
             fields[gs->inspector.physics_field].name, values[gs->inspector.physics_field],
             gs->loop.fp_prev_riding, gs->checkpoint_index);
    lines[4] = tuning;
    char entity[160];
    int index = gs->inspector.entity_index - 1;
    if (index >= 0 && index < gs->fish_count) {
        const Fish *fish = &gs->fish[index];
        snprintf(entity, sizeof(entity), "Fish[%d] y %.1f vy %.1f wait %.2f", index, fish->y, fish->vy, fish->jump_timer);
    } else if ((index -= gs->fish_count) >= 0 && index < gs->float_platform_count) {
        const FloatPlatform *fp = &gs->float_platforms[index];
        snprintf(entity, sizeof(entity), "Platform[%d] rail %.2f fall %d stand %.2f", index, fp->t, fp->falling, fp->stand_timer);
    } else if ((index -= gs->float_platform_count) >= 0 && index < gs->circular_saw_count) {
        const CircularSaw *saw = &gs->circular_saws[index];
        snprintf(entity, sizeof(entity), "Saw[%d] x %.1f dir %d angle %.0f", index, saw->x, saw->direction, saw->spin_angle);
    } else {
        snprintf(entity, sizeof(entity), "Player ground %d climb %d hurt %.2f", gs->player.on_ground, gs->player.on_vine, gs->player.hurt_timer);
    }
    lines[5] = entity;
    SDL_SetRenderDrawColor(gs->renderer, 8, 12, 20, 235);
    SDL_Rect panel = {0, 28, GAME_W, 88};
    SDL_RenderFillRect(gs->renderer, &panel);
    for (int i = 0; i < 6; i++) {
        if (!gs->inspector.labels[i] || strcmp(gs->inspector.text[i], lines[i])) {
            SDL_Surface *surface = TTF_RenderUTF8_Solid(gs->hud.font, lines[i], (SDL_Color){240,240,200,255});
            if (!surface) continue;
            SDL_Texture *texture = SDL_CreateTextureFromSurface(gs->renderer, surface);
            if (texture) {
                SDL_DestroyTexture(gs->inspector.labels[i]);
                gs->inspector.labels[i] = texture;
                gs->inspector.width[i] = surface->w;
                gs->inspector.height[i] = surface->h;
                SDL_strlcpy(gs->inspector.text[i], lines[i], sizeof(gs->inspector.text[i]));
            }
            SDL_FreeSurface(surface);
        }
        SDL_Rect dst = {4, 30 + 14*i, gs->inspector.width[i], gs->inspector.height[i]};
        if (gs->inspector.labels[i]) SDL_RenderCopy(gs->renderer, gs->inspector.labels[i], NULL, &dst);
    }
    /* Highlight the resolved contact surface and show the physical foot point. */
    int x = (int)(gs->player.x + gs->player.w / 2) - (int)gs->camera.x;
    int y = (int)(gs->player.y + gs->player.h - PLAYER_FLOOR_SINK);
    SDL_SetRenderDrawColor(gs->renderer, gs->player.on_ground ? 0 : 255, 255, 255, 255);
    SDL_RenderDrawLine(gs->renderer, x - 5, y, x + 5, y);
}

void game_inspector_cleanup(GameState *gs)
{
    for (int i = 0; i < 6; i++) {
        SDL_DestroyTexture(gs->inspector.labels[i]);
        gs->inspector.labels[i] = NULL;
    }
}
