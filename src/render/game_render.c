/*
 * game_render.c — Rendering system implementation.
 *
 * Handles all game rendering including the 33 render layers,
 * parallax backgrounds, entities, player, effects, HUD, and overlays.
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

/* ------------------------------------------------------------------ */
/* Layer groups                                                       */
/*                                                                    */
/* Each helper draws a run of neighbouring layers, back to front. The */
/* helpers appear in the order game_render_frame calls them, so       */
/* reading this file top to bottom follows the draw order. Anything   */
/* drawn later covers what was drawn before it.                       */
/* ------------------------------------------------------------------ */

/* The player's reduced-motion setting (no profile means "off"). */
static int reduced_motion(const GameState *gs)
{
    return gs->screen.profile && gs->screen.profile->data.settings.reduced_motion;
}

/*
 * draw_background — The multi-layer parallax background, back-to-front.
 * Each layer scrolls at a fraction of cam_x to simulate depth; reduced
 * motion holds it still.
 */
static void draw_background(GameState *gs, int cam_x)
{
    parallax_render(&gs->world.parallax, reduced_motion(gs) ? 0 : cam_x);
}

/*
 * draw_floor — 9-slice floor rendering, camera-aware, world-wide.
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
static void draw_floor(GameState *gs, int cam_x)
{
    const int P = FLOOR_PIECE_W;   /* 9-slice piece size: 16 px */

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
            sprite_draw(gs->assets.textures.floor_tile, &src, &dst, 0, SPRITE_NORMAL, WHITE);
        }
    }
}

/*
 * draw_ground — Platform pillars, then the floor.
 *
 * The platforms go BEFORE the floor so the floor tiles render on top,
 * hiding the 16 px of each pillar that sinks below FLOOR_Y. This makes
 * the pillars look like they grow out of the ground.
 */
static void draw_ground(GameState *gs, int cam_x)
{
    platforms_render(gs->world.platforms, gs->world.platform_count,
                     gs->assets.textures.platform, cam_x);
    draw_floor(gs, cam_x);
}

/*
 * draw_surfaces — Everything the player stands on or climbs, plus the
 * static spikes that share their layer: float platforms, spike rows and
 * platforms, bridges, bouncepads, rails, vines, ladders and ropes.
 */
static void draw_surfaces(GameState *gs, int cam_x)
{
    /*
     * Draw floating platforms above the floor and pillar layer but below
     * bouncepads and entities, so they read as mid-air surfaces.
     */
    float_platforms_render(gs->world.float_platforms, gs->world.float_platform_count,
                           gs->assets.textures.float_platform, cam_x);

    /* Draw ground spikes on the floor surface, same layer as platforms */
    spike_rows_render(gs->world.spike_rows, gs->world.spike_row_count,
                      gs->assets.textures.spike, cam_x);

    /* Draw spike platforms in the same layer as float platforms */
    spike_platforms_render(gs->world.spike_platforms, gs->world.spike_platform_count,
                           gs->assets.textures.spike_platform, cam_x);

    /* Draw bridges in the same layer as float platforms */
    bridges_render(gs->world.bridges, gs->world.bridge_count,
                   gs->assets.textures.bridge, cam_x);

    /*
     * Draw bouncepads between the platform pillars and vine decorations.
     * This places them visually on the floor surface, with vines potentially
     * overlapping the edges for a natural overgrown look.
     */
    /* Render each bouncepad variant with its own texture */
    bouncepads_render(gs->world.bouncepads_medium, gs->world.bouncepad_medium_count,
                      gs->assets.textures.bouncepad_medium, cam_x);
    if (gs->assets.textures.bouncepad_small) {
        bouncepads_render(gs->world.bouncepads_small, gs->world.bouncepad_small_count,
                          gs->assets.textures.bouncepad_small, cam_x);
    }
    if (gs->assets.textures.bouncepad_high) {
        bouncepads_render(gs->world.bouncepads_high, gs->world.bouncepad_high_count,
                          gs->assets.textures.bouncepad_high, cam_x);
    }

    /*
     * Draw rail tracks before vines and entities so rail tiles appear
     * behind all game objects — the track is part of the background layer.
     */
    if (gs->assets.textures.rail) {
        rails_render(gs->world.rails, gs->world.rail_count,
                     gs->assets.textures.rail, cam_x);
    }

    /* Draw vine decorations on ground and platform tops, behind entities */
    if (gs->assets.textures.vine_green || gs->assets.textures.vine_brown) {
        vines_render(gs->world.vines, gs->world.vine_count,
                     gs->assets.textures.vine_green, gs->assets.textures.vine_brown, cam_x);
    }

    /* Draw ladders and ropes in the same layer as vines */
    if (gs->assets.textures.ladder) {
        ladders_render(gs->world.ladders, gs->world.ladder_count,
                       gs->assets.textures.ladder, cam_x);
    }
    if (gs->assets.textures.rope) {
        ropes_render(gs->world.ropes, gs->world.rope_count,
                     gs->assets.textures.rope, cam_x);
    }
}

/* draw_collectibles — Coins and stars, on top of the platforms and
 * before the water and player. */
static void draw_collectibles(GameState *gs, int cam_x)
{
    coins_render(gs->world.coins, gs->world.coin_count,
                 gs->assets.textures.coin, cam_x);

    /* Draw health stars alongside coins — one renderer, the texture picks
     * the colour (yellow, then green, then red). */
    health_stars_render(gs->world.star_yellows, gs->world.star_yellow_count,
                        gs->assets.textures.star_yellow, cam_x);
    health_stars_render(gs->world.star_greens, gs->world.star_green_count,
                        gs->assets.textures.star_green, cam_x);
    health_stars_render(gs->world.star_reds, gs->world.star_red_count,
                        gs->assets.textures.star_red, cam_x);

    /* Draw the end-of-level last star using its dedicated sprite */
    last_star_render(&gs->world.last_star,
                     gs->assets.textures.last_star, cam_x);
}

