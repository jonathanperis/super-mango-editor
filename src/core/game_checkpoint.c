/*
 * game_checkpoint.c — Resolve authored or legacy respawn checkpoints.
 */

#include "game_checkpoint.h"

#include "../levels/level.h"

void game_checkpoint_feedback_set(GameState *gs, CheckpointFeedbackKind kind,
                                  Uint32 now, Uint32 duration)
{
    if (!gs) return;
    gs->checkpoint_feedback_kind = kind;
    gs->checkpoint_feedback_until = kind == CHECKPOINT_FEEDBACK_NONE
        ? 0 : now + duration;
}

void game_checkpoint_feedback_clear_expired(GameState *gs, Uint32 now)
{
    if (!gs || gs->checkpoint_feedback_kind == CHECKPOINT_FEEDBACK_NONE) return;
    if ((Sint32)(now - gs->checkpoint_feedback_until) >= 0) {
        game_checkpoint_feedback_set(gs, CHECKPOINT_FEEDBACK_NONE, now, 0);
    }
}

void game_checkpoint_update_authored(GameState *gs)
{
    const LevelDef *def;

    if (!gs) return;
    def = (const LevelDef *)gs->runtime.current_level;

    if (!def || def->checkpoint_count <= 0) return;

    {
        int best_index = gs->checkpoint_index;
        if (best_index < 0 || best_index >= def->checkpoint_count ||
            gs->respawn_x != def->checkpoints[best_index].x ||
            gs->respawn_y != def->checkpoints[best_index].y) {
            best_index = -1;
        }
        float best_x = (best_index >= 0)
                     ? def->checkpoints[best_index].x
                     : -1.0f;

        /* Scan instead of sorting: authored record order remains canonical. */
        for (int i = 0; i < def->checkpoint_count; i++) {
            const CheckpointPlacement *placement = &def->checkpoints[i];
            if (placement->x <= gs->player.x && placement->x > best_x) {
                best_index = i;
                best_x = placement->x;
            }
        }

        if (best_index != gs->checkpoint_index) {
            gs->checkpoint_index = best_index;
            gs->respawn_x = def->checkpoints[best_index].x;
            gs->respawn_y = def->checkpoints[best_index].y;
            game_checkpoint_feedback_set(gs, CHECKPOINT_FEEDBACK_SAVED,
                                          SDL_GetTicks(), 1200);
            if (gs->debug_mode) {
                debug_log(&gs->debug, "CHECKPOINT saved at x=%.0f y=%.0f",
                          gs->respawn_x, gs->respawn_y);
            }
        }
        return;
    }
}

void game_checkpoint_update(GameState *gs)
{
    const LevelDef *def;

    if (!gs) return;
    def = (const LevelDef *)gs->runtime.current_level;

    if (def && def->checkpoint_count > 0) {
        game_checkpoint_update_authored(gs);
        return;
    }

    /* Legacy levels retain automatic screen-boundary behavior exactly. */
    {
        int current_screen = (int)(gs->player.x / GAME_W);
        float new_checkpoint = current_screen * GAME_W;

        if (current_screen > gs->legacy_checkpoint_screen) {
            gs->legacy_checkpoint_screen = current_screen;
            gs->respawn_x = new_checkpoint;
            game_checkpoint_feedback_set(gs, CHECKPOINT_FEEDBACK_SAVED,
                                          SDL_GetTicks(), 1200);
            if (gs->debug_mode) {
                debug_log(&gs->debug, "CHECKPOINT saved at x=%.0f", gs->respawn_x);
            }
        }
    }
}
