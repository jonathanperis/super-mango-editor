/*
 * canvas.c — Level canvas renderer for the Super Mango level editor.
 *
 * This is the most complex rendering module in the editor.  It draws the
 * entire 1600x300 game world inside the editor viewport, reproducing every
 * entity at its exact game display size and derived Y position.  The result
 * is a WYSIWYG preview: what designers see here matches the running game.
 *
 * Rendering order (back to front):
 *   1. Sky background
 *   2. Platforms (pillars) — 9-slice tiled
 *   3. Floor — 9-slice tiled with sea gap edge caps
 *   4. Water strips in sea gaps
 *   5. Entities in game render order (rails, surfaces, collectibles,
 *      hazards, enemies)
 *   6. Selection highlight
 *   7. Ghost preview (placement mode)
 *   8. Grid overlay (optional)
 */

#include <SDL.h>
#include <SDL_image.h>  /* IMG_LoadTexture (not used here, but available) */
#include <stdio.h>      /* snprintf for debug labels                      */

#include "editor.h"     /* EditorState, EntityType, CANVAS_W, TOOLBAR_H, etc. */
#include "../game.h"    /* GAME_W, GAME_H, FLOOR_Y, TILE_SIZE, WORLD_W,
                           SEA_GAP_W, MAX_* constants, GRAVITY              */

/* ------------------------------------------------------------------ */
/* Entity display dimensions — extracted from game source headers       */
/* ------------------------------------------------------------------ */
/*
 * These constants mirror the values defined in each entity's header file.
 * They describe how big entities appear on screen in logical pixels.
 * We duplicate them here (rather than including 20+ headers) to keep the
 * editor's include graph shallow and compilation fast.
 */

/* Spider / Jumping Spider — 64-px frame slot, only rows 22..31 visible */
#define SPIDER_FRAME_W     64
#define SPIDER_ART_H       10
#define SPIDER_ART_Y       22   /* source rect crop y offset within frame */

/* Bird / Faster Bird — 48-px frame slot, only rows 17..30 visible */
#define BIRD_FRAME_W       48
#define BIRD_ART_H         14
#define BIRD_ART_Y         17   /* source rect crop y offset within frame */

/* Fish / Faster Fish — rendered at full 48x48 frame */
#define FISH_FRAME_W       48
#define FISH_FRAME_H       48

/* Collectibles — small sprites at logical pixel sizes */
#define COIN_DISPLAY_W     16
#define COIN_DISPLAY_H     16
#define YSTAR_DISPLAY_W    16
#define YSTAR_DISPLAY_H    16
#define LSTAR_DISPLAY_W    24
#define LSTAR_DISPLAY_H    24

/* Axe trap — tall 48x64 frame rendered at pivot point */
#define AXE_FRAME_W        48
#define AXE_FRAME_H        64

/* Circular saw — 32x32 display on screen */
#define SAW_DISPLAY_W      32
#define SAW_DISPLAY_H      32

/* Spike row — 16x16 tiles lined up horizontally on the floor */
#define SPIKE_TILE_W       16
#define SPIKE_TILE_H       16

/* Spike platform — 16-px wide pieces, cropped vertically */
#define SPIKE_PLAT_PIECE_W 16
#define SPIKE_PLAT_SRC_Y    5   /* first content row in each piece  */
#define SPIKE_PLAT_SRC_H   11   /* content height (rows 5-15)       */

/* Spike block — 24x24 display on screen, rides rails */
#define SPIKE_DISPLAY_W    24
#define SPIKE_DISPLAY_H    24

/* Floating platform — 16-px wide pieces, 16 px tall */
#define FPLAT_PIECE_W      16
#define FPLAT_PIECE_H      16

/* Bridge — 16x16 tiles forming a crumble walkway */
#define BRIDGE_TILE_W      16
#define BRIDGE_TILE_H      16

/* Bouncepad — all three variants share the same frame/crop dimensions */
#define BP_SRC_Y           14   /* first non-transparent row in the frame */
#define BP_SRC_H           18   /* height of art region (rows 14-31)      */
#define BP_FRAME_W         48   /* width of one frame slot in the sheet   */

/* Vine — stacked tiles with overlapping vertical step */
#define VINE_W             16
#define VINE_SRC_Y          8   /* content starts at row 8                */
#define VINE_SRC_H         32   /* content height within the tile         */
#define VINE_H             32   /* display height of one tile segment     */
#define VINE_STEP          19   /* vertical spacing between stacked tiles */

/* Ladder — tightly stacked tiles */
#define LADDER_W           16
#define LADDER_SRC_Y       13   /* content starts at row 13               */
#define LADDER_SRC_H       22   /* content height within the tile         */
#define LADDER_H           22   /* display height of one tile segment     */
#define LADDER_STEP         8   /* tight overlap for seamless stacking    */

/* Rope — stacked tiles with vertical step */
#define ROPE_SRC_X          2   /* 2 px padding on left side of source    */
#define ROPE_SRC_Y          6   /* 6 px top padding in source             */
#define ROPE_SRC_W         12   /* source crop width                      */
#define ROPE_SRC_H         36   /* source crop height                     */
#define ROPE_W             12   /* display width                          */
#define ROPE_H             36   /* display height of one tile segment     */
#define ROPE_STEP          34   /* vertical spacing between stacked tiles */

/* Rail — 16x16 tiles forming a closed or open track path */
#define RAIL_TILE_W        16
#define RAIL_TILE_H        16

/* Blue flame — 48x48 display, erupts from sea gaps */
#define BLUE_FLAME_W       48
#define BLUE_FLAME_H       48

/* Player spawn — full 48x48 idle frame */
#define PLAYER_SPAWN_W     48
#define PLAYER_SPAWN_H     48

/* Water art strip height — matches water.h constant */
#define WATER_ART_H        31

/* ------------------------------------------------------------------ */
/* World-to-screen coordinate transform helpers                        */
/* ------------------------------------------------------------------ */

/*
 * w2s_x — Convert a world-space x coordinate to a screen-space x.
 *
 * Subtracts the camera scroll offset and applies the zoom factor so that
 * the visible portion of the 1600-px world maps onto the CANVAS_W-pixel
 * viewport.  The result can be negative (entity is off-screen left).
 */
static inline int w2s_x(const EditorState *es, float wx) {
    return (int)((wx - es->camera.x) * es->camera.zoom);
}

/*
 * w2s_y — Convert a world-space y coordinate to a screen-space y.
 *
 * Applies zoom and adds TOOLBAR_H to offset below the toolbar strip.
 * The y axis is not scrolled (the game world is only 300 px tall and
 * always fits vertically in the canvas at reasonable zoom levels).
 */
static inline int w2s_y(const EditorState *es, float wy) {
    return (int)(wy * es->camera.zoom) + TOOLBAR_H;
}

/*
 * w2s_w / w2s_h — Scale a width or height from world pixels to screen pixels.
 *
 * Ensures that a 1-pixel minimum is returned so thin entities remain visible
 * at extreme zoom-out levels.
 */
static inline int w2s_w(const EditorState *es, int w) {
    int sw = (int)(w * es->camera.zoom);
    return sw < 1 ? 1 : sw;
}

static inline int w2s_h(const EditorState *es, int h) {
    int sh = (int)(h * es->camera.zoom);
    return sh < 1 ? 1 : sh;
}

/* ------------------------------------------------------------------ */
/* Internal rendering helpers — forward declarations                   */
/* ------------------------------------------------------------------ */

static void render_sky(EditorState *es);
static void render_platforms(EditorState *es);
static void render_floor(EditorState *es);
static void render_water(EditorState *es);
static void render_rails(EditorState *es);
static void render_float_platforms(EditorState *es);
static void render_spike_rows(EditorState *es);
static void render_spike_platforms(EditorState *es);
static void render_bridges(EditorState *es);
static void render_bouncepads(EditorState *es);
static void render_vines(EditorState *es);
static void render_ladders(EditorState *es);
static void render_ropes(EditorState *es);
static void render_coins(EditorState *es);
static void render_yellow_stars(EditorState *es);
static void render_last_star(EditorState *es);
static void render_player_spawn(EditorState *es);
static void render_blue_flames(EditorState *es);
static void render_fish(EditorState *es);
static void render_faster_fish(EditorState *es);
static void render_spike_blocks(EditorState *es);
static void render_axe_traps(EditorState *es);
static void render_circular_saws(EditorState *es);
static void render_spiders(EditorState *es);
static void render_jumping_spiders(EditorState *es);
static void render_birds(EditorState *es);
static void render_faster_birds(EditorState *es);
static void render_selection(EditorState *es);
static void render_ghost(EditorState *es);
static void render_grid(EditorState *es);

