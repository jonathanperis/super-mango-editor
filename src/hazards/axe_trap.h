/*
 * axe_trap.h — Public interface for the AxeTrap entity.
 *
 * An AxeTrap is a swinging axe mounted at the top-centre of a tall platform
 * pillar.  It swings like a grandfather clock pendulum — slowly oscillating
 * back and forth with a sinusoidal motion.  One variant performs a continuous
 * full 360° rotation instead of pendulum swings.
 *
 * The axe damages the player on contact (1 heart, with knockback).
 * A sound effect plays each time the axe reaches the extremes of its swing
 * (or completes a full rotation for the spinning variant).
 */
#pragma once

#include "../shared/graphics.h"
#include "../shared/audio.h"
#include "hazard_defs.h"  /* AXE_FRAME_*, AXE_DISPLAY_*, MAX_AXE_TRAPS, AxeTrapMode */

/* ------------------------------------------------------------------ */

/*
 * AXE_SWING_AMPLITUDE — maximum pendulum angle in degrees from vertical.
 *
 * The axe swings from −60° to +60° (120° total arc), matching the wide
 * sweep of a grandfather clock pendulum.
 */
#define AXE_SWING_AMPLITUDE  60.0f

/*
 * AXE_SWING_PERIOD — time for one full pendulum cycle (left → right → left)
 * in seconds.  2.0 s gives a stately, ominous rhythm.
 */
#define AXE_SWING_PERIOD  2.0f

/*
 * AXE_SPIN_SPEED — degrees per second for the full-rotation variant.
 * 180°/s = one full rotation every 2 seconds.
 */
#define AXE_SPIN_SPEED  180.0f

/*
 * AxeTrap — all the data for one swinging/spinning axe hazard.
 *
 * The pivot point (x, y) is the top-centre of the axe handle — this is
 * the point around which the sprite rotates.  It is placed at the
 * horizontal centre of the column, at the top of the pillar.
 *
 * angle         : current rotation in degrees.  0° = handle up, blade down.
 *                 Positive = clockwise.
 * time          : seconds into the current swing cycle (0 .. AXE_SWING_PERIOD,
 *                 wrapping each cycle); drives the pendulum sine wave.
 * mode          : AXE_MODE_PENDULUM or AXE_MODE_SPIN.
 * sound_played  : prevents the swing SFX from retriggering on the same
 *                 half-cycle.  Resets when the axe crosses centre.
 * active        : 1 = active hazard, 0 = disabled.
 */
typedef struct {
    float        x;            /* pivot x in world space (top-centre of handle)  */
    float        y;            /* pivot y in world space                          */
    float        angle;        /* current rotation degrees (0 = straight down)   */
    float        time;         /* seconds into the swing cycle (wraps)            */
    AxeTrapMode  mode;         /* pendulum or full-spin                           */
    int          sound_played; /* 1 = SFX already fired this half-swing           */
    int          active;       /* 1 = hazard is live                              */
} AxeTrap;

/* ---- Function declarations ---------------------------------------- */

/*
 * axe_traps_update — Advance the swing/spin animation for all traps.
 *
 * dt is delta time in seconds.  snd_axe is played when the axe reaches
 * a swing extreme (pendulum) or completes a full rotation (spin).
 * player_x is the player's world-space x position — used to scale the
 * sound volume based on proximity (louder when closer, silent off-screen).
 * cam_x is the camera left edge — axes off-screen are silent.
 */
void axe_traps_update(AxeTrap *traps, int count, float dt,
                      SoundEffect *snd_axe, float player_x, int cam_x);

/*
 * axe_traps_render — Draw all active traps around the handle pivot.
 *
 * tex is the shared Axe_Trap.png texture.  cam_x converts world → screen.
 */
void axe_traps_render(const AxeTrap *traps, int count,
                      Texture2D *tex, int cam_x);

/*
 * axe_trap_get_hitbox — Return the collision rectangle in world space.
 *
 * The hitbox approximates the blade area of the axe at its current angle.
 * For simplicity we use the full rotated bounding box of the lower half
 * of the sprite (the blade region).
 */
IntRect axe_trap_get_hitbox(const AxeTrap *trap);
