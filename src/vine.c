/*
 * vine.c — Static vine/plant decorations placed on the ground and platforms.
 *
 * Vines are placed once at init time.  No state changes after that —
 * vine_render just blits each instance with the camera offset applied.
 *
 * Ground placement: one vine every ~130–160 logical px across WORLD_W,
 * chosen to avoid overlapping platform pillars (each 48 px wide).
 *
 * Platform placement: four of the eight pillars get a vine on their top
 * surface — every other pillar for a natural, irregular appearance.
 * Each pillar is 48 px wide; the 16-px vine is offset 4 px from the
 * pillar's left edge so it sits visually near the centre.
 *
 * Coordinate reference  (GAME_H=300, TILE_SIZE=48, FLOOR_Y=252):
 *   Ground vine top  = FLOOR_Y  - VINE_H  = 252 - 48 = 204
 *   Platform vine top = platform.y - VINE_H
 *     Pillar 0 (x= 80, y=156): vine top = 156 - 48 = 108
 *     Pillar 3 (x=680, y=108): vine top = 108 - 48 =  60
 *     Pillar 4 (x=880, y=156): vine top = 156 - 48 = 108
 *     Pillar 6 (x=1300,y=156): vine top = 156 - 48 = 108
 */
#include "vine.h"
#include "game.h"   /* FLOOR_Y, TILE_SIZE */

/* ------------------------------------------------------------------ */

void vine_init(VineDecor *vines, int *count)
{
    int n = 0;
    /* Embed vines 14 px into the surface so they look well planted.
     * x positions are chosen to keep at least ~32 px clear of any coin
     * so vines and coins never appear stacked on the same spot.       */
    const int   embed     = 14;
    const float ground_y  = (float)(FLOOR_Y - VINE_H + embed);  /* = 218 */

    /* ── Ground vines ─────────────────────────────────────────────── */
    /* Spread across all four screens, avoiding the 48-px pillar slots.
     * Coins sit at ground x ≈ 30, 170, 350, 430, 595, 760, 820, 965,
     * 1130, 1230, 1390, 1555 — vines are offset away from each.      */

    vines[n++] = (VineDecor){ 58.0f,   ground_y }; /* screen 1 left  (coin@30 cleared) */
    vines[n++] = (VineDecor){ 210.0f,  ground_y }; /* screen 1 mid   (coin@170 cleared)*/
    vines[n++] = (VineDecor){ 318.0f,  ground_y }; /* screen 1 right (coin@350 cleared)*/
    vines[n++] = (VineDecor){ 468.0f,  ground_y }; /* screen 2 left  (coin@430 cleared)*/
    vines[n++] = (VineDecor){ 635.0f,  ground_y }; /* screen 2 mid   (coin@595 cleared)*/
    vines[n++] = (VineDecor){ 800.0f,  ground_y }; /* screen 2 right (coin@760 ok)     */
    vines[n++] = (VineDecor){ 1010.0f, ground_y }; /* screen 3       (coin@965 cleared)*/
    vines[n++] = (VineDecor){ 1165.0f, ground_y }; /* screen 3 right (coin@1130 cleared*/
    vines[n++] = (VineDecor){ 1265.0f, ground_y }; /* screen 4 left  (coin@1230 cleared*/
    vines[n++] = (VineDecor){ 1420.0f, ground_y }; /* screen 4 right (coin@1390 ok)    */

    /* ── Platform vines ───────────────────────────────────────────── */
    /* Placed on alternating pillars; offset +4 px so the vine sits
     * near the visual centre of the 48-px wide pillar top.          */

    /* Pillar 0 — x=80,  top y=156  (medium, 2 tiles tall)           */
    vines[n++] = (VineDecor){ 84.0f,   (float)(FLOOR_Y - 2 * TILE_SIZE - VINE_H + embed) };

    /* Pillar 3 — x=680, top y=108  (tall,   3 tiles tall)           */
    vines[n++] = (VineDecor){ 684.0f,  (float)(FLOOR_Y - 3 * TILE_SIZE - VINE_H + embed) };

    /* Pillar 4 — x=880, top y=156  (medium, 2 tiles tall)           */
    vines[n++] = (VineDecor){ 884.0f,  (float)(FLOOR_Y - 2 * TILE_SIZE - VINE_H + embed) };

    /* Pillar 6 — x=1300, top y=156 (medium, 2 tiles tall)           */
    vines[n++] = (VineDecor){ 1304.0f, (float)(FLOOR_Y - 2 * TILE_SIZE - VINE_H + embed) };

    *count = n;
}

/* ------------------------------------------------------------------ */

void vine_render(const VineDecor *vines, int count,
                 SDL_Renderer *renderer, SDL_Texture *tex, int cam_x)
{
    for (int i = 0; i < count; i++) {
        const VineDecor *v = &vines[i];

        SDL_Rect dst = {
            (int)v->x - cam_x,
            (int)v->y,
            VINE_W,
            VINE_H
        };

        /* Skip instances fully outside the current viewport */
        if (dst.x + VINE_W < 0 || dst.x >= 400) continue;

        SDL_RenderCopy(renderer, tex, NULL, &dst);
    }
}
