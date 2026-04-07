/*
 * entity_utils.h — Shared utility functions for entity behaviour.
 *
 * These helpers eliminate the boilerplate that is repeated verbatim in
 * every enemy and hazard module:
 *
 *   - animate_frame_ms  : advance a sprite-sheet animation driven by an
 *                          accumulated millisecond timer.
 *   - patrol_update     : move an entity horizontally and reverse it at
 *                          its patrol boundaries.
 *   - patrol_gap_reverse: reverse direction when the entity's art centre
 *                          enters a sea-gap (hole in the ground floor).
 *
 * Include this header in any .c file that implements patrol-based movement
 * or frame-based animation.  No SDL2 types are needed here — the functions
 * only operate on primitive C types.
 */
#pragma once

#include <SDL.h>  /* Uint32 */

/* ------------------------------------------------------------------ */
/* Animation                                                           */
/* ------------------------------------------------------------------ */

/*
 * animate_frame_ms — advance a frame-based sprite animation by one game tick.
 *
 * Accumulates elapsed time in *timer_ms.  Once the accumulated time exceeds
 * frame_ms, the current frame advances by one (wrapping at frame_count).
 *
 * Parameters:
 *   frame_index  : pointer to the current frame index — updated in place.
 *   timer_ms     : pointer to the accumulated time counter — updated in place.
 *   dt           : seconds elapsed this frame (e.g. 0.016 at 60 FPS).
 *   frame_ms     : how many milliseconds each frame is shown before advancing.
 *   frame_count  : total number of frames in this animation loop.
 *
 * Returns 1 if the animation wrapped back to frame 0 this tick (useful for
 * triggering one-shot effects like a sound at the start of each cycle),
 * 0 if the frame did not advance or did not wrap.
 *
 * Usage:
 *   int wrapped = animate_frame_ms(&s->frame_index, &s->anim_timer_ms,
 *                                  dt, SPIDER_FRAME_MS, SPIDER_FRAMES);
 */
int animate_frame_ms(int *frame_index, Uint32 *timer_ms,
                     float dt, Uint32 frame_ms, int frame_count);

/* ------------------------------------------------------------------ */
/* Horizontal patrol movement                                          */
/* ------------------------------------------------------------------ */

/*
 * patrol_update — move entity horizontally and reverse at patrol boundaries.
 *
 * Advances x by vx * dt, then checks if the entity has reached or passed
 * either end of its patrol range.  On reaching the right boundary (patrol_x1)
 * the entity is snapped to the boundary and its velocity is set to -speed.
 * On reaching the left boundary (patrol_x0) it is snapped and set to +speed.
 *
 * Parameters:
 *   x          : pointer to entity world-space x — updated in place.
 *   vx         : pointer to horizontal velocity — updated in place.
 *   entity_w   : display width of the entity (px); used for right-edge snap.
 *   patrol_x0  : left  boundary of the patrol range in world-space px.
 *   patrol_x1  : right boundary of the patrol range in world-space px.
 *   speed      : absolute patrol speed in px/s; always a positive value.
 *   dt         : seconds elapsed this frame.
 *
 * Usage:
 *   patrol_update(&s->x, &s->vx, SPIDER_FRAME_W,
 *                 s->patrol_x0, s->patrol_x1, SPIDER_SPEED, dt);
 */
void patrol_update(float *x, float *vx, float entity_w,
                   float patrol_x0, float patrol_x1,
                   float speed, float dt);

/* ------------------------------------------------------------------ */
/* Sea-gap avoidance                                                   */
/* ------------------------------------------------------------------ */

/* ------------------------------------------------------------------ */
/* Audio                                                               */
/* ------------------------------------------------------------------ */

/*
 * sound_volume_for_distance — compute a Mix_Chunk volume (0–max_volume)
 * that falls off linearly with horizontal distance.
 *
 * At dist == 0              → max_volume  (loudest).
 * At dist == audible_range  → 0          (silent).
 * Beyond audible_range      → 0          (silent, clamped).
 *
 * Parameters:
 *   dist          : absolute horizontal distance from player to sound source (px).
 *   audible_range : distance at which the sound becomes completely silent (px).
 *   max_volume    : volume returned when the player is at the source (0–128).
 *
 * Usage:
 *   int vol = sound_volume_for_distance(fabsf(player_cx - source_cx),
 *                                       (float)GAME_W, 128);
 *   Mix_PlayChannel(-1, snd, 0);
 *   Mix_Volume(channel, vol);
 */
int sound_volume_for_distance(float dist, float audible_range, int max_volume);

/* ------------------------------------------------------------------ */
/* Sea-gap avoidance                                                   */
/* ------------------------------------------------------------------ */

/*
 * patrol_gap_reverse — reverse direction when art centre enters a floor gap.
 *
 * Floor gaps are holes in the ground floor at known x positions (each
 * FLOOR_GAP_W pixels wide).  Ground-patrol enemies must not walk across them.
 * This function checks whether the centre of the entity's visible art region
 * is currently over any gap and, if so, snaps the entity back to the gap
 * edge and reverses its velocity.
 *
 * Parameters:
 *   x              : pointer to entity world-space x — updated in place.
 *   vx             : pointer to horizontal velocity — updated in place.
 *   art_x_offset   : distance (px) from entity.x to the left edge of the art.
 *   art_w          : width (px) of the visible art region.
 *   speed          : absolute patrol speed in px/s (always positive).
 *   floor_gaps     : array of gap left-edge x positions in world-space px.
 *   floor_gap_count: number of entries in floor_gaps.
 *   floor_gap_w    : width of each gap in px (pass FLOOR_GAP_W from game.h).
 *
 * Usage:
 *   patrol_gap_reverse(&s->x, &s->vx,
 *                      SPIDER_ART_X, SPIDER_ART_W, SPIDER_SPEED,
 *                      floor_gaps, floor_gap_count, FLOOR_GAP_W);
 */
void patrol_gap_reverse(float *x, float *vx,
                        float art_x_offset, float art_w, float speed,
                        const int *floor_gaps, int floor_gap_count, int floor_gap_w);
