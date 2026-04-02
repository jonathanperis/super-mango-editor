/*
 * bouncepad.h — Public interface for the Bouncepad module.
 *
 * A bouncepad is a wooden spring platform that launches the player upward
 * with a larger-than-normal jump impulse.  When the player lands on it the
 * pad plays a 3-frame squash/release animation (frame 2 → 1 → 0) driven by
 * an accumulated millisecond timer, then returns to its compressed idle pose.
 *
 * Sprite sheet: assets/Bouncepad_Wood.png  (144×48 px, 3 cols × 1 row)
 *   Frame 0 (x=  0): extended / post-launch
 *   Frame 1 (x= 48): mid-compress
 *   Frame 2 (x= 96): fully compressed — default idle state
 */
#pragma once

#include <SDL.h>   /* SDL_Renderer, SDL_Texture, SDL_Rect */

/* ------------------------------------------------------------------ */
/* Constants                                                           */
/* ------------------------------------------------------------------ */

/* Maximum bouncepad instances the game can hold simultaneously. */
#define MAX_BOUNCEPADS   4

/* Display size of one bouncepad frame in logical pixels. */
#define BOUNCEPAD_W      48
#define BOUNCEPAD_H      48

/*
 * Bounce impulse presets — three strength tiers.
 *
 * BOUNCEPAD_VY_SMALL  : gentle hop — clears 1-tile pillars but not 2-tile.
 * BOUNCEPAD_VY_MEDIUM : standard launch — clears 2-tile pillars comfortably.
 * BOUNCEPAD_VY_HIGH   : powerful launch — clears 3-tile pillars with margin.
 *
 * Negative because SDL's Y axis increases downward (up = negative vy).
 */
#define BOUNCEPAD_VY_SMALL   -380.0f
#define BOUNCEPAD_VY_MEDIUM  -536.25f
#define BOUNCEPAD_VY_HIGH    -700.0f

/* Legacy define — kept for existing code that references it. */
#define BOUNCEPAD_VY  BOUNCEPAD_VY_MEDIUM

/*
 * BouncepadType — selects which texture variant to use.
 */
typedef enum {
    BOUNCEPAD_GREEN,   /* small jump — Bouncepad_Green.png  */
    BOUNCEPAD_WOOD,    /* medium jump — Bouncepad_Wood.png  */
    BOUNCEPAD_RED      /* high jump — Bouncepad_Red.png     */
} BouncepadType;

/*
 * BOUNCEPAD_FRAME_MS — how many milliseconds each animation frame is shown
 * during the release sequence (frame 1, then frame 0).
 */
#define BOUNCEPAD_FRAME_MS  80

/*
 * BOUNCEPAD_SRC_Y / BOUNCEPAD_SRC_H — vertical crop of the sprite sheet.
 *
 * Pixel analysis of Bouncepad_Wood.png shows the actual art occupies
 * rows 14–31 (18 px) within each 48-px frame; the remaining rows are
 * fully transparent padding.  Cropping to this region lets us position
 * the pad so its art bottom lands exactly on FLOOR_Y without rendering
 * invisible transparent space below it.
 */
#define BOUNCEPAD_SRC_Y  14   /* first non-transparent row in the frame  */
#define BOUNCEPAD_SRC_H  18   /* height of the art region (rows 14–31)   */

/*
 * BOUNCEPAD_ART_X / BOUNCEPAD_ART_W — horizontal crop of each frame.
 *
 * The art occupies cols 16–31 (16 px) within each 48-px frame slot;
 * the left 16 px and right 16 px are fully transparent padding.
 * These constants define the collision box width so it matches the
 * visible art, not the full frame slot.
 */
#define BOUNCEPAD_ART_X  16   /* first non-transparent col in the frame  */
#define BOUNCEPAD_ART_W  16   /* width  of the art region (cols 16–31)   */

/* ------------------------------------------------------------------ */
/* Types                                                               */
/* ------------------------------------------------------------------ */

/*
 * BounceState — whether the pad is at rest or in the middle of its
 * squash/release animation.
 */
typedef enum {
    BOUNCE_IDLE,    /* pad is at rest; shows frame 2 (compressed)   */
    BOUNCE_ACTIVE   /* release animation is running (frames 1 → 0)  */
} BounceState;

/*
 * Bouncepad — all data needed for one bouncepad instance.
 *
 * Positions use float for consistency with the rest of the physics system;
 * they are cast to int at render time when building SDL_Rect.
 */
typedef struct {
    float        x;             /* left edge in world-space logical pixels   */
    float        y;             /* top  edge in world-space logical pixels   */
    int          w;             /* display width  (BOUNCEPAD_W = 48 px)      */
    int          h;             /* display height (BOUNCEPAD_H = 48 px)      */
    BounceState  state;         /* IDLE or ACTIVE                            */
    int          anim_frame;    /* current displayed frame index (0, 1, or 2)*/
    Uint32       anim_timer_ms; /* ms accumulated in the current anim frame  */
    float        launch_vy;     /* upward impulse applied to player on land  */
    BouncepadType pad_type;     /* GREEN / WOOD / RED — selects texture      */
} Bouncepad;

/* ------------------------------------------------------------------ */
/* Function declarations                                               */
/* ------------------------------------------------------------------ */

/*
 * bouncepad_place — Initialise one bouncepad instance at a given position.
 *
 * Sets all fields to their correct defaults (BOUNCE_IDLE, anim_frame=2,
 * standard dimensions) so the individual variant inits (bouncepad_small.c,
 * bouncepad_medium.c, bouncepad_high.c) only need to specify the parts that
 * differ: x position, launch impulse, and pad type.
 *
 * Parameters:
 *   pad       : pointer to the Bouncepad to initialise.
 *   x         : world-space left-edge position in logical pixels.
 *   launch_vy : upward impulse applied to the player on landing (negative).
 *   pad_type  : BOUNCEPAD_GREEN / BOUNCEPAD_WOOD / BOUNCEPAD_RED.
 */
void bouncepad_place(Bouncepad *pad, float x, float launch_vy, BouncepadType pad_type);

/* Place all bouncepads at their initial world positions. */
void bouncepads_init(Bouncepad *pads, int *count);

/*
 * Advance the release animation for every ACTIVE pad.
 * dt_ms is delta time converted to milliseconds (dt * 1000).
 */
void bouncepads_update(Bouncepad *pads, int count, Uint32 dt_ms);

/* Draw each bouncepad at its current animation frame, offset by cam_x. */
void bouncepads_render(const Bouncepad *pads, int count,
                       SDL_Renderer *renderer, SDL_Texture *tex, int cam_x);
