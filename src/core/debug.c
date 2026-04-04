/*
 * debug.c — Implementation of the debug overlay: collision boxes, FPS
 *           counter, and scrolling event log.
 *
 * This file includes game.h to access GameState and all entity types.
 * The public debug_render function receives GameState as a void pointer
 * (to avoid a circular include in debug.h) and casts it here.
 */

#include <SDL.h>
#include <SDL_ttf.h>
#include <stdio.h>     /* snprintf  */
#include <string.h>    /* memset    */
#include <stdarg.h>    /* va_list, va_start, va_end — for debug_log printf */

/* Platform-specific memory usage */
#if defined(__APPLE__)
#include <mach/mach.h>
#elif defined(__linux__) || defined(__EMSCRIPTEN__)
/* /proc/self/status parsing */
#elif defined(_WIN32)
#include <windows.h>
#include <psapi.h>
#endif

#include "debug.h"
#include "../game.h"   /* GameState, GAME_W, GAME_H, FLOOR_Y, entity types */

/* ------------------------------------------------------------------ */
/* Platform-specific memory query                                      */
/* ------------------------------------------------------------------ */

/*
 * get_resident_mb — Return the process's resident memory in megabytes.
 * Uses platform-specific APIs; returns 0 on unsupported platforms.
 */
static float get_resident_mb(void)
{
#if defined(__APPLE__)
    struct mach_task_basic_info info;
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                  (task_info_t)&info, &count) == KERN_SUCCESS)
        return (float)info.resident_size / (1024.0f * 1024.0f);
#elif defined(__linux__)
    FILE *f = fopen("/proc/self/status", "r");
    if (f) {
        char line[128];
        while (fgets(line, sizeof(line), f)) {
            long kb;
            if (sscanf(line, "VmRSS: %ld kB", &kb) == 1) {
                fclose(f);
                return (float)kb / 1024.0f;
            }
        }
        fclose(f);
    }
#elif defined(_WIN32)
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
        return (float)pmc.WorkingSetSize / (1024.0f * 1024.0f);
#endif
    return 0.0f;
}

/* VINE_STEP is now defined in vine.h (included via game.h). */

/* ------------------------------------------------------------------ */
/* Static helpers                                                      */
/* ------------------------------------------------------------------ */

/*
 * render_debug_text — Draw a string at (x, y) using the given font and colour.
 *
 * Works identically to the render_text helper in hud.c but accepts an
 * SDL_Color parameter so debug text can be drawn in different colours
 * (green for FPS, white for log entries).
 *
 * TTF_RenderText_Solid rasterises the text into an 8-bit surface with
 * transparent background — fast and pixel-crisp.  The per-frame
 * surface+texture allocation is negligible for the small number of
 * debug strings drawn each frame.
 */
static void render_debug_text(TTF_Font *font, SDL_Renderer *renderer,
                              const char *text, int x, int y,
                              SDL_Color colour)
{
    SDL_Surface *surf = TTF_RenderText_Solid(font, text, colour);
    if (!surf) return;

    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
    if (tex) {
        SDL_Rect dst = { x, y, surf->w, surf->h };
        SDL_RenderCopy(renderer, tex, NULL, &dst);
        SDL_DestroyTexture(tex);
    }
    SDL_FreeSurface(surf);
}

/* ------------------------------------------------------------------ */

/*
 * draw_collision_boxes — Outline every entity's collision hitbox.
 *
 * For each entity type we compute the same SDL_Rect used by the
 * collision code in game.c, subtract cam_x to convert from world
 * coordinates to screen coordinates, then draw an unfilled rectangle
 * with SDL_RenderDrawRect.  Each entity type gets a distinct colour
 * so they are easy to tell apart at a glance.
 */
