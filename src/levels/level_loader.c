/*
 * level_loader.c — Translate LevelDef placement data into the live GameWorld.
 *
 * This is the single file that knows both (a) the layout of every entity struct
 * and (b) the LevelDef placement format.  Entity modules know HOW things behave;
 * level files know WHERE things go; this file is the bridge between the two.
 *
 * How to read it:
 *   1. One small load_<things>() function per placement array, grouped like
 *      LevelDef (geometry, collectibles, enemies, hazards, surfaces,
 *      climbables). Each copies placements into gs->world and sets the count.
 *   2. s_level_loaders, near the end: a table listing every load function in
 *      the order it runs, and whether a lost life runs it again.
 *   3. level_apply and level_reset, which walk that table.
 *
 * level_load  : called at game_init, on a phase transition and when an
 *               experiment recording restarts the level.
 * level_reset : called on player death via reset_current_level; runs only
 *               the table rows marked LOAD_EVERY_LIFE, so static geometry (sea
 *               gaps, rails, platforms, decorations) is kept and coins
 *               collected during the current attempt stay collected.
 */

#include "../core/game_random.h"
#include "../game.h"  /* GameState: this file reads its fields */
#include "../shared/platform.h"  /* str_copy */
#include <stdio.h>     /* fprintf, stderr */
#include <string.h>    /* strcmp: platform tile paths */

#include "level_loader.h"
#include "level_physics.h"

/* Include every entity header so we can fill their structs directly. */
#include "../player/player.h"
#include "../surfaces/platform.h"
#include "../surfaces/rail.h"
#include "../collectibles/coin.h"
#include "../collectibles/health_star.h"
#include "../collectibles/last_star.h"
#include "../entities/spider.h"
#include "../entities/jumping_spider.h"
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
#include "../surfaces/float_platform.h"
#include "../surfaces/bridge.h"
#include "../surfaces/bouncepad.h"
#include "../surfaces/vine.h"
#include "../surfaces/ladder.h"
#include "../surfaces/rope.h"
#include "../effects/water.h"   /* WATER_ART_H — used for fish water_y computation */

#define ARRAY_LEN(arr) ((int)(sizeof(arr) / sizeof((arr)[0])))

/* ------------------------------------------------------------------ */
/* Internal helpers                                                    */
/* ------------------------------------------------------------------ */

/*
 * rand_range — return a pseudo-random float in [lo, hi].
 * Used to stagger fish jump timers so they don't all leap simultaneously.
 */
static float rand_range(float lo, float hi) {
    return lo + game_random_unit() * (hi - lo);
}

/* ------------------------------------------------------------------ */
/* Static geometry                                                     */
/* ------------------------------------------------------------------ */

/*
 * load_floor_gaps — Copy floor gap x-positions from the level definition.
 *
 * Floor gaps are holes in the ground floor.  Their positions feed into both
 * spider gap-avoidance logic and the blue flame eruption system.
 */
static void load_floor_gaps(GameWorld *world, const LevelDef *def)
{
    for (int i = 0; i < def->floor_gap_count; i++)
        world->floor_gaps[i] = def->floor_gaps[i];
    world->floor_gap_count = def->floor_gap_count;
}

/*
 * load_rails — Build rail path data from the placement array.
 *
 * Rails must be built before any entity that rides them (spike_blocks,
 * float_platforms in RAIL mode) so their Rail* pointers are valid.
 */
static void load_rails(GameWorld *world, const LevelDef *def)
{
    rail_init_from_placements(world->rails, &world->rail_count,
                              def->rails, def->rail_count);
}

#ifdef MANGO_TESTING
/* Test builds count image loads, so a test can prove each path loads once. */
static int s_test_tile_loads;
int level_loader_test_tile_loads(void) { return s_test_tile_loads; }
#endif

static PlatformTile *platform_tile_find(PlatformTileCache *cache, const char *path)
{
    for (int i = 0; i < cache->count; i++)
        if (strcmp(cache->tiles[i].path, path) == 0) return &cache->tiles[i];
    return NULL;
}

