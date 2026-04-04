/*
 * spider.h — Public interface for the spider enemy module.
 *
 * Sprite facts (Spider_1.png, 192×48):
 *   - 3 animation frames, each 64×48 px in the sheet.
 *   - Visible art occupies y=22..31 (SPIDER_ART_H = 10 px) per frame.
 *   - Top 22 px and bottom 16 px of each frame are transparent.
 *   - Horizontal art varies per frame but each full 64-px wide slot is
 *     rendered; alpha-blending handles the per-frame transparent margins.
 *
 * Behaviour:
 *   - Spiders walk horizontally on the ground floor (no gravity or jump).
 *   - Each spider has a patrol range [patrol_x0, patrol_x1]; it reverses
 *     direction when it reaches either end.
 *   - The sprite is flipped horizontally when walking right (base sprite
 *     faces left).
 *   - Animation cycles through 3 frames at SPIDER_FRAME_MS ms per frame.
 *   - No collision with the player is implemented in this version.
 */
#pragma once

#include <SDL.h>

#define MAX_SPIDERS     16     /* maximum simultaneous spiders on screen    */
#define SPIDER_FRAMES    3     /* animation frames in Spider_1.png          */
#define SPIDER_FRAME_W   64    /* width of one frame slot in the sheet (px) */
#define SPIDER_ART_X     20    /* first visible col within each frame slot  */
#define SPIDER_ART_W     25    /* width of visible art  (cols 20..44)       */
#define SPIDER_ART_Y     22    /* first visible row within each frame slot  */
#define SPIDER_ART_H     10    /* height of visible art (rows 22..31)       */
#define SPIDER_SPEED     50.0f /* walk speed in logical pixels per second   */
#define SPIDER_FRAME_MS  150   /* ms each animation frame is held           */

/*
 * Spider — state for one spider enemy instance.
 *
 * x            : left edge of the 48-px frame slot in logical space.
 * vx           : horizontal velocity (px/s); sign encodes direction.
 * patrol_x0/x1 : left and right patrol boundary; reverses on contact.
 * frame_index  : which column of Spider_1.png is currently displayed.
 * anim_timer_ms: accumulator driving frame advances.
 */
typedef struct {
    float  x;
    float  vx;
    float  patrol_x0;
    float  patrol_x1;
    int    frame_index;
    Uint32 anim_timer_ms;
} Spider;

/*
 * spiders_init — Fill the spider array with starting positions.
 *
 * Defines two patrol spiders on the ground floor.  Sets *count to the
 * number of active spiders so callers don't need to know MAX_SPIDERS.
 */
void spiders_init(Spider *spiders, int *count);

/*
 * spiders_update — Move spiders and advance their animation each frame.
 *
 * spiders : array of Spider structs (from GameState)
 * count   : number of active entries
 * dt      : delta-time in seconds (frame-rate independent movement)
 */
void spiders_update(Spider *spiders, int count, float dt,
                    const int *floor_gaps, int floor_gap_count);

/*
 * spiders_render — Draw all active spiders using the shared texture.
 *
 * Blits the current animation frame for each spider, flipping the sprite
 * horizontally when the spider is walking left (vx < 0).
 * cam_x is the camera left-edge offset (world px); subtract it from dst.x
 * to convert world coordinates to screen coordinates.
 */
void spiders_render(const Spider *spiders, int count,
                    SDL_Renderer *renderer, SDL_Texture *tex, int cam_x);
