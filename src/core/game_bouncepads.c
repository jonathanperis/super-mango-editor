/*
 * game_bouncepads.c — Combined bouncepad list and hit response helpers.
 */

#include "game_bouncepads.h"

#include <SDL_mixer.h>

int game_bouncepads_collect(const GameState *gs, Bouncepad *out_pads)
{
    int count = 0;

    for (int i = 0; i < gs->bouncepad_medium_count; i++) {
        out_pads[count++] = gs->bouncepads_medium[i];
    }
    for (int i = 0; i < gs->bouncepad_small_count; i++) {
        out_pads[count++] = gs->bouncepads_small[i];
    }
    for (int i = 0; i < gs->bouncepad_high_count; i++) {
        out_pads[count++] = gs->bouncepads_high[i];
    }

    return count;
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

    if (gs->audio.spring) Mix_PlayChannel(-1, gs->audio.spring, 0);

    if (gs->debug_mode) {
        static const char *pad_names[] = { "GREEN(small)", "WOOD(medium)",
                                           "RED(high)" };
        const char *name = pad_names[1];
        if (bounce_idx < mc) name = pad_names[1];
        else if (bounce_idx < mc + sc) name = pad_names[0];
        else name = pad_names[2];
        debug_log(&gs->debug, "BOUNCE %s", name);
    }
}
