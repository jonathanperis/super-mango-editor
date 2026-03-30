/*
 * circular_saw.h — Public interface for the CircularSaw entity.
 *
 * A CircularSaw is a fast-patrolling, continuously rotating hazard that
 * moves back and forth along a horizontal line at ground level.  Unlike
 * the SpikeBlock it does not ride a Rail — it simply bounces between two
 * world-space x coordinates.
 *
 * The saw damages the player on contact (1 heart, with knockback) using
 * the same apply_damage path as spike blocks and axe traps.
 */
#pragma once

#include <SDL.h>
#include "player.h"  /* Player — for the push response signature */

/* ------------------------------------------------------------------ */

/*
 * SAW_FRAME_W / SAW_FRAME_H — source dimensions of Circular_Saw.png.
 *
 * The sprite is a single 32×32 px image (no sprite sheet grid).
 */
#define SAW_FRAME_W  32
#define SAW_FRAME_H  32

/*
 * SAW_DISPLAY_W / SAW_DISPLAY_H — on-screen size in logical pixels.
 *
 * Rendered at native sprite size so each pixel maps 1:1 in the 400×300
 * logical canvas (then 2× scaled to the 800×600 OS window).
 */
#define SAW_DISPLAY_W  32
#define SAW_DISPLAY_H  32

/*
 * SAW_SPIN_DEG_PER_SEC — rotation speed in degrees per second.
 *
 * 720°/s = two full rotations per second, giving a fast buzzing visual
 * cue that clearly signals danger.
 */
#define SAW_SPIN_DEG_PER_SEC  720.0f

/*
 * SAW_PATROL_SPEED — horizontal patrol speed in logical pixels per second.
 *
 * 180 px/s is noticeably faster than the player's 200 px/s walk speed,
 * making the saw a real threat that the player must time their crossing
 * around.
 */
#define SAW_PATROL_SPEED  180.0f

/*
 * Push impulse constants — applied to the player on contact.
 *
 * SAW_PUSH_SPEED : magnitude of the push velocity vector (px/s).
 * SAW_PUSH_VY    : upward bounce component (negative = up in SDL).
 */
#define SAW_PUSH_SPEED  220.0f
#define SAW_PUSH_VY    -150.0f

/* ------------------------------------------------------------------ */

#define MAX_CIRCULAR_SAWS  4  /* max CircularSaw instances in one level */

/*
 * CircularSaw — a fast rotating hazard that patrols a horizontal line.
 *
 * x, y       : world-space top-left corner of the display rect.
 * patrol_x0  : left limit of the patrol line (world-space x).
 * patrol_x1  : right limit of the patrol line (world-space x).
 * direction  : +1 = moving right, -1 = moving left.
 * spin_angle : current rotation in degrees [0, 360) — purely visual.
 * active     : 1 = alive and rendered, 0 = disabled.
 */
typedef struct {
    float x;
    float y;
    int   w;
    int   h;
    float patrol_x0;
    float patrol_x1;
    int   direction;
    float spin_angle;
    int   active;
} CircularSaw;

/* ---- Function declarations ---------------------------------------- */

/* Populate the circular saw array with level placements. */
void circular_saws_init(CircularSaw *saws, int *count);

/* Advance patrol movement and spin animation for all saws. */
void circular_saws_update(CircularSaw *saws, int count, float dt);

/* Draw all active saws using SDL_RenderCopyEx rotation. */
void circular_saws_render(const CircularSaw *saws, int count,
                          SDL_Renderer *renderer, SDL_Texture *tex, int cam_x);

/* Return the collision rectangle in world space. */
SDL_Rect circular_saw_get_hitbox(const CircularSaw *saw);

/* Apply push impulse to the player on contact. */
void circular_saw_push_player(const CircularSaw *saw, Player *player);
