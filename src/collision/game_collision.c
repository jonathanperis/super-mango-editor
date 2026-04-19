/*
 * game_collision.c — Collision detection system implementation.
 *
 * Handles all player-entity collision detection including enemies,
 * hazards, and collectibles. Uses macro-based patterns to reduce
 * repetitive boilerplate code.
 */

#include "game_collision.h"
#include "collision_damage.h"

#include "../game.h"           /* game_load_next_phase for phase transition */
#include "../levels/level.h"   /* LevelDef for next_phase check */
#include "../player/player.h"
#include "../core/debug.h"

/* Entity headers for hitbox functions */
#include "../entities/bird.h"
#include "../entities/faster_bird.h"
#include "../entities/fish.h"
#include "../entities/faster_fish.h"
#include "../hazards/axe_trap.h"
#include "../hazards/circular_saw.h"
#include "../hazards/spike.h"
#include "../hazards/spike_platform.h"
#include "../hazards/spike_block.h"
#include "../hazards/blue_flame.h"
#include "../collectibles/coin.h"
#include "../collectibles/last_star.h"
#include "../collectibles/star_yellow.h"
#include "../collectibles/star_green.h"
#include "../collectibles/star_red.h"

#include <SDL_mixer.h>  /* Mix_PlayChannel for collectible sounds */

/* ------------------------------------------------------------------ */
/* Collision helper macros                                            */
/* ------------------------------------------------------------------ */

/* Test all entities in an array against player; apply damage on hit.
 * Usage: COLLIDE_DAMAGE(gs->spiders, gs->spider_count, spider_get_hitbox, "spider")
 */
#define COLLIDE_DAMAGE(arr, count, get_hitbox_fn, name) \
    for (int i = 0; i < (count) && gs->player.hurt_timer == 0.0f; i++) { \
        SDL_Rect ehit = get_hitbox_fn(&(arr)[i]); \
        if (SDL_HasIntersection(&phit, &ehit)) { \
            if (gs->debug_mode) debug_log(&gs->debug, "HIT %s[%d]", name, i); \
            float sx = ehit.x + ehit.w * 0.5f; \
            float sy = ehit.y + ehit.h * 0.5f; \
            apply_damage(gs, 1, 1, sx, sy); \
            break; \
        } \
    }

/* Test all entities in an array with 'active' field; apply damage on hit.
 * Usage: COLLIDE_DAMAGE_ACTIVE(gs->axe_traps, gs->axe_trap_count, axe_trap_get_hitbox, "axe")
 */
#define COLLIDE_DAMAGE_ACTIVE(arr, count, get_hitbox_fn, name) \
    for (int i = 0; i < (count) && gs->player.hurt_timer == 0.0f; i++) { \
        if (!(arr)[i].active) continue; \
        SDL_Rect ehit = get_hitbox_fn(&(arr)[i]); \
        if (SDL_HasIntersection(&phit, &ehit)) { \
            if (gs->debug_mode) debug_log(&gs->debug, "HIT %s[%d]", name, i); \
            float sx = ehit.x + ehit.w * 0.5f; \
            float sy = ehit.y + ehit.h * 0.5f; \
            apply_damage(gs, 1, 1, sx, sy); \
            break; \
        } \
    }

/* ------------------------------------------------------------------ */
/* Hitbox builders                                                    */
/* ------------------------------------------------------------------ */

SDL_Rect spider_build_hitbox(const Spider *s)
{
    return (SDL_Rect){
        (int)s->x + SPIDER_ART_X,
        FLOOR_Y - SPIDER_ART_H,
        SPIDER_ART_W,
        SPIDER_ART_H
    };
}

SDL_Rect jumping_spider_build_hitbox(const JumpingSpider *js)
{
    return (SDL_Rect){
        (int)js->x + JSPIDER_ART_X,
        FLOOR_Y - JSPIDER_ART_H + (int)js->y,
        JSPIDER_ART_W,
        JSPIDER_ART_H
    };
}

/* ------------------------------------------------------------------ */
/* Collision detection                                                */
/* ------------------------------------------------------------------ */