/*
 * platform_tiles_keep_used — Unload the tiles the new level does not name.
 *
 * Step 1 marks every cached path that def still uses; step 2 unloads the
 * rest and closes the gaps. A Replay or an F8 restart of the same level
 * therefore keeps every texture, and the cache never holds more than one
 * entry per platform, so MAX_PLATFORMS entries are always enough.
 */
static void platform_tiles_keep_used(PlatformTileCache *cache, const LevelDef *def)
{
    for (int i = 0; i < cache->count; i++) cache->tiles[i].in_use = 0;
    for (int i = 0; i < def->platform_count; i++) {
        PlatformTile *tile = platform_tile_find(cache, def->platforms[i].tile_path);
        if (tile) tile->in_use = 1;
    }
    int kept = 0;
    for (int i = 0; i < cache->count; i++) {
        if (cache->tiles[i].in_use) cache->tiles[kept++] = cache->tiles[i];
        else texture_unload(cache->tiles[i].texture);
    }
    cache->count = kept;
}

/* Return the shared texture for path, loading it on first use. */
static Texture2D *platform_tile_acquire(PlatformTileCache *cache, const char *path)
{
    PlatformTile *tile = platform_tile_find(cache, path);
    if (tile) return tile->texture;
    if (cache->count >= MAX_PLATFORMS) return NULL;  /* unreachable; see above */

    tile = &cache->tiles[cache->count++];
    str_copy(tile->path, path, sizeof(tile->path));
    tile->in_use = 1;
    tile->texture = texture_load(path);
#ifdef MANGO_TESTING
    s_test_tile_loads++;
#endif
    if (!tile->texture)
        fprintf(stderr, "Warning: Failed to load platform tile %s: %s\n",
                path, "texture unavailable");
    return tile->texture;
}

void level_release_platform_tiles(GameState *gs)
{
    /* Platforms only borrow these; forget their pointers first. */
    for (int i = 0; i < gs->world.platform_count; i++) gs->world.platforms[i].tex = NULL;
    PlatformTileCache *cache = &gs->world.platform_tiles;
    for (int i = 0; i < cache->count; i++) {
        texture_unload(cache->tiles[i].texture);
        cache->tiles[i].texture = NULL;
    }
    cache->count = 0;
}

/*
 * load_platforms — Derive pillar geometry from tile-height placements.
 *
 * Each pillar is shifted 16 px into the floor so the grass top edge meets
 * the ground seamlessly.  Width is tile_width tiles of TILE_SIZE (48 px);
 * a tile_width of 0 means one tile.
 *
 * If a platform specifies a tile_path, it borrows that image from
 * world->platform_tiles, which loads each distinct path once.  Otherwise it
 * is drawn with the shared default pillar texture, gs->assets.textures.platform
 * (grass_platform.png, loaded by game_resources.c and passed as default_tex
 * to platforms_render).
 */
static void load_platforms(GameWorld *world, const LevelDef *def)
{
    platform_tiles_keep_used(&world->platform_tiles, def);

    for (int i = 0; i < def->platform_count; i++) {
        const PlatformPlacement *p = &def->platforms[i];
        Platform *platform = &world->platforms[i];
        int tw = (p->tile_width > 0) ? p->tile_width : 1;
        platform->x = p->x;
        platform->y = level_platform_top_y(p->tile_height);
        platform->w = tw * TILE_SIZE;
        platform->h = p->tile_height * TILE_SIZE;
        platform->tex = NULL;

        /* Borrow the level's shared copy of this platform's tileset. */
        if (p->tile_path[0] != '\0')
            platform->tex = platform_tile_acquire(&world->platform_tiles, p->tile_path);
    }
    world->platform_count = def->platform_count;
}

/* ------------------------------------------------------------------ */
/* Collectibles                                                        */
/* ------------------------------------------------------------------ */

static void load_coins(GameWorld *world, const LevelDef *def)
{
    for (int i = 0; i < def->coin_count; i++) {
        world->coins[i].x      = def->coins[i].x;
        world->coins[i].y      = def->coins[i].y;
        world->coins[i].active = 1;
    }
    world->coin_count = def->coin_count;
}