/* ------------------------------------------------------------------ */
/* Helper — draw an outlined rectangle (selection / ghost highlight)    */
/* ------------------------------------------------------------------ */

/*
 * draw_outline — Draw a rectangular outline with a given line thickness.
 *
 * Uses four SDL_RenderFillRect calls (top, bottom, left, right) instead
 * of SDL_RenderDrawRect, because SDL_RenderDrawRect only draws 1-pixel
 * lines and we need variable thickness for visibility at different zooms.
 */
static void draw_outline(SDL_Renderer *r, int x, int y, int w, int h,
                         int thickness) {
    /* Top edge */
    SDL_Rect top    = { x, y, w, thickness };
    SDL_RenderFillRect(r, &top);
    /* Bottom edge */
    SDL_Rect bottom = { x, y + h - thickness, w, thickness };
    SDL_RenderFillRect(r, &bottom);
    /* Left edge (between top and bottom) */
    SDL_Rect left   = { x, y + thickness, thickness, h - 2 * thickness };
    SDL_RenderFillRect(r, &left);
    /* Right edge */
    SDL_Rect right  = { x + w - thickness, y + thickness, thickness,
                        h - 2 * thickness };
    SDL_RenderFillRect(r, &right);
}

/* ------------------------------------------------------------------ */
/* Helper — draw a filled rect with texture source crop                */
/* ------------------------------------------------------------------ */

/*
 * draw_tex — Draw a texture (or cropped region) at a world position.
 *
 * src can be NULL to draw the entire texture.  wx/wy are world-space
 * coordinates; dw/dh are world-space dimensions that get scaled to screen.
 */
static void draw_tex(EditorState *es, SDL_Texture *tex, const SDL_Rect *src,
                     float wx, float wy, int dw, int dh) {
    if (!tex) return;
    SDL_Rect dst = {
        w2s_x(es, wx),
        w2s_y(es, wy),
        w2s_w(es, dw),
        w2s_h(es, dh)
    };
    SDL_RenderCopy(es->renderer, tex, src, &dst);
}

/* ------------------------------------------------------------------ */
/* canvas_render — Main entry point: draw the full level preview        */
/* ------------------------------------------------------------------ */

void canvas_render(EditorState *es) {
    /*
     * Set a clip rectangle so that nothing we draw bleeds outside the canvas
     * area into the toolbar, status bar, or side panel.
     */
    SDL_Rect clip = { 0, TOOLBAR_H, CANVAS_W, CANVAS_H };
    SDL_RenderSetClipRect(es->renderer, &clip);

    /* ---- Layer 1: Sky background ---- */
    render_sky(es);

    /* ---- Layer 2: Platforms (pillars) — behind the floor ---- */
    render_platforms(es);

    /* ---- Layer 3: Floor (9-slice with sea gap edge caps) ---- */
    render_floor(es);

    /* ---- Layer 4: Water in sea gaps ---- */
    render_water(es);

    /* ---- Layer 5: Entities in game render order (back to front) ---- */

    /* Rail paths — drawn first as background guides */
    render_rails(es);

    /* Surfaces — floating platforms, spikes, bridges, bouncepads */
    render_float_platforms(es);
    render_spike_rows(es);
    render_spike_platforms(es);
    render_bridges(es);
    render_bouncepads(es);

    /* Climbables — vines, ladders, ropes */
    render_vines(es);
    render_ladders(es);
    render_ropes(es);

    /* Collectibles */
    render_coins(es);
    render_yellow_stars(es);
    render_last_star(es);

    /* Player spawn — draw idle frame at the spawn position */
    render_player_spawn(es);

    /* Hazards */
    render_blue_flames(es);
    render_spike_blocks(es);
    render_axe_traps(es);
    render_circular_saws(es);

    /* Enemies (water enemies first, then ground, then air) */
    render_fish(es);
    render_faster_fish(es);
    render_spiders(es);
    render_jumping_spiders(es);
    render_birds(es);
    render_faster_birds(es);

    /* ---- Layer 6: Selection highlight ---- */
    render_selection(es);

    /* ---- Layer 7: Ghost preview (placement mode) ---- */
    render_ghost(es);

    /* ---- Layer 8: Grid overlay (if enabled) ---- */
    if (es->show_grid) {
        render_grid(es);
    }

    /* Remove the clip rectangle so other UI elements can draw freely */
    SDL_RenderSetClipRect(es->renderer, NULL);
}

/* ------------------------------------------------------------------ */
/* canvas_screen_to_world — Reverse transform: screen -> world         */
/* ------------------------------------------------------------------ */

/*
 * Inverts the w2s_x / w2s_y transform.
 *
 *   world_x = screen_x / zoom + camera.x
 *   world_y = (screen_y - TOOLBAR_H) / zoom
 *
 * The caller uses this to map mouse clicks on the canvas to entity
 * positions in the level.
 */
void canvas_screen_to_world(const EditorState *es, int sx, int sy,
                            float *wx, float *wy) {
    *wx = (float)sx / es->camera.zoom + es->camera.x;
    *wy = (float)(sy - TOOLBAR_H) / es->camera.zoom;
}

/* ------------------------------------------------------------------ */
/* canvas_contains — Hit test for the canvas rectangle                 */
/* ------------------------------------------------------------------ */

int canvas_contains(int sx, int sy) {
    return sx >= 0 && sx < CANVAS_W &&
           sy >= TOOLBAR_H && sy < TOOLBAR_H + CANVAS_H;
}

/* ================================================================== */
/* Internal rendering functions — one per visual layer / entity type    */
/* ================================================================== */

/* ---- Sky --------------------------------------------------------- */

/*
 * render_sky — Fill the canvas with a light blue sky colour (#87CEEB).
 *
 * This provides the background behind all world geometry.  The colour
 * approximates a clear daytime sky that matches the game's parallax
 * background tone.
 */
static void render_sky(EditorState *es) {
    SDL_SetRenderDrawColor(es->renderer, 0x87, 0xCE, 0xEB, 0xFF);
    SDL_Rect sky = { 0, TOOLBAR_H, CANVAS_W, CANVAS_H };
    SDL_RenderFillRect(es->renderer, &sky);
}

/* ---- Platforms (pillars) ----------------------------------------- */

/*
 * render_platforms — Draw each ground pillar using 9-slice rendering.
 *
 * Mirrors the game's platforms_render: Grass_Oneway.png is a 48x48 sheet
 * divided into a 3x3 grid of 16x16 pieces.  Each piece has a structural
 * role (top/middle/bottom x left/center/right) and the renderer tiles
 * them to build pillars of arbitrary height.
 *
 * If the platform texture is unavailable, fall back to a solid brown rect.
 */
static void render_platforms(EditorState *es) {
    const int P = TILE_SIZE / 3;  /* 9-slice piece size: 16 px */

    for (int i = 0; i < es->level.platform_count; i++) {
        const PlatformPlacement *pp = &es->level.platforms[i];

        /*
         * Derive world-space y and height from tile_height, matching
         * the game's platform_init formula:
         *   y = FLOOR_Y - tile_height * TILE_SIZE + 16
         *   h = tile_height * TILE_SIZE
         */
        float plat_x = pp->x;
        int   plat_h = pp->tile_height * TILE_SIZE;
        float plat_y = (float)(FLOOR_Y - pp->tile_height * TILE_SIZE + 16);

        if (es->textures.platform) {
            /* 9-slice tiling — walk every 16x16 piece inside the pillar */
            for (int ty = 0; ty < plat_h; ty += P) {
                int piece_row;
                if (ty == 0)               piece_row = 0;  /* top: grass  */
                else if (ty + P >= plat_h) piece_row = 2;  /* bottom: base*/
                else                       piece_row = 1;  /* middle: dirt*/

                for (int tx = 0; tx < TILE_SIZE; tx += P) {
                    int piece_col;
                    if (tx == 0)                  piece_col = 0;  /* left  */
                    else if (tx + P >= TILE_SIZE) piece_col = 2;  /* right */
                    else                          piece_col = 1;  /* center*/

                    SDL_Rect src = { piece_col * P, piece_row * P, P, P };
                    SDL_Rect dst = {
                        w2s_x(es, plat_x + (float)tx),
                        w2s_y(es, plat_y + (float)ty),
                        w2s_w(es, P),
                        w2s_h(es, P)
                    };
                    SDL_RenderCopy(es->renderer, es->textures.platform,
                                   &src, &dst);
                }
            }
        } else {
            /* Fallback: solid brown rectangle */
            SDL_SetRenderDrawColor(es->renderer, 0x8B, 0x6B, 0x4B, 0xFF);
            SDL_Rect dst = {
                w2s_x(es, plat_x),
                w2s_y(es, plat_y),
                w2s_w(es, TILE_SIZE),
                w2s_h(es, plat_h)
            };
            SDL_RenderFillRect(es->renderer, &dst);
        }
    }
}

