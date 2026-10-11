/*
 * game_render.c — Rendering system implementation.
 *
 * Handles all game rendering including the 33 render layers,
 * parallax backgrounds, entities, player, effects, HUD, and overlays.
 *
 * How to read it:
 *   1. One small render_<layer>() function per layer, in draw order. Each
 *      calls the module that knows how to draw that kind (spiders_render in
 *      spider.c, and so on) with the array, the count and the texture it
 *      needs, so the function is where you see what a layer is made of.
 *   2. s_render_layers, near the end: a table listing those functions back
 *      to front. That order is the draw order, and the manual's Render
 *      Order table (architecture page) lists the same rows.
 *   3. game_render_frame, which clears the canvas, walks the table, and
 *      draws the overlays on top.
 */

#include "game_render.h"
#include "../core/game_checkpoint.h"  /* game_checkpoint_clock_ms */
#include "../screens/settings_menu.h"

#include "../core/debug.h"
#include "../core/game_inspector.h"
#include "../core/game_ghost.h"
#include "../core/game_overlay.h"
#include "../screens/hud.h"

/* Effect headers */
#include "../effects/parallax.h"
#include "../effects/water.h"
#include "../effects/fog.h"

/* Surface headers */
#include "../surfaces/platform.h"
#include "../surfaces/float_platform.h"
#include "../surfaces/bouncepad.h"
#include "../surfaces/bridge.h"
#include "../surfaces/rail.h"
#include "../surfaces/vine.h"
#include "../surfaces/ladder.h"
#include "../surfaces/rope.h"

/* Hazard headers */
#include "../hazards/spike.h"
#include "../hazards/spike_platform.h"
#include "../hazards/spike_block.h"
#include "../hazards/axe_trap.h"
#include "../hazards/circular_saw.h"
#include "../hazards/blue_flame.h"

/* Entity headers */
#include "../entities/spider.h"
#include "../entities/jumping_spider.h"
#include "../entities/bird.h"
#include "../entities/faster_bird.h"
#include "../entities/fish.h"
#include "../entities/faster_fish.h"

/* Collectible headers */
#include "../collectibles/coin.h"
#include "../collectibles/health_star.h"
#include "../collectibles/last_star.h"

/* Player header */
#include "../player/player.h"

/* The player's reduced-motion setting (no profile means "off"). */
static int reduced_motion(const GameState *gs)
{
    return gs->screen.profile && gs->screen.profile->data.settings.reduced_motion;
}

/* ------------------------------------------------------------------ */
/* Layers 1-3: background and ground                                   */
/* ------------------------------------------------------------------ */

/*
 * Layer 1: the multi-layer parallax background, back-to-front. Each layer
 * scrolls at a fraction of cam_x to simulate depth; reduced motion holds it
 * still.
 */
static void render_background(GameState *gs, int cam_x)
{
    parallax_render(&gs->world.parallax, reduced_motion(gs) ? 0 : cam_x);
}

/*
 * Layer 2: platform pillars. They go BEFORE the floor so the floor tiles
 * render on top, hiding the 16 px of each pillar that sinks below FLOOR_Y.
 * This makes the pillars look like they grow out of the ground.
 */
static void render_platforms(GameState *gs, int cam_x)
{
    platforms_render(gs->world.platforms, gs->world.platform_count,
                     gs->assets.textures.platform, cam_x);
}

/*
 * Layer 3: the floor. 9-slice rendering, camera-aware, world-wide.
 *
 * The 48×48 floor tileset (the level's floor_tile_path, by default
 * grass_tileset.png) is divided into a 3×3 grid of 16×16 pieces
 * (TILE_SIZE / 3 = 16). Layout:
 *
 *   [TL][TC][TR]   row 0  y= 0..15  ← grass edge
 *   [ML][MC][MR]   row 1  y=16..31  ← dirt interior
 *   [BL][BC][BR]   row 2  y=32..47  ← floor base edge
 *
 * Piece column selection (based on world-space tx):
 *   • tx <= 0, or the piece just right of a floor gap
 *                                → col 0 (left edge cap)
 *   • tx + P >= gs->world.runtime.world_w, or the piece just left of a gap
 *                                → col 2 (right edge cap)
 *   • every other column, or one that is both edges
 *                                → col 1 (seamless center fill)
 *
 * We iterate tx in world coordinates starting from the tile-aligned
 * column just behind cam_x, and stop once tx is off the right edge
 * of the screen. dst.x = tx - cam_x converts world → screen.
 */