/*
 * The three star colours are one runtime type (HealthStar), but LevelDef
 * gives each colour its own placement struct, so each colour has its own
 * short loop; place_health_star does the part they share.
 */
static void place_health_star(HealthStar *star, float x, float y)
{
    star->x      = x;
    star->y      = y;
    star->active = 1;
}

static void load_star_yellows(GameWorld *world, const LevelDef *def)
{
    for (int i = 0; i < def->star_yellow_count; i++)
        place_health_star(&world->star_yellows[i], def->star_yellows[i].x, def->star_yellows[i].y);
    world->star_yellow_count = def->star_yellow_count;
}

static void load_star_greens(GameWorld *world, const LevelDef *def)
{
    for (int i = 0; i < def->star_green_count; i++)
        place_health_star(&world->star_greens[i], def->star_greens[i].x, def->star_greens[i].y);
    world->star_green_count = def->star_green_count;
}

static void load_star_reds(GameWorld *world, const LevelDef *def)
{
    for (int i = 0; i < def->star_red_count; i++)
        place_health_star(&world->star_reds[i], def->star_reds[i].x, def->star_reds[i].y);
    world->star_red_count = def->star_red_count;
}

static void load_last_star(GameWorld *world, const LevelDef *def)
{
    world->last_star.x         = def->last_star.x;
    world->last_star.y         = def->last_star.y;
    world->last_star.w         = LAST_STAR_DISPLAY_W;
    world->last_star.h         = LAST_STAR_DISPLAY_H;
    world->last_star.collected = 0;

    /* If the level didn't place one, use a visible default position */
    if (def->last_star.x == 0.0f && def->last_star.y == 0.0f) {
        world->last_star.x = 145.0f;
        world->last_star.y = 167.0f;
    }
    world->last_star.active = 1;
}

/* ------------------------------------------------------------------ */
/* Enemies                                                             */
/* ------------------------------------------------------------------ */

static void load_spiders(GameWorld *world, const LevelDef *def)
{
    for (int i = 0; i < def->spider_count; i++) {
        const SpiderPlacement *p = &def->spiders[i];
        Spider *s = &world->spiders[i];
        s->x             = p->x;
        s->vx            = p->vx;
        s->patrol_x0     = p->patrol_x0;
        s->patrol_x1     = p->patrol_x1;
        s->frame_index   = p->frame_index;
        s->anim_timer_ms = 0;
    }
    world->spider_count = def->spider_count;
}

static void load_jumping_spiders(GameWorld *world, const LevelDef *def)
{
    for (int i = 0; i < def->jumping_spider_count; i++) {
        const JumpingSpiderPlacement *p = &def->jumping_spiders[i];
        JumpingSpider *s = &world->jumping_spiders[i];
        s->x             = p->x;
        s->y             = 0.0f;  /* always on ground */
        s->vx            = p->vx;
        s->vy            = 0.0f;
        s->patrol_x0     = p->patrol_x0;
        s->patrol_x1     = p->patrol_x1;
        s->jump_timer    = 0.0f;
        s->on_ground     = 1;
        s->frame_index   = 0;
        s->anim_timer_ms = 0;
    }
    world->jumping_spider_count = def->jumping_spider_count;
}

/*
 * Birds and faster birds share one struct (FasterBird is a Bird) and one
 * placement type, so one helper fills either array.
 */
static void load_bird_array(Bird *birds, const BirdPlacement *placed, int count)
{
    for (int i = 0; i < count; i++) {
        birds[i].x             = placed[i].x;
        birds[i].base_y        = placed[i].base_y;
        birds[i].vx            = placed[i].vx;
        birds[i].patrol_x0     = placed[i].patrol_x0;
        birds[i].patrol_x1     = placed[i].patrol_x1;
        birds[i].frame_index   = placed[i].frame_index;
        birds[i].anim_timer_ms = 0;
    }
}

static void load_birds(GameWorld *world, const LevelDef *def)
{
    load_bird_array(world->birds, def->birds, def->bird_count);
    world->bird_count = def->bird_count;
}

