/*
 * game_render.c — Rendering system implementation.
 *
 * Handles all game rendering including the 32 render layers,
 * parallax backgrounds, entities, player, effects, HUD, and overlays.
 */

#include "game_render.h"

#include "../core/debug.h"
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
#include "../collectibles/star_yellow.h"
#include "../collectibles/star_green.h"
#include "../collectibles/star_red.h"
#include "../collectibles/last_star.h"

/* Player header */
#include "../player/player.h"

#include <SDL_ttf.h>  /* TTF_RenderText_Solid for overlays */

/* ------------------------------------------------------------------ */
/* Main render function                                               */
/* ------------------------------------------------------------------ */

void game_render_frame(GameState *gs, int cam_x, float dt)
{
    /*
     * Update the debug overlay even while paused so the FPS counter
     * keeps measuring render frames and log entries age correctly.
     */
    if (gs->debug_mode) debug_update(&gs->debug, dt);

    /*
     * SDL_RenderClear — fill the back buffer with the draw colour.
     * We always clear before drawing to avoid leftover pixels from the
     * previous frame showing through.
     */
    SDL_RenderClear(gs->renderer);

    /*
     * Draw the multi-layer parallax background, back-to-front.
     * Each layer scrolls at a fraction of cam_x to simulate depth.
     * cam_x is the integer camera offset computed above.
     */
    parallax_render(&gs->parallax, gs->renderer, cam_x);

    /*
     * Draw the platforms BEFORE the floor so the floor tiles render
     * on top, hiding the 16 px of each pillar that sinks below FLOOR_Y.
     * This makes the pillars look like they grow out of the ground.
     */
    platforms_render(gs->platforms, gs->platform_count,
                     gs->renderer, gs->platform_tex, cam_x);

    /*
     * 9-slice floor rendering — camera-aware, world-wide.
     *
     * The 48×48 Grass_Tileset.png is divided into a 3×3 grid of 16×16 pieces
     * (TILE_SIZE / 3 = 16). Layout:
     *
     *   [TL][TC][TR]   row 0  y= 0..15  ← grass edge
     *   [ML][MC][MR]   row 1  y=16..31  ← dirt interior
     *   [BL][BC][BR]   row 2  y=32..47  ← floor base edge
     *
     * Piece column selection (based on world-space tx):
     *   • tx == 0              → col 0 (left  world edge cap)
     *   • tx + P >= WORLD_W    → col 2 (right world edge cap)
     *   • all other columns    → col 1 (seamless center fill)
     *
     * We iterate tx in world coordinates starting from the tile-aligned
     * column just behind cam_x, and stop once tx is off the right edge
     * of the screen. dst.x = tx - cam_x converts world → screen.
     */
    const int P = TILE_SIZE / 3;   /* 9-slice piece size: 16 px */

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
            for (int g = 0; g < gs->floor_gap_count; g++) {
                int gx = gs->floor_gaps[g];
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
            if (tx + P >= gs->world_w)  at_right_edge = 1;

            /* Gap boundaries: piece is a right-cap if next piece is gap,
             * left-cap if previous piece was gap. */
            for (int g = 0; g < gs->floor_gap_count; g++) {
                int gx = gs->floor_gaps[g];
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
             *        Pieces outside the viewport are discarded by SDL's
             *        internal clipping — no manual culling needed.
             */
            SDL_Rect src = { piece_col * P, piece_row * P, P, P };
            SDL_Rect dst = { tx - cam_x,    ty,            P, P };
            SDL_RenderCopy(gs->renderer, gs->floor_tile, &src, &dst);
        }
    }

    /*
     * Draw floating platforms above the floor and pillar layer but below
     * bouncepads and entities, so they read as mid-air surfaces.
     */
    float_platforms_render(gs->float_platforms, gs->float_platform_count,
                           gs->renderer, gs->float_platform_tex, cam_x);

    /* Draw ground spikes on the floor surface, same layer as platforms */
    spike_rows_render(gs->spike_rows, gs->spike_row_count,
                      gs->renderer, gs->spike_tex, cam_x);

    /* Draw spike platforms in the same layer as float platforms */
    spike_platforms_render(gs->spike_platforms, gs->spike_platform_count,
                           gs->renderer, gs->spike_platform_tex, cam_x);

    /* Draw bridges in the same layer as float platforms */
    bridges_render(gs->bridges, gs->bridge_count,
                   gs->renderer, gs->bridge_tex, cam_x);

    /*
     * Draw bouncepads between the platform pillars and vine decorations.
     * This places them visually on the floor surface, with vines potentially
     * overlapping the edges for a natural overgrown look.
     */
    /* Render each bouncepad variant with its own texture */
    bouncepads_render(gs->bouncepads_medium, gs->bouncepad_medium_count,
                      gs->renderer, gs->bouncepad_medium_tex, cam_x);
    if (gs->bouncepad_small_tex) {
        bouncepads_render(gs->bouncepads_small, gs->bouncepad_small_count,
                          gs->renderer, gs->bouncepad_small_tex, cam_x);
    }
    if (gs->bouncepad_high_tex) {
        bouncepads_render(gs->bouncepads_high, gs->bouncepad_high_count,
                          gs->renderer, gs->bouncepad_high_tex, cam_x);
    }
                      
    /*
     * Draw rail tracks before vines and entities so rail tiles appear
     * behind all game objects — the track is part of the background layer.
     */
    if (gs->rail_tex) {
        rail_render(gs->rails, gs->rail_count,
                    gs->renderer, gs->rail_tex, cam_x);
    }

    /* Draw vine decorations on ground and platform tops, behind entities */
    if (gs->vine_green_tex || gs->vine_brown_tex) {
        vine_render(gs->vines, gs->vine_count,
                    gs->renderer, gs->vine_green_tex, gs->vine_brown_tex, cam_x);
    }

    /* Draw ladders and ropes in the same layer as vines */
    if (gs->ladder_tex) {
        ladder_render(gs->ladders, gs->ladder_count,
                      gs->renderer, gs->ladder_tex, cam_x);
    }
    if (gs->rope_tex) {
        rope_render(gs->ropes, gs->rope_count,
                    gs->renderer, gs->rope_tex, cam_x);
    }

    /* Draw coins on top of the platforms, before the water and player */
    coins_render(gs->coins, gs->coin_count,
                 gs->renderer, gs->coin_tex, cam_x);

    /* Draw star yellows alongside coins — same layer, same visibility */
    star_yellows_render(gs->star_yellows, gs->star_yellow_count,
                     gs->renderer, gs->star_yellow_tex, cam_x);

    /* Draw star greens — same mechanics and display size as yellow stars */
    star_greens_render(gs->star_greens, gs->star_green_count,
                      gs->renderer, gs->star_green_tex, cam_x);

    /* Draw star reds — same mechanics and display size as yellow stars */
    star_reds_render(gs->star_reds, gs->star_red_count,
                      gs->renderer, gs->star_red_tex, cam_x);

    /* Draw the end-of-level last star using its dedicated sprite */
    last_star_render(&gs->last_star, gs->renderer,
                     gs->last_star_tex, cam_x);

    /* Draw blue flames behind the water and fish, in front of ground */
    blue_flames_render(gs->blue_flames, gs->blue_flame_count,
                  gs->renderer, gs->blue_flame_tex, cam_x);
    /* Draw fire flames in the same layer (fire variant texture) */
    blue_flames_render(gs->fire_flames, gs->fire_flame_count,
                  gs->renderer, gs->fire_flame_tex, cam_x);

    /* Draw fish behind the water strip (submerged look) but in front of
     * the ground, so the water wave art occludes the submerged portion. */
    fish_render(gs->fish, gs->fish_count,
            gs->renderer, gs->fish_tex, cam_x);
    /* Draw faster fish in the same layer as regular fish */
    faster_fish_render(gs->faster_fish, gs->faster_fish_count,
                       gs->renderer, gs->faster_fish_tex, cam_x);

    /*
     * Draw the water strip on top of the floor/platforms and fish.
     * The full 384-px sheet scrolls rightward as a seamless loop.
     */
    if (gs->water_enabled) water_render(&gs->water, gs->renderer);

    /* Draw spike blocks above the water strip but below the player */
    if (gs->spike_block_tex) {
        spike_blocks_render(gs->spike_blocks, gs->spike_block_count,
                            gs->renderer, gs->spike_block_tex, cam_x);
    }

    /* Draw axe traps above spike blocks and water, before spiders */
    axe_traps_render(gs->axe_traps, gs->axe_trap_count,
                     gs->renderer, gs->axe_trap_tex, cam_x);

    /* Draw circular saws in the same hazard layer as axe traps */
    circular_saws_render(gs->circular_saws, gs->circular_saw_count,
                         gs->renderer, gs->circular_saw_tex, cam_x);

    /* Draw spiders on top of the water strip, before the player */
    spiders_render(gs->spiders, gs->spider_count,
                   gs->renderer, gs->spider_tex, cam_x);
    /* Draw jumping spiders in the same layer as regular spiders */
    jumping_spiders_render(gs->jumping_spiders, gs->jumping_spider_count,
                           gs->renderer, gs->jumping_spider_tex, cam_x);

    /* Draw birds in the sky, in front of spiders but behind the player */
    birds_render(gs->birds, gs->bird_count,
                 gs->renderer, gs->bird_tex, cam_x);
    faster_birds_render(gs->faster_birds, gs->faster_bird_count,
                        gs->renderer, gs->faster_bird_tex, cam_x);

    /* Draw the player sprite on top of everything */
    player_render(&gs->player, gs->renderer, cam_x);

    /* Draw fog/mist as the topmost layer — rendered after the player.
     * Only active when the level definition enables fog (fog_enabled == 1). */
    if (gs->fog_enabled) fog_render(&gs->fog, gs->renderer);

    /* Draw the HUD overlay on top of everything (hearts, lives, score) */
    hud_render(&gs->hud, gs->renderer,
               gs->hearts, gs->lives, gs->score);

    /* Draw debug overlays (collision boxes, FPS, event log) if active */
    if (gs->debug_mode) {
        debug_render(&gs->debug, gs->hud.font, gs->renderer, gs, cam_x);
    }

    /*
     * Gamepad init HUD message — shown while the background thread is running.
     *
     * The texture is pre-rendered once when the thread starts (state 1→2
     * in the state machine below) and reused each frame until init completes
     * (state 2→0).  This avoids calling TTF_RenderText_Solid and uploading
     * to GPU memory on every frame for the duration of the init window.
     */
    if (gs->ctrl_pending_init == 2 && gs->ctrl_init_msg_tex) {
        int tw, th;
        SDL_QueryTexture(gs->ctrl_init_msg_tex, NULL, NULL, &tw, &th);
        /* Bottom-right corner, 6 px from each edge. */
        SDL_Rect dst = { GAME_W - tw - 6, GAME_H - th - 6, tw, th };
        SDL_RenderCopy(gs->renderer, gs->ctrl_init_msg_tex, NULL, &dst);
    }

    /* Level complete overlay — rendered last on top of everything */
    if (gs->level_complete) {
        render_level_complete_overlay(gs);
    }

    /*
     * SDL_RenderPresent — swap the back buffer to the screen.
     * Everything drawn since RenderClear was on a hidden buffer.
     * This call makes it visible instantly, preventing flicker.
     * With VSync enabled, this call also blocks until the monitor
     * is ready for the next frame (typically ~16ms at 60 Hz).
     */
    SDL_RenderPresent(gs->renderer);
}
