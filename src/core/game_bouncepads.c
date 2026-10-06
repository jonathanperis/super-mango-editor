/*
 * game_bouncepads.c — Combined bouncepad list and hit response helpers.
 */

#include "game_bouncepads.h"

#include "../shared/audio.h"

void game_bouncepads_lists(const GameState *gs,
                           BouncepadList lists[GAME_BOUNCEPAD_LIST_COUNT])
{
    /* Views, not copies: the pads stay in their GameState arrays. The order
     * (medium, small, high) is the one game_bouncepads_handle_hit decodes. */
    lists[0] = (BouncepadList){gs->bouncepads_medium, gs->bouncepad_medium_count};
    lists[1] = (BouncepadList){gs->bouncepads_small, gs->bouncepad_small_count};
    lists[2] = (BouncepadList){gs->bouncepads_high, gs->bouncepad_high_count};
}

void game_bouncepads_handle_hit(GameState *gs, int bounce_idx)
{
    if (bounce_idx < 0) return;

    Bouncepad *bp = NULL;
    int mc = gs->bouncepad_medium_count;
    int sc = gs->bouncepad_small_count;

    if (bounce_idx < mc) {
        bp = &gs->bouncepads_medium[bounce_idx];
    } else if (bounce_idx < mc + sc) {
        bp = &gs->bouncepads_small[bounce_idx - mc];
    } else {
        bp = &gs->bouncepads_high[bounce_idx - mc - sc];
    }

    bp->state         = BOUNCE_ACTIVE;
    bp->anim_frame    = 1;
    bp->anim_timer_ms = 0;

    sound_play(gs->audio.spring, 128);

    if (gs->debug_mode) {
        static const char *pad_names[] = { "GREEN(small)", "WOOD(medium)",
                                           "RED(high)" };
        const char *name;
        if (bounce_idx < mc) name = pad_names[1];
        else if (bounce_idx < mc + sc) name = pad_names[0];
        else name = pad_names[2];
        debug_log(&gs->debug, "BOUNCE %s", name);
    }
}

void game_bouncepads_update_animations(GameState *gs, float dt)
{
    float elapsed_ms = dt * 1000.0f;   /* keep the fraction: 16.67, not 16 */

    bouncepads_update(gs->bouncepads_medium, gs->bouncepad_medium_count,
                      elapsed_ms);
    bouncepads_update(gs->bouncepads_small, gs->bouncepad_small_count,
                      elapsed_ms);
    bouncepads_update(gs->bouncepads_high, gs->bouncepad_high_count,
                      elapsed_ms);
}
