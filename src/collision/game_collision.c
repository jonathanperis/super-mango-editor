/*
 * game_collision.c — Collision detection system implementation.
 *
 * Handles all player-entity collision detection including enemies,
 * hazards, and collectibles. Uses macro-based patterns to reduce
 * repetitive boilerplate code.
 */

#include "game_collision.h"
#include "collision_damage.h"

#include "../game.h"           /* game_complete_level for summary flow */
#include "../player/player.h"
#include "../core/debug.h"
#include "../core/game_score.h"

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
#include "../collectibles/health_star.h"

#include "../shared/audio.h"

/* A life loss replaces entities and the player position. End this pass so no
 * remaining collision uses the hitbox sampled before that replacement. */
static int damage_ends_pass(GameState *gs, float source_x, float source_y)
{
    int lives = gs->world.lives;
    apply_damage(gs, 1, 1, source_x, source_y);
    return gs->screen.game_over || gs->world.lives != lives;
}

/*
 * collect_health_stars — Pick up every active star of one colour that the
 * player's hitbox touches: restore one heart (capped at MAX_HEARTS), play the
 * pickup sound and hide the star. `name` only labels the debug log.
 */
static void collect_health_stars(GameState *gs, const IntRect *phit,
                                 HealthStar *stars, int count, const char *name)
{
    for (int i = 0; i < count; i++) {
        if (!stars[i].active) continue;
        IntRect sbox = health_star_get_hitbox(&stars[i]);
        if (rect_intersects(phit, &sbox)) {
            stars[i].active = 0;
            if (gs->world.hearts < MAX_HEARTS) gs->world.hearts++;
            sound_play(gs->assets.audio.coin, 128);
            if (gs->screen.debug_mode) debug_log(&gs->screen.debug, "%s[%d] collected", name, i);
        }
    }
}

/* ------------------------------------------------------------------ */
/* Collision helper macros                                            */
/* ------------------------------------------------------------------ */

/* Test all entities in an array against player; apply damage on hit.
 * Usage: COLLIDE_DAMAGE(gs->world.spiders, gs->world.spider_count, spider_build_hitbox, "spider")
 */
#define COLLIDE_DAMAGE(arr, count, get_hitbox_fn, name) \
    for (int i = 0; i < (count) && gs->world.player.hurt_timer == 0.0f; i++) { \
        IntRect ehit = get_hitbox_fn(&(arr)[i]); \
        if (rect_intersects(&phit, &ehit)) { \
            if (gs->screen.debug_mode) debug_log(&gs->screen.debug, "HIT %s[%d]", name, i); \
            float sx = ehit.x + ehit.w * 0.5f; \
            float sy = ehit.y + ehit.h * 0.5f; \
            if (damage_ends_pass(gs, sx, sy)) return; \
            break; \
        } \
    }

/* Test all entities in an array with 'active' field; apply damage on hit.
 * Usage: COLLIDE_DAMAGE_ACTIVE(gs->world.axe_traps, gs->world.axe_trap_count, axe_trap_get_hitbox, "axe")
 */
#define COLLIDE_DAMAGE_ACTIVE(arr, count, get_hitbox_fn, name) \
    for (int i = 0; i < (count) && gs->world.player.hurt_timer == 0.0f; i++) { \
        if (!(arr)[i].active) continue; \
        IntRect ehit = get_hitbox_fn(&(arr)[i]); \
        if (rect_intersects(&phit, &ehit)) { \
            if (gs->screen.debug_mode) debug_log(&gs->screen.debug, "HIT %s[%d]", name, i); \
            float sx = ehit.x + ehit.w * 0.5f; \
            float sy = ehit.y + ehit.h * 0.5f; \
            if (damage_ends_pass(gs, sx, sy)) return; \
            break; \
        } \
    }

/* ------------------------------------------------------------------ */
/* Hitbox builders                                                    */
/* ------------------------------------------------------------------ */

IntRect spider_build_hitbox(const Spider *s)
{
    return (IntRect){
        (int)s->x + SPIDER_ART_X,
        FLOOR_Y - SPIDER_ART_H,
        SPIDER_ART_W,
        SPIDER_ART_H
    };
}