static void load_faster_birds(GameWorld *world, const LevelDef *def)
{
    load_bird_array(world->faster_birds, def->faster_birds, def->faster_bird_count);
    world->faster_bird_count = def->faster_bird_count;
}

/*
 * Fish and faster fish also share a struct and a placement type; only the
 * range of their random first jump differs.
 *
 * water_y — the y-position of a fish while swimming. Fish rest with their
 * sprite half-submerged in the water. WATER_ART_H = 31 px visible water
 * strip; FISH_RENDER_H = 48 (the faster fish sprite is the same size).
 * water_y = water_surface − (render_h / 2) = (GAME_H − WATER_ART_H) − 24
 *
 * Each fish draws one random number, in array order: keep that order, or
 * seeded replays would give different fish different jump times.
 */
static void load_fish_array(Fish *fish, const FishPlacement *placed, int count,
                            float jump_min, float jump_max)
{
    float water_y = (float)(GAME_H - WATER_ART_H) - FISH_RENDER_H / 2.0f;

    for (int i = 0; i < count; i++) {
        fish[i].x             = placed[i].x;
        fish[i].y             = water_y;
        fish[i].vx            = placed[i].vx;
        fish[i].vy            = 0.0f;
        fish[i].patrol_x0     = placed[i].patrol_x0;
        fish[i].patrol_x1     = placed[i].patrol_x1;
        fish[i].jump_timer    = rand_range(jump_min, jump_max);
        fish[i].water_y       = water_y;
        fish[i].frame_index   = 0;
        fish[i].anim_timer_ms = 0;
    }
}

static void load_fish(GameWorld *world, const LevelDef *def)
{
    load_fish_array(world->fish, def->fish, def->fish_count,
                    FISH_JUMP_MIN, FISH_JUMP_MAX);
    world->fish_count = def->fish_count;
}

static void load_faster_fish(GameWorld *world, const LevelDef *def)
{
    load_fish_array(world->faster_fish, def->faster_fish, def->faster_fish_count,
                    FFISH_JUMP_MIN, FFISH_JUMP_MAX);
    world->faster_fish_count = def->faster_fish_count;
}

/* ------------------------------------------------------------------ */
/* Hazards                                                             */
/* ------------------------------------------------------------------ */

static void load_axe_traps(GameWorld *world, const LevelDef *def)
{
    for (int i = 0; i < def->axe_trap_count; i++) {
        const AxeTrapPlacement *p = &def->axe_traps[i];
        AxeTrap *axe = &world->axe_traps[i];
        /*
         * Pivot x: horizontal centre of the host pillar (left_edge + half_width).
         * Pivot y: top surface of a 3-tile pillar.
         */
        axe->x            = p->pillar_x + TILE_SIZE / 2.0f;
        axe->y            = level_axe_trap_y(p);
        axe->angle        = 0.0f;
        axe->time         = 0.0f;
        axe->mode         = p->mode;
        axe->sound_played = 0;
        axe->active       = 1;
    }
    world->axe_trap_count = def->axe_trap_count;
}

static void load_circular_saws(GameWorld *world, const LevelDef *def)
{
    for (int i = 0; i < def->circular_saw_count; i++) {
        const CircularSawPlacement *p = &def->circular_saws[i];
        CircularSaw *saw = &world->circular_saws[i];
        /*
         * y is fixed at the 2-tile platform top minus the saw's display height.
         * This puts the saw riding along a flat surface at the height of a
         * 2-tile pillar's top edge.
         */
        saw->x          = p->x;
        saw->y          = level_circular_saw_y(p);
        saw->w          = SAW_DISPLAY_W;
        saw->h          = SAW_DISPLAY_H;
        saw->patrol_x0  = p->patrol_x0;
        saw->patrol_x1  = p->patrol_x1;
        saw->direction  = p->direction;
        saw->spin_angle = 0.0f;
        saw->active     = 1;
    }
    world->circular_saw_count = def->circular_saw_count;
}

