/*
 * game_collision.c — Collision detection system implementation.
 *
 * Handles all player-entity collision detection including enemies,
 * hazards, and collectibles.
 *
 * How to read it:
 *   1. One small <thing>_touches() function per kind of enemy or hazard:
 *      "does item i hurt the player box now, and which box touched it?"
 *   2. s_damage_sources: a table listing those kinds in the order they are
 *      tested, and collide_damage_sources(), the one loop that walks it.
 *   3. game_collide(): the hurt timer, that loop, then the collectibles.
 */

#include <stddef.h>  /* offsetof */

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

#define ARRAY_LEN(arr) ((int)(sizeof(arr) / sizeof((arr)[0])))

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
/* Damage sources: what hurts on touch                                */
/* ------------------------------------------------------------------ */

/*
 * Each <thing>_touches(world, i, player, &hit) answers one question for
 * item i of one array: does it hurt the player box right now? When it
 * does, it returns 1 and stores in *hit the box that touched, whose
 * centre is where the knockback pushes away from. Inactive items, and
 * flames still waiting below their gap, never hurt.
 */

static int spider_touches(const GameWorld *world, int i, const IntRect *player, IntRect *hit)
{
    *hit = spider_build_hitbox(&world->spiders[i]);
    return rect_intersects(player, hit);
}

static int jumping_spider_touches(const GameWorld *world, int i, const IntRect *player, IntRect *hit)
{
    *hit = jumping_spider_build_hitbox(&world->jumping_spiders[i]);
    return rect_intersects(player, hit);
}

static int bird_touches(const GameWorld *world, int i, const IntRect *player, IntRect *hit)
{
    *hit = bird_get_hitbox(&world->birds[i]);
    return rect_intersects(player, hit);
}

static int faster_bird_touches(const GameWorld *world, int i, const IntRect *player, IntRect *hit)
{
    *hit = faster_bird_get_hitbox(&world->faster_birds[i]);
    return rect_intersects(player, hit);
}

static int fish_touches(const GameWorld *world, int i, const IntRect *player, IntRect *hit)
{
    *hit = fish_get_hitbox(&world->fish[i]);
    return rect_intersects(player, hit);
}

static int faster_fish_touches(const GameWorld *world, int i, const IntRect *player, IntRect *hit)
{
    *hit = faster_fish_get_hitbox(&world->faster_fish[i]);
    return rect_intersects(player, hit);
}

static int axe_trap_touches(const GameWorld *world, int i, const IntRect *player, IntRect *hit)
{
    if (!world->axe_traps[i].active) return 0;
    *hit = axe_trap_get_hitbox(&world->axe_traps[i]);
    return rect_intersects(player, hit);
}

static int circular_saw_touches(const GameWorld *world, int i, const IntRect *player, IntRect *hit)
{
    if (!world->circular_saws[i].active) return 0;
    *hit = circular_saw_get_hitbox(&world->circular_saws[i]);
    return rect_intersects(player, hit);
}

static int spike_block_touches(const GameWorld *world, int i, const IntRect *player, IntRect *hit)
{
    if (!world->spike_blocks[i].active) return 0;
    *hit = spike_block_get_hitbox(&world->spike_blocks[i]);
    return rect_intersects(player, hit);
}

/* A spike row is tested one tile at a time, so the knockback pushes away
 * from the tile the player touched, not from the middle of a long row. */
static int spike_row_touches(const GameWorld *world, int i, const IntRect *player, IntRect *hit)
{
    const SpikeRow *row = &world->spike_rows[i];
    if (!row->active) return 0;
    for (int t = 0; t < row->count; t++) {
        *hit = (IntRect){ (int)row->x + t * SPIKE_TILE_W, (int)row->y,
                          SPIKE_TILE_W, SPIKE_TILE_H };
        if (rect_intersects(player, hit)) return 1;
    }
    return 0;
}

/*
 * Spike platforms use spike_platform_get_rect(), which reaches 2 px above
 * the platform. A player standing on top has their bottom snapped to
 * exactly sp->y, and the strict overlap test would miss a box whose top is
 * also sp->y; the extra 2 px make top-landing damage work.
 */
static int spike_platform_touches(const GameWorld *world, int i, const IntRect *player, IntRect *hit)
{
    if (!world->spike_platforms[i].active) return 0;
    *hit = spike_platform_get_rect(&world->spike_platforms[i]);
    return rect_intersects(player, hit);
}

