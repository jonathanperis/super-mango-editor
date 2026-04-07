/*
 * entity_utils.c — Implementations of shared entity utility functions.
 *
 * See entity_utils.h for full documentation of each function.
 */

#include "entity_utils.h"

/* ------------------------------------------------------------------ */

/*
 * animate_frame_ms — advance frame-based animation by one game tick.
 *
 * We accumulate elapsed ms and subtract frame_ms each time a frame advances,
 * rather than resetting to zero.  This prevents drift: if a frame takes
 * 18 ms instead of 16 ms, the leftover 2 ms roll into the next frame's count
 * so animations stay perfectly on beat over time.
 */
int animate_frame_ms(int *frame_index, Uint32 *timer_ms,
                     float dt, Uint32 frame_ms, int frame_count)
{
    *timer_ms += (Uint32)(dt * 1000.0f);
    if (*timer_ms >= frame_ms) {
        *timer_ms -= frame_ms;
        *frame_index = (*frame_index + 1) % frame_count;
        /*
         * Report a wrap-around: the caller can use this to trigger a
         * one-shot effect (e.g. a wing-flap sound) once per cycle.
         */
        return (*frame_index == 0) ? 1 : 0;
    }
    return 0;
}

/* ------------------------------------------------------------------ */

/* ------------------------------------------------------------------ */

/*
 * sound_volume_for_distance — linear volume fade based on distance.
 *
 * Returns max_volume at dist=0, linearly interpolating to 0 at
 * dist=audible_range, and clamping to 0 beyond that.  The formula is
 * the same used by every entity module; centralising it here removes
 * the four identical copies that previously lived in jumping_spider.c,
 * bird.c, faster_bird.c, and axe_trap.c.
 */
int sound_volume_for_distance(float dist, float audible_range, int max_volume)
{
    if (dist >= audible_range) return 0;
    if (dist <= 0.0f)          return max_volume;
    /* Linear ramp: fraction goes from 0.0 (closest) to 1.0 (farthest). */
    float fraction = dist / audible_range;
    return (int)((1.0f - fraction) * (float)max_volume);
}

/* ------------------------------------------------------------------ */

/*
 * patrol_update — horizontal movement with boundary reversal.
 *
 * Right-boundary check uses entity_w so the snap point is the entity's
 * right edge reaching patrol_x1, not the left edge overshooting it.
 * This prevents the entity from visually "sliding" past its turn point
 * before reversing.
 */
void patrol_update(float *x, float *vx, float entity_w,
                   float patrol_x0, float patrol_x1,
                   float speed, float dt)
{
    /* Apply velocity × delta-time for frame-rate-independent movement. */
    *x += *vx * dt;

    if (*vx > 0.0f && *x + entity_w >= patrol_x1) {
        /* Right edge reached patrol_x1 — snap and turn left. */
        *x  = patrol_x1 - entity_w;
        *vx = -speed;
    } else if (*vx < 0.0f && *x <= patrol_x0) {
        /* Left edge reached patrol_x0 — snap and turn right. */
        *x  = patrol_x0;
        *vx =  speed;
    }
}

/* ------------------------------------------------------------------ */

/*
 * patrol_gap_reverse — reverse before walking over a sea gap.
 *
 * art_center is the world-x of the middle of the entity's visible sprite.
 * We check the centre rather than the edges so the entity turns exactly when
 * it is about to step onto air, not one frame earlier or later.
 *
 * On a match we snap the entity back to the gap edge (based on travel
 * direction) and reverse velocity.  The 'break' exits after the first gap
 * match; an entity cannot straddle two gaps simultaneously at FLOOR_GAP_W=32.
 */
void patrol_gap_reverse(float *x, float *vx,
                        float art_x_offset, float art_w, float speed,
                        const int *floor_gaps, int floor_gap_count, int floor_gap_w)
{
    float art_center = *x + art_x_offset + art_w * 0.5f;
    float gw = (float)floor_gap_w;

    for (int g = 0; g < floor_gap_count; g++) {
        float gx = (float)floor_gaps[g];
        if (art_center >= gx && art_center < gx + gw) {
            if (*vx > 0.0f) {
                /* Travelling right — snap left edge of art to gap left edge. */
                *x  = gx - art_x_offset - art_w;
                *vx = -speed;
            } else {
                /* Travelling left — snap left edge of art to gap right edge. */
                *x  = gx + gw - art_x_offset;
                *vx =  speed;
            }
            break;
        }
    }
}