/* ---- Floor (9-slice with sea gap handling) ----------------------- */

/*
 * render_floor — Draw the ground floor across the full world width.
 *
 * Replicates the game's 9-slice floor rendering:
 *   - Grass_Tileset.png is 48x48, divided into 3x3 grid of 16x16 pieces.
 *   - Three rows of pieces fill FLOOR_Y to GAME_H (48 px = 3 x 16).
 *   - Pieces inside sea gaps are skipped.
 *   - Pieces at gap boundaries use edge-cap columns for clean edges.
 */
static void render_floor(EditorState *es) {
    const int P = TILE_SIZE / 3;  /* 9-slice piece size: 16 px */

    for (int ty = FLOOR_Y; ty < GAME_H; ty += P) {
        /* Choose which row of the 9-slice to sample */
        int piece_row;
        if (ty == FLOOR_Y)         piece_row = 0;  /* top:    grass edge */
        else if (ty + P >= GAME_H) piece_row = 2;  /* bottom: base edge  */
        else                       piece_row = 1;  /* middle: dirt fill  */

        for (int tx = 0; tx < WORLD_W; tx += P) {
            /*
             * Skip this piece if it falls inside any sea gap.
             * A piece at tx is inside a gap when both edges are within
             * the gap bounds: gap_x <= tx AND tx + P <= gap_x + SEA_GAP_W.
             */
            int in_gap = 0;
            for (int g = 0; g < es->level.sea_gap_count; g++) {
                int gx = es->level.sea_gaps[g];
                if (tx >= gx && tx + P <= gx + SEA_GAP_W) {
                    in_gap = 1;
                    break;
                }
            }
            if (in_gap) continue;

            /*
             * Choose which column of the 9-slice to sample.
             * Use edge caps at world boundaries and gap boundaries so
             * the floor has clean left/right edges beside each hole.
             */
            int piece_col;
            int at_left_edge  = 0;
            int at_right_edge = 0;

            /* World boundaries */
            if (tx <= 0)           at_left_edge  = 1;
            if (tx + P >= WORLD_W) at_right_edge = 1;

            /* Gap boundaries */
            for (int g = 0; g < es->level.sea_gap_count; g++) {
                int gx = es->level.sea_gaps[g];
                if (tx + P == gx)             at_right_edge = 1;
                if (tx == gx + SEA_GAP_W)     at_left_edge  = 1;
            }

            if (at_left_edge && at_right_edge) piece_col = 1;
            else if (at_left_edge)             piece_col = 0;
            else if (at_right_edge)            piece_col = 2;
            else                               piece_col = 1;

            SDL_Rect src = { piece_col * P, piece_row * P, P, P };
            SDL_Rect dst = {
                w2s_x(es, (float)tx),
                w2s_y(es, (float)ty),
                w2s_w(es, P),
                w2s_h(es, P)
            };
            SDL_RenderCopy(es->renderer, es->textures.floor_tile,
                           &src, &dst);
        }
    }
}

/* ---- Water ------------------------------------------------------- */

/*
 * render_water — Draw a solid blue rect in each sea gap to represent water.
 *
 * The game's water strip sits at y = GAME_H - WATER_ART_H with a height of
 * WATER_ART_H (31 px).  We use a flat colour (#1A6BA0) here because the
 * canvas does not need animated water frames — just a clear visual indicator
 * that water exists in this gap.
 */
static void render_water(EditorState *es) {
    SDL_SetRenderDrawColor(es->renderer, 0x1A, 0x6B, 0xA0, 0xFF);

    for (int g = 0; g < es->level.sea_gap_count; g++) {
        float gx = (float)es->level.sea_gaps[g];
        float wy = (float)(GAME_H - WATER_ART_H);

        SDL_Rect dst = {
            w2s_x(es, gx),
            w2s_y(es, wy),
            w2s_w(es, SEA_GAP_W),
            w2s_h(es, WATER_ART_H)
        };
        SDL_RenderFillRect(es->renderer, &dst);
    }
}

/* ---- Rails ------------------------------------------------------- */

/*
 * render_rails — Draw rail paths as coloured outlines.
 *
 * For RECT rails: draw a rectangle at (rail.x, rail.y, rail.w*16, rail.h*16).
 * For HORIZ rails: draw a horizontal line at (rail.x, rail.y) spanning
 *   rail.w * 16 pixels.
 * Both use green (#00AA00) with 2-pixel width for visibility.
 */
static void render_rails(EditorState *es) {
    SDL_SetRenderDrawColor(es->renderer, 0x00, 0xAA, 0x00, 0xFF);

    for (int i = 0; i < es->level.rail_count; i++) {
        const RailPlacement *rp = &es->level.rails[i];
        float rx = (float)rp->x;
        float ry = (float)rp->y;

        if (rp->layout == RAIL_LAYOUT_RECT) {
            int rw = rp->w * RAIL_TILE_W;
            int rh = rp->h * RAIL_TILE_H;

            /* Draw rectangle outline using four 2-px-thick edges */
            int sx = w2s_x(es, rx);
            int sy = w2s_y(es, ry);
            int sw = w2s_w(es, rw);
            int sh = w2s_h(es, rh);
            draw_outline(es->renderer, sx, sy, sw, sh, 2);
        } else {
            /* RAIL_LAYOUT_HORIZ — horizontal line */
            int rw = rp->w * RAIL_TILE_W;

            int sx0 = w2s_x(es, rx);
            int sx1 = w2s_x(es, rx + (float)rw);
            int sy  = w2s_y(es, ry);

            /* Draw 2-pixel thick horizontal line */
            SDL_Rect line = { sx0, sy, sx1 - sx0, 2 };
            SDL_RenderFillRect(es->renderer, &line);
        }

        /* Draw rail tile texture if available */
        if (es->textures.rail) {
            if (rp->layout == RAIL_LAYOUT_RECT) {
                int tw = rp->w;
                int th = rp->h;
                /* Top row */
                for (int t = 0; t < tw; t++) {
                    draw_tex(es, es->textures.rail, NULL,
                             rx + (float)(t * RAIL_TILE_W), ry,
                             RAIL_TILE_W, RAIL_TILE_H);
                }
                /* Bottom row */
                for (int t = 0; t < tw; t++) {
                    draw_tex(es, es->textures.rail, NULL,
                             rx + (float)(t * RAIL_TILE_W),
                             ry + (float)((th - 1) * RAIL_TILE_H),
                             RAIL_TILE_W, RAIL_TILE_H);
                }
                /* Left column (excluding corners) */
                for (int t = 1; t < th - 1; t++) {
                    draw_tex(es, es->textures.rail, NULL,
                             rx, ry + (float)(t * RAIL_TILE_H),
                             RAIL_TILE_W, RAIL_TILE_H);
                }
                /* Right column (excluding corners) */
                for (int t = 1; t < th - 1; t++) {
                    draw_tex(es, es->textures.rail, NULL,
                             rx + (float)((tw - 1) * RAIL_TILE_W),
                             ry + (float)(t * RAIL_TILE_H),
                             RAIL_TILE_W, RAIL_TILE_H);
                }
            } else {
                /* HORIZ — line of tiles */
                int tw = rp->w;
                for (int t = 0; t < tw; t++) {
                    draw_tex(es, es->textures.rail, NULL,
                             rx + (float)(t * RAIL_TILE_W), ry,
                             RAIL_TILE_W, RAIL_TILE_H);
                }
            }
        }
    }
}

/* ---- Floating platforms ------------------------------------------ */

/*
 * render_float_platforms — Draw floating / crumble / rail platforms.
 *
 * Each platform is tile_count x FPLAT_PIECE_W (16) pixels wide and
 * FPLAT_PIECE_H (16) pixels tall.  For STATIC and CRUMBLE modes the
 * placement x/y is used directly.  For RAIL mode we draw at the
 * placement coordinates (which are set to the rail origin in the level
 * definition for v1 of the editor).
 */