static void load_spike_rows(GameWorld *world, const LevelDef *def)
{
    for (int i = 0; i < def->spike_row_count; i++) {
        const SpikeRowPlacement *p = &def->spike_rows[i];
        /* y: top edge of spikes sits flush with the ground floor surface. */
        world->spike_rows[i].x      = p->x;
        world->spike_rows[i].y      = (float)(FLOOR_Y - SPIKE_TILE_H);
        world->spike_rows[i].count  = p->count;
        world->spike_rows[i].active = 1;
    }
    world->spike_row_count = def->spike_row_count;
}

static void load_spike_platforms(GameWorld *world, const LevelDef *def)
{
    for (int i = 0; i < def->spike_platform_count; i++) {
        const SpikePlatformPlacement *p = &def->spike_platforms[i];
        world->spike_platforms[i].x      = p->x;
        world->spike_platforms[i].y      = p->y;
        world->spike_platforms[i].w      = p->tile_count * SPIKE_PLAT_PIECE_W;
        world->spike_platforms[i].active = 1;
    }
    world->spike_platform_count = def->spike_platform_count;
}

static void load_spike_blocks(GameWorld *world, const LevelDef *def)
{
    for (int i = 0; i < def->spike_block_count; i++) {
        const SpikeBlockPlacement *p = &def->spike_blocks[i];
        /*
         * spike_block_init expects a Rail pointer and t_offset.
         * rail_index references the world->rails array built by load_rails().
         */
        spike_block_init(&world->spike_blocks[i],
                         &world->rails[p->rail_index],
                         p->t_offset, p->speed);
    }
    world->spike_block_count = def->spike_block_count;
}

/*
 * place_flame — Set one flame waiting below its floor gap.
 *
 * Blue flames and fire flames share the BlueFlame struct and its eruption
 * mechanics (only the texture differs, chosen in game_render.c). Each
 * placement names the gap x where the flame erupts; every entry is used:
 * x = 0 is the gap at the world's left edge, not an "unused" marker.
 * Flame n starts its wait n * 0.5 s late, so neighbours do not erupt
 * together.
 */
static void place_flame(BlueFlame *f, float gap_x, int n)
{
    f->gap_x      = gap_x;
    f->x          = gap_x + (FLOOR_GAP_W - BLUE_FLAME_DISPLAY_W) / 2.0f;
    f->start_y    = (float)(FLOOR_Y + TILE_SIZE);
    f->y          = f->start_y;
    f->vy         = 0.0f;
    f->w          = BLUE_FLAME_DISPLAY_W;
    f->h          = BLUE_FLAME_DISPLAY_H;
    f->angle      = 0.0f;
    f->state      = BLUE_FLAME_WAITING;
    f->timer      = (float)n * 0.5f;
    f->anim_timer = 0.0f;
    f->anim_frame = 0;
    f->active     = 1;
}

static void load_blue_flames(GameWorld *world, const LevelDef *def)
{
    int n = 0;
    for (int i = 0; i < def->blue_flame_count && n < MAX_BLUE_FLAMES; i++) {
        place_flame(&world->blue_flames[n], def->blue_flames[i].x, n);
        n++;
    }
    world->blue_flame_count = n;
}

static void load_fire_flames(GameWorld *world, const LevelDef *def)
{
    int n = 0;
    for (int i = 0; i < def->fire_flame_count && n < MAX_FIRE_FLAMES; i++) {
        place_flame(&world->fire_flames[n], def->fire_flames[i].x, n);
        n++;
    }
    world->fire_flame_count = n;
}

/* ------------------------------------------------------------------ */
/* Surfaces                                                            */
/* ------------------------------------------------------------------ */

static void load_float_platforms(GameWorld *world, const LevelDef *def)
{
    for (int i = 0; i < def->float_platform_count; i++) {
        const FloatPlatformPlacement *p = &def->float_platforms[i];
        const Rail *rail = (p->mode == FLOAT_PLATFORM_RAIL)
                           ? &world->rails[p->rail_index]
                           : NULL;
        float stand_lim  = (p->mode == FLOAT_PLATFORM_CRUMBLE)
                           ? CRUMBLE_STAND_LIMIT : 0.0f;

        float_platform_init(&world->float_platforms[i],
                            p->mode, p->x, p->y, p->tile_count,
                            stand_lim,
                            rail, p->t_offset, p->speed);
    }
    world->float_platform_count = def->float_platform_count;
}

