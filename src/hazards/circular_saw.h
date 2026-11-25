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

#include "../shared/graphics.h"
#include "hazard_defs.h"  /* SAW_FRAME_*, SAW_DISPLAY_*, MAX_CIRCULAR_SAWS */

/* ------------------------------------------------------------------ */

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
 * SAW_PUSH_VY    : upward bounce component (negative = up on screen).
 */
#define SAW_PUSH_SPEED  220.0f
#define SAW_PUSH_VY    (-150.0f)

/* ------------------------------------------------------------------ */

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

/* Advance patrol movement and spin animation for all saws. */
void circular_saws_update(CircularSaw *saws, int count, float dt);

/* Draw all active saws around their centre pivot. */
void circular_saws_render(const CircularSaw *saws, int count,
                          Texture2D *tex, int cam_x);

/* Return the collision rectangle in world space. */
IntRect circular_saw_get_hitbox(const CircularSaw *saw);