static void render_float_platforms(EditorState *es) {
    for (int i = 0; i < es->level.float_platform_count; i++) {
        const FloatPlatformPlacement *fp = &es->level.float_platforms[i];
        float fx = fp->x;
        float fy = fp->y;
        int   total_w = fp->tile_count * FPLAT_PIECE_W;

        if (es->textures.float_platform) {
            /* Draw each piece of the 3-slice platform strip */
            for (int p = 0; p < fp->tile_count; p++) {
                /*
                 * 3-slice piece selection:
                 *   piece 0 = left cap (src x = 0)
                 *   piece 1 = centre fill (src x = FPLAT_PIECE_W)
                 *   piece 2 = right cap (src x = 2 * FPLAT_PIECE_W)
                 */
                int piece;
                if (fp->tile_count == 1)              piece = 1; /* single */
                else if (p == 0)                      piece = 0; /* left   */
                else if (p == fp->tile_count - 1)     piece = 2; /* right  */
                else                                  piece = 1; /* centre */

                SDL_Rect src = { piece * FPLAT_PIECE_W, 0,
                                 FPLAT_PIECE_W, FPLAT_PIECE_H };
                draw_tex(es, es->textures.float_platform, &src,
                         fx + (float)(p * FPLAT_PIECE_W), fy,
                         FPLAT_PIECE_W, FPLAT_PIECE_H);
            }
        } else {
            /* Fallback: grey rectangle */
            SDL_SetRenderDrawColor(es->renderer, 0x80, 0x80, 0x80, 0xFF);
            SDL_Rect dst = {
                w2s_x(es, fx), w2s_y(es, fy),
                w2s_w(es, total_w), w2s_h(es, FPLAT_PIECE_H)
            };
            SDL_RenderFillRect(es->renderer, &dst);
        }
    }
}

/* ---- Spike rows -------------------------------------------------- */

/*
 * render_spike_rows — Draw ground spike strips.
 *
 * Each spike row sits at y = FLOOR_Y - SPIKE_TILE_H = 236, and is
 * count x SPIKE_TILE_W (16) pixels wide.  Each tile is 16x16.
 */
static void render_spike_rows(EditorState *es) {
    float spike_y = (float)(FLOOR_Y - SPIKE_TILE_H);

    for (int i = 0; i < es->level.spike_row_count; i++) {
        const SpikeRowPlacement *sr = &es->level.spike_rows[i];

        for (int t = 0; t < sr->count; t++) {
            draw_tex(es, es->textures.spike, NULL,
                     sr->x + (float)(t * SPIKE_TILE_W), spike_y,
                     SPIKE_TILE_W, SPIKE_TILE_H);
        }
    }
}

/* ---- Spike platforms --------------------------------------------- */

/*
 * render_spike_platforms — Draw elevated spike hazard surfaces.
 *
 * Each platform is tile_count x SPIKE_PLAT_PIECE_W pixels wide.
 * Source rect crops to rows SPIKE_PLAT_SRC_Y..+SPIKE_PLAT_SRC_H
 * for the visible art content.
 */
static void render_spike_platforms(EditorState *es) {
    for (int i = 0; i < es->level.spike_platform_count; i++) {
        const SpikePlatformPlacement *sp = &es->level.spike_platforms[i];

        for (int t = 0; t < sp->tile_count; t++) {
            /*
             * 3-slice piece selection:
             *   0 = left, 1 = centre, 2 = right
             */
            int piece;
            if (sp->tile_count == 1)            piece = 1;
            else if (t == 0)                    piece = 0;
            else if (t == sp->tile_count - 1)   piece = 2;
            else                                piece = 1;

            SDL_Rect src = { piece * SPIKE_PLAT_PIECE_W, SPIKE_PLAT_SRC_Y,
                             SPIKE_PLAT_PIECE_W, SPIKE_PLAT_SRC_H };
            draw_tex(es, es->textures.spike_platform, &src,
                     sp->x + (float)(t * SPIKE_PLAT_PIECE_W), sp->y,
                     SPIKE_PLAT_PIECE_W, SPIKE_PLAT_SRC_H);
        }
    }
}

/* ---- Bridges ----------------------------------------------------- */

/*
 * render_bridges — Draw tiled crumble walkways.
 *
 * Each bridge is brick_count x BRIDGE_TILE_W pixels wide, each tile 16x16.
 */
static void render_bridges(EditorState *es) {
    for (int i = 0; i < es->level.bridge_count; i++) {
        const BridgePlacement *br = &es->level.bridges[i];

        for (int t = 0; t < br->brick_count; t++) {
            SDL_Rect src = { 0, 0, BRIDGE_TILE_W, BRIDGE_TILE_H };
            draw_tex(es, es->textures.bridge, &src,
                     br->x + (float)(t * BRIDGE_TILE_W), br->y,
                     BRIDGE_TILE_W, BRIDGE_TILE_H);
        }
    }
}

/* ---- Bouncepads -------------------------------------------------- */

/*
 * render_bouncepads — Draw all three bouncepad variants.
 *
 * All bouncepads share the same frame dimensions (48 px wide).
 * The visible art is cropped to rows BP_SRC_Y..+BP_SRC_H (14..31, 18 px).
 * Frame 2 (x = 96) is the idle/compressed state shown in the editor.
 * y = FLOOR_Y - BP_SRC_H = 252 - 18 = 234.
 */
static void render_bouncepads(EditorState *es) {
    float bp_y = (float)(FLOOR_Y - BP_SRC_H);

    /* Source rect for frame 2 (idle): x = 2 * BP_FRAME_W = 96 */
    SDL_Rect src = { 2 * BP_FRAME_W, BP_SRC_Y, BP_FRAME_W, BP_SRC_H };

    /* Small bouncepads (green) */
    for (int i = 0; i < es->level.bouncepad_small_count; i++) {
        draw_tex(es, es->textures.bouncepad_small, &src,
                 es->level.bouncepads_small[i].x, bp_y,
                 BP_FRAME_W, BP_SRC_H);
    }

    /* Medium bouncepads (wood) */
    for (int i = 0; i < es->level.bouncepad_medium_count; i++) {
        draw_tex(es, es->textures.bouncepad_medium, &src,
                 es->level.bouncepads_medium[i].x, bp_y,
                 BP_FRAME_W, BP_SRC_H);
    }

    /* High bouncepads (red) */
    for (int i = 0; i < es->level.bouncepad_high_count; i++) {
        draw_tex(es, es->textures.bouncepad_high, &src,
                 es->level.bouncepads_high[i].x, bp_y,
                 BP_FRAME_W, BP_SRC_H);
    }
}

/* ---- Vines ------------------------------------------------------- */

/*
 * render_vines — Draw hanging vine decorations.
 *
 * Each vine is tile_count tiles tall, with each tile displayed at
 * VINE_W x VINE_H and stacked with VINE_STEP vertical spacing.
 * Source rect crops to (0, VINE_SRC_Y, VINE_W, VINE_SRC_H).
 */
static void render_vines(EditorState *es) {
    SDL_Rect src = { 0, VINE_SRC_Y, VINE_W, VINE_SRC_H };

    for (int i = 0; i < es->level.vine_count; i++) {
        const VinePlacement *v = &es->level.vines[i];

        for (int t = 0; t < v->tile_count; t++) {
            float tile_y = v->y + (float)(t * VINE_STEP);
            draw_tex(es, es->textures.vine, &src,
                     v->x, tile_y, VINE_W, VINE_H);
        }
    }
}

/* ---- Ladders ----------------------------------------------------- */

/*
 * render_ladders — Draw climbable ladders.
 *
 * Each ladder is tile_count tiles tall, with each tile displayed at
 * LADDER_W x LADDER_H and stacked with LADDER_STEP vertical spacing.
 * Source rect crops to (0, LADDER_SRC_Y, LADDER_W, LADDER_SRC_H).
 */
static void render_ladders(EditorState *es) {
    SDL_Rect src = { 0, LADDER_SRC_Y, LADDER_W, LADDER_SRC_H };

    for (int i = 0; i < es->level.ladder_count; i++) {
        const LadderPlacement *ld = &es->level.ladders[i];

        for (int t = 0; t < ld->tile_count; t++) {
            float tile_y = ld->y + (float)(t * LADDER_STEP);
            draw_tex(es, es->textures.ladder, &src,
                     ld->x, tile_y, LADDER_W, LADDER_H);
        }
    }
}

/* ---- Ropes ------------------------------------------------------- */

/*
 * render_ropes — Draw climbable ropes.
 *
 * Each rope is tile_count tiles tall, with each tile displayed at
 * ROPE_W x ROPE_H and stacked with ROPE_STEP vertical spacing.
 * Source rect crops to (ROPE_SRC_X, ROPE_SRC_Y, ROPE_SRC_W, ROPE_SRC_H).
 */
