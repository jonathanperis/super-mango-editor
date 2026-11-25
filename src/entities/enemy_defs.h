/*
 * enemy_defs.h — Level limits and sprite sizes of the enemies.
 *
 * Only #defines live here, and nothing is included: no raylib, no runtime
 * structs.  The level format (levels/level.h), the serializer and the
 * editor need these numbers to size placement arrays and draw or hit-test
 * an enemy, and the game needs the same numbers to update and draw one.
 * Keeping them in this small header gives each number one home that every
 * side can include, so the serializer does not have to pull in raylib just
 * to learn how many spiders a level may hold.
 *
 * The movement tuning (speeds, jump impulses, animation timing) stays in
 * each enemy's own header (spider.h, bird.h, fish.h...), next to the code
 * that uses it.
 *
 * Frame/art numbers describe one cell of the sprite sheet: FRAME_W is the
 * width of a whole cell, ART_X/ART_Y/ART_W/ART_H the visible part inside
 * it (the rest is transparent padding).  All values are pixels.
 */
#pragma once

/* ---- Spider (spider.h, Spider_1.png) ------------------------------------ */

#define MAX_SPIDERS     16     /* maximum simultaneous spiders on screen    */
#define SPIDER_FRAMES    3     /* animation frames in Spider_1.png          */
#define SPIDER_FRAME_W   64    /* width of one frame slot in the sheet (px) */
#define SPIDER_ART_X     20    /* first visible col within each frame slot  */
#define SPIDER_ART_W     25    /* width of visible art  (cols 20..44)       */
#define SPIDER_ART_Y     22    /* first visible row within each frame slot  */
#define SPIDER_ART_H     10    /* height of visible art (rows 22..31)       */

/* ---- Jumping spider (jumping_spider.h, Spider_2.png) -------------------- */

#define MAX_JUMPING_SPIDERS 16
#define JSPIDER_FRAMES       3       /* animation frames in Spider_2.png      */
#define JSPIDER_FRAME_W      64      /* width  of one frame slot (px)         */
#define JSPIDER_ART_X        20      /* first visible col within each frame   */
#define JSPIDER_ART_W        25      /* width  of visible art (cols 20..44)   */
#define JSPIDER_ART_Y        22      /* first visible row within each frame   */
#define JSPIDER_ART_H        10      /* height of visible art (rows 22..31)   */

/* ---- Bird (bird.h) ------------------------------------------------------ */

#define MAX_BIRDS       16
#define BIRD_FRAMES      3       /* animation frames in the sheet            */
#define BIRD_FRAME_W     48      /* width  of one frame slot (px)            */
#define BIRD_ART_X       17      /* first visible col within each frame      */
#define BIRD_ART_W       15      /* width  of visible art (cols 17..31)      */
#define BIRD_ART_Y       17      /* first visible row within each frame      */
#define BIRD_ART_H       14      /* height of visible art (rows 17..30)      */

/* ---- Faster bird (faster_bird.h) ---------------------------------------- */

#define MAX_FASTER_BIRDS    16
#define FBIRD_FRAMES         3       /* animation frames in the sheet         */
#define FBIRD_FRAME_W        48      /* width  of one frame slot (px)         */
#define FBIRD_ART_X          17      /* first visible col within each frame   */
#define FBIRD_ART_W          15      /* width  of visible art (cols 17..31)   */
#define FBIRD_ART_Y          17      /* first visible row within each frame   */
#define FBIRD_ART_H          14      /* height of visible art (rows 17..30)   */

/* ---- Fish and faster fish (fish.h, faster_fish.h; same sprite size) ------ */

#define MAX_FISH           16       /* maximum simultaneous fish instances     */
#define MAX_FASTER_FISH    16
#define FISH_FRAMES         2       /* horizontal frames in Fish_2.png (96×48) */
#define FISH_FRAME_W       48       /* width of one frame slot in the sheet    */
#define FISH_FRAME_H       48       /* height of one frame slot in the sheet   */
#define FISH_RENDER_W      48       /* on-screen render width  (matches player) */
#define FISH_RENDER_H      48       /* on-screen render height (matches player) */