static void render_floor(GameState *gs, int cam_x)
{
    const int P = FLOOR_PIECE_W;   /* 9-slice piece size: 16 px */

    /* The level's own tileset, or the shared default when it names none. */
    Texture2D *tileset = gs->world.floor_tile ? gs->world.floor_tile
                                              : gs->assets.textures.floor_tile;

    /* First piece column at or before the left edge of the viewport */
    int floor_start_tx = (cam_x / P) * P;

    for (int ty = FLOOR_Y; ty < GAME_H; ty += P) {
        /* Choose which row of the 9-slice to sample */
        int piece_row;
        if (ty == FLOOR_Y)         piece_row = 0;  /* top:    grass edge */
        else if (ty + P >= GAME_H) piece_row = 2;  /* bottom: base edge  */
        else                       piece_row = 1;  /* middle: dirt fill  */

        for (int tx = floor_start_tx; tx < cam_x + GAME_W; tx += P) {
            /*
             * Skip this piece if it falls inside any sea gap.
             * A piece at tx is inside a gap when:
             *   gap_x <= tx  AND  tx + P <= gap_x + FLOOR_GAP_W
             * (both edges of the 16-px piece are within the 32-px gap).
             */
            int in_gap = 0;
            for (int g = 0; g < gs->world.floor_gap_count; g++) {
                int gx = gs->world.floor_gaps[g];
                if (tx >= gx && tx + P <= gx + FLOOR_GAP_W) {
                    in_gap = 1;
                    break;
                }
            }
            if (in_gap) continue;

            /*
             * Choose which column of the 9-slice to sample.
             * Use edge caps at gap boundaries so the floor has
             * clean left/right edges beside each hole.
             */
            int piece_col;
            int at_left_edge  = 0;
            int at_right_edge = 0;

            /* World boundaries */
            if (tx <= 0)                at_left_edge  = 1;
            if (tx + P >= gs->world.runtime.world_w)  at_right_edge = 1;

            /* Gap boundaries: piece is a right-cap if next piece is gap,
             * left-cap if previous piece was gap. */
            for (int g = 0; g < gs->world.floor_gap_count; g++) {
                int gx = gs->world.floor_gaps[g];
                if (tx + P == gx)                at_right_edge = 1;  /* gap starts right after this piece */
                if (tx     == gx + FLOOR_GAP_W)  at_left_edge  = 1;  /* gap ends right before this piece  */
            }

            if (at_left_edge && at_right_edge) piece_col = 1;  /* squeezed — use fill */
            else if (at_left_edge)             piece_col = 0;
            else if (at_right_edge)            piece_col = 2;
            else                               piece_col = 1;

            /*
             * src  — the 16×16 region to cut from the tile sheet.
             * dst  — world → screen: dst.x = tx - cam_x.
             *        Pieces outside the viewport are discarded by the target's
             *        internal clipping — no manual culling needed.
             */
            IntRect src = { piece_col * P, piece_row * P, P, P };
            IntRect dst = { tx - cam_x,    ty,            P, P };
            sprite_draw(tileset, &src, &dst, 0, SPRITE_NORMAL, WHITE);
        }
    }
}

/* ------------------------------------------------------------------ */
/* Layers 4-14: surfaces, static spikes and climbables                 */
/* ------------------------------------------------------------------ */

/* Layer 4: floating platforms, above the floor and pillars but below
 * bouncepads and entities, so they read as mid-air surfaces. */
static void render_float_platforms(GameState *gs, int cam_x)
{
    float_platforms_render(gs->world.float_platforms, gs->world.float_platform_count,
                           gs->assets.textures.float_platform, cam_x);
}

/* Layer 5: ground spikes on the floor surface. */
static void render_spike_rows(GameState *gs, int cam_x)
{
    spike_rows_render(gs->world.spike_rows, gs->world.spike_row_count,
                      gs->assets.textures.spike, cam_x);
}

/* Layer 6: spike platforms. */
static void render_spike_platforms(GameState *gs, int cam_x)
{
    spike_platforms_render(gs->world.spike_platforms, gs->world.spike_platform_count,
                           gs->assets.textures.spike_platform, cam_x);
}

/* Layer 7: bridges. */
static void render_bridges(GameState *gs, int cam_x)
{
    bridges_render(gs->world.bridges, gs->world.bridge_count,
                   gs->assets.textures.bridge, cam_x);
}

