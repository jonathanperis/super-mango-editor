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
 * player's hitbox touches: restore one heart, play the pickup sound and
 * hide the star. `name` only labels the debug log.
 *
 * A player who already has MAX_HEARTS leaves the star where it is. Taking
 * it would heal nothing and remove it until the next life, so running past
 * one at full health would throw away healing the level was designed with.
 */
static void collect_health_stars(GameState *gs, const IntRect *phit,
                                 HealthStar *stars, int count, const char *name)
{
    for (int i = 0; i < count; i++) {
        if (!stars[i].active) continue;
        if (gs->world.hearts >= MAX_HEARTS) return;   /* nothing to heal */
        IntRect sbox = health_star_get_hitbox(&stars[i]);
        if (rect_intersects(phit, &sbox)) {
            stars[i].active = 0;
            gs->world.hearts++;
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
 * Each <thing>_hitbox(world, i, &box) answers one question for item i of
 * one array: can it hurt right now, and where? When it can, it returns 1
 * and stores the box that hurts in *box. Inactive items, and flames still
 * waiting below their gap, return 0: they have no box at the moment.
 *
 * The same functions serve two readers. game_collide tests the player
 * against the box, and the debug overlay (F3) draws it. Because both ask
 * here, the outline on screen is always the box that is really tested;
 * an overlay that worked the rectangles out again for itself could drift
 * away from the collision code without anyone noticing.
 */

static int spider_hitbox(const GameWorld *world, int i, IntRect *box)
{
    *box = spider_build_hitbox(&world->spiders[i]);
    return 1;
}

static int jumping_spider_hitbox(const GameWorld *world, int i, IntRect *box)
{
    *box = jumping_spider_build_hitbox(&world->jumping_spiders[i]);
    return 1;
}

static int bird_hitbox(const GameWorld *world, int i, IntRect *box)
{
    *box = bird_get_hitbox(&world->birds[i]);
    return 1;
}

static int faster_bird_hitbox(const GameWorld *world, int i, IntRect *box)
{
    *box = faster_bird_get_hitbox(&world->faster_birds[i]);
    return 1;
}

static int fish_hitbox(const GameWorld *world, int i, IntRect *box)
{
    *box = fish_get_hitbox(&world->fish[i]);
    return 1;
}

static int faster_fish_hitbox(const GameWorld *world, int i, IntRect *box)
{
    *box = faster_fish_get_hitbox(&world->faster_fish[i]);
    return 1;
}

static int axe_trap_hitbox(const GameWorld *world, int i, IntRect *box)
{
    if (!world->axe_traps[i].active) return 0;
    *box = axe_trap_get_hitbox(&world->axe_traps[i]);
    return 1;
}

static int circular_saw_hitbox(const GameWorld *world, int i, IntRect *box)
{
    if (!world->circular_saws[i].active) return 0;
    *box = circular_saw_get_hitbox(&world->circular_saws[i]);
    return 1;
}

static int spike_block_hitbox(const GameWorld *world, int i, IntRect *box)
{
    if (!world->spike_blocks[i].active) return 0;
    *box = spike_block_get_hitbox(&world->spike_blocks[i]);
    return 1;
}

/* The whole row: what the overlay outlines. */
static int spike_row_hitbox(const GameWorld *world, int i, IntRect *box)
{
    if (!world->spike_rows[i].active) return 0;
    *box = spike_row_get_rect(&world->spike_rows[i]);
    return 1;
}

/* A spike row is tested one tile at a time, so the knockback pushes away
 * from the tile the player touched, not from the middle of a long row.
 * The tiles together cover exactly the row's box above. */
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
static int spike_platform_hitbox(const GameWorld *world, int i, IntRect *box)
{
    if (!world->spike_platforms[i].active) return 0;
    *box = spike_platform_get_rect(&world->spike_platforms[i]);
    return 1;
}

/* Blue and fire flames share BlueFlame; one waiting below its gap is harmless. */
static int flame_hitbox(const BlueFlame *flame, IntRect *box)
{
    if (!flame->active || flame->state == BLUE_FLAME_WAITING) return 0;
    *box = blue_flame_get_hitbox(flame);
    return 1;
}

static int blue_flame_hitbox(const GameWorld *world, int i, IntRect *box)
{
    return flame_hitbox(&world->blue_flames[i], box);
}

static int fire_flame_hitbox(const GameWorld *world, int i, IntRect *box)
{
    return flame_hitbox(&world->fire_flames[i], box);
}

/*
 * s_damage_sources — every kind of enemy and hazard that hurts on touch, in
 * the order game_collide tests them.
 *
 * Each row names the kind for the debug log ("HIT spider[3]"), says where
 * its count lives in GameWorld (offsetof gives the byte offset of that
 * int field), and points at its hitbox function. The order only matters
 * when two things touch the player in the same step: the first row wins
 * and decides the knockback direction.
 *
 * `touches` is for a kind whose hit needs more than "the player overlaps
 * the box": a spike row reports the single tile that was touched. NULL,
 * for every other kind, means exactly that overlap test.
 *
 * Adding an enemy or hazard that hurts on touch means one <thing>_hitbox()
 * above and one row here; the debug overlay then draws it without being
 * told (it walks this table through game_damage_source_hitbox). The other
 * places a new enemy needs are in the checklist above s_update_steps in
 * core/game_update.c.
 */
typedef struct {
    const char *name;
    size_t      count_offset;  /* offsetof(GameWorld, <things>_count) */
    int       (*hitbox)(const GameWorld *world, int i, IntRect *box);
    int       (*touches)(const GameWorld *world, int i, const IntRect *player, IntRect *hit);
} DamageSource;

#define WORLD_COUNT(field) offsetof(GameWorld, field)

static const DamageSource s_damage_sources[] = {
    /* debug name      count in GameWorld                  hitbox function         touches */
    /* ---- Enemies ------------------------------------------------------------------ */
    { "spider",         WORLD_COUNT(spider_count),          spider_hitbox,          NULL },
    { "jspider",        WORLD_COUNT(jumping_spider_count),  jumping_spider_hitbox,  NULL },
    { "bird",           WORLD_COUNT(bird_count),            bird_hitbox,            NULL },
    { "fbird",          WORLD_COUNT(faster_bird_count),     faster_bird_hitbox,     NULL },
    { "fish",           WORLD_COUNT(fish_count),            fish_hitbox,            NULL },
    { "ffish",          WORLD_COUNT(faster_fish_count),     faster_fish_hitbox,     NULL },
    /* ---- Hazards ------------------------------------------------------------------ */
    { "axe",            WORLD_COUNT(axe_trap_count),        axe_trap_hitbox,        NULL },
    { "saw",            WORLD_COUNT(circular_saw_count),    circular_saw_hitbox,    NULL },
    { "spike_block",    WORLD_COUNT(spike_block_count),     spike_block_hitbox,     NULL },
    { "spike",          WORLD_COUNT(spike_row_count),       spike_row_hitbox,       spike_row_touches },
    { "spike_platform", WORLD_COUNT(spike_platform_count),  spike_platform_hitbox,  NULL },
    { "blue_flame",     WORLD_COUNT(blue_flame_count),      blue_flame_hitbox,      NULL },
    { "fire_flame",     WORLD_COUNT(fire_flame_count),      fire_flame_hitbox,      NULL },
};

/* Read the int count field that sits count_offset bytes into the world. */
static int world_count(const GameWorld *world, size_t count_offset)
{
    return *(const int *)((const char *)world + count_offset);
}

int game_damage_source_count(void)
{
    return ARRAY_LEN(s_damage_sources);
}

const char *game_damage_source_name(int source)
{
    if (source < 0 || source >= ARRAY_LEN(s_damage_sources)) return NULL;
    return s_damage_sources[source].name;
}

int game_damage_source_items(const GameWorld *world, int source)
{
    if (!world || source < 0 || source >= ARRAY_LEN(s_damage_sources)) return 0;
    return world_count(world, s_damage_sources[source].count_offset);
}

int game_damage_source_hitbox(const GameWorld *world, int source, int item, IntRect *box)
{
    if (!box || item < 0 || item >= game_damage_source_items(world, source)) return 0;
    return s_damage_sources[source].hitbox(world, item, box);
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
            /* No box right now (inactive, waiting), or the player is not
             * in it. A kind with its own test narrows the box it reports. */
            if (!source->hitbox(&gs->world, i, &hit) || !rect_intersects(player, &hit))
                continue;
            if (source->touches && !source->touches(&gs->world, i, player, &hit)) continue;
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
        IntRect cbox = coin_get_hitbox(&gs->world.coins[i]);
        if (rect_intersects(&phit, &cbox)) {
            gs->world.coins[i].active = 0;
            game_award_score(gs, gs->world.rules.coin_score);
            /* The chime climbs with a streak of quick pickups. */
            sound_play_ex(gs->assets.audio.coin, 128, 0.0f,
                          coin_streak_pitch(&gs->screen.coin_streak, gs->world.sim_steps));
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
