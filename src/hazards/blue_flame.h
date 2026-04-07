/*
 * blue_flame.h — Public interface for the BlueFlame entity.
 *
 * A BlueFlame is a hazard that erupts from a sea gap (hole in the floor),
 * rises above the tallest platform pillar, flips upside-down with a
 * rotation at the apex, and descends back into the hole.  The cycle
 * repeats indefinitely.
 *
 * The blue flame uses a 2-frame animation from blue_flame.png (96×48, two
 * 48×48 frames side by side) that plays during both the rise and fall.
 * At the apex the sprite rotates 180° over a short duration to visually
 * flip upside-down before descending.
 *
 * The blue flame damages the player on contact (1 heart, with knockback).
 */
#pragma once

#include <SDL.h>

/* ------------------------------------------------------------------ */

/*
 * BLUE_FLAME_FRAME_W / BLUE_FLAME_FRAME_H — size of one animation frame in the sheet.
 *
 * blue_flame.png is 96×48: two 48×48 frames laid out horizontally.
 */
#define BLUE_FLAME_FRAME_W  48
#define BLUE_FLAME_FRAME_H  48

/*
 * BLUE_FLAME_DISPLAY_W / BLUE_FLAME_DISPLAY_H — on-screen size in logical pixels.
 *
 * Rendered at 48×48 so the blue flame fits within the sea gap
 * and doesn't look oversized relative to other hazards.
 */
#define BLUE_FLAME_DISPLAY_W  48
#define BLUE_FLAME_DISPLAY_H  48

/*
 * BLUE_FLAME_FRAME_COUNT — number of animation frames in the sheet.
 */
#define BLUE_FLAME_FRAME_COUNT  2

/*
 * BLUE_FLAME_ANIM_SPEED — seconds between frame advances.
 *
 * 0.1 s = 10 fps, giving a flickering fire effect.
 */
#define BLUE_FLAME_ANIM_SPEED  0.1f

/*
 * BLUE_FLAME_LAUNCH_VY — initial upward impulse in logical px/s.
 *
 * -550 px/s gives a fast eruption from the hole.  The blue flame decelerates
 * naturally as BLUE_FLAME_RISE_DECEL slows it each frame, so it eases into
 * the apex smoothly.
 */
#define BLUE_FLAME_LAUNCH_VY    -550.0f

/*
 * BLUE_FLAME_RISE_DECEL — deceleration applied each frame during the rise (px/s²).
 *
 * Matches GRAVITY (800 px/s²) so the rise and fall are symmetric,
 * like a real projectile tossed upward.
 */
#define BLUE_FLAME_RISE_DECEL   800.0f

/*
 * BLUE_FLAME_APEX_Y — the world-space y coordinate the blue flame reaches at the top.
 *
 * Set to 60 px, which is well above the tallest 3-tile pillar top (y=124).
 * This makes the blue flame clearly erupting past all platforms.
 */
#define BLUE_FLAME_APEX_Y  60.0f

/*
 * BLUE_FLAME_FLIP_DURATION — seconds to rotate 180° at the apex.
 *
 * 0.12 s is very quick — the blue flame barely pauses before descending.
 */
#define BLUE_FLAME_FLIP_DURATION  0.12f

/*
 * BLUE_FLAME_WAIT_DURATION — seconds the blue flame stays hidden below the floor
 * before the next eruption cycle.
 */
#define BLUE_FLAME_WAIT_DURATION  1.5f

/* ------------------------------------------------------------------ */

#define MAX_BLUE_FLAMES 16  /* max BlueFlame instances in one level */

/*
 * BlueFlameState — the four phases of the blue flame's eruption cycle.
 *
 * BLUE_FLAME_WAITING  : hidden below the floor, counting down to next eruption.
 * BLUE_FLAME_RISING   : moving upward from the sea gap toward BLUE_FLAME_APEX_Y.
 * BLUE_FLAME_FLIPPING : at the apex, rotating 180° over BLUE_FLAME_FLIP_DURATION.
 * BLUE_FLAME_FALLING  : descending back into the sea gap, sprite upside-down.
 */
typedef enum {
    BLUE_FLAME_WAITING,
    BLUE_FLAME_RISING,
    BLUE_FLAME_FLIPPING,
    BLUE_FLAME_FALLING
} BlueFlameState;

/*
 * BlueFlame — all the data for one erupting blue flame hazard.
 *
 * gap_x      : world-space x of the sea gap this blue flame belongs to.
 * start_y    : the y where the blue flame begins (just below the floor).
 * x, y       : current world-space top-left corner of the display rect.
 * angle      : current rotation in degrees (0 = normal, 180 = upside-down).
 * state      : current phase (waiting, rising, flipping, falling).
 * timer      : accumulated seconds in the current state.
 * anim_timer : accumulated seconds for frame animation.
 * anim_frame : current frame index (0 or 1).
 * active     : 1 = alive and cycling, 0 = disabled.
 */
typedef struct {
    float          gap_x;
    float          start_y;
    float          x;
    float          y;
    float          vy;          /* vertical velocity for physics-based movement  */
    int            w;
    int            h;
    float          angle;
    BlueFlameState state;
    float          timer;
    float          anim_timer;
    int            anim_frame;
    int            active;
} BlueFlame;

/* ---- Function declarations ---------------------------------------- */

/*
 * blue_flames_init — Populate the blue flame array, placing one blue flame per sea gap.
 *
 * gap_xs is the array of sea gap x positions; gap_count is its length.
 * Only gaps that are not at the world edge (x > 0) get a blue flame.
 */
void blue_flames_init(BlueFlame *blue_flames, int *count,
                      const int *gap_xs, int gap_count);

/* Advance the eruption cycle (rise, flip, fall, wait) for all blue flames. */
void blue_flames_update(BlueFlame *blue_flames, int count, float dt);

/* Draw all active blue flames using the 2-frame animation and rotation. */
void blue_flames_render(const BlueFlame *blue_flames, int count,
                        SDL_Renderer *renderer, SDL_Texture *tex, int cam_x);

/* Return the collision rectangle in world space. */
SDL_Rect blue_flame_get_hitbox(const BlueFlame *blue_flame);