static void render_ropes(EditorState *es) {
    SDL_Rect src = { ROPE_SRC_X, ROPE_SRC_Y, ROPE_SRC_W, ROPE_SRC_H };

    for (int i = 0; i < es->level.rope_count; i++) {
        const RopePlacement *rp = &es->level.ropes[i];

        for (int t = 0; t < rp->tile_count; t++) {
            float tile_y = rp->y + (float)(t * ROPE_STEP);
            draw_tex(es, es->textures.rope, &src,
                     rp->x, tile_y, ROPE_W, ROPE_H);
        }
    }
}

/* ---- Coins ------------------------------------------------------- */

/*
 * render_coins — Draw coin collectibles at 16x16 display size.
 *
 * Uses the full texture (src = NULL) scaled to COIN_DISPLAY_W x _H.
 */
static void render_coins(EditorState *es) {
    for (int i = 0; i < es->level.coin_count; i++) {
        draw_tex(es, es->textures.coin, NULL,
                 es->level.coins[i].x, es->level.coins[i].y,
                 COIN_DISPLAY_W, COIN_DISPLAY_H);
    }
}

/* ---- Yellow stars ------------------------------------------------ */

/*
 * render_yellow_stars — Draw health-restoring star pickups at 16x16.
 */
static void render_yellow_stars(EditorState *es) {
    for (int i = 0; i < es->level.yellow_star_count; i++) {
        draw_tex(es, es->textures.yellow_star, NULL,
                 es->level.yellow_stars[i].x,
                 es->level.yellow_stars[i].y,
                 YSTAR_DISPLAY_W, YSTAR_DISPLAY_H);
    }
}

/* ---- Last star --------------------------------------------------- */

/*
 * render_last_star — Draw the end-of-level star at 24x24.
 */
static void render_last_star(EditorState *es) {
    draw_tex(es, es->textures.last_star, NULL,
             es->level.last_star.x, es->level.last_star.y,
             LSTAR_DISPLAY_W, LSTAR_DISPLAY_H);
}

/* ---- Player spawn ------------------------------------------------ */

/*
 * render_player_spawn — Draw the player idle frame at the spawn position.
 *
 * Uses row 0, col 0 of the player sprite sheet (48x48) for the idle frame.
 * Rendered at PLAYER_SPAWN_W x PLAYER_SPAWN_H (48x48) in world space.
 */
static void render_player_spawn(EditorState *es) {
    if (!es->textures.player) return;

    SDL_Rect src = { 0, 0, PLAYER_SPAWN_W, PLAYER_SPAWN_H };
    draw_tex(es, es->textures.player, &src,
             es->level.player_start_x, es->level.player_start_y,
             PLAYER_SPAWN_W, PLAYER_SPAWN_H);
}

/* ---- Blue flames ------------------------------------------------- */

/*
 * render_blue_flames — Draw blue flame entities from placement data.
 *
 * Each blue flame is positioned centred within its gap x coordinate.
 * The preview is drawn at FLOOR_Y - BLUE_FLAME_H so designers can see
 * where the flame will erupt from.
 *
 * x = gap_x + (SEA_GAP_W - BLUE_FLAME_W) / 2
 * y = FLOOR_Y - BLUE_FLAME_H  (visible preview position above gap)
 */
static void render_blue_flames(EditorState *es) {
    if (!es->textures.blue_flame) return;

    for (int i = 0; i < es->level.blue_flame_count; i++) {
        float gap_x = es->level.blue_flames[i].x;
        float fx = gap_x + (float)(SEA_GAP_W - BLUE_FLAME_W) / 2.0f;
        float fy = (float)(FLOOR_Y - BLUE_FLAME_H);

        draw_tex(es, es->textures.blue_flame, NULL,
                 fx, fy, BLUE_FLAME_W, BLUE_FLAME_H);
    }
}

/* ---- Fish -------------------------------------------------------- */

/*
 * render_fish — Draw slow jumping fish in water lanes.
 *
 * Fish y position is derived from the water surface:
 *   water_y = (GAME_H - WATER_ART_H) - FISH_FRAME_H / 2
 *           = (300 - 31) - 24 = 245
 * Rendered at full 48x48 frame size.
 */
static void render_fish(EditorState *es) {
    float fish_y = (float)(GAME_H - WATER_ART_H) - FISH_FRAME_H / 2.0f;

    for (int i = 0; i < es->level.fish_count; i++) {
        draw_tex(es, es->textures.fish, NULL,
                 es->level.fish[i].x, fish_y,
                 FISH_FRAME_W, FISH_FRAME_H);
    }
}

/* ---- Faster fish ------------------------------------------------- */

/*
 * render_faster_fish — Draw fast jumping fish (same dimensions as slow).
 */
static void render_faster_fish(EditorState *es) {
    float fish_y = (float)(GAME_H - WATER_ART_H) - FISH_FRAME_H / 2.0f;

    for (int i = 0; i < es->level.faster_fish_count; i++) {
        draw_tex(es, es->textures.faster_fish, NULL,
                 es->level.faster_fish[i].x, fish_y,
                 FISH_FRAME_W, FISH_FRAME_H);
    }
}

/* ---- Spike blocks ------------------------------------------------ */

/*
 * render_spike_blocks — Draw rail-riding spike blocks at 24x24.
 *
 * For v1 of the editor we use a simplified position: the spike block
 * is drawn near its rail's origin.  For RECT rails the block appears
 * at the rail origin.  For HORIZ rails it appears at rail.x + t_offset * 16.
 * This is approximate but sufficient for visual level editing.
 */
static void render_spike_blocks(EditorState *es) {
    for (int i = 0; i < es->level.spike_block_count; i++) {
        const SpikeBlockPlacement *sb = &es->level.spike_blocks[i];
        int ri = sb->rail_index;

        /* Validate rail index is within bounds */
        if (ri < 0 || ri >= es->level.rail_count) continue;

        const RailPlacement *rp = &es->level.rails[ri];
        float sx, sy;

        if (rp->layout == RAIL_LAYOUT_RECT) {
            /* Place at rail origin, centered on the tile */
            sx = (float)rp->x + (float)(RAIL_TILE_W - SPIKE_DISPLAY_W) / 2.0f;
            sy = (float)rp->y + (float)(RAIL_TILE_H - SPIKE_DISPLAY_H) / 2.0f;
        } else {
            /* HORIZ: approximate position along the rail */
            sx = (float)rp->x + sb->t_offset * (float)RAIL_TILE_W;
            sy = (float)rp->y + (float)(RAIL_TILE_H - SPIKE_DISPLAY_H) / 2.0f;
        }

        draw_tex(es, es->textures.spike_block, NULL,
                 sx, sy, SPIKE_DISPLAY_W, SPIKE_DISPLAY_H);
    }
}

/* ---- Axe traps --------------------------------------------------- */

/*
 * render_axe_traps — Draw swinging / spinning axe hazards.
 *
 * The axe hangs from a platform pillar.  Its pivot is at:
 *   x = pillar_x + TILE_SIZE/2 - AXE_FRAME_W/2
 *   y = FLOOR_Y - 3*TILE_SIZE + 16  (top of a 3-tile pillar = 124)
 *
 * Rendered at AXE_FRAME_W x AXE_FRAME_H (48x64).
 */
static void render_axe_traps(EditorState *es) {
    for (int i = 0; i < es->level.axe_trap_count; i++) {
        const AxeTrapPlacement *at = &es->level.axe_traps[i];

        float ax = at->pillar_x;
        float ay = (at->y != 0.0f) ? at->y : (float)(FLOOR_Y - 3 * TILE_SIZE + 16);

        draw_tex(es, es->textures.axe_trap, NULL,
                 ax, ay, AXE_FRAME_W, AXE_FRAME_H);
    }
}

/* ---- Circular saws ----------------------------------------------- */

/*
 * render_circular_saws — Draw fast rotating patrol saws at 32x32.
 *
 * Saw y is derived from the bridge/platform height:
 *   y = FLOOR_Y - 2 * TILE_SIZE + 16 - SAW_DISPLAY_H
 *     = 252 - 96 + 16 - 32 = 140
 */
static void render_circular_saws(EditorState *es) {
    float default_y = (float)(FLOOR_Y - 2 * TILE_SIZE + 16 - SAW_DISPLAY_H);

    for (int i = 0; i < es->level.circular_saw_count; i++) {
        const CircularSawPlacement *cs = &es->level.circular_saws[i];
        float sy = (cs->y != 0.0f) ? cs->y : default_y;
        draw_tex(es, es->textures.circular_saw, NULL,
                 cs->x, sy, SAW_DISPLAY_W, SAW_DISPLAY_H);
    }
}

/* ---- Spiders ----------------------------------------------------- */

