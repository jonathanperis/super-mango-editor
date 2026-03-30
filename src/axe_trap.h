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

#include <SDL.h>       /* SDL_Texture, SDL_Renderer, SDL_Rect */
#include <SDL_mixer.h> /* Mix_Chunk — swinging-axe sound       */

/* ------------------------------------------------------------------ */

/*
 * AXE_FRAME_W / AXE_FRAME_H — source dimensions of Axe_Trap.png.
 *
 * The sprite is a single 48×64 px image (no sprite sheet grid).
 * The handle is at the top; the blade fans out at the bottom.
 */
#define AXE_FRAME_W  48
#define AXE_FRAME_H  64

/*
 * AXE_DISPLAY_W / AXE_DISPLAY_H — on-screen size in logical pixels.
 *
 * Rendered at native sprite size so each pixel maps 1:1 in the 400×300
 * logical canvas (then 2× scaled to the 800×600 OS window).
 */
#define AXE_DISPLAY_W  48
#define AXE_DISPLAY_H  64

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
 * MAX_AXE_TRAPS — upper bound on how many axe traps the level can hold.
 * Stored as a fixed-size array inside GameState.
 */
#define MAX_AXE_TRAPS  4

/* ------------------------------------------------------------------ */

/*
 * AxeTrapMode — selects the swing behaviour for each trap instance.
 *
 * AXE_MODE_PENDULUM : grandfather-clock sinusoidal swing (−60° to +60°).
 * AXE_MODE_SPIN     : continuous 360° clockwise rotation.
 */
typedef enum {
    AXE_MODE_PENDULUM,
    AXE_MODE_SPIN
} AxeTrapMode;

/*
 * AxeTrap — all the data for one swinging/spinning axe hazard.
 *
 * The pivot point (x, y) is the top-centre of the axe handle — this is
 * the point around which the sprite rotates.  It is placed at the
 * horizontal centre of the column, at the top of the pillar.
 *
 * angle         : current rotation in degrees.  0° = handle up, blade down.
 *                 Positive = clockwise.
 * time          : accumulated time in seconds, drives the sine oscillation
 *                 for pendulum mode.
 * mode          : AXE_MODE_PENDULUM or AXE_MODE_SPIN.
 * sound_played  : prevents the swing SFX from retriggering on the same
 *                 half-cycle.  Resets when the axe crosses centre.
 * active        : 1 = active hazard, 0 = disabled.
 */
typedef struct {
    float        x;            /* pivot x in world space (top-centre of handle)  */
    float        y;            /* pivot y in world space                          */
    float        angle;        /* current rotation degrees (0 = straight down)   */
    float        time;         /* accumulated seconds for pendulum sine wave      */
    AxeTrapMode  mode;         /* pendulum or full-spin                           */
    int          sound_played; /* 1 = SFX already fired this half-swing           */
    int          active;       /* 1 = hazard is live                              */
} AxeTrap;

/* ---- Function declarations ---------------------------------------- */

/*
 * axe_traps_init — Populate the axe trap array with level placements.
 *
 * Places axe traps centred on the tall (3-tile) platform pillars.
 * Writes into `traps[]` and sets *count to the number placed.
 */
void axe_traps_init(AxeTrap *traps, int *count);

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
                      Mix_Chunk *snd_axe, float player_x, int cam_x);

/*
 * axe_traps_render — Draw all active traps using SDL_RenderCopyEx rotation.
 *
 * tex is the shared Axe_Trap.png texture.  cam_x converts world → screen.
 */
void axe_traps_render(const AxeTrap *traps, int count,
                      SDL_Renderer *renderer, SDL_Texture *tex, int cam_x);

/*
 * axe_trap_get_hitbox — Return the collision rectangle in world space.
 *
 * The hitbox approximates the blade area of the axe at its current angle.
 * For simplicity we use the full rotated bounding box of the lower half
 * of the sprite (the blade region).
 */
SDL_Rect axe_trap_get_hitbox(const AxeTrap *trap);
