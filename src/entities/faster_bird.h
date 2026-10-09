/*
 * faster_bird.h — Public interface for the Faster Bird enemy.
 *
 * Sprite facts (Bird_1.png, 144×48):
 *   - 3 animation frames, each 48×48 px in the sheet.
 *   - Visible art occupies x=17..31 (15 px), y=17..30 (14 px) per frame.
 *   - Same layout as Bird_2.png but different colours.
 *
 * Behaviour:
 *   - Faster horizontal speed and quicker wing animation than the Bird.
 *   - Tighter, more aggressive sine-wave curve (higher frequency).
 *   - Deals hurt damage on contact.
 */
#pragma once

#include "bird.h"   /* Bird, BirdSpec */

/* ---- Constants ---------------------------------------------------------- */

#define MAX_FASTER_BIRDS    16
#define FBIRD_FRAMES         3       /* animation frames in the sheet         */
#define FBIRD_FRAME_W        48      /* width  of one frame slot (px)         */
#define FBIRD_ART_X          17      /* first visible col within each frame   */
#define FBIRD_ART_W          15      /* width  of visible art (cols 17..31)   */
#define FBIRD_ART_Y          17      /* first visible row within each frame   */
#define FBIRD_ART_H          14      /* height of visible art (rows 17..30)   */
/* FBIRD_SPEED — the usual speed, nearly 2x the bird, px/s; the level's vx
 * is the speed actually used (shipped levels author this value). */
#define FBIRD_SPEED          80.0f
#define FBIRD_FRAME_MS       90      /* faster wing animation                 */

/*
 * Faster sine-wave: tighter curves make the flight more erratic.
 */
#define FBIRD_WAVE_AMP       15.0f
#define FBIRD_WAVE_FREQ      0.025f

/* ---- Types -------------------------------------------------------------- */

/*
 * FasterBird — state for one fast sky enemy.
 *
 * The state is exactly a Bird; the separate name keeps GameState and level
 * loading readable (gs->faster_birds[i] is clearly the fast variant).
 */
typedef Bird FasterBird;

/* ---- Function declarations ---------------------------------------------- */

/* Move and animate faster birds (bird.c with FBIRD_* tuning). */
void faster_birds_update(FasterBird *birds, int count, float dt,
                         SoundEffect *snd_flap, float player_x, int cam_x);

void faster_birds_render(const FasterBird *birds, int count,
                         Texture2D *tex, int cam_x);

IntRect faster_bird_get_hitbox(const FasterBird *b);