/*
 * render_spiders — Draw ground-patrol spiders.
 *
 * Spiders sit on the floor: y = FLOOR_Y - SPIDER_ART_H = 252 - 10 = 242.
 * Source rect crops to (0, SPIDER_ART_Y, SPIDER_FRAME_W, SPIDER_ART_H)
 * to show only the visible art rows (22..31) of the 64-px frame.
 * Displayed at SPIDER_FRAME_W x SPIDER_ART_H (64x10).
 */
static void render_spiders(EditorState *es) {
    float spider_y = (float)(FLOOR_Y - SPIDER_ART_H);
    SDL_Rect src = { 0, SPIDER_ART_Y, SPIDER_FRAME_W, SPIDER_ART_H };

    for (int i = 0; i < es->level.spider_count; i++) {
        draw_tex(es, es->textures.spider, &src,
                 es->level.spiders[i].x, spider_y,
                 SPIDER_FRAME_W, SPIDER_ART_H);
    }
}

/* ---- Jumping spiders --------------------------------------------- */

/*
 * render_jumping_spiders — Draw jumping spider variants.
 *
 * Same visual dimensions as regular spiders (same sprite sheet layout).
 * y = FLOOR_Y - SPIDER_ART_H = 242 (on the ground in the editor preview).
 */
static void render_jumping_spiders(EditorState *es) {
    float spider_y = (float)(FLOOR_Y - SPIDER_ART_H);
    SDL_Rect src = { 0, SPIDER_ART_Y, SPIDER_FRAME_W, SPIDER_ART_H };

    for (int i = 0; i < es->level.jumping_spider_count; i++) {
        draw_tex(es, es->textures.jumping_spider, &src,
                 es->level.jumping_spiders[i].x, spider_y,
                 SPIDER_FRAME_W, SPIDER_ART_H);
    }
}

/* ---- Birds ------------------------------------------------------- */

/*
 * render_birds — Draw slow sine-wave sky patrol birds.
 *
 * Source rect crops to (0, BIRD_ART_Y, BIRD_FRAME_W, BIRD_ART_H)
 * to show only the visible art rows (17..30) of the 48-px frame.
 * y = base_y from the placement data (varies per bird).
 * Displayed at BIRD_FRAME_W x BIRD_ART_H (48x14).
 */
static void render_birds(EditorState *es) {
    SDL_Rect src = { 0, BIRD_ART_Y, BIRD_FRAME_W, BIRD_ART_H };

    for (int i = 0; i < es->level.bird_count; i++) {
        draw_tex(es, es->textures.bird, &src,
                 es->level.birds[i].x,
                 es->level.birds[i].base_y,
                 BIRD_FRAME_W, BIRD_ART_H);
    }
}

/* ---- Faster birds ------------------------------------------------ */

/*
 * render_faster_birds — Draw fast sine-wave sky patrol birds.
 *
 * Same crop and display dimensions as slow birds.
 */
static void render_faster_birds(EditorState *es) {
    SDL_Rect src = { 0, BIRD_ART_Y, BIRD_FRAME_W, BIRD_ART_H };

    for (int i = 0; i < es->level.faster_bird_count; i++) {
        draw_tex(es, es->textures.faster_bird, &src,
                 es->level.faster_birds[i].x,
                 es->level.faster_birds[i].base_y,
                 BIRD_FRAME_W, BIRD_ART_H);
    }
}

/* ---- Selection highlight ----------------------------------------- */

/*
 * render_selection — Draw a 2-pixel accent-coloured outline around the
 *                    currently selected entity.
 *
 * Reads es->selection to determine which entity (if any) is selected,
 * then computes that entity's screen-space bounding box and draws
 * an outline in UI_ACCENT colour (#4A90D9).
 */