static void draw_collision_boxes(SDL_Renderer *renderer,
                                 const GameState *gs, int cam_x)
{
    SDL_Rect r;

    /* ---- Player — green (0, 255, 0) -------------------------------- */
    /*
     * player_get_hitbox returns the inset physics box used by all
     * collision checks in game.c (padded from the 48×48 sprite frame).
     */
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    r = player_get_hitbox(&gs->player);
    r.x -= cam_x;
    SDL_RenderDrawRect(renderer, &r);

    /* ---- Floor gaps — dark blue (0, 50, 200) ---------------------------- */
    /*
     * Each floor kill zone is FLOOR_GAP_W wide, drawn at the water surface
     * (FLOOR_Y + 16).  The box height matches the Water.png art band.
     */
    SDL_SetRenderDrawColor(renderer, 0, 50, 200, 255);
    for (int g = 0; g < gs->floor_gap_count; g++) {
        r = (SDL_Rect){ gs->floor_gaps[g] - cam_x, GAME_H - WATER_ART_H,
                        FLOOR_GAP_W, WATER_ART_H };
        SDL_RenderDrawRect(renderer, &r);
    }

    /* ---- Platforms — blue (0, 100, 255) ----------------------------- */
    SDL_SetRenderDrawColor(renderer, 0, 100, 255, 255);
    for (int i = 0; i < gs->platform_count; i++) {
        const Platform *p = &gs->platforms[i];
        r = (SDL_Rect){ (int)p->x - cam_x, (int)p->y, p->w, p->h };
        SDL_RenderDrawRect(renderer, &r);
    }

    /* ---- Float platforms — teal (0, 200, 180) ------------------------- */
    /*
     * Draw the bounding rect for each active float platform.
     * Uses float_platform_get_rect for world-space coords.
     */
    SDL_SetRenderDrawColor(renderer, 0, 200, 180, 255);
    for (int i = 0; i < gs->float_platform_count; i++) {
        if (!gs->float_platforms[i].active) continue;
        r = float_platform_get_rect(&gs->float_platforms[i]);
        r.x -= cam_x;
        SDL_RenderDrawRect(renderer, &r);
    }

    /* ---- Bridge bricks — brown (180, 120, 60) -------------------------- */
    SDL_SetRenderDrawColor(renderer, 180, 120, 60, 255);
    for (int i = 0; i < gs->bridge_count; i++) {
        const Bridge *br = &gs->bridges[i];
        for (int j = 0; j < br->brick_count; j++) {
            const BridgeBrick *bk = &br->bricks[j];
            if (!bk->active) continue;
            r = (SDL_Rect){
                (int)br->x + j * BRIDGE_TILE_W - cam_x,
                (int)(br->base_y + bk->y_offset),
                BRIDGE_TILE_W, BRIDGE_TILE_H
            };
            SDL_RenderDrawRect(renderer, &r);
        }
    }

    /* ---- Coins (active only) — yellow (255, 255, 0) ----------------- */
    SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
    for (int i = 0; i < gs->coin_count; i++) {
        if (!gs->coins[i].active) continue;
        r = (SDL_Rect){
            (int)gs->coins[i].x - cam_x, (int)gs->coins[i].y,
            COIN_DISPLAY_W, COIN_DISPLAY_H
        };
        SDL_RenderDrawRect(renderer, &r);
    }

    /* ---- Star yellows (active only) — magenta (255, 0, 255) ---------- */
    SDL_SetRenderDrawColor(renderer, 255, 0, 255, 255);
    for (int i = 0; i < gs->star_yellow_count; i++) {
        if (!gs->star_yellows[i].active) continue;
        r = (SDL_Rect){
            (int)gs->star_yellows[i].x - cam_x, (int)gs->star_yellows[i].y,
            STAR_YELLOW_DISPLAY_W, STAR_YELLOW_DISPLAY_H
        };
        SDL_RenderDrawRect(renderer, &r);
    }

    /* ---- Star greens (active only) — green (0, 200, 0) -------------- */
    SDL_SetRenderDrawColor(renderer, 0, 200, 0, 255);
    for (int i = 0; i < gs->star_green_count; i++) {
        if (!gs->star_greens[i].active) continue;
        r = (SDL_Rect){
            (int)gs->star_greens[i].x - cam_x, (int)gs->star_greens[i].y,
            STAR_YELLOW_DISPLAY_W, STAR_YELLOW_DISPLAY_H
        };
        SDL_RenderDrawRect(renderer, &r);
    }

    /* ---- Star reds (active only) — dark red (200, 0, 0) ------------- */
    SDL_SetRenderDrawColor(renderer, 200, 0, 0, 255);
    for (int i = 0; i < gs->star_red_count; i++) {
        if (!gs->star_reds[i].active) continue;
        r = (SDL_Rect){
            (int)gs->star_reds[i].x - cam_x, (int)gs->star_reds[i].y,
            STAR_YELLOW_DISPLAY_W, STAR_YELLOW_DISPLAY_H
        };
        SDL_RenderDrawRect(renderer, &r);
    }

    /* ---- Spiders — red (255, 0, 0) ---------------------------------- */
    /*
     * Spider hitbox matches game.c: the visible art occupies rows 22..31
     * of each 64-px frame slot, giving SPIDER_ART_H = 10 px of height.
     * The spider walks on the floor, so its top edge is FLOOR_Y - ART_H.
     */
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    for (int i = 0; i < gs->spider_count; i++) {
        const Spider *s = &gs->spiders[i];
        r = (SDL_Rect){
            (int)s->x + SPIDER_ART_X - cam_x,
            FLOOR_Y - SPIDER_ART_H,
            SPIDER_ART_W,
            SPIDER_ART_H
        };
        SDL_RenderDrawRect(renderer, &r);
    }

    /* ---- Spider patrol ranges — dim red (180, 0, 0) ----------------- */
    /*
     * Draw a thin horizontal line from patrol_x0 to patrol_x1 at the
     * spider's walking height to show the full patrol coverage.
     */
    SDL_SetRenderDrawColor(renderer, 180, 0, 0, 255);
    for (int i = 0; i < gs->spider_count; i++) {
        const Spider *s = &gs->spiders[i];
        int py = FLOOR_Y - SPIDER_ART_H / 2;   /* midpoint of spider art */
        /*
         * The art doesn't start at x=0 of the frame — it starts at
         * SPIDER_ART_X.  The spider reverses when x + SPIDER_FRAME_W
         * hits patrol_x1, so the rightmost art edge is
         * patrol_x1 − SPIDER_FRAME_W + SPIDER_ART_X + SPIDER_ART_W.
         */
        int left  = (int)s->patrol_x0 + SPIDER_ART_X;
        int right = (int)s->patrol_x1 - SPIDER_FRAME_W + SPIDER_ART_X + SPIDER_ART_W;
        SDL_RenderDrawLine(renderer,
                           left  - cam_x, py,
                           right - cam_x, py);
    }

    /* ---- Jumping spiders — magenta (255, 0, 180) --------------------- */
    SDL_SetRenderDrawColor(renderer, 255, 0, 180, 255);
    for (int i = 0; i < gs->jumping_spider_count; i++) {
        const JumpingSpider *js = &gs->jumping_spiders[i];
        r = (SDL_Rect){
            (int)js->x + JSPIDER_ART_X - cam_x,
            FLOOR_Y - JSPIDER_ART_H + (int)js->y,
            JSPIDER_ART_W,
            JSPIDER_ART_H
        };
        SDL_RenderDrawRect(renderer, &r);
    }

    /* ---- Jumping spider patrol ranges — dim magenta (180, 0, 120) --- */
    SDL_SetRenderDrawColor(renderer, 180, 0, 120, 255);
    for (int i = 0; i < gs->jumping_spider_count; i++) {
        const JumpingSpider *js = &gs->jumping_spiders[i];
        int jpy = FLOOR_Y - JSPIDER_ART_H / 2;
        int left  = (int)js->patrol_x0 + JSPIDER_ART_X;
        int right = (int)js->patrol_x1 - JSPIDER_FRAME_W + JSPIDER_ART_X + JSPIDER_ART_W;
        SDL_RenderDrawLine(renderer,
                           left  - cam_x, jpy,
                           right - cam_x, jpy);
    }

    /* ---- Birds — orange (255, 160, 0) -------------------------------- */
    SDL_SetRenderDrawColor(renderer, 255, 160, 0, 255);
    for (int i = 0; i < gs->bird_count; i++) {
        r = bird_get_hitbox(&gs->birds[i]);
        r.x -= cam_x;
        SDL_RenderDrawRect(renderer, &r);
    }

    /* ---- Bird patrol ranges — dim orange (180, 120, 0) --------------- */
    SDL_SetRenderDrawColor(renderer, 180, 120, 0, 255);
    for (int i = 0; i < gs->bird_count; i++) {
        const Bird *b = &gs->birds[i];
        int left  = (int)b->patrol_x0 + BIRD_ART_X;
        int right = (int)b->patrol_x1 - BIRD_FRAME_W + BIRD_ART_X + BIRD_ART_W;
        int py    = (int)b->base_y + BIRD_ART_H / 2;
        SDL_RenderDrawLine(renderer,
                           left  - cam_x, py,
                           right - cam_x, py);
    }

    /* ---- Faster birds — pink (255, 100, 200) ------------------------- */
    SDL_SetRenderDrawColor(renderer, 255, 100, 200, 255);
    for (int i = 0; i < gs->faster_bird_count; i++) {
        r = faster_bird_get_hitbox(&gs->faster_birds[i]);
        r.x -= cam_x;
        SDL_RenderDrawRect(renderer, &r);
    }

    /* ---- Faster bird patrol ranges — dim pink (180, 70, 140) --------- */
    SDL_SetRenderDrawColor(renderer, 180, 70, 140, 255);
    for (int i = 0; i < gs->faster_bird_count; i++) {
        const FasterBird *fb = &gs->faster_birds[i];
        int left  = (int)fb->patrol_x0 + FBIRD_ART_X;
        int right = (int)fb->patrol_x1 - FBIRD_FRAME_W + FBIRD_ART_X + FBIRD_ART_W;
        int py    = (int)fb->base_y + FBIRD_ART_H / 2;
        SDL_RenderDrawLine(renderer,
                           left  - cam_x, py,
                           right - cam_x, py);
    }

    /* ---- Fish — light red (255, 50, 50) ----------------------------- */
    /*
     * fish_get_hitbox returns an inset collision box (8 px padding
     * on each side of the 48×48 frame).
     */
    SDL_SetRenderDrawColor(renderer, 255, 50, 50, 255);
    for (int i = 0; i < gs->fish_count; i++) {
        r = fish_get_hitbox(&gs->fish[i]);
        r.x -= cam_x;
        SDL_RenderDrawRect(renderer, &r);
    }

    /* ---- Fish patrol ranges — dim red (180, 50, 50) ----------------- */
    /*
     * Horizontal line from patrol_x0 to patrol_x1 at the fish's resting
     * water_y level.  Shows the full area the fish can cover.
     */
    SDL_SetRenderDrawColor(renderer, 180, 50, 50, 255);
    for (int i = 0; i < gs->fish_count; i++) {
        const Fish *f = &gs->fish[i];
        int fy = (int)f->water_y + 24;   /* midpoint of 48-px frame */
        /*
         * The fish reverses when x + FISH_RENDER_W hits patrol_x1,
         * so the rightmost hitbox edge is patrol_x1 − FISH_HITBOX_PAD_X.
         * The leftmost hitbox edge is patrol_x0 + FISH_HITBOX_PAD_X.
         */
        int left  = (int)f->patrol_x0 + FISH_HITBOX_PAD_X;
        int right = (int)f->patrol_x1 - FISH_HITBOX_PAD_X;
        SDL_RenderDrawLine(renderer,
                           left  - cam_x, fy,
                           right - cam_x, fy);
    }

    /* ---- Spike blocks (active only) — orange (255, 140, 0) --------- */
    SDL_SetRenderDrawColor(renderer, 255, 140, 0, 255);
    for (int i = 0; i < gs->spike_block_count; i++) {
        if (!gs->spike_blocks[i].active) continue;
        r = spike_block_get_hitbox(&gs->spike_blocks[i]);
        r.x -= cam_x;
        SDL_RenderDrawRect(renderer, &r);
    }

    /* ---- Axe traps (active only) — dark red (200, 0, 50) ------------- */
    /*
     * Draw the rotated blade hitbox returned by axe_trap_get_hitbox.
     * The hitbox follows the blade's current position (trig-based),
     * so the debug rect swings/spins with the axe animation.
     * Also draw a small cross at the pivot point (top of the handle).
     */
    SDL_SetRenderDrawColor(renderer, 200, 0, 50, 255);
    for (int i = 0; i < gs->axe_trap_count; i++) {
        if (!gs->axe_traps[i].active) continue;
        r = axe_trap_get_hitbox(&gs->axe_traps[i]);
        r.x -= cam_x;
        SDL_RenderDrawRect(renderer, &r);

        /* Pivot cross — 6 px arms centred on the axe's mount point */
        int px = (int)gs->axe_traps[i].x - cam_x;
        int py = (int)gs->axe_traps[i].y + 4;   /* matches render pivot offset */
        SDL_RenderDrawLine(renderer, px - 3, py, px + 3, py);
        SDL_RenderDrawLine(renderer, px, py - 3, px, py + 3);
    }

    /* ---- Spike rows (active only) — bright yellow-red (220, 180, 0) -- */
    SDL_SetRenderDrawColor(renderer, 220, 180, 0, 255);
    for (int i = 0; i < gs->spike_row_count; i++) {
        if (!gs->spike_rows[i].active) continue;
        r = spike_row_get_rect(&gs->spike_rows[i]);
        r.x -= cam_x;
        SDL_RenderDrawRect(renderer, &r);
    }

    /* ---- Spike platforms (active only) — dark magenta (180, 0, 120) - */
    SDL_SetRenderDrawColor(renderer, 180, 0, 120, 255);
    for (int i = 0; i < gs->spike_platform_count; i++) {
        if (!gs->spike_platforms[i].active) continue;
        r = spike_platform_get_rect(&gs->spike_platforms[i]);
        r.x -= cam_x;
        SDL_RenderDrawRect(renderer, &r);
    }

    /* ---- Blue flames (visible only) — fiery orange-red (255, 80, 0) -- */
    /*
     * Draw the inset hitbox for each blue flame that is currently visible
     * (not in WAITING state).  Skipping hidden blue flames avoids drawing
     * debug rects below the floor where the blue flame is resting.
     */
    SDL_SetRenderDrawColor(renderer, 255, 80, 0, 255);
    for (int i = 0; i < gs->blue_flame_count; i++) {
        if (!gs->blue_flames[i].active) continue;
        if (gs->blue_flames[i].state == BLUE_FLAME_WAITING) continue;
        r = blue_flame_get_hitbox(&gs->blue_flames[i]);
        r.x -= cam_x;
        SDL_RenderDrawRect(renderer, &r);
    }

    /* ---- Fire flames (visible only) — warm orange-red (255, 120, 0) -- */
    /*
     * Draw the inset hitbox for each fire flame that is currently visible
     * (not in WAITING state).  Uses the same BlueFlame struct and hitbox
     * function as blue flames but with a different debug colour.
     */
    SDL_SetRenderDrawColor(renderer, 255, 120, 0, 255);
    for (int i = 0; i < gs->fire_flame_count; i++) {
        if (!gs->fire_flames[i].active) continue;
        if (gs->fire_flames[i].state == BLUE_FLAME_WAITING) continue;
        r = blue_flame_get_hitbox(&gs->fire_flames[i]);
        r.x -= cam_x;
        SDL_RenderDrawRect(renderer, &r);
    }

    /* ---- Circular saws (active only) — bright orange (255, 140, 0) --- */
    /*
     * Draw the inset hitbox returned by circular_saw_get_hitbox.
     * The hitbox is slightly smaller than the display rect to account
     * for the transparent corners of the circular blade sprite.
     */
    SDL_SetRenderDrawColor(renderer, 255, 140, 0, 255);
    for (int i = 0; i < gs->circular_saw_count; i++) {
        if (!gs->circular_saws[i].active) continue;
        r = circular_saw_get_hitbox(&gs->circular_saws[i]);
        r.x -= cam_x;
        SDL_RenderDrawRect(renderer, &r);
    }

    /* ---- Faster fish — light magenta (220, 100, 180) ---------------- */
    SDL_SetRenderDrawColor(renderer, 220, 100, 180, 255);
    for (int i = 0; i < gs->faster_fish_count; i++) {
        r = faster_fish_get_hitbox(&gs->faster_fish[i]);
        r.x -= cam_x;
        SDL_RenderDrawRect(renderer, &r);
    }

    /* ---- Last star — gold (255, 215, 0) ----------------------------- */
    if (gs->last_star.active) {
        SDL_SetRenderDrawColor(renderer, 255, 215, 0, 255);
        r = last_star_get_hitbox(&gs->last_star);
        r.x -= cam_x;
        SDL_RenderDrawRect(renderer, &r);
    }

    /* ---- Ladders — brown-orange (180, 120, 60) ---------------------- */
    SDL_SetRenderDrawColor(renderer, 180, 120, 60, 255);
    for (int i = 0; i < gs->ladder_count; i++) {
        const LadderDecor *ld = &gs->ladders[i];
        int lh = (ld->tile_count - 1) * LADDER_STEP + LADDER_H;
        r = (SDL_Rect){ (int)ld->x - cam_x, (int)ld->y, LADDER_W, lh };
        SDL_RenderDrawRect(renderer, &r);
    }

    /* ---- Ropes — tan (200, 160, 100) -------------------------------- */
    SDL_SetRenderDrawColor(renderer, 200, 160, 100, 255);
    for (int i = 0; i < gs->rope_count; i++) {
        const RopeDecor *rp = &gs->ropes[i];
        int rh = (rp->tile_count - 1) * ROPE_STEP + ROPE_H;
        r = (SDL_Rect){ (int)rp->x - cam_x, (int)rp->y, ROPE_W, rh };
        SDL_RenderDrawRect(renderer, &r);
    }

    /* ---- Bouncepads — medium=cyan, small=green, high=red ------------- */
    SDL_SetRenderDrawColor(renderer, 0, 255, 255, 255);
    for (int i = 0; i < gs->bouncepad_medium_count; i++) {
        const Bouncepad *bp = &gs->bouncepads_medium[i];
        r = (SDL_Rect){ (int)bp->x + BOUNCEPAD_ART_X - cam_x, (int)bp->y,
                        BOUNCEPAD_ART_W, bp->h };
        SDL_RenderDrawRect(renderer, &r);
    }
    SDL_SetRenderDrawColor(renderer, 0, 200, 0, 255);
    for (int i = 0; i < gs->bouncepad_small_count; i++) {
        const Bouncepad *bp = &gs->bouncepads_small[i];
        r = (SDL_Rect){ (int)bp->x + BOUNCEPAD_ART_X - cam_x, (int)bp->y,
                        BOUNCEPAD_ART_W, bp->h };
        SDL_RenderDrawRect(renderer, &r);
    }
    SDL_SetRenderDrawColor(renderer, 255, 50, 50, 255);
    for (int i = 0; i < gs->bouncepad_high_count; i++) {
        const Bouncepad *bp = &gs->bouncepads_high[i];
        r = (SDL_Rect){ (int)bp->x + BOUNCEPAD_ART_X - cam_x, (int)bp->y,
                        BOUNCEPAD_ART_W, bp->h };
        SDL_RenderDrawRect(renderer, &r);
    }

    /* ---- Vines — dark green (0, 180, 0) ----------------------------- */
    /*
     * Show the vine grab zone — the 24 px-wide area (VINE_W + 2 × 4 px
     * padding) that the player can interact with to start climbing.
     * The total height spans tile_count tiles stepped by VINE_STEP,
     * plus the remaining VINE_H of the last tile.
     */
    #define VINE_GRAB_PAD_DBG 4   /* must match VINE_GRAB_PAD in player.c */
    SDL_SetRenderDrawColor(renderer, 0, 180, 0, 255);
    for (int i = 0; i < gs->vine_count; i++) {
        const VineDecor *v = &gs->vines[i];
        int vine_total_h = (v->tile_count - 1) * VINE_STEP + VINE_H;
        int grab_x = (int)v->x - VINE_GRAB_PAD_DBG;
        int grab_w = VINE_W + 2 * VINE_GRAB_PAD_DBG;
        r = (SDL_Rect){ grab_x - cam_x, (int)v->y, grab_w, vine_total_h };
        SDL_RenderDrawRect(renderer, &r);
    }

    /* ---- Rail tiles — purple (160, 80, 255) -------------------------- */
    /*
     * Outline each 16×16 rail tile to show the paths spike blocks ride on.
     * Helps verify rail layout and spot misplaced tiles at a glance.
     */
    SDL_SetRenderDrawColor(renderer, 160, 80, 255, 255);
    for (int i = 0; i < gs->rail_count; i++) {
        const Rail *rl = &gs->rails[i];
        for (int t = 0; t < rl->count; t++) {
            r = (SDL_Rect){
                rl->tiles[t].x - cam_x, rl->tiles[t].y,
                RAIL_TILE_W, RAIL_TILE_H
            };
            SDL_RenderDrawRect(renderer, &r);
        }
    }

    /* ---- HUD elements — white (255, 255, 255) ------------------------ */
    /*
     * HUD items live in screen space (no camera offset).  We replicate
     * the same positioning math from hud_render so the outlines match
     * exactly where each element is drawn.
     */
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    {
        int hud_row_y = HUD_MARGIN;

        /* Hearts — one box per displayed heart */
        for (int i = 0; i < gs->hearts; i++) {
            r = (SDL_Rect){
                HUD_MARGIN + i * (HUD_HEART_SIZE + HUD_HEART_GAP),
                hud_row_y + (HUD_ROW_H - HUD_HEART_SIZE) / 2,
                HUD_HEART_SIZE,
                HUD_HEART_SIZE
            };
            SDL_RenderDrawRect(renderer, &r);
        }

        /* Player icon */
        int icon_x = HUD_MARGIN + MAX_HEARTS * (HUD_HEART_SIZE + HUD_HEART_GAP) + 6;
        r = (SDL_Rect){
            icon_x,
            hud_row_y + (HUD_ROW_H - HUD_ICON_H) / 2,
            HUD_ICON_W, HUD_ICON_H
        };
        SDL_RenderDrawRect(renderer, &r);

        /* Lives text — measure width to get the bounding box */
        {
            char lives_buf[8];
            snprintf(lives_buf, sizeof(lives_buf), "x%d", gs->lives);
            int lw = 0;
            TTF_SizeText(gs->hud.font, lives_buf, &lw, NULL);
            r = (SDL_Rect){
                icon_x + HUD_ICON_W + 4,
                hud_row_y + (HUD_ROW_H - 13) / 2,
                lw, 13
            };
            SDL_RenderDrawRect(renderer, &r);
        }

        /* Score text + coin icon — right-aligned, same formula as hud_render */
        {
            char score_buf[32];
            snprintf(score_buf, sizeof(score_buf), "SCORE: %d", gs->score);
            int sw = 0;
            TTF_SizeText(gs->hud.font, score_buf, &sw, NULL);
            int coin_gap = 3;
            int total_w = sw + coin_gap + HUD_COIN_ICON_SIZE;
            int score_x = GAME_W - HUD_MARGIN - total_w;
            /* Text hitbox */
            r = (SDL_Rect){
                score_x,
                hud_row_y + (HUD_ROW_H - 13) / 2,
                sw, 13
            };
            SDL_RenderDrawRect(renderer, &r);
            /* Coin icon hitbox */
            r = (SDL_Rect){
                score_x + sw + coin_gap,
                hud_row_y + (HUD_ROW_H - HUD_COIN_ICON_SIZE) / 2,
                HUD_COIN_ICON_SIZE, HUD_COIN_ICON_SIZE
            };
            SDL_RenderDrawRect(renderer, &r);
        }
    }

    /*
     * Restore the draw colour to white (already set above for HUD boxes,
     * but explicit for clarity).
     */
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
}

