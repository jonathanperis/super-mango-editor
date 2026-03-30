/*
 * flame.h — Public interface for the Flame entity.
 *
 * A Flame is a hazard that erupts from a sea gap (hole in the floor),
 * rises above the tallest platform pillar, flips upside-down with a
 * rotation at the apex, and descends back into the hole.  The cycle
 * repeats indefinitely.
 *
 * The flame uses a 2-frame animation from Flame_2.png (96×48, two
 * 48×48 frames side by side) that plays during both the rise and fall.
 * At the apex the sprite rotates 180° over a short duration to visually
 * flip upside-down before descending.
 *
 * The flame damages the player on contact (1 heart, with knockback).
 */
#pragma once

#include <SDL.h>

/* ------------------------------------------------------------------ */

/*
 * FLAME_FRAME_W / FLAME_FRAME_H — size of one animation frame in the sheet.
 *
 * Flame_2.png is 96×48: two 48×48 frames laid out horizontally.
 */
#define FLAME_FRAME_W  48
#define FLAME_FRAME_H  48

/*
 * FLAME_DISPLAY_W / FLAME_DISPLAY_H — on-screen size in logical pixels.
 *
 * Rendered at 32×32 so the flame fits within the 32-px-wide sea gap
 * and doesn't look oversized relative to other hazards.
 */
#define FLAME_DISPLAY_W  48
#define FLAME_DISPLAY_H  48

/*
 * FLAME_FRAME_COUNT — number of animation frames in the sheet.
 */
#define FLAME_FRAME_COUNT  2

/*
 * FLAME_ANIM_SPEED — seconds between frame advances.
 *
 * 0.1 s = 10 fps, giving a flickering fire effect.
 */
#define FLAME_ANIM_SPEED  0.1f

/*
 * FLAME_LAUNCH_VY — initial upward impulse in logical px/s.
 *
 * -500 px/s gives a fast eruption from the hole.  The flame decelerates
 * naturally as FLAME_RISE_DECEL slows it each frame, so it eases into
 * the apex smoothly.
 */
#define FLAME_LAUNCH_VY    -550.0f

/*
 * FLAME_RISE_DECEL — deceleration applied each frame during the rise (px/s²).
 *
 * Matches GRAVITY (800 px/s²) so the rise and fall are symmetric,
 * like a real projectile tossed upward.
 */
#define FLAME_RISE_DECEL   800.0f

/*
 * FLAME_APEX_Y — the world-space y coordinate the flame reaches at the top.
 *
 * Set to 60 px, which is well above the tallest 3-tile pillar top (y=124).
 * This makes the flame clearly erupting past all platforms.
 */
#define FLAME_APEX_Y  60.0f

/*
 * FLAME_FLIP_DURATION — seconds to rotate 180° at the apex.
 *
 * 0.12 s is very quick — the flame barely pauses before descending.
 */
#define FLAME_FLIP_DURATION  0.12f

/*
 * FLAME_WAIT_DURATION — seconds the flame stays hidden below the floor
 * before the next eruption cycle.
 */
#define FLAME_WAIT_DURATION  1.5f

/* ------------------------------------------------------------------ */

#define MAX_FLAMES  8  /* max Flame instances in one level */

/*
 * FlameState — the four phases of the flame's eruption cycle.
 *
 * FLAME_WAITING  : hidden below the floor, counting down to next eruption.
 * FLAME_RISING   : moving upward from the sea gap toward FLAME_APEX_Y.
 * FLAME_FLIPPING : at the apex, rotating 180° over FLAME_FLIP_DURATION.
 * FLAME_FALLING  : descending back into the sea gap, sprite upside-down.
 */
typedef enum {
    FLAME_WAITING,
    FLAME_RISING,
    FLAME_FLIPPING,
    FLAME_FALLING
} FlameState;

/*
 * Flame — all the data for one erupting flame hazard.
 *
 * gap_x      : world-space x of the sea gap this flame belongs to.
 * start_y    : the y where the flame begins (just below the floor).
 * x, y       : current world-space top-left corner of the display rect.
 * angle      : current rotation in degrees (0 = normal, 180 = upside-down).
 * state      : current phase (waiting, rising, flipping, falling).
 * timer      : accumulated seconds in the current state.
 * anim_timer : accumulated seconds for frame animation.
 * anim_frame : current frame index (0 or 1).
 * active     : 1 = alive and cycling, 0 = disabled.
 */
typedef struct {
    float      gap_x;
    float      start_y;
    float      x;
    float      y;
    float      vy;          /* vertical velocity for physics-based movement  */
    int        w;
    int        h;
    float      angle;
    FlameState state;
    float      timer;
    float      anim_timer;
    int        anim_frame;
    int        active;
} Flame;

/* ---- Function declarations ---------------------------------------- */

/*
 * flames_init — Populate the flame array, placing one flame per sea gap.
 *
 * gap_xs is the array of sea gap x positions; gap_count is its length.
 * Only gaps that are not at the world edge (x > 0) get a flame.
 */
void flames_init(Flame *flames, int *count,
                 const int *gap_xs, int gap_count);

/* Advance the eruption cycle (rise, flip, fall, wait) for all flames. */
void flames_update(Flame *flames, int count, float dt);

/* Draw all active flames using the 2-frame animation and rotation. */
void flames_render(const Flame *flames, int count,
                   SDL_Renderer *renderer, SDL_Texture *tex, int cam_x);

/* Return the collision rectangle in world space. */
SDL_Rect flame_get_hitbox(const Flame *flame);