static void render_selection(EditorState *es) {
    if (es->selection.index < 0) return;

    float wx = 0, wy = 0;
    int   ww = 0, wh = 0;

    /*
     * Determine the world-space bounding box of the selected entity.
     * Each entity type has a different way of computing its position and size.
     */
    switch (es->selection.type) {
    case ENT_PLATFORM: {
        if (es->selection.index >= es->level.platform_count) return;
        const PlatformPlacement *p = &es->level.platforms[es->selection.index];
        wx = p->x;
        wy = (float)(FLOOR_Y - p->tile_height * TILE_SIZE + 16);
        ww = TILE_SIZE;
        wh = p->tile_height * TILE_SIZE;
        break;
    }
    case ENT_SEA_GAP: {
        if (es->selection.index >= es->level.sea_gap_count) return;
        wx = (float)es->level.sea_gaps[es->selection.index];
        wy = (float)FLOOR_Y;
        ww = SEA_GAP_W;
        wh = GAME_H - FLOOR_Y;
        break;
    }
    case ENT_RAIL: {
        if (es->selection.index >= es->level.rail_count) return;
        const RailPlacement *r = &es->level.rails[es->selection.index];
        wx = (float)r->x;
        wy = (float)r->y;
        ww = r->w * RAIL_TILE_W;
        wh = (r->layout == RAIL_LAYOUT_RECT) ? r->h * RAIL_TILE_H : RAIL_TILE_H;
        break;
    }
    case ENT_COIN: {
        if (es->selection.index >= es->level.coin_count) return;
        wx = es->level.coins[es->selection.index].x;
        wy = es->level.coins[es->selection.index].y;
        ww = COIN_DISPLAY_W;
        wh = COIN_DISPLAY_H;
        break;
    }
    case ENT_YELLOW_STAR: {
        if (es->selection.index >= es->level.yellow_star_count) return;
        wx = es->level.yellow_stars[es->selection.index].x;
        wy = es->level.yellow_stars[es->selection.index].y;
        ww = YSTAR_DISPLAY_W;
        wh = YSTAR_DISPLAY_H;
        break;
    }
    case ENT_LAST_STAR: {
        wx = es->level.last_star.x;
        wy = es->level.last_star.y;
        ww = LSTAR_DISPLAY_W;
        wh = LSTAR_DISPLAY_H;
        break;
    }
    case ENT_PLAYER_SPAWN: {
        wx = es->level.player_start_x;
        wy = es->level.player_start_y;
        ww = PLAYER_SPAWN_W;
        wh = PLAYER_SPAWN_H;
        break;
    }
    case ENT_SPIDER: {
        if (es->selection.index >= es->level.spider_count) return;
        wx = es->level.spiders[es->selection.index].x;
        wy = (float)(FLOOR_Y - SPIDER_ART_H);
        ww = SPIDER_FRAME_W;
        wh = SPIDER_ART_H;
        break;
    }
    case ENT_JUMPING_SPIDER: {
        if (es->selection.index >= es->level.jumping_spider_count) return;
        wx = es->level.jumping_spiders[es->selection.index].x;
        wy = (float)(FLOOR_Y - SPIDER_ART_H);
        ww = SPIDER_FRAME_W;
        wh = SPIDER_ART_H;
        break;
    }
    case ENT_BIRD: {
        if (es->selection.index >= es->level.bird_count) return;
        wx = es->level.birds[es->selection.index].x;
        wy = es->level.birds[es->selection.index].base_y;
        ww = BIRD_FRAME_W;
        wh = BIRD_ART_H;
        break;
    }
    case ENT_FASTER_BIRD: {
        if (es->selection.index >= es->level.faster_bird_count) return;
        wx = es->level.faster_birds[es->selection.index].x;
        wy = es->level.faster_birds[es->selection.index].base_y;
        ww = BIRD_FRAME_W;
        wh = BIRD_ART_H;
        break;
    }
    case ENT_FISH: {
        if (es->selection.index >= es->level.fish_count) return;
        wx = es->level.fish[es->selection.index].x;
        wy = (float)(GAME_H - WATER_ART_H) - FISH_FRAME_H / 2.0f;
        ww = FISH_FRAME_W;
        wh = FISH_FRAME_H;
        break;
    }
    case ENT_FASTER_FISH: {
        if (es->selection.index >= es->level.faster_fish_count) return;
        wx = es->level.faster_fish[es->selection.index].x;
        wy = (float)(GAME_H - WATER_ART_H) - FISH_FRAME_H / 2.0f;
        ww = FISH_FRAME_W;
        wh = FISH_FRAME_H;
        break;
    }
    case ENT_AXE_TRAP: {
        if (es->selection.index >= es->level.axe_trap_count) return;
        const AxeTrapPlacement *at = &es->level.axe_traps[es->selection.index];
        wx = at->pillar_x;
        wy = (at->y != 0.0f) ? at->y : (float)(FLOOR_Y - 3 * TILE_SIZE + 16);
        ww = AXE_FRAME_W;
        wh = AXE_FRAME_H;
        break;
    }
    case ENT_CIRCULAR_SAW: {
        if (es->selection.index >= es->level.circular_saw_count) return;
        const CircularSawPlacement *cs = &es->level.circular_saws[es->selection.index];
        wx = cs->x;
        wy = (cs->y != 0.0f) ? cs->y : (float)(FLOOR_Y - 2 * TILE_SIZE + 16 - SAW_DISPLAY_H);
        ww = SAW_DISPLAY_W;
        wh = SAW_DISPLAY_H;
        break;
    }
    case ENT_SPIKE_ROW: {
        if (es->selection.index >= es->level.spike_row_count) return;
        const SpikeRowPlacement *sr =
            &es->level.spike_rows[es->selection.index];
        wx = sr->x;
        wy = (float)(FLOOR_Y - SPIKE_TILE_H);
        ww = sr->count * SPIKE_TILE_W;
        wh = SPIKE_TILE_H;
        break;
    }
    case ENT_SPIKE_PLATFORM: {
        if (es->selection.index >= es->level.spike_platform_count) return;
        const SpikePlatformPlacement *sp =
            &es->level.spike_platforms[es->selection.index];
        wx = sp->x;
        wy = sp->y;
        ww = sp->tile_count * SPIKE_PLAT_PIECE_W;
        wh = SPIKE_PLAT_SRC_H;
        break;
    }
    case ENT_SPIKE_BLOCK: {
        if (es->selection.index >= es->level.spike_block_count) return;
        const SpikeBlockPlacement *sb =
            &es->level.spike_blocks[es->selection.index];
        int ri = sb->rail_index;
        if (ri >= 0 && ri < es->level.rail_count) {
            const RailPlacement *rp = &es->level.rails[ri];
            if (rp->layout == RAIL_LAYOUT_RECT) {
                wx = (float)rp->x;
                wy = (float)rp->y;
            } else {
                wx = (float)rp->x + sb->t_offset * (float)RAIL_TILE_W;
                wy = (float)rp->y;
            }
        }
        ww = SPIKE_DISPLAY_W;
        wh = SPIKE_DISPLAY_H;
        break;
    }
    case ENT_BLUE_FLAME: {
        if (es->selection.index >= es->level.blue_flame_count) return;
        float gap_x = es->level.blue_flames[es->selection.index].x;
        wx = gap_x + (float)(SEA_GAP_W - BLUE_FLAME_W) / 2.0f;
        wy = (float)(FLOOR_Y - BLUE_FLAME_H);
        ww = BLUE_FLAME_W;
        wh = BLUE_FLAME_H;
        break;
    }
    case ENT_FLOAT_PLATFORM: {
        if (es->selection.index >= es->level.float_platform_count) return;
        const FloatPlatformPlacement *fp =
            &es->level.float_platforms[es->selection.index];
        wx = fp->x;
        wy = fp->y;
        ww = fp->tile_count * FPLAT_PIECE_W;
        wh = FPLAT_PIECE_H;
        break;
    }
    case ENT_BRIDGE: {
        if (es->selection.index >= es->level.bridge_count) return;
        const BridgePlacement *br =
            &es->level.bridges[es->selection.index];
        wx = br->x;
        wy = br->y;
        ww = br->brick_count * BRIDGE_TILE_W;
        wh = BRIDGE_TILE_H;
        break;
    }
    case ENT_BOUNCEPAD_SMALL: {
        if (es->selection.index >= es->level.bouncepad_small_count) return;
        wx = es->level.bouncepads_small[es->selection.index].x;
        wy = (float)(FLOOR_Y - BP_SRC_H);
        ww = BP_FRAME_W;
        wh = BP_SRC_H;
        break;
    }
    case ENT_BOUNCEPAD_MEDIUM: {
        if (es->selection.index >= es->level.bouncepad_medium_count) return;
        wx = es->level.bouncepads_medium[es->selection.index].x;
        wy = (float)(FLOOR_Y - BP_SRC_H);
        ww = BP_FRAME_W;
        wh = BP_SRC_H;
        break;
    }
    case ENT_BOUNCEPAD_HIGH: {
        if (es->selection.index >= es->level.bouncepad_high_count) return;
        wx = es->level.bouncepads_high[es->selection.index].x;
        wy = (float)(FLOOR_Y - BP_SRC_H);
        ww = BP_FRAME_W;
        wh = BP_SRC_H;
        break;
    }
    case ENT_VINE: {
        if (es->selection.index >= es->level.vine_count) return;
        const VinePlacement *v = &es->level.vines[es->selection.index];
        wx = v->x;
        wy = v->y;
        ww = VINE_W;
        wh = (v->tile_count - 1) * VINE_STEP + VINE_H;
        break;
    }
    case ENT_LADDER: {
        if (es->selection.index >= es->level.ladder_count) return;
        const LadderPlacement *ld = &es->level.ladders[es->selection.index];
        wx = ld->x;
        wy = ld->y;
        ww = LADDER_W;
        wh = (ld->tile_count - 1) * LADDER_STEP + LADDER_H;
        break;
    }
    case ENT_ROPE: {
        if (es->selection.index >= es->level.rope_count) return;
        const RopePlacement *rp = &es->level.ropes[es->selection.index];
        wx = rp->x;
        wy = rp->y;
        ww = ROPE_W;
        wh = (rp->tile_count - 1) * ROPE_STEP + ROPE_H;
        break;
    }
    default:
        return;  /* unknown type — nothing to highlight */
    }

    /* Draw 2-pixel accent outline around the selected entity */
    SDL_SetRenderDrawColor(es->renderer, 0x4A, 0x90, 0xD9, 0xFF);
    int sx = w2s_x(es, wx) - 2;
    int sy = w2s_y(es, wy) - 2;
    int sw = w2s_w(es, ww) + 4;
    int sh = w2s_h(es, wh) + 4;
    draw_outline(es->renderer, sx, sy, sw, sh, 2);
}

/* ---- Ghost preview (placement mode) ------------------------------ */

/*
 * render_ghost — Draw a semi-transparent preview of the entity to be placed.
 *
 * When the active tool is TOOL_PLACE the designer sees a 50% alpha preview
 * of the selected palette entity at the current cursor position.  This gives
 * immediate visual feedback before committing the placement.
 */