/* ------------------------------------------------------------------ */

/*
 * draw_fps — Render the FPS counter at the bottom-right of the screen.
 *
 * Right-aligned using TTF_SizeText to measure the rendered width —
 * same technique as the score display in hud.c.
 * Drawn in green to distinguish from normal HUD text.
 */
static void draw_performance(const DebugOverlay *dbg, TTF_Font *font,
                             SDL_Renderer *renderer)
{
    SDL_Color green  = {   0, 255,   0, 255 };
    SDL_Color yellow = { 255, 255,   0, 255 };
    SDL_Color red    = { 255,  80,  80, 255 };
    SDL_Color cyan   = { 100, 220, 255, 255 };

    int y = GAME_H - 52;  /* start above the bottom edge */
    char buf[48];
    int text_w;

    /* FPS counter — left-aligned, green if 55+, yellow if 30-54, red if <30 */
    snprintf(buf, sizeof(buf), "FPS: %d", dbg->fps_display);
    SDL_Color fps_col = dbg->fps_display >= 55 ? green
                      : dbg->fps_display >= 30 ? yellow : red;
    render_debug_text(font, renderer, buf, HUD_MARGIN, y, fps_col);
    y += 13;

    /* Frame time + CPU budget */
    snprintf(buf, sizeof(buf), "CPU: %.1fms (%.0f%%)",
             (double)dbg->frame_ms_display, (double)dbg->cpu_percent);
    SDL_Color cpu_col = dbg->frame_ms_display < 12.0f ? green
                      : dbg->frame_ms_display < 16.7f ? yellow : red;
    render_debug_text(font, renderer, buf, HUD_MARGIN, y, cpu_col);
    y += 13;

    /* Memory usage */
    if (dbg->mem_mb > 0.0f) {
        snprintf(buf, sizeof(buf), "MEM: %.1f MB", (double)dbg->mem_mb);
        render_debug_text(font, renderer, buf, HUD_MARGIN, y, cyan);
    }

    (void)text_w; /* suppress unused warning */
}