static void load_bridges(GameWorld *world, const LevelDef *def)
{
    for (int i = 0; i < def->bridge_count; i++) {
        const BridgePlacement *p = &def->bridges[i];
        Bridge *b = &world->bridges[i];

        b->x           = p->x;
        b->base_y      = p->y;
        b->brick_count = p->brick_count;

        /* Reset each brick to its default resting state. */
        for (int k = 0; k < p->brick_count; k++) {
            b->bricks[k].y_offset   = 0.0f;
            b->bricks[k].fall_vy    = 0.0f;
            b->bricks[k].falling    = 0;
            b->bricks[k].active     = 1;
            b->bricks[k].fall_delay = -1.0f;
        }
    }
    world->bridge_count = def->bridge_count;
}

/*
 * Bouncepads — GameWorld keeps small/medium/high pads in separate arrays
 * because each variant uses its own texture.  LevelDef mirrors this split
 * exactly, so one helper fills any of the three.
 */
static void load_bouncepad_array(Bouncepad *pads, const BouncepadPlacement *placed, int count)
{
    for (int i = 0; i < count; i++)
        bouncepad_place(&pads[i], placed[i].x, placed[i].launch_vy, placed[i].pad_type);
}

static void load_bouncepads_small(GameWorld *world, const LevelDef *def)
{
    load_bouncepad_array(world->bouncepads_small, def->bouncepads_small, def->bouncepad_small_count);
    world->bouncepad_small_count = def->bouncepad_small_count;
}

static void load_bouncepads_medium(GameWorld *world, const LevelDef *def)
{
    load_bouncepad_array(world->bouncepads_medium, def->bouncepads_medium, def->bouncepad_medium_count);
    world->bouncepad_medium_count = def->bouncepad_medium_count;
}

static void load_bouncepads_high(GameWorld *world, const LevelDef *def)
{
    load_bouncepad_array(world->bouncepads_high, def->bouncepads_high, def->bouncepad_high_count);
    world->bouncepad_high_count = def->bouncepad_high_count;
}

/* ------------------------------------------------------------------ */
/* Decorations & climbables                                            */
/* ------------------------------------------------------------------ */

static void load_vines(GameWorld *world, const LevelDef *def)
{
    for (int i = 0; i < def->vine_count; i++) {
        world->vines[i].x          = def->vines[i].x;
        world->vines[i].y          = def->vines[i].y;
        world->vines[i].tile_count = def->vines[i].tile_count;
        world->vines[i].type       = (VineType)def->vines[i].vine_type;
    }
    world->vine_count = def->vine_count;
}

static void load_ladders(GameWorld *world, const LevelDef *def)
{
    for (int i = 0; i < def->ladder_count; i++) {
        world->ladders[i].x          = def->ladders[i].x;
        world->ladders[i].y          = def->ladders[i].y;
        world->ladders[i].tile_count = def->ladders[i].tile_count;
    }
    world->ladder_count = def->ladder_count;
}

static void load_ropes(GameWorld *world, const LevelDef *def)
{
    for (int i = 0; i < def->rope_count; i++) {
        world->ropes[i].x          = def->ropes[i].x;
        world->ropes[i].y          = def->ropes[i].y;
        world->ropes[i].tile_count = def->ropes[i].tile_count;
    }
    world->rope_count = def->rope_count;
}

/* ------------------------------------------------------------------ */
/* The loader table                                                    */
/* ------------------------------------------------------------------ */

