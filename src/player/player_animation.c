/*
 * player_animation.c — Player sprite animation state machine.
 */

#include "player_animation.h"

#include "player_internal.h"  /* FRAME_W/H */

/*
 * Animation tables — indexed by AnimState (0=IDLE, 1=WALK, 2=JUMP, 3=FALL).
 *
 * ANIM_FRAME_COUNT : how many frames the animation has.
 * ANIM_FRAME_MS    : how long each frame is shown, in milliseconds.
 * ANIM_ROW         : which row of the sprite sheet the animation lives on.
 *
 * Frame layout confirmed from Player.png (192×288, 4 cols × 6 rows, 48×48 each):
 *   Row 0 — Idle : 4 frames  (cols 0–3)
 *   Row 1 — Walk : 4 frames  (cols 0–3)
 *   Row 2 — Jump : 2 frames  (cols 0–1, rising-phase poses)
 *   Row 3 — Fall : 1 frame   (col 0, descent pose)
 */
static const int ANIM_FRAME_COUNT[5] = { 4,   4,   2,   1,   2   };
static const int ANIM_FRAME_MS[5]    = { 150, 100, 150, 200, 100 };
static const int ANIM_ROW[5]         = { 0,   1,   2,   3,   4   };

/* Below this horizontal speed, show idle rather than walking. */
#define WALK_ANIM_MIN_VX 8.0f

/*
 * player_animate — Choose the correct animation state and advance its frame.
 *
 * Called once per frame from player_update, after physics are resolved.
 * Uses the player's velocity and on_ground flag to determine which animation
 * should be playing, then advances the frame timer.  On state transitions the
 * frame index is reset to 0 so animations always start from the beginning.
 */
void player_animate(Player *player, Uint32 dt_ms) {
    /* Determine the target animation from current physics state */
    AnimState target;
    if (player->on_vine) {
        target = ANIM_CLIMB;
    } else if (!player->on_ground) {
        /*
         * In the air: negative vy means moving upward (SDL y-axis is inverted),
         * positive vy means falling down under gravity.
         */
        target = (player->vy < 0.0f) ? ANIM_JUMP : ANIM_FALL;
    } else if (player->vx > WALK_ANIM_MIN_VX || player->vx < -WALK_ANIM_MIN_VX) {
        /*
         * Only show the walk animation above WALK_ANIM_MIN_VX (8 px/s).
         * During a ground skid vx tapers to 0; below the threshold it looks
         * odd to cycle the walk frames for just 1–2 frames, so we show idle.
         */
        target = ANIM_WALK;
    } else {
        target = ANIM_IDLE;
    }

    /* Reset the frame counter whenever the animation state changes */
    if (target != player->anim_state) {
        player->anim_state       = target;
        player->anim_frame_index = 0;
        player->anim_timer_ms    = 0;
    }

    /*
     * Advance the frame timer. When it exceeds the per-frame duration,
     * subtract the duration (rather than resetting to 0) so any leftover
     * time carries into the next frame — keeping animation speed accurate.
     *
     * While climbing with vy == 0 (holding still on the vine), freeze the
     * animation on the current frame so the player looks like they are
     * clinging motionless.
     */
    if (player->anim_state == ANIM_CLIMB && player->vy == 0.0f) {
        /* Freeze — do not advance the timer or frame index */
    } else {
        player->anim_timer_ms += dt_ms;
        Uint32 frame_duration = (Uint32)ANIM_FRAME_MS[player->anim_state];
        if (player->anim_timer_ms >= frame_duration) {
            player->anim_timer_ms -= frame_duration;
            player->anim_frame_index =
                (player->anim_frame_index + 1) % ANIM_FRAME_COUNT[player->anim_state];
        }
    }

    /*
     * Update the source rectangle to point at the correct cell on the sheet.
     *   frame.x = column × FRAME_W  (horizontal offset into the sheet)
     *   frame.y = row    × FRAME_H  (vertical  offset into the sheet)
     */
    player->frame.x = player->anim_frame_index * FRAME_W;
    player->frame.y = ANIM_ROW[player->anim_state] * FRAME_H;
}
