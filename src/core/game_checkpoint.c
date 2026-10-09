/*
 * game_checkpoint.c — Resolve authored or legacy respawn checkpoints.
 */

#include "game_checkpoint.h"
#include "../shared/platform.h"  /* clock_millis */

#include "../levels/level.h"
#include "../hazards/spike.h"          /* SpikeRow, SPIKE_TILE_W */
#include "../hazards/spike_platform.h" /* SpikePlatform */
#include "../hazards/blue_flame.h"     /* BlueFlame (blue and fire variants) */

void game_checkpoint_feedback_set(GameState *gs, CheckpointFeedbackKind kind,
                                  uint32_t now, uint32_t duration)
{
    if (!gs) return;
    gs->checkpoint_feedback_kind = kind;
    gs->checkpoint_feedback_until = kind == CHECKPOINT_FEEDBACK_NONE
        ? 0 : now + duration;
}

void game_checkpoint_feedback_clear_expired(GameState *gs, uint32_t now)
{
    if (!gs || gs->checkpoint_feedback_kind == CHECKPOINT_FEEDBACK_NONE) return;
    if ((int32_t)(now - gs->checkpoint_feedback_until) >= 0) {
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
        /* checkpoint_index and respawn_x/y are always written together
         * (below, at level load and on retry), so a valid index already
         * names the current respawn. Only its range needs checking. */
        int best_index = gs->checkpoint_index;
        if (best_index < 0 || best_index >= def->checkpoint_count) {
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

        /*
         * best_index is -1 when the player has not reached any placement yet
         * (or the remembered index is out of range).
         * Keep the current respawn; checkpoints[-1] would read before the
         * start of the array.
         */
        if (best_index < 0) return;

        if (best_index != gs->checkpoint_index) {
            gs->checkpoint_index = best_index;
            gs->respawn_x = def->checkpoints[best_index].x;
            gs->respawn_y = def->checkpoints[best_index].y;
            game_checkpoint_feedback_set(gs, CHECKPOINT_FEEDBACK_SAVED,
                                          (uint32_t)clock_millis(), 1200);
            if (gs->debug_mode) {
                debug_log(&gs->debug, "CHECKPOINT saved at x=%.0f y=%.0f",
                          gs->respawn_x, gs->respawn_y);
            }
        }
        return;
    }
}

/*
 * LEGACY_CHECKPOINT_STEP — spacing in logical pixels between candidate
 * respawn columns tried by legacy_safe_respawn_x(). Eight pixels is half a
 * spike tile, fine enough to find the gap between two hazards without
 * testing every single pixel.
 */
#define LEGACY_CHECKPOINT_STEP 8.0f

/* Do [a0, a1) and [b0, b1) share any x? Touching edges do not overlap. */
static int spans_overlap(float a0, float a1, float b0, float b1)
{
    return a0 < b1 && b0 < a1;
}

/*
 * legacy_respawn_column_is_safe — Can the player respawn with its left
 * edge at x?
 *
 * player_reset centres the sprite in a TILE_SIZE-wide column starting at
 * spawn_x and lets it fall onto the ground floor, so the whole column must
 * stand on solid floor (no floor gap) and must not touch a static hazard:
 * ground spike rows, spike platforms (the player could land on one while
 * falling) or the blue/fire flames that erupt from the floor.
 *
 * Moving dangers (saws, axes, spike blocks, enemies) are deliberately not
 * checked: where they are depends on the moment of respawn, not on the
 * level layout. Levels that need a respawn clear of them should author
 * [[checkpoints]] instead.
 */
static int legacy_respawn_column_is_safe(const GameState *gs, float x)
{
    float x1 = x + (float)TILE_SIZE;

    for (int i = 0; i < gs->floor_gap_count; i++) {
        float gap_x = (float)gs->floor_gaps[i];
        if (spans_overlap(x, x1, gap_x, gap_x + (float)FLOOR_GAP_W)) return 0;
    }
    for (int i = 0; i < gs->spike_row_count; i++) {
        const SpikeRow *row = &gs->spike_rows[i];
        if (row->active && spans_overlap(x, x1, row->x,
                row->x + (float)(row->count * SPIKE_TILE_W))) return 0;
    }
    for (int i = 0; i < gs->spike_platform_count; i++) {
        const SpikePlatform *sp = &gs->spike_platforms[i];
        if (sp->active && spans_overlap(x, x1, sp->x, sp->x + (float)sp->w)) return 0;
    }
    for (int i = 0; i < gs->blue_flame_count; i++) {
        const BlueFlame *flame = &gs->blue_flames[i];
        if (flame->active && spans_overlap(x, x1, flame->x, flame->x + (float)flame->w)) return 0;
    }
    for (int i = 0; i < gs->fire_flame_count; i++) {
        const BlueFlame *flame = &gs->fire_flames[i];
        if (flame->active && spans_overlap(x, x1, flame->x, flame->x + (float)flame->w)) return 0;
    }
    return 1;
}

/*
 * legacy_safe_respawn_x — Pick the respawn x for a newly entered screen.
 *
 * The screen edge itself may sit over a gap or a spike row, and respawning
 * there would cost another life at once. Walk left from the edge, over
 * ground the player has already crossed, and return the first safe column.
 * Candidates stop before the previous respawn: if none is safe, return -1
 * and the caller keeps the previous checkpoint.
 */
static float legacy_safe_respawn_x(const GameState *gs, float boundary_x)
{
    for (float x = boundary_x; x > gs->respawn_x && x >= 0.0f;
         x -= LEGACY_CHECKPOINT_STEP) {
        if (legacy_respawn_column_is_safe(gs, x)) return x;
    }
    return -1.0f;
}

void game_checkpoint_update(GameState *gs)
{
    const LevelDef *def;

    if (!gs) return;
    def = (const LevelDef *)gs->runtime.current_level;

    /* Levels with authored placements use only those, and game_update_active
     * already sampled them this step (game_checkpoint_update_authored, before
     * floor-gap damage), so there is nothing left to do here. */
    if (def && def->checkpoint_count > 0) {
        return;
    }

    /* Legacy levels save automatically at each newly entered screen. */
    {
        int current_screen = (int)(gs->player.x / GAME_W);

        if (current_screen > gs->legacy_checkpoint_screen) {
            /* Record the screen even when no safe column exists, so the
             * search runs once per screen rather than every frame. */
            gs->legacy_checkpoint_screen = current_screen;
            float new_checkpoint = legacy_safe_respawn_x(gs, (float)(current_screen * GAME_W));
            if (new_checkpoint < 0.0f) return;
            gs->respawn_x = new_checkpoint;
            game_checkpoint_feedback_set(gs, CHECKPOINT_FEEDBACK_SAVED,
                                          (uint32_t)clock_millis(), 1200);
            if (gs->debug_mode) {
                debug_log(&gs->debug, "CHECKPOINT saved at x=%.0f", gs->respawn_x);
            }
        }
    }
}