/*
 * s_level_loaders — every load function, in the order it runs.
 *
 * Each row is one placement array: the function that copies it into the
 * world, and when that runs:
 *   LOAD_ONCE        only when a level is loaded (level_apply). Static
 *                    geometry and decorations never change during play;
 *                    coins stay collected after a lost life (see
 *                    level_reset).
 *   LOAD_EVERY_LIFE  also after each lost life (level_reset), so enemies,
 *                    hazards, surfaces and health stars start over.
 *
 * Order matters twice: floor gaps and rails come before the entities that
 * use them (spike blocks and rail platforms keep pointers into rails), and
 * fish draw random numbers, so moving their rows would change seeded runs.
 *
 * Adding a new kind of placement means one load_<things>() above and one
 * row here.
 */
typedef enum {
    LOAD_ONCE,
    LOAD_EVERY_LIFE
} LoaderWhen;

typedef struct {
    void      (*load)(GameWorld *world, const LevelDef *def);
    LoaderWhen  when;
} LevelLoader;

static const LevelLoader s_level_loaders[] = {
    /* load function            when                                 */
    /* ---- Static geometry ----------------------------------------- */
    { load_floor_gaps,          LOAD_ONCE       },
    { load_rails,               LOAD_ONCE       },
    { load_platforms,           LOAD_ONCE       },
    /* ---- Collectibles -------------------------------------------- */
    { load_coins,               LOAD_ONCE       },
    { load_star_yellows,        LOAD_EVERY_LIFE },
    { load_star_greens,         LOAD_EVERY_LIFE },
    { load_star_reds,           LOAD_EVERY_LIFE },
    { load_last_star,           LOAD_EVERY_LIFE },
    /* ---- Enemies ------------------------------------------------- */
    { load_spiders,             LOAD_EVERY_LIFE },
    { load_jumping_spiders,     LOAD_EVERY_LIFE },
    { load_birds,               LOAD_EVERY_LIFE },
    { load_faster_birds,        LOAD_EVERY_LIFE },
    { load_fish,                LOAD_EVERY_LIFE },
    { load_faster_fish,         LOAD_EVERY_LIFE },
    /* ---- Hazards ------------------------------------------------- */
    { load_axe_traps,           LOAD_EVERY_LIFE },
    { load_circular_saws,       LOAD_EVERY_LIFE },
    { load_spike_rows,          LOAD_EVERY_LIFE },
    { load_spike_platforms,     LOAD_EVERY_LIFE },
    { load_spike_blocks,        LOAD_EVERY_LIFE },
    { load_blue_flames,         LOAD_EVERY_LIFE },
    { load_fire_flames,         LOAD_EVERY_LIFE },
    /* ---- Surfaces ------------------------------------------------ */
    { load_float_platforms,     LOAD_EVERY_LIFE },
    { load_bridges,             LOAD_EVERY_LIFE },
    { load_bouncepads_small,    LOAD_EVERY_LIFE },
    { load_bouncepads_medium,   LOAD_EVERY_LIFE },
    { load_bouncepads_high,     LOAD_EVERY_LIFE },
    /* ---- Decorations & climbables -------------------------------- */
    { load_vines,               LOAD_ONCE       },
    { load_ladders,             LOAD_ONCE       },
    { load_ropes,               LOAD_ONCE       },
};

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

/*
 * level_load — Validate a LevelDef, then populate GameState from it.
 *
 * Use this for a definition nobody has checked yet (a test fixture built in
 * code). A file read by level_load_toml has already passed the same check,
 * so the level session calls level_apply directly and validates once.
 */
int level_load(GameState *gs, const LevelDef *def)
{
    char err[128];
    if (level_validate_runtime(def, err, sizeof(err)) != 0) {
        fprintf(stderr, "level_load: invalid level definition: %s\n", err);
        return -1;
    }
    level_apply(gs, def);
    return 0;
}

/*
 * level_apply — Populate the world from a validated LevelDef.
 *
 * It returns nothing because nothing in it can fail: counts and links were
 * checked by level_validate_runtime, and a missing tile image only warns.
 * That lets a level change commit without a failure point after the old
 * level has been replaced.
 *
 * Steps: run every s_level_loaders row (top to bottom), then place the
 * player and apply the level-wide settings.
 */