/*
 * Layers 8-10: bouncepads, one layer per variant, each with its own
 * texture. They sit on the floor surface, with vines potentially
 * overlapping their edges for a natural overgrown look. The small and high
 * pads are skipped when their sprite is not loaded.
 */
static void render_bouncepads_medium(GameState *gs, int cam_x)
{
    bouncepads_render(gs->world.bouncepads_medium, gs->world.bouncepad_medium_count,
                      gs->assets.textures.bouncepad_medium, cam_x);
}

static void render_bouncepads_small(GameState *gs, int cam_x)
{
    if (gs->assets.textures.bouncepad_small) {
        bouncepads_render(gs->world.bouncepads_small, gs->world.bouncepad_small_count,
                          gs->assets.textures.bouncepad_small, cam_x);
    }
}

static void render_bouncepads_high(GameState *gs, int cam_x)
{
    if (gs->assets.textures.bouncepad_high) {
        bouncepads_render(gs->world.bouncepads_high, gs->world.bouncepad_high_count,
                          gs->assets.textures.bouncepad_high, cam_x);
    }
}

/* Layer 11: rail tracks, before vines and entities so the track tiles
 * appear behind every game object. */
static void render_rails(GameState *gs, int cam_x)
{
    if (gs->assets.textures.rail) {
        rails_render(gs->world.rails, gs->world.rail_count,
                     gs->assets.textures.rail, cam_x);
    }
}

/* Layer 12: vines on ground and platform tops, behind entities. One
 * renderer takes both textures and picks one per vine. */
static void render_vines(GameState *gs, int cam_x)
{
    if (gs->assets.textures.vine_green || gs->assets.textures.vine_brown) {
        vines_render(gs->world.vines, gs->world.vine_count,
                     gs->assets.textures.vine_green, gs->assets.textures.vine_brown, cam_x);
    }
}

/* Layer 13: ladders. */
static void render_ladders(GameState *gs, int cam_x)
{
    if (gs->assets.textures.ladder) {
        ladders_render(gs->world.ladders, gs->world.ladder_count,
                       gs->assets.textures.ladder, cam_x);
    }
}

/* Layer 14: ropes. */
static void render_ropes(GameState *gs, int cam_x)
{
    if (gs->assets.textures.rope) {
        ropes_render(gs->world.ropes, gs->world.rope_count,
                     gs->assets.textures.rope, cam_x);
    }
}

/* ------------------------------------------------------------------ */
/* Layers 15-17: collectibles                                          */
/* ------------------------------------------------------------------ */

/* Layer 15: coins, on top of the platforms and before the water and
 * player. */
static void render_coins(GameState *gs, int cam_x)
{
    coins_render(gs->world.coins, gs->world.coin_count,
                 gs->assets.textures.coin, cam_x);
}

/* Layer 16: health stars. One renderer; the texture picks the colour
 * (yellow, then green, then red). */
static void render_health_stars(GameState *gs, int cam_x)
{
    health_stars_render(gs->world.star_yellows, gs->world.star_yellow_count,
                        gs->assets.textures.star_yellow, cam_x);
    health_stars_render(gs->world.star_greens, gs->world.star_green_count,
                        gs->assets.textures.star_green, cam_x);
    health_stars_render(gs->world.star_reds, gs->world.star_red_count,
                        gs->assets.textures.star_red, cam_x);
}

/* Layer 17: the end-of-level last star, with its own sprite. */
static void render_last_star(GameState *gs, int cam_x)
{
    last_star_render(&gs->world.last_star,
                     gs->assets.textures.last_star, cam_x);
}

/* ------------------------------------------------------------------ */
/* Layers 18-21: what lives in the floor gaps, then the water          */
/* ------------------------------------------------------------------ */

/* Layer 18: flames, in front of the ground and behind the water. One
 * renderer; the texture picks blue, then fire. */
static void render_flames(GameState *gs, int cam_x)
{
    blue_flames_render(gs->world.blue_flames, gs->world.blue_flame_count,
                  gs->assets.textures.blue_flame, cam_x);
    blue_flames_render(gs->world.fire_flames, gs->world.fire_flame_count,
                  gs->assets.textures.fire_flame, cam_x);
}

/* Layer 19: fish, behind the water strip (submerged look) but in front of
 * the ground, so the water wave art occludes the submerged portion. */
