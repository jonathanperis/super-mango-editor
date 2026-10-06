/*
 * health_star.c — Rendering and hitbox for the yellow/green/red health stars.
 *
 * Placement is handled by the level loader from LevelDef data; collection
 * (restore a heart) lives in game_collision.c.
 */

#include "health_star.h"

/* ------------------------------------------------------------------ */

IntRect health_star_get_hitbox(const HealthStar *star)
{
    IntRect r = {
        (int)star->x,
        (int)star->y,
        HEALTH_STAR_DISPLAY_W,
        HEALTH_STAR_DISPLAY_H
    };
    return r;
}

/* ------------------------------------------------------------------ */

void health_stars_render(const HealthStar *stars, int count,
                         Texture2D *tex, int cam_x)
{
    if (!tex) return;

    for (int i = 0; i < count; i++) {
        if (!stars[i].active) continue;

        /* World x minus the camera's left edge gives the screen x. */
        IntRect dst = health_star_get_hitbox(&stars[i]);
        dst.x -= cam_x;

        sprite_draw(tex, NULL, &dst, 0, SPRITE_NORMAL, WHITE);
    }
}