IntRect jumping_spider_build_hitbox(const JumpingSpider *js)
{
    return (IntRect){
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
    if (gs->screen.game_over || gs->screen.completion.complete) return;
    /* Count down invincibility timer */
    if (gs->world.player.hurt_timer > 0.0f) {
        gs->world.player.hurt_timer -= dt;
        if (gs->world.player.hurt_timer < 0.0f)
            gs->world.player.hurt_timer = 0.0f;
    }

    IntRect phit = player_get_hitbox(&gs->world.player);

    /* ---- Enemy collisions ---------------------------------------- */
    COLLIDE_DAMAGE(gs->world.spiders, gs->world.spider_count, spider_build_hitbox, "spider");
    COLLIDE_DAMAGE(gs->world.jumping_spiders, gs->world.jumping_spider_count,
                   jumping_spider_build_hitbox, "jspider");
    COLLIDE_DAMAGE(gs->world.birds, gs->world.bird_count, bird_get_hitbox, "bird");
    COLLIDE_DAMAGE(gs->world.faster_birds, gs->world.faster_bird_count, faster_bird_get_hitbox, "fbird");
    COLLIDE_DAMAGE(gs->world.fish, gs->world.fish_count, fish_get_hitbox, "fish");
    COLLIDE_DAMAGE(gs->world.faster_fish, gs->world.faster_fish_count, faster_fish_get_hitbox, "ffish");

    /* ---- Hazard collisions --------------------------------------- */
    COLLIDE_DAMAGE_ACTIVE(gs->world.axe_traps, gs->world.axe_trap_count, axe_trap_get_hitbox, "axe");
    COLLIDE_DAMAGE_ACTIVE(gs->world.circular_saws, gs->world.circular_saw_count, circular_saw_get_hitbox, "saw");
    COLLIDE_DAMAGE_ACTIVE(gs->world.spike_blocks, gs->world.spike_block_count, spike_block_get_hitbox, "spike_block");

    /* Ground spikes — nested loop for tiles */
    if (gs->world.player.hurt_timer == 0.0f) {
        for (int i = 0; i < gs->world.spike_row_count && gs->world.player.hurt_timer == 0.0f; i++) {
            if (!gs->world.spike_rows[i].active) continue;
            for (int t = 0; t < gs->world.spike_rows[i].count; t++) {
                int tx = (int)gs->world.spike_rows[i].x + t * SPIKE_TILE_W;
                IntRect stile = { tx, (int)gs->world.spike_rows[i].y,
                                   SPIKE_TILE_W, SPIKE_TILE_H };
                if (rect_intersects(&phit, &stile)) {
                    if (gs->screen.debug_mode) debug_log(&gs->screen.debug, "HIT spike[%d]", i);
                    float sx = stile.x + stile.w * 0.5f;
                    float sy = stile.y + stile.h * 0.5f;
                    if (damage_ends_pass(gs, sx, sy)) return;
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
     * their bottom to sp->y as well, so the intersection's strict less-than
     * test evaluates phit.bottom > sphit.top as sp->y > sp->y — false — and no
     * damage fires.  spike_platform_get_rect() extends the hitbox 2 px upward
     * (y = sp->y - 2) so the standing player's hitbox always overlaps, making
     * top-landing damage work correctly.
     */
    if (gs->world.player.hurt_timer == 0.0f) {
        for (int i = 0; i < gs->world.spike_platform_count; i++) {
            if (!gs->world.spike_platforms[i].active) continue;
            IntRect sphit = spike_platform_get_rect(&gs->world.spike_platforms[i]);
            if (rect_intersects(&phit, &sphit)) {
                if (gs->screen.debug_mode) debug_log(&gs->screen.debug, "HIT spike_platform[%d]", i);
                float sx = sphit.x + sphit.w * 0.5f;
                float sy = sphit.y + sphit.h * 0.5f;
                if (damage_ends_pass(gs, sx, sy)) return;
                break;
            }
        }
    }

    /* Blue flames — skip if in WAITING state */
    if (gs->world.player.hurt_timer == 0.0f) {
        for (int i = 0; i < gs->world.blue_flame_count; i++) {
            if (!gs->world.blue_flames[i].active) continue;
            if (gs->world.blue_flames[i].state == BLUE_FLAME_WAITING) continue;
            IntRect bfhit = blue_flame_get_hitbox(&gs->world.blue_flames[i]);
            if (rect_intersects(&phit, &bfhit)) {
                if (gs->screen.debug_mode) debug_log(&gs->screen.debug, "HIT blue_flame[%d]", i);
                float sx = bfhit.x + bfhit.w * 0.5f;
                float sy = bfhit.y + bfhit.h * 0.5f;
                if (damage_ends_pass(gs, sx, sy)) return;
                break;
            }
        }
    }

    /* Fire flames — same logic as blue flames */
    if (gs->world.player.hurt_timer == 0.0f) {
        for (int i = 0; i < gs->world.fire_flame_count; i++) {
            if (!gs->world.fire_flames[i].active) continue;
            if (gs->world.fire_flames[i].state == BLUE_FLAME_WAITING) continue;
            IntRect ffhit = blue_flame_get_hitbox(&gs->world.fire_flames[i]);
            if (rect_intersects(&phit, &ffhit)) {
                if (gs->screen.debug_mode) debug_log(&gs->screen.debug, "HIT fire_flame[%d]", i);
                float sx = ffhit.x + ffhit.w * 0.5f;
                float sy = ffhit.y + ffhit.h * 0.5f;
                if (damage_ends_pass(gs, sx, sy)) return;
                break;
            }
        }
    }

    /* ---- Collectible collisions ---------------------------------- */
    /* Coins — add score, possible bonus life */
    for (int i = 0; i < gs->world.coin_count; i++) {
        if (!gs->world.coins[i].active) continue;
        IntRect cbox = {
            (int)gs->world.coins[i].x, (int)gs->world.coins[i].y,
            COIN_DISPLAY_W, COIN_DISPLAY_H
        };
        if (rect_intersects(&phit, &cbox)) {
            gs->world.coins[i].active = 0;
            game_award_score(gs, gs->world.rules.coin_score);
            sound_play(gs->assets.audio.coin, 128);
            if (gs->screen.debug_mode) debug_log(&gs->screen.debug, "COIN[%d] collected", i);
        }
    }

    /* Stars — every colour restores one heart; one loop serves all three. */
    collect_health_stars(gs, &phit, gs->world.star_yellows, gs->world.star_yellow_count, "STAR_YELLOW");
    collect_health_stars(gs, &phit, gs->world.star_greens, gs->world.star_green_count, "STAR_GREEN");
    collect_health_stars(gs, &phit, gs->world.star_reds, gs->world.star_red_count, "STAR_RED");

    /* Last star — triggers phase transition or level completion */
    if (gs->world.last_star.active) {
        IntRect lsbox = last_star_get_hitbox(&gs->world.last_star);
        if (rect_intersects(&phit, &lsbox)) {
            gs->world.last_star.active = 0;
            gs->world.last_star.collected = 1;
            sound_play(gs->assets.audio.coin, 128);
            if (gs->screen.debug_mode) debug_log(&gs->screen.debug, "LAST STAR collected");
            
            game_complete_level(gs);
        }
    }

#undef COLLIDE_DAMAGE
#undef COLLIDE_DAMAGE_ACTIVE
}