static void render_fish(GameState *gs, int cam_x)
{
    fish_render(gs->world.fish, gs->world.fish_count,
            gs->assets.textures.fish, cam_x);
}

/* Layer 20: faster fish. */
static void render_faster_fish(GameState *gs, int cam_x)
{
    faster_fish_render(gs->world.faster_fish, gs->world.faster_fish_count,
                       gs->assets.textures.faster_fish, cam_x);
}

/*
 * Layer 21: the water strip, on top of the floor, platforms and fish. The
 * full 384-px sheet scrolls rightward as a seamless loop. It spans the
 * screen, so it needs no camera.
 */
static void render_water(GameState *gs, int cam_x)
{
    (void)cam_x;
    if (gs->world.runtime.water_enabled) water_render(&gs->world.water);
}

/* ------------------------------------------------------------------ */
/* Layers 22-24: moving hazards, above the water, below the enemies    */
/* ------------------------------------------------------------------ */

/* Layer 22: spike blocks. */
static void render_spike_blocks(GameState *gs, int cam_x)
{
    if (gs->assets.textures.spike_block) {
        spike_blocks_render(gs->world.spike_blocks, gs->world.spike_block_count,
                            gs->assets.textures.spike_block, cam_x);
    }
}

/* Layer 23: axe traps. */
static void render_axe_traps(GameState *gs, int cam_x)
{
    axe_traps_render(gs->world.axe_traps, gs->world.axe_trap_count,
                     gs->assets.textures.axe_trap, cam_x);
}

/* Layer 24: circular saws. */
static void render_circular_saws(GameState *gs, int cam_x)
{
    circular_saws_render(gs->world.circular_saws, gs->world.circular_saw_count,
                         gs->assets.textures.circular_saw, cam_x);
}

/* ------------------------------------------------------------------ */
/* Layers 25-28: enemies, all behind the player                        */
/* ------------------------------------------------------------------ */

/* Layer 25: spiders. */
static void render_spiders(GameState *gs, int cam_x)
{
    spiders_render(gs->world.spiders, gs->world.spider_count,
                   gs->assets.textures.spider, cam_x);
}

/* Layer 26: jumping spiders. */
static void render_jumping_spiders(GameState *gs, int cam_x)
{
    jumping_spiders_render(gs->world.jumping_spiders, gs->world.jumping_spider_count,
                           gs->assets.textures.jumping_spider, cam_x);
}

/* Layer 27: birds, in the sky in front of the spiders. */
static void render_birds(GameState *gs, int cam_x)
{
    birds_render(gs->world.birds, gs->world.bird_count,
                 gs->assets.textures.bird, cam_x);
}

/* Layer 28: faster birds. */
static void render_faster_birds(GameState *gs, int cam_x)
{
    faster_birds_render(gs->world.faster_birds, gs->world.faster_bird_count,
                        gs->assets.textures.faster_bird, cam_x);
}

/* ------------------------------------------------------------------ */
/* Layers 29-33: ghost, player, fog, HUD and debug                     */
/* ------------------------------------------------------------------ */

/*
 * Layer 29: the time-trial ghost. Mango as he was at this moment of the
 * best run (game_ghost.c picks the sample), drawn just behind the real one.
 *
 * It is translucent, so it never hides the player. High contrast makes it
 * more solid and outlines its body in cyan, a colour nothing else uses, so
 * it cannot be mistaken for the player (outlined in white). Reduced motion
 * holds one pose per animation (its first frame) instead of flickering
 * through the frames; the ghost still moves.
 */
static void render_ghost(GameState *gs, int cam_x)
{
    const GhostSample *sample = game_ghost_current(gs);
    if (!sample) return;
    int high_contrast = gs->screen.profile->data.settings.high_contrast;
    int cell = sample->cell & (GHOST_CELL_FACING_LEFT - 1);
    if (reduced_motion(gs)) cell -= cell % GHOST_SHEET_COLS;
    IntRect source = {(cell % GHOST_SHEET_COLS) * GHOST_SHEET_FRAME,
                      (cell / GHOST_SHEET_COLS) * GHOST_SHEET_FRAME,
                      GHOST_SHEET_FRAME, GHOST_SHEET_FRAME};
    /* Stored positions carry GHOST_COORD_OFFSET; take it off again. */
    IntRect dest = {(int)sample->x - GHOST_COORD_OFFSET - cam_x, (int)sample->y - GHOST_COORD_OFFSET,
                    gs->world.player.w, gs->world.player.h};
    int flip = (sample->cell & GHOST_CELL_FACING_LEFT) ? SPRITE_FLIP_X : SPRITE_NORMAL;
    Color tint = {255, 255, 255, (unsigned char)(high_contrast ? 170 : 120)};
    sprite_draw(gs->world.player.texture, &source, &dest, 0, flip, tint);
    if (high_contrast) {
        /* The player's hitbox sits at the same offset inside every frame. */
        IntRect body = player_get_hitbox(&gs->world.player);
        DrawRectangleLines(dest.x + body.x - (int)gs->world.player.x, dest.y + body.y - (int)gs->world.player.y,
                           body.w, body.h, (Color){0, 230, 255, 255});
    }
}