/* Blue and fire flames share BlueFlame; one waiting below its gap is harmless. */
static int flame_touches(const BlueFlame *flame, const IntRect *player, IntRect *hit)
{
    if (!flame->active || flame->state == BLUE_FLAME_WAITING) return 0;
    *hit = blue_flame_get_hitbox(flame);
    return rect_intersects(player, hit);
}

static int blue_flame_touches(const GameWorld *world, int i, const IntRect *player, IntRect *hit)
{
    return flame_touches(&world->blue_flames[i], player, hit);
}

static int fire_flame_touches(const GameWorld *world, int i, const IntRect *player, IntRect *hit)
{
    return flame_touches(&world->fire_flames[i], player, hit);
}

/*
 * s_damage_sources — every kind of enemy and hazard that hurts on touch, in
 * the order game_collide tests them.
 *
 * Each row names the kind for the debug log ("HIT spider[3]"), says where
 * its count lives in GameWorld (offsetof gives the byte offset of that
 * int field), and points at its touches function. The order only matters
 * when two things touch the player in the same step: the first row wins
 * and decides the knockback direction.
 *
 * Adding an enemy or hazard that hurts on touch means one <thing>_touches()
 * above and one row here.
 */
typedef struct {
    const char *name;
    size_t      count_offset;  /* offsetof(GameWorld, <things>_count) */
    int       (*touches)(const GameWorld *world, int i, const IntRect *player, IntRect *hit);
} DamageSource;

#define WORLD_COUNT(field) offsetof(GameWorld, field)

static const DamageSource s_damage_sources[] = {
    /* debug name      count in GameWorld                       touches function       */
    /* ---- Enemies ------------------------------------------------------------------ */
    { "spider",         WORLD_COUNT(spider_count),               spider_touches          },
    { "jspider",        WORLD_COUNT(jumping_spider_count),       jumping_spider_touches  },
    { "bird",           WORLD_COUNT(bird_count),                 bird_touches            },
    { "fbird",          WORLD_COUNT(faster_bird_count),          faster_bird_touches     },
    { "fish",           WORLD_COUNT(fish_count),                 fish_touches            },
    { "ffish",          WORLD_COUNT(faster_fish_count),          faster_fish_touches     },
    /* ---- Hazards ------------------------------------------------------------------ */
    { "axe",            WORLD_COUNT(axe_trap_count),             axe_trap_touches        },
    { "saw",            WORLD_COUNT(circular_saw_count),         circular_saw_touches    },
    { "spike_block",    WORLD_COUNT(spike_block_count),          spike_block_touches     },
    { "spike",          WORLD_COUNT(spike_row_count),            spike_row_touches       },
    { "spike_platform", WORLD_COUNT(spike_platform_count),       spike_platform_touches  },
    { "blue_flame",     WORLD_COUNT(blue_flame_count),           blue_flame_touches      },
    { "fire_flame",     WORLD_COUNT(fire_flame_count),           fire_flame_touches      },
};

/* Read the int count field that sits count_offset bytes into the world. */
static int world_count(const GameWorld *world, size_t count_offset)
{
    return *(const int *)((const char *)world + count_offset);
}

/*
 * collide_damage_sources — Walk s_damage_sources and apply the first hit.
 *
 * At most one hit per step: apply_damage starts the hurt timer, and the
 * player is invincible until it runs out, so the search stops at the first
 * touch. Returns 1 when that hit cost a life (or the game); the level has
 * then been reset, and the caller must not test anything else this step.
 */
static int collide_damage_sources(GameState *gs, const IntRect *player)
{
    for (int s = 0; s < ARRAY_LEN(s_damage_sources); s++) {
        const DamageSource *source = &s_damage_sources[s];
        int count = world_count(&gs->world, source->count_offset);
        for (int i = 0; i < count; i++) {
            IntRect hit;
            if (!source->touches(&gs->world, i, player, &hit)) continue;
            if (gs->screen.debug_mode) debug_log(&gs->screen.debug, "HIT %s[%d]", source->name, i);
            float sx = hit.x + hit.w * 0.5f;
            float sy = hit.y + hit.h * 0.5f;
            return damage_ends_pass(gs, sx, sy);
        }
    }
    return 0;
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

    /* ---- Enemy and hazard collisions (skipped while invincible) -- */
    if (gs->world.player.hurt_timer == 0.0f && collide_damage_sources(gs, &phit))
        return;

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
}