/* ------------------------------------------------------------------ */

/*
 * draw_player_state — Show player physics info below the FPS counter.
 *
 * Displays velocity, animation state, ground status, and hurt timer
 * so you can read the player's internal state while playing.
 * Positioned at the bottom-right, stacking upward from the FPS line.
 */
static void draw_player_state(const Player *player, TTF_Font *font,
                               SDL_Renderer *renderer, int cam_x)
{
    SDL_Color white = { 255, 255, 255, 255 };
    char buf[64];
    int text_w = 0;
    const int line_h = 14;   /* 13 px font + 1 px gap */

    /* State names matching the AnimState enum order */
    static const char *state_names[] = { "IDLE", "WALK", "JUMP", "FALL", "CLIMB" };
    const char *state_str = state_names[player->anim_state];

    /* Line 1 (bottom): velocity */
    snprintf(buf, sizeof(buf), "vx:%.0f vy:%.0f", player->vx, player->vy);
    TTF_SizeText(font, buf, &text_w, NULL);
    render_debug_text(font, renderer, buf,
                      GAME_W - HUD_MARGIN - text_w, GAME_H - 20 - line_h, white);

    /* Line 2: state + ground + facing + climb source */
    {
        const char *climb_str = "";
        if (player->on_vine) {
            static const char *climb_names[] = { " VINE", " LADDER", " ROPE" };
            climb_str = climb_names[player->climb_source];
        }
        snprintf(buf, sizeof(buf), "%s %s %s%s",
                 state_str,
                 player->on_ground ? "GND" : "AIR",
                 player->facing_left ? "<-" : "->",
                 climb_str);
    }
    TTF_SizeText(font, buf, &text_w, NULL);
    render_debug_text(font, renderer, buf,
                      GAME_W - HUD_MARGIN - text_w, GAME_H - 20 - 2 * line_h, white);

    /* Line 3: hurt timer (only when active) */
    if (player->hurt_timer > 0.0f) {
        snprintf(buf, sizeof(buf), "HURT:%.1fs", player->hurt_timer);
        TTF_SizeText(font, buf, &text_w, NULL);
        SDL_Color red = { 255, 80, 80, 255 };
        render_debug_text(font, renderer, buf,
                          GAME_W - HUD_MARGIN - text_w, GAME_H - 20 - 3 * line_h, red);
    }

    /* ---- Velocity vector line from the player's centre -------------- */
    /*
     * Draw a short line from the player's hitbox centre in the direction
     * of current velocity.  Scaled down (÷ 4) so it stays visible
     * without overshooting the screen at high speeds.
     */
    {
        SDL_Rect hit = player_get_hitbox(player);
        int cx = hit.x + hit.w / 2 - cam_x;
        int cy = hit.y + hit.h / 2;
        int vx_end = cx + (int)(player->vx / 4.0f);
        int vy_end = cy + (int)(player->vy / 4.0f);
        SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
        SDL_RenderDrawLine(renderer, cx, cy, vx_end, vy_end);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    }
}