void level_apply(GameState *gs, const LevelDef *def)
{
    GameWorld *world = &gs->world;

    /* Store a pointer to the active level definition for the rest of the
     * game (checkpoints, camera, completion, audio settings) to read */
    world->runtime.current_level = def;

    /* Set world width from screen_count (default 4 screens if not specified) */
    int screens = (def->screen_count > 0) ? def->screen_count : 4;
    world->runtime.world_w = screens * GAME_W;

    /* ---- Every placement array, in table order ---------------------- */
    for (int i = 0; i < ARRAY_LEN(s_level_loaders); i++)
        s_level_loaders[i].load(world, def);

    /* ---- Player spawn and checkpoints ------------------------------- */

    /*
     * Player spawn — override the defaults set by player_init.
     * player_init has already run by this point, so w/h are valid.
     * FLOOR_SINK is 16 px (defined in player.c); we use the same literal
     * here to keep the spawn formula consistent with player_reset.
     */
    level_effective_spawn(def, &world->respawn_x, &world->respawn_y);
    world->checkpoint_index = -1;
    world->checkpoint_feedback_kind = CHECKPOINT_FEEDBACK_NONE;
    world->checkpoint_feedback_until = 0;
    world->legacy_checkpoint_screen = 0;
    /* A new level, or F8's restart of this one, starts its simulated clock
     * at 0, so a restarted run counts the same steps as the first one. */
    world->sim_steps = 0;
    world->player.spawn_x = world->respawn_x;
    world->player.spawn_y = world->respawn_y;
    world->player.x = world->respawn_x + (TILE_SIZE - world->player.w) / 2.0f;
    world->player.y = world->respawn_y - world->player.h + 16;  /* 16 = FLOOR_SINK */

    /* ---- Level-wide configuration ---------------------------------- */
    /*
     * Enable foreground systems based on what the level definition provides.
     * Fog is driven by fog_layers (the atmospheric overlay textures).
     * Water/lava strip is driven by foreground_layers (the animated bottom strip).
     * Each system is independent — a level can have fog without water, or vice versa.
     */
    world->runtime.fog_enabled   = (def->fog_layer_count > 0) ? 1 : 0;
    world->runtime.water_enabled = (def->foreground_layer_count > 0) ? 1 : 0;

    /*
     * Game rules — use level-defined values if set (>0), otherwise fall
     * back to engine defaults defined in hud.h and coin.h.
     */
    world->hearts          = def->initial_hearts  > 0 ? def->initial_hearts  : MAX_HEARTS;
    world->lives           = def->initial_lives   > 0 ? def->initial_lives   : DEFAULT_LIVES;
    world->score           = 0;
    world->rules.score_per_life  = def->score_per_life  > 0 ? def->score_per_life  : SCORE_PER_LIFE;
    world->score_life_next = world->rules.score_per_life;
    world->rules.coin_score      = def->coin_score     > 0 ? def->coin_score      : COIN_SCORE;

    /* Negative physics values mean engine default; never inherit stale phases. */
    level_apply_player_physics(&world->player, def);
}

/*
 * level_reset — Reset mutable state after a player death.
 *
 * Resets the player, then runs only the s_level_loaders rows marked
 * LOAD_EVERY_LIFE: enemies, hazards, surfaces and health stars start over;
 * static geometry and decorations (which never change during play) and
 * coins are left as they are.
 *
 * Collectible rule for one attempt at a level:
 *   - Coins stay collected across life-loss respawns.  Score and the next
 *     bonus-life threshold also survive a death, so re-activating coins
 *     would let a player farm score (and bonus lives) by dying on purpose.
 *     Only a fresh attempt brings them back: Retry after game over
 *     (game_restart_after_game_over), Replay, or loading a level.
 *   - Health stars and the last star DO respawn.  Stars award no score and
 *     hearts are refilled on respawn anyway, so they cannot be farmed; each
 *     life simply meets the same healing the level was designed with.
 */
void level_reset(GameState *gs, const LevelDef *def)
{
    player_reset(&gs->world.player);

    for (int i = 0; i < ARRAY_LEN(s_level_loaders); i++)
        if (s_level_loaders[i].when == LOAD_EVERY_LIFE)
            s_level_loaders[i].load(&gs->world, def);
}
