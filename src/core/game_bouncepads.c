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
    lists[0] = (BouncepadList){gs->world.bouncepads_medium, gs->world.bouncepad_medium_count};
    lists[1] = (BouncepadList){gs->world.bouncepads_small, gs->world.bouncepad_small_count};
    lists[2] = (BouncepadList){gs->world.bouncepads_high, gs->world.bouncepad_high_count};
}

void game_bouncepads_handle_hit(GameState *gs, int bounce_idx)
{
    if (bounce_idx < 0) return;

    Bouncepad *bp = NULL;
    int mc = gs->world.bouncepad_medium_count;
    int sc = gs->world.bouncepad_small_count;

    if (bounce_idx < mc) {
        bp = &gs->world.bouncepads_medium[bounce_idx];
    } else if (bounce_idx < mc + sc) {
        bp = &gs->world.bouncepads_small[bounce_idx - mc];
    } else {
        bp = &gs->world.bouncepads_high[bounce_idx - mc - sc];
    }

    bp->state         = BOUNCE_ACTIVE;
    bp->anim_frame    = 1;
    bp->anim_timer_ms = 0;

    sound_play(gs->assets.audio.spring, 128);

    if (gs->screen.debug_mode) {
        static const char *pad_names[] = { "GREEN(small)", "WOOD(medium)",
                                           "RED(high)" };
        const char *name;
        if (bounce_idx < mc) name = pad_names[1];
        else if (bounce_idx < mc + sc) name = pad_names[0];
        else name = pad_names[2];
        debug_log(&gs->screen.debug, "BOUNCE %s", name);
    }
}

void game_bouncepads_update_animations(GameState *gs, float dt)
{
    bouncepads_update(gs->world.bouncepads_medium, gs->world.bouncepad_medium_count, dt);
    bouncepads_update(gs->world.bouncepads_small, gs->world.bouncepad_small_count, dt);
    bouncepads_update(gs->world.bouncepads_high, gs->world.bouncepad_high_count, dt);
}