/* ------------------------------------------------------------------ */

/*
 * draw_debug_log — Render the scrolling event log at the bottom-left.
 *
 * Messages are stacked bottom-up: the newest entry sits at the bottom
 * and older entries stack above it.  Entries whose age exceeds
 * DEBUG_LOG_DISPLAY_SEC are skipped (effectively faded out).
 *
 * The font is 13 px tall; we add 1 px gap → 14 px per line.
 * Starting y is GAME_H − 20 (safely above the bottom edge).
 */
static void draw_debug_log(const DebugOverlay *dbg, TTF_Font *font,
                           SDL_Renderer *renderer)
{
    if (dbg->log_count == 0) return;

    SDL_Color white   = { 255, 255, 255, 255 };
    SDL_Color dimmed   = { 180, 180, 180, 255 };
    const int line_h   = 14;      /* 13 px font + 1 px gap                  */
    int       y        = GAME_H - 20;
    int       drawn    = 0;

    /*
     * Walk backward from the most recent entry (log_head - 1) toward
     * older entries.  The modulo arithmetic wraps around the circular
     * buffer.  We draw at most log_count entries.
     */
    for (int k = 0; k < dbg->log_count; k++) {
        int idx = (dbg->log_head - 1 - k + DEBUG_LOG_MAX_ENTRIES)
                  % DEBUG_LOG_MAX_ENTRIES;
        const DebugLogEntry *e = &dbg->log[idx];

        /* Skip expired entries */
        if (e->age >= DEBUG_LOG_DISPLAY_SEC) continue;

        /*
         * Entries in their last second (age > 3.0) are drawn dimmed
         * for a subtle fade-out hint before they disappear.
         */
        SDL_Color col = (e->age > DEBUG_LOG_DISPLAY_SEC - 1.0f)
                        ? dimmed : white;
        render_debug_text(font, renderer, e->text,
                          HUD_MARGIN, y - drawn * line_h, col);
        drawn++;
    }
}