/* Layer 30: the player, after every enemy and hazard, so nothing in the
 * level hides them. */
static void render_player(GameState *gs, int cam_x)
{
    player_render(&gs->world.player, cam_x);
}

/*
 * Layer 31: fog/mist over the whole scene, after the player. Only active
 * when the level enables fog and reduced motion is off. It covers the
 * screen, so it needs no camera.
 */
static void render_fog(GameState *gs, int cam_x)
{
    (void)cam_x;
    if (gs->world.runtime.fog_enabled && !reduced_motion(gs)) fog_render(&gs->world.fog);
}

/*
 * draw_high_contrast_aids — With the high-contrast setting on, outline the
 * player's hitbox over the fog and darken the strip the HUD is drawn on.
 */
static void draw_high_contrast_aids(GameState *gs, int cam_x)
{
    if (gs->screen.profile && gs->screen.profile->data.settings.high_contrast) {
        IntRect hit = player_get_hitbox(&gs->world.player);
        hit.x -= cam_x;
        DrawRectangleLines(hit.x-1,hit.y-1,hit.w+2,hit.h+2,BLACK);
        DrawRectangleLines(hit.x,hit.y,hit.w,hit.h,WHITE);
        DrawRectangle(0,0,GAME_W,22,BLACK);
    }
}

/* Layer 32: the HUD (hearts, lives, score). The high-contrast aids go
 * first, so their dark strip ends up behind the hearts. */
static void render_hud(GameState *gs, int cam_x)
{
    draw_high_contrast_aids(gs, cam_x);
    hud_render(&gs->screen.hud,
               gs->world.hearts, gs->world.lives, gs->world.score,
               gs->world.checkpoint_index,
               gs->world.checkpoint_feedback_kind,
               gs->world.checkpoint_feedback_until,
               game_checkpoint_clock_ms(gs));
}

/* Layer 33: the debug overlays (collision boxes, FPS, event log) and the
 * inspector panels, if active. */
static void render_debug(GameState *gs, int cam_x)
{
    if (gs->screen.debug_mode) {
        debug_render(&gs->screen.debug, gs->screen.hud.font, gs, cam_x);
        game_inspector_render(gs);
    }
}

/* ------------------------------------------------------------------ */
/* The layer table                                                     */
/* ------------------------------------------------------------------ */

/*
 * s_render_layers — every layer of the frame, back to front.
 *
 * Each row is the layer's number, its name, and the render_<layer>()
 * function above that draws it. `render` is a function pointer: the row
 * stores which function to call, and game_render_frame calls it through
 * the pointer. The number and the name are for people: they are the Layer
 * and What columns of the Render Order table in the manual's architecture
 * page, and `make docs-drift` fails when the two tables differ.
 *
 * Order is everything here: a layer covers what the rows above it drew
 * (the painter's algorithm). That is why the platforms come before the
 * floor that hides their feet, the fish before the water they swim in,
 * and the fog after the player.
 *
 * To draw a new enemy, say a bat: write render_bats() beside
 * render_birds(), add its row where it should sit in depth, renumber the
 * rows after it, and add the same row to the manual's table. See
 * s_update_steps in game_update.c for the other three tables a new enemy
 * needs a row in.
 */
typedef void (*RenderFn)(GameState *gs, int cam_x);

typedef struct {
    int         layer;   /* 1 is drawn first, so it is the furthest back */
    const char *name;    /* what the layer shows                         */
    RenderFn    render;  /* the function that draws it                   */
} RenderLayer;

