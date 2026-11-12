/*
 * game_bouncepads.c — Combined bouncepad list and hit response helpers.
 */

#include "game_bouncepads.h"

#include "../shared/audio.h"

void game_bouncepads_lists(const GameState *gs,
                           BouncepadList lists[GAME_BOUNCEPAD_LIST_COUNT])
{
    /* Views, not copies: the pads stay in their GameState arrays. */
    lists[GAME_BOUNCEPAD_LIST_MEDIUM] =
        (BouncepadList){gs->world.bouncepads_medium, gs->world.bouncepad_medium_count};
    lists[GAME_BOUNCEPAD_LIST_SMALL] =
        (BouncepadList){gs->world.bouncepads_small, gs->world.bouncepad_small_count};
    lists[GAME_BOUNCEPAD_LIST_HIGH] =
        (BouncepadList){gs->world.bouncepads_high, gs->world.bouncepad_high_count};
}

void game_bouncepads_handle_hit(GameState *gs, int list, int index)
{
    /* The same three arrays, writable this time, with their debug names. */
    static const char *const pad_names[GAME_BOUNCEPAD_LIST_COUNT] = {
        [GAME_BOUNCEPAD_LIST_MEDIUM] = "WOOD(medium)",
        [GAME_BOUNCEPAD_LIST_SMALL]  = "GREEN(small)",
        [GAME_BOUNCEPAD_LIST_HIGH]   = "RED(high)",
    };
    Bouncepad *pads;
    int count;

    switch (list) {
    case GAME_BOUNCEPAD_LIST_MEDIUM:
        pads = gs->world.bouncepads_medium; count = gs->world.bouncepad_medium_count; break;
    case GAME_BOUNCEPAD_LIST_SMALL:
        pads = gs->world.bouncepads_small;  count = gs->world.bouncepad_small_count;  break;
    case GAME_BOUNCEPAD_LIST_HIGH:
        pads = gs->world.bouncepads_high;   count = gs->world.bouncepad_high_count;   break;
    default:
        return;   /* no pad was hit */
    }
    if (index < 0 || index >= count) return;

    Bouncepad *bp = &pads[index];
    bp->state         = BOUNCE_ACTIVE;
    bp->anim_frame    = 1;
    bp->anim_timer_ms = 0;

    sound_play(gs->assets.audio.spring, 128);

    if (gs->screen.debug_mode) debug_log(&gs->screen.debug, "BOUNCE %s", pad_names[list]);
}