/* ================================================================== */
/* Public functions                                                    */
/* ================================================================== */

/*
 * debug_init — Reset all debug overlay state.
 *
 * Called once at game_init time when --debug is active.
 * Sets the FPS baseline tick so the first sample interval is accurate.
 */
void debug_init(DebugOverlay *dbg)
{
    memset(dbg, 0, sizeof(*dbg));
    dbg->fps_prev_ticks = SDL_GetTicks64();
}

/* ------------------------------------------------------------------ */

/*
 * debug_update — Advance FPS measurement and age log entries.
 *
 * Called once per frame (even while paused) so the FPS counter stays
 * accurate and log entries expire on schedule.
 *
 * FPS measurement: count frames between samples.  Every
 * DEBUG_FPS_SAMPLE_MS (500 ms) compute the display value and reset.
 * This gives a stable reading that updates twice per second.
 */
void debug_update(DebugOverlay *dbg, float dt)
{
    /* ---- Frame time (CPU proxy) ------------------------------------ */
    dbg->frame_ms = dt * 1000.0f;

    /* ---- FPS sampling ----------------------------------------------- */
    dbg->fps_frame_count++;

    Uint64 now     = SDL_GetTicks64();
    Uint64 elapsed = now - dbg->fps_prev_ticks;
    if (elapsed >= DEBUG_FPS_SAMPLE_MS) {
        dbg->fps_display     = (int)(dbg->fps_frame_count * 1000 / elapsed);
        dbg->fps_frame_count = 0;
        dbg->fps_prev_ticks  = now;

        /* Smooth the frame time display (average over the sample period) */
        dbg->frame_ms_display = dbg->frame_ms;
        dbg->cpu_percent = dbg->frame_ms_display / 16.667f * 100.0f;

        /* Update memory usage once per sample (avoid per-frame syscalls) */
        dbg->mem_mb = get_resident_mb();
    }

    /* ---- Age log entries -------------------------------------------- */
    for (int i = 0; i < dbg->log_count; i++) {
        dbg->log[i].age += dt;
    }
}

