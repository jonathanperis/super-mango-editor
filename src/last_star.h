/*
 * last_star.h — Public interface for the LastStar entity.
 *
 * The LastStar is a collectible placed at the end of the level that
 * triggers the "phase passed" event when the player picks it up.
 * It uses the Stars_Ui.png sprite (same as the HUD heart icon).
 *
 * For now, collecting the star sets a flag — the actual phase
 * transition behaviour will be implemented later.
 */
#pragma once

#include <SDL.h>

/* ------------------------------------------------------------------ */

#define LAST_STAR_DISPLAY_W  24   /* on-screen width  in logical pixels  */
#define LAST_STAR_DISPLAY_H  24   /* on-screen height in logical pixels  */

/*
 * LastStar — the end-of-level collectible.
 *
 * x, y    : world-space top-left corner.
 * active  : 1 = waiting to be collected, 0 = already collected.
 * collected: 1 = player picked it up (triggers phase-passed event).
 */
typedef struct {
    float x;
    float y;
    int   w;
    int   h;
    int   active;
    int   collected;
} LastStar;

/* ---- Function declarations ---------------------------------------- */

/* Place the last star at its end-phase position. */
void last_star_init(LastStar *star);

/* Draw the star if still active. */
void last_star_render(const LastStar *star,
                      SDL_Renderer *renderer, SDL_Texture *tex, int cam_x);

/* Return the collision rectangle in world space. */
SDL_Rect last_star_get_hitbox(const LastStar *star);