static void render_ghost(EditorState *es) {
    if (es->tool != TOOL_PLACE) return;
    if (!canvas_contains(es->mouse_x, es->mouse_y)) return;

    /* Convert cursor position to world coordinates */
    float wx, wy;
    canvas_screen_to_world(es, es->mouse_x, es->mouse_y, &wx, &wy);

    /* Determine the texture and display size for the palette entity type */
    SDL_Texture *tex = NULL;
    SDL_Rect src_rect;
    int use_src = 0;  /* 1 = use src_rect crop, 0 = draw full texture */
    int dw = 0, dh = 0;

    switch (es->palette_type) {
    case ENT_COIN:
        tex = es->textures.coin;
        dw = COIN_DISPLAY_W; dh = COIN_DISPLAY_H;
        break;
    case ENT_YELLOW_STAR:
        tex = es->textures.yellow_star;
        dw = YSTAR_DISPLAY_W; dh = YSTAR_DISPLAY_H;
        break;
    case ENT_LAST_STAR:
        tex = es->textures.last_star;
        dw = LSTAR_DISPLAY_W; dh = LSTAR_DISPLAY_H;
        break;
    case ENT_PLAYER_SPAWN:
        tex = es->textures.player;
        src_rect = (SDL_Rect){ 0, 0, PLAYER_SPAWN_W, PLAYER_SPAWN_H };
        use_src = 1;
        dw = PLAYER_SPAWN_W; dh = PLAYER_SPAWN_H;
        break;
    case ENT_SPIDER:
        tex = es->textures.spider;
        src_rect = (SDL_Rect){ 0, SPIDER_ART_Y, SPIDER_FRAME_W, SPIDER_ART_H };
        use_src = 1;
        dw = SPIDER_FRAME_W; dh = SPIDER_ART_H;
        break;
    case ENT_JUMPING_SPIDER:
        tex = es->textures.jumping_spider;
        src_rect = (SDL_Rect){ 0, SPIDER_ART_Y, SPIDER_FRAME_W, SPIDER_ART_H };
        use_src = 1;
        dw = SPIDER_FRAME_W; dh = SPIDER_ART_H;
        break;
    case ENT_BIRD:
        tex = es->textures.bird;
        src_rect = (SDL_Rect){ 0, BIRD_ART_Y, BIRD_FRAME_W, BIRD_ART_H };
        use_src = 1;
        dw = BIRD_FRAME_W; dh = BIRD_ART_H;
        break;
    case ENT_FASTER_BIRD:
        tex = es->textures.faster_bird;
        src_rect = (SDL_Rect){ 0, BIRD_ART_Y, BIRD_FRAME_W, BIRD_ART_H };
        use_src = 1;
        dw = BIRD_FRAME_W; dh = BIRD_ART_H;
        break;
    case ENT_FISH:
        tex = es->textures.fish;
        dw = FISH_FRAME_W; dh = FISH_FRAME_H;
        break;
    case ENT_FASTER_FISH:
        tex = es->textures.faster_fish;
        dw = FISH_FRAME_W; dh = FISH_FRAME_H;
        break;
    case ENT_AXE_TRAP:
        tex = es->textures.axe_trap;
        dw = AXE_FRAME_W; dh = AXE_FRAME_H;
        break;
    case ENT_CIRCULAR_SAW:
        tex = es->textures.circular_saw;
        dw = SAW_DISPLAY_W; dh = SAW_DISPLAY_H;
        break;
    case ENT_SPIKE_ROW:
        tex = es->textures.spike;
        dw = SPIKE_TILE_W; dh = SPIKE_TILE_H;
        break;
    case ENT_SPIKE_PLATFORM:
        tex = es->textures.spike_platform;
        src_rect = (SDL_Rect){ 0, SPIKE_PLAT_SRC_Y,
                               SPIKE_PLAT_PIECE_W, SPIKE_PLAT_SRC_H };
        use_src = 1;
        dw = SPIKE_PLAT_PIECE_W; dh = SPIKE_PLAT_SRC_H;
        break;
    case ENT_SPIKE_BLOCK:
        tex = es->textures.spike_block;
        dw = SPIKE_DISPLAY_W; dh = SPIKE_DISPLAY_H;
        break;
    case ENT_BLUE_FLAME:
        tex = es->textures.blue_flame;
        dw = BLUE_FLAME_W; dh = BLUE_FLAME_H;
        break;
    case ENT_FLOAT_PLATFORM:
        tex = es->textures.float_platform;
        dw = FPLAT_PIECE_W; dh = FPLAT_PIECE_H;
        break;
    case ENT_BRIDGE:
        tex = es->textures.bridge;
        dw = BRIDGE_TILE_W; dh = BRIDGE_TILE_H;
        break;
    case ENT_BOUNCEPAD_SMALL:
        tex = es->textures.bouncepad_small;
        src_rect = (SDL_Rect){ 2 * BP_FRAME_W, BP_SRC_Y,
                               BP_FRAME_W, BP_SRC_H };
        use_src = 1;
        dw = BP_FRAME_W; dh = BP_SRC_H;
        break;
    case ENT_BOUNCEPAD_MEDIUM:
        tex = es->textures.bouncepad_medium;
        src_rect = (SDL_Rect){ 2 * BP_FRAME_W, BP_SRC_Y,
                               BP_FRAME_W, BP_SRC_H };
        use_src = 1;
        dw = BP_FRAME_W; dh = BP_SRC_H;
        break;
    case ENT_BOUNCEPAD_HIGH:
        tex = es->textures.bouncepad_high;
        src_rect = (SDL_Rect){ 2 * BP_FRAME_W, BP_SRC_Y,
                               BP_FRAME_W, BP_SRC_H };
        use_src = 1;
        dw = BP_FRAME_W; dh = BP_SRC_H;
        break;
    case ENT_VINE:
        tex = es->textures.vine;
        src_rect = (SDL_Rect){ 0, VINE_SRC_Y, VINE_W, VINE_SRC_H };
        use_src = 1;
        dw = VINE_W; dh = VINE_H;
        break;
    case ENT_LADDER:
        tex = es->textures.ladder;
        src_rect = (SDL_Rect){ 0, LADDER_SRC_Y, LADDER_W, LADDER_SRC_H };
        use_src = 1;
        dw = LADDER_W; dh = LADDER_H;
        break;
    case ENT_ROPE:
        tex = es->textures.rope;
        src_rect = (SDL_Rect){ ROPE_SRC_X, ROPE_SRC_Y,
                               ROPE_SRC_W, ROPE_SRC_H };
        use_src = 1;
        dw = ROPE_W; dh = ROPE_H;
        break;
    case ENT_PLATFORM:
        tex = es->textures.platform;
        dw = TILE_SIZE; dh = 2 * TILE_SIZE;  /* default 2-tile pillar */
        break;
    case ENT_SEA_GAP:
        /* Sea gap: draw a blue outline (no texture) */
        dw = SEA_GAP_W; dh = GAME_H - FLOOR_Y;
        break;
    case ENT_RAIL:
        /* Rail: draw a green outline (no texture) */
        dw = 3 * RAIL_TILE_W; dh = 3 * RAIL_TILE_H;
        break;
    default:
        return;
    }

    /* Set 50% alpha for the ghost preview */
    if (tex) {
        SDL_SetTextureAlphaMod(tex, 128);
    }

    /* Center the ghost on the cursor */
    float ghost_x = wx - (float)dw / 2.0f;
    float ghost_y = wy - (float)dh / 2.0f;

    if (tex) {
        draw_tex(es, tex, use_src ? &src_rect : NULL,
                 ghost_x, ghost_y, dw, dh);
        /* Restore full opacity */
        SDL_SetTextureAlphaMod(tex, 255);
    } else {
        /* Fallback for non-textured entities (sea gaps, rails) */
        if (es->palette_type == ENT_SEA_GAP) {
            SDL_SetRenderDrawColor(es->renderer, 0x1A, 0x6B, 0xA0, 0x80);
        } else {
            SDL_SetRenderDrawColor(es->renderer, 0x00, 0xAA, 0x00, 0x80);
        }
        SDL_Rect dst = {
            w2s_x(es, ghost_x), w2s_y(es, ghost_y),
            w2s_w(es, dw), w2s_h(es, dh)
        };
        SDL_RenderFillRect(es->renderer, &dst);
    }
}

/* ---- Grid overlay ------------------------------------------------ */

/*
 * render_grid — Draw grid lines, screen boundaries, and the FLOOR_Y line.
 *
 * Grid lines:
 *   TILE_SIZE (48 px) spacing in dark grey (#404040) at alpha 64.
 *
 * Screen boundaries:
 *   Vertical lines at x = 0, 400, 800, 1200 in blue (#4A90D9), 2 px wide.
 *   These show where each in-game screen starts.
 *
 * Floor line:
 *   Horizontal line at FLOOR_Y in red (#D94A4A), 2 px wide.
 *   Shows the top edge of the ground floor.
 */
static void render_grid(EditorState *es) {
    /* ---- TILE_SIZE grid ---- */
    SDL_SetRenderDrawColor(es->renderer, 0x40, 0x40, 0x40, 0x40);

    /* Vertical grid lines */
    for (int wx = 0; wx <= WORLD_W; wx += TILE_SIZE) {
        int sx = w2s_x(es, (float)wx);
        if (sx < 0 || sx > CANVAS_W) continue;

        SDL_Rect line = { sx, TOOLBAR_H, 1, CANVAS_H };
        SDL_RenderFillRect(es->renderer, &line);
    }

    /* Horizontal grid lines */
    for (int wy = 0; wy <= GAME_H; wy += TILE_SIZE) {
        int sy = w2s_y(es, (float)wy);
        if (sy < TOOLBAR_H || sy > TOOLBAR_H + CANVAS_H) continue;

        SDL_Rect line = { 0, sy, CANVAS_W, 1 };
        SDL_RenderFillRect(es->renderer, &line);
    }

    /* ---- Screen boundaries (blue, 2 px wide) ---- */
    SDL_SetRenderDrawColor(es->renderer, 0x4A, 0x90, 0xD9, 0xFF);

    for (int screen = 0; screen < WORLD_W / GAME_W; screen++) {
        int wx = screen * GAME_W;
        int sx = w2s_x(es, (float)wx);
        if (sx < -2 || sx > CANVAS_W + 2) continue;

        SDL_Rect line = { sx, TOOLBAR_H, 2, CANVAS_H };
        SDL_RenderFillRect(es->renderer, &line);
    }

    /* ---- FLOOR_Y line (red, 2 px wide) ---- */
    SDL_SetRenderDrawColor(es->renderer, 0xD9, 0x4A, 0x4A, 0xFF);

    int floor_sy = w2s_y(es, (float)FLOOR_Y);
    if (floor_sy >= TOOLBAR_H && floor_sy <= TOOLBAR_H + CANVAS_H) {
        SDL_Rect line = { 0, floor_sy, CANVAS_W, 2 };
        SDL_RenderFillRect(es->renderer, &line);
    }
}