/* ------------------------------------------------------------------ */

/*
 * debug_log — Push a formatted message into the circular log buffer.
 *
 * Uses va_start / vsnprintf / va_end to accept printf-style arguments.
 * Writes into the slot at log_head, resets its age to 0, and advances
 * the head.  When the buffer is full the oldest entry is overwritten.
 */
void debug_log(DebugOverlay *dbg, const char *fmt, ...)
{
    DebugLogEntry *entry = &dbg->log[dbg->log_head];

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(entry->text, DEBUG_LOG_MSG_LEN, fmt, ap);
    va_end(ap);

    entry->age = 0.0f;

    dbg->log_head = (dbg->log_head + 1) % DEBUG_LOG_MAX_ENTRIES;
    if (dbg->log_count < DEBUG_LOG_MAX_ENTRIES)
        dbg->log_count++;
}

/* ------------------------------------------------------------------ */

/*
 * debug_render — Draw all debug overlays.
 *
 * Called once per frame after the HUD and before SDL_RenderPresent.
 * Casts the opaque gs_ptr to const GameState* so we can read entity
 * arrays, counts, and camera position without a circular include.
 */
void debug_render(const DebugOverlay *dbg, TTF_Font *font,
                  SDL_Renderer *renderer, const void *gs_ptr, int cam_x)
{
    const GameState *gs = (const GameState *)gs_ptr;

    draw_collision_boxes(renderer, gs, cam_x);
    draw_performance(dbg, font, renderer);
    draw_player_state(&gs->player, font, renderer, cam_x);
    draw_debug_log(dbg, font, renderer);

    /* "DEBUG MODE" label — centred at the top of the screen */
    {
        const char *label = "DEBUG MODE";
        int label_w = 0;
        TTF_SizeText(font, label, &label_w, NULL);
        SDL_Color yellow = { 255, 255, 0, 255 };
        render_debug_text(font, renderer, label,
                          (GAME_W - label_w) / 2, HUD_MARGIN, yellow);
    }
}