static const RenderLayer s_render_layers[] = {
    /* layer, name,               render                    */
    /* ---- Background and ground ---------------------------------- */
    {  1, "background",           render_background        },
    {  2, "platforms",            render_platforms         },
    {  3, "floor",                render_floor             },
    /* ---- Surfaces, static spikes and climbables ----------------- */
    {  4, "float platforms",      render_float_platforms   },
    {  5, "spike rows",           render_spike_rows        },
    {  6, "spike platforms",      render_spike_platforms   },
    {  7, "bridges",              render_bridges           },
    {  8, "bouncepads (medium)",  render_bouncepads_medium },
    {  9, "bouncepads (small)",   render_bouncepads_small  },
    { 10, "bouncepads (high)",    render_bouncepads_high   },
    { 11, "rails",                render_rails             },
    { 12, "vines",                render_vines             },
    { 13, "ladders",              render_ladders           },
    { 14, "ropes",                render_ropes             },
    /* ---- Collectibles ------------------------------------------- */
    { 15, "coins",                render_coins             },
    { 16, "health stars",         render_health_stars      },
    { 17, "last star",            render_last_star         },
    /* ---- In the floor gaps, then the water over them ------------ */
    { 18, "blue and fire flames", render_flames            },
    { 19, "fish",                 render_fish              },
    { 20, "faster fish",          render_faster_fish       },
    { 21, "water",                render_water             },
    /* ---- Moving hazards ----------------------------------------- */
    { 22, "spike blocks",         render_spike_blocks      },
    { 23, "axe traps",            render_axe_traps         },
    { 24, "circular saws",        render_circular_saws     },
    /* ---- Enemies ------------------------------------------------ */
    { 25, "spiders",              render_spiders           },
    { 26, "jumping spiders",      render_jumping_spiders   },
    { 27, "birds",                render_birds             },
    { 28, "faster birds",         render_faster_birds      },
    /* ---- Ghost, player and what is drawn over them -------------- */
    { 29, "time-trial ghost",     render_ghost             },
    { 30, "player",               render_player            },
    { 31, "fog",                  render_fog               },
    { 32, "HUD",                  render_hud               },
    { 33, "debug overlay",        render_debug             },
};

#define RENDER_LAYER_COUNT (sizeof(s_render_layers) / sizeof(s_render_layers[0]))

/*
 * draw_overlay — The player-facing overlay (level complete, game over or
 * pause), if one is up. It is not a layer of the level: it covers them all.
 */
static void draw_overlay(GameState *gs)
{
    GameOverlayState overlay = game_overlay_state(gs);
    if (overlay == GAME_OVERLAY_LEVEL_COMPLETE) {
        render_level_complete_overlay(gs);
    } else if (overlay == GAME_OVERLAY_GAME_OVER) {
        render_game_over_overlay(gs);
    } else if (overlay == GAME_OVERLAY_PAUSED) {
        render_pause_overlay(gs);
    }
}

/* ------------------------------------------------------------------ */
/* Main render function                                               */
/* ------------------------------------------------------------------ */

int game_render_frame(GameState *gs, int cam_x, float dt)
{
    /*
     * Update the debug overlay even while paused so the FPS counter
     * keeps measuring render frames and log entries age correctly.
     */
    if (gs->screen.debug_mode) debug_update(&gs->screen.debug, dt);

    /*
     * Clear the logical render target.
     * We always clear before drawing to avoid leftover pixels from the
     * previous frame showing through.
     */
    if (!IsRenderTextureValid(gs->screen.frame_target)) {
        if (gs->screen.route == GAME_ROUTE_NONE) gs->screen.route = GAME_ROUTE_FATAL;
        return 0;
    }

    BeginDrawing();
    BeginTextureMode(gs->screen.frame_target);
    ClearBackground(BLACK);

    /* Back to front: each layer covers what the ones before it drew. */
    for (size_t i = 0; i < RENDER_LAYER_COUNT; i++) {
        s_render_layers[i].render(gs, cam_x);
    }

    /* On top of all the layers: the level-complete, game-over or pause
     * overlay, if one is up. */
    draw_overlay(gs);

    /*
     * The settings panel sits above even the overlays. Then present the
     * logical render target to the screen: everything so far was drawn
     * off-screen, and this makes it visible at once, preventing flicker.
     * With VSync enabled, presenting also blocks until the monitor is
     * ready for the next frame (typically ~16ms at 60 Hz).
     */
    settings_menu_render(gs->screen.settings_menu, gs->screen.profile, gs->screen.hud.font);
    display_present(gs->screen.frame_target);
    return 1;
}