void game_collide(GameState *gs, float dt)
{
    /* Count down invincibility timer */
    if (gs->player.hurt_timer > 0.0f) {
        gs->player.hurt_timer -= dt;
        if (gs->player.hurt_timer < 0.0f)
            gs->player.hurt_timer = 0.0f;
        return;  /* No collisions while invincible */
    }

    SDL_Rect phit = player_get_hitbox(&gs->player);

    /* ---- Enemy collisions ---------------------------------------- */
    for (int i = 0; i < gs->spider_count && gs->player.hurt_timer == 0.0f; i++) {
        SDL_Rect shit = spider_build_hitbox(&gs->spiders[i]);
        if (SDL_HasIntersection(&phit, &shit)) {
            if (gs->debug_mode) debug_log(&gs->debug, "HIT spider[%d]", i);
            float sx = shit.x + shit.w * 0.5f;
            float sy = shit.y + shit.h * 0.5f;
            apply_damage(gs, 1, 1, sx, sy);
        }
    }

    for (int i = 0; i < gs->jumping_spider_count && gs->player.hurt_timer == 0.0f; i++) {
        SDL_Rect jhit = jumping_spider_build_hitbox(&gs->jumping_spiders[i]);
        if (SDL_HasIntersection(&phit, &jhit)) {
            if (gs->debug_mode) debug_log(&gs->debug, "HIT jspider[%d]", i);
            float sx = jhit.x + jhit.w * 0.5f;
            float sy = jhit.y + jhit.h * 0.5f;
            apply_damage(gs, 1, 1, sx, sy);
        }
    }

    COLLIDE_DAMAGE(gs->birds, gs->bird_count, bird_get_hitbox, "bird");
    COLLIDE_DAMAGE(gs->faster_birds, gs->faster_bird_count, faster_bird_get_hitbox, "fbird");
    COLLIDE_DAMAGE(gs->fish, gs->fish_count, fish_get_hitbox, "fish");
    COLLIDE_DAMAGE(gs->faster_fish, gs->faster_fish_count, faster_fish_get_hitbox, "ffish");

    /* ---- Hazard collisions --------------------------------------- */
    COLLIDE_DAMAGE_ACTIVE(gs->axe_traps, gs->axe_trap_count, axe_trap_get_hitbox, "axe");
    COLLIDE_DAMAGE_ACTIVE(gs->circular_saws, gs->circular_saw_count, circular_saw_get_hitbox, "saw");
    COLLIDE_DAMAGE_ACTIVE(gs->spike_blocks, gs->spike_block_count, spike_block_get_hitbox, "spike_block");

    /* Ground spikes — nested loop for tiles */
    if (gs->player.hurt_timer == 0.0f) {
        for (int i = 0; i < gs->spike_row_count; i++) {
            if (!gs->spike_rows[i].active) continue;
            for (int t = 0; t < gs->spike_rows[i].count; t++) {
                int tx = (int)gs->spike_rows[i].x + t * SPIKE_TILE_W;
                SDL_Rect stile = { tx, (int)gs->spike_rows[i].y,
                                   SPIKE_TILE_W, SPIKE_TILE_H };
                if (SDL_HasIntersection(&phit, &stile)) {
                    if (gs->debug_mode) debug_log(&gs->debug, "HIT spike[%d]", i);
                    float sx = stile.x + stile.w * 0.5f;
                    float sy = stile.y + stile.h * 0.5f;
                    apply_damage(gs, 1, 1, sx, sy);
                    goto next_spike_row;
                }
            }
        next_spike_row:;
        }
    }

    /* Spike platforms — use spike_platform_get_rect() for the extended hitbox.
     *
     * The inline hitbox (y = sp->y, h = SPIKE_PLAT_SRC_H) placed the top edge
     * exactly at sp->y.  When the player stands on top, the physics engine snaps
     * their bottom to sp->y as well, so SDL_HasIntersection's strict less-than
     * test evaluates phit.bottom > sphit.top as sp->y > sp->y — false — and no
     * damage fires.  spike_platform_get_rect() extends the hitbox 2 px upward
     * (y = sp->y - 2) so the standing player's hitbox always overlaps, making
     * top-landing damage work correctly.
     */
    if (gs->player.hurt_timer == 0.0f) {
        for (int i = 0; i < gs->spike_platform_count; i++) {
            if (!gs->spike_platforms[i].active) continue;
            SDL_Rect sphit = spike_platform_get_rect(&gs->spike_platforms[i]);
            if (SDL_HasIntersection(&phit, &sphit)) {
                if (gs->debug_mode) debug_log(&gs->debug, "HIT spike_platform[%d]", i);
                float sx = sphit.x + sphit.w * 0.5f;
                float sy = sphit.y + sphit.h * 0.5f;
                apply_damage(gs, 1, 1, sx, sy);
                break;
            }
        }
    }

    /* Blue flames — skip if in WAITING state */
    if (gs->player.hurt_timer == 0.0f) {
        for (int i = 0; i < gs->blue_flame_count; i++) {
            if (!gs->blue_flames[i].active) continue;
            if (gs->blue_flames[i].state == BLUE_FLAME_WAITING) continue;
            SDL_Rect bfhit = blue_flame_get_hitbox(&gs->blue_flames[i]);
            if (SDL_HasIntersection(&phit, &bfhit)) {
                if (gs->debug_mode) debug_log(&gs->debug, "HIT blue_flame[%d]", i);
                float sx = bfhit.x + bfhit.w * 0.5f;
                float sy = bfhit.y + bfhit.h * 0.5f;
                apply_damage(gs, 1, 1, sx, sy);
                break;
            }
        }
    }

    /* Fire flames — same logic as blue flames */
    if (gs->player.hurt_timer == 0.0f) {
        for (int i = 0; i < gs->fire_flame_count; i++) {
            if (!gs->fire_flames[i].active) continue;
            if (gs->fire_flames[i].state == BLUE_FLAME_WAITING) continue;
            SDL_Rect ffhit = blue_flame_get_hitbox(&gs->fire_flames[i]);
            if (SDL_HasIntersection(&phit, &ffhit)) {
                if (gs->debug_mode) debug_log(&gs->debug, "HIT fire_flame[%d]", i);
                float sx = ffhit.x + ffhit.w * 0.5f;
                float sy = ffhit.y + ffhit.h * 0.5f;
                apply_damage(gs, 1, 1, sx, sy);
                break;
            }
        }
    }

    /* ---- Collectible collisions ---------------------------------- */
    /* Coins — add score, possible bonus life */
    for (int i = 0; i < gs->coin_count; i++) {
        if (!gs->coins[i].active) continue;
        SDL_Rect cbox = {
            (int)gs->coins[i].x, (int)gs->coins[i].y,
            COIN_DISPLAY_W, COIN_DISPLAY_H
        };
        if (SDL_HasIntersection(&phit, &cbox)) {
            gs->coins[i].active = 0;
            gs->score += gs->coin_score;
            if (gs->snd_coin) Mix_PlayChannel(-1, gs->snd_coin, 0);
            if (gs->score >= gs->score_life_next) {
                gs->lives++;
                gs->score_life_next += gs->score_per_life;
            }
            if (gs->debug_mode) debug_log(&gs->debug, "COIN[%d] collected", i);
        }
    }

    /* Stars — restore health, same pattern for all colors */
    for (int i = 0; i < gs->star_yellow_count; i++) {
        if (!gs->star_yellows[i].active) continue;
        SDL_Rect sbox = {
            (int)gs->star_yellows[i].x, (int)gs->star_yellows[i].y,
            STAR_YELLOW_DISPLAY_W, STAR_YELLOW_DISPLAY_H
        };
        if (SDL_HasIntersection(&phit, &sbox)) {
            gs->star_yellows[i].active = 0;
            if (gs->hearts < MAX_HEARTS) gs->hearts++;
            if (gs->snd_coin) Mix_PlayChannel(-1, gs->snd_coin, 0);
            if (gs->debug_mode) debug_log(&gs->debug, "STAR_YELLOW[%d] collected", i);
        }
    }

    for (int i = 0; i < gs->star_green_count; i++) {
        if (!gs->star_greens[i].active) continue;
        SDL_Rect sbox = {
            (int)gs->star_greens[i].x, (int)gs->star_greens[i].y,
            STAR_GREEN_DISPLAY_W, STAR_GREEN_DISPLAY_H
        };
        if (SDL_HasIntersection(&phit, &sbox)) {
            gs->star_greens[i].active = 0;
            if (gs->hearts < MAX_HEARTS) gs->hearts++;
            if (gs->snd_coin) Mix_PlayChannel(-1, gs->snd_coin, 0);
            if (gs->debug_mode) debug_log(&gs->debug, "STAR_GREEN[%d] collected", i);
        }
    }

    for (int i = 0; i < gs->star_red_count; i++) {
        if (!gs->star_reds[i].active) continue;
        SDL_Rect sbox = {
            (int)gs->star_reds[i].x, (int)gs->star_reds[i].y,
            STAR_RED_DISPLAY_W, STAR_RED_DISPLAY_H
        };
        if (SDL_HasIntersection(&phit, &sbox)) {
            gs->star_reds[i].active = 0;
            if (gs->hearts < MAX_HEARTS) gs->hearts++;
            if (gs->snd_coin) Mix_PlayChannel(-1, gs->snd_coin, 0);
            if (gs->debug_mode) debug_log(&gs->debug, "STAR_RED[%d] collected", i);
        }
    }

    /* Last star — triggers phase transition or level completion */
    if (gs->last_star.active) {
        SDL_Rect lsbox = last_star_get_hitbox(&gs->last_star);
        if (SDL_HasIntersection(&phit, &lsbox)) {
            gs->last_star.active = 0;
            gs->last_star.collected = 1;
            if (gs->snd_coin) Mix_PlayChannel(-1, gs->snd_coin, 0);
            if (gs->debug_mode) debug_log(&gs->debug, "LAST STAR collected");
            
            /* Check if there's a next phase to load */
            const LevelDef *def = (const LevelDef *)gs->current_level;
            if (def && def->next_phase[0] != '\0') {
                /* Attempt phase transition; fall back to level_complete on failure */
                if (game_load_next_phase(gs) != 0) {
                    gs->level_complete = 1;
                }
            } else {
                gs->level_complete = 1;
            }
        }
    }

#undef COLLIDE_DAMAGE
#undef COLLIDE_DAMAGE_ACTIVE
}