/*
 * draw_water_layer — What lives in the floor gaps: flames and fish first,
 * then the water strip over them so the wave art hides their submerged
 * part.
 */
static void draw_water_layer(GameState *gs, int cam_x)
{
    /* Draw blue flames behind the water and fish, in front of ground */
    blue_flames_render(gs->world.blue_flames, gs->world.blue_flame_count,
                  gs->assets.textures.blue_flame, cam_x);
    /* Draw fire flames in the same layer (fire variant texture) */
    blue_flames_render(gs->world.fire_flames, gs->world.fire_flame_count,
                  gs->assets.textures.fire_flame, cam_x);

    /* Draw fish behind the water strip (submerged look) but in front of
     * the ground, so the water wave art occludes the submerged portion. */
    fish_render(gs->world.fish, gs->world.fish_count,
            gs->assets.textures.fish, cam_x);
    /* Draw faster fish in the same layer as regular fish */
    faster_fish_render(gs->world.faster_fish, gs->world.faster_fish_count,
                       gs->assets.textures.faster_fish, cam_x);

    /*
     * Draw the water strip on top of the floor/platforms and fish.
     * The full 384-px sheet scrolls rightward as a seamless loop.
     */
    if (gs->world.runtime.water_enabled) water_render(&gs->world.water);
}

/* draw_moving_hazards — Spike blocks, axe traps and circular saws, above
 * the water strip but below enemies and the player. */
static void draw_moving_hazards(GameState *gs, int cam_x)
{
    if (gs->assets.textures.spike_block) {
        spike_blocks_render(gs->world.spike_blocks, gs->world.spike_block_count,
                            gs->assets.textures.spike_block, cam_x);
    }

    axe_traps_render(gs->world.axe_traps, gs->world.axe_trap_count,
                     gs->assets.textures.axe_trap, cam_x);

    /* Draw circular saws in the same hazard layer as axe traps */
    circular_saws_render(gs->world.circular_saws, gs->world.circular_saw_count,
                         gs->assets.textures.circular_saw, cam_x);
}

/* draw_enemies — Spiders, then birds in the sky in front of them; all
 * behind the player. */
static void draw_enemies(GameState *gs, int cam_x)
{
    spiders_render(gs->world.spiders, gs->world.spider_count,
                   gs->assets.textures.spider, cam_x);
    /* Draw jumping spiders in the same layer as regular spiders */
    jumping_spiders_render(gs->world.jumping_spiders, gs->world.jumping_spider_count,
                           gs->assets.textures.jumping_spider, cam_x);

    birds_render(gs->world.birds, gs->world.bird_count,
                 gs->assets.textures.bird, cam_x);
    faster_birds_render(gs->world.faster_birds, gs->world.faster_bird_count,
                        gs->assets.textures.faster_bird, cam_x);
}

/*
 * draw_ghost — The time-trial ghost: Mango as he was at this moment of the
 * best run (game_ghost.c picks the sample), drawn just behind the real one.
 *
 * It is translucent, so it never hides the player. High contrast makes it
 * more solid and outlines its body in cyan, a colour nothing else uses, so
 * it cannot be mistaken for the player (outlined in white). Reduced motion
 * holds one pose per animation (its first frame) instead of flickering
 * through the frames; the ghost still moves.
 */
static void draw_ghost(GameState *gs, int cam_x)
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

/*
 * draw_foreground — Fog/mist over the whole scene, after the player. Only
 * active when the level enables fog and reduced motion is off. High
 * contrast then outlines the player's hitbox and darkens the HUD strip.
 */
static void draw_foreground(GameState *gs, int cam_x)
{
    if (gs->world.runtime.fog_enabled && !reduced_motion(gs)) fog_render(&gs->world.fog);
    if (gs->screen.profile && gs->screen.profile->data.settings.high_contrast) {
        IntRect hit = player_get_hitbox(&gs->world.player);
        hit.x -= cam_x;
        DrawRectangleLines(hit.x-1,hit.y-1,hit.w+2,hit.h+2,BLACK);
        DrawRectangleLines(hit.x,hit.y,hit.w,hit.h,WHITE);
        DrawRectangle(0,0,GAME_W,22,BLACK);
    }
}

/*
 * draw_hud_and_overlays — The HUD (hearts, lives, score), then the debug
 * overlays when active, then the player-facing overlay (level complete,
 * game over or pause), which covers everything else.
 */
static void draw_hud_and_overlays(GameState *gs, int cam_x)
{
    hud_render(&gs->screen.hud,
               gs->world.hearts, gs->world.lives, gs->world.score,
               gs->world.checkpoint_index,
               gs->world.checkpoint_feedback_kind,
               gs->world.checkpoint_feedback_until,
               game_checkpoint_clock_ms(gs));

    /* Draw debug overlays (collision boxes, FPS, event log) if active */
    if (gs->screen.debug_mode) {
        debug_render(&gs->screen.debug, gs->screen.hud.font, gs, cam_x);
        game_inspector_render(gs);
    }

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

    /* Back to front; architecture.md lists every layer in this order. */
    draw_background(gs, cam_x);
    draw_ground(gs, cam_x);
    draw_surfaces(gs, cam_x);
    draw_collectibles(gs, cam_x);
    draw_water_layer(gs, cam_x);
    draw_moving_hazards(gs, cam_x);
    draw_enemies(gs, cam_x);
    draw_ghost(gs, cam_x);
    player_render(&gs->world.player, cam_x);
    draw_foreground(gs, cam_x);
    draw_hud_and_overlays(gs, cam_x);

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
