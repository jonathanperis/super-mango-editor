/*
 * jumping_spider.h — Public interface for the jumping spider enemy.
 *
 * Sprite facts (Spider_2.png, 192×48):
 *   - 3 animation frames, each 64×48 px in the sheet.
 *   - Visible art occupies x=20..44 (25 px), y=22..31 (10 px) per frame.
 *   - Identical layout to Spider_1.png but with different colours.
 *
 * Behaviour:
 *   - Patrols horizontally on the ground floor like a regular spider.
 *   - Periodically jumps in a short arc while continuing to walk,
 *     making it harder to avoid than a ground-only spider.
 *   - Respects sea gaps — reverses at gap edges.
 *   - The sprite is flipped horizontally when walking right.
 */
#pragma once

#include <SDL.h>
#include <SDL_mixer.h>

/* ---- Constants ---------------------------------------------------------- */

#define MAX_JUMPING_SPIDERS 16
#define JSPIDER_FRAMES       3       /* animation frames in Spider_2.png      */
#define JSPIDER_FRAME_W      64      /* width  of one frame slot (px)         */
#define JSPIDER_ART_X        20      /* first visible col within each frame   */
#define JSPIDER_ART_W        25      /* width  of visible art (cols 20..44)   */
#define JSPIDER_ART_Y        22      /* first visible row within each frame   */
#define JSPIDER_ART_H        10      /* height of visible art (rows 22..31)   */
#define JSPIDER_SPEED        55.0f   /* walk speed in logical px/s            */
#define JSPIDER_FRAME_MS     150     /* ms each animation frame is held       */

/*
 * Jump parameters — the spider jumps when it reaches a sea gap edge.
 * JSPIDER_JUMP_VY is the upward impulse; JSPIDER_GRAVITY pulls it back down.
 * At 55 px/s the spider is airborne ~0.67 s, covering ~37 px horizontally —
 * enough to clear the 32-px sea gaps.
 */
#define JSPIDER_JUMP_VY     -200.0f  /* upward impulse in px/s (negative = up)*/
#define JSPIDER_GRAVITY      600.0f  /* downward accel in px/s² while airborne*/

/* ---- Types -------------------------------------------------------------- */

typedef struct {
    float  x;              /* left edge of the frame slot in world space     */
    float  y;              /* vertical offset from ground (0 = on floor)     */
    float  vx;             /* horizontal velocity (px/s); sign = direction   */
    float  vy;             /* vertical velocity (px/s); 0 when on ground    */
    float  patrol_x0;      /* left patrol boundary                          */
    float  patrol_x1;      /* right patrol boundary                         */
    float  jump_timer;     /* seconds until next jump; counts down each frame*/
    int    on_ground;       /* 1 = walking on floor, 0 = mid-jump           */
    int    frame_index;    /* current animation frame (0–2)                  */
    Uint32 anim_timer_ms;  /* accumulator for frame advances                */
} JumpingSpider;

/* ---- Function declarations ---------------------------------------------- */

/* Place all jumping spiders at their initial world positions. */
void jumping_spiders_init(JumpingSpider *spiders, int *count);

/* Move, jump, patrol, animate each jumping spider. */
void jumping_spiders_update(JumpingSpider *spiders, int count, float dt,
                            const int *floor_gaps, int floor_gap_count,
                            Mix_Chunk *snd_attack, float player_x, int cam_x);

/* Draw all jumping spiders using the shared texture. */
void jumping_spiders_render(const JumpingSpider *spiders, int count,
                            SDL_Renderer *renderer, SDL_Texture *tex,
                            int cam_x);
