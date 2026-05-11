/*
 * player_lifecycle.c — Player init, rendering, reset, hitbox, and cleanup helpers.
 */

#include <SDL_image.h>  /* IMG_LoadTexture */
#include <stdio.h>
#include <stdlib.h>

#include "player.h"
#include "player_internal.h"
#include "../game.h"  /* FLOOR_Y, TILE_SIZE — spawn/reset placement */

/* ---- Horizontal movement physics (default values) ------------------------
 *
 * All values are in logical pixels and seconds.  These are the engine
 * defaults — each can be overridden per-level via LevelDef.physics in the
 * .toml file (set to 0 there to keep the default).
 *
 * Walk (default, no run key):
 *   WALK_MAX_SPEED    — top speed while walking (px/s).
 *   WALK_GROUND_ACCEL — how quickly the player reaches walk speed on the
 *                       ground (px/s²). Higher = snappier start.
 *
 * Run (Shift on keyboard / Right Bumper on gamepad):
 *   RUN_MAX_SPEED     — top speed while running (px/s).
 *   RUN_GROUND_ACCEL  — ramp-up while running on the ground (px/s²).
 *
 * Friction (shared by both walk and run):
 *   GROUND_FRICTION       — deceleration when no key held on ground (px/s²).
 *   GROUND_COUNTER_ACCEL  — extra brake when pressing opposite direction (px/s²).
 *
 * Air control:
 *   AIR_ACCEL_WALK    — air accel in walk arc (px/s²).
 *   AIR_ACCEL_RUN     — air accel in run arc, less control (px/s²).
 *   AIR_FRICTION      — passive drag in air when no key held (px/s²).
 *
 */
#define WALK_MAX_SPEED         100.0f   /* px/s  */
#define RUN_MAX_SPEED          250.0f   /* px/s  */
#define WALK_GROUND_ACCEL      750.0f   /* px/s² */
#define RUN_GROUND_ACCEL       600.0f   /* px/s² */
#define GROUND_FRICTION        550.0f   /* px/s² */
#define GROUND_COUNTER_ACCEL   100.0f   /* px/s² */
#define AIR_ACCEL_WALK         350.0f   /* px/s² */
#define AIR_ACCEL_RUN          180.0f   /* px/s² */
#define AIR_FRICTION            80.0f   /* px/s² */

/*
 * player_init — Load the sprite and place the player in the center of the window.
 */
void player_init(Player *player, SDL_Renderer *renderer)
{
    /*
     * IMG_LoadTexture — decode the PNG sprite sheet and upload it to the GPU.
     * The sheet is 192×288 px and contains a 4-column × 6-row grid of 48×48
     * frames. We only draw one frame at a time using a source clipping rect.
     */
    player->texture = IMG_LoadTexture(renderer, "assets/sprites/player/player.png");
    if (!player->texture) {
        fprintf(stderr, "Failed to load Player.png: %s\n", IMG_GetError());
        exit(EXIT_FAILURE);
    }

    /*
     * frame — the source rectangle: which 48×48 cell to cut from the sheet.
     * {x=0, y=0} → top-left cell, which is the first idle/standing pose.
     * We keep frame.w and frame.h constant at PLAYER_FRAME_W/H for now.
     */
    player->frame.x = 0;
    player->frame.y = 0;
    player->frame.w = PLAYER_FRAME_W;
    player->frame.h = PLAYER_FRAME_H;

    /* The on-screen display size matches the frame size exactly. */
    player->w = PLAYER_FRAME_W;
    player->h = PLAYER_FRAME_H;

    /*
     * Place the player horizontally centered, sitting on top of the floor.
     * FLOOR_Y is the top edge of the grass tiles. FLOOR_SINK adds a small
     * downward offset to compensate for transparent padding at the bottom of
     * the sprite frame, so the feet visually rest on the grass surface.
     */
    /*
     * Default spawn on top of the first platform (x=80, y=172, width=TILE_SIZE).
     * Centre horizontally on the pillar; snap vertically using the same
     * formula as platform landing: plat_y − player_h + FLOOR_SINK.
     *
     * spawn_x/spawn_y store the level-defined spawn point.  level_load will
     * override these (and reposition x/y) once the LevelDef is available.
     */
    player->spawn_x  = 80.0f;
    player->spawn_y  = (float)(FLOOR_Y - 2 * TILE_SIZE + 16);
    player->x        = player->spawn_x + (TILE_SIZE - player->w) / 2.0f;
    player->y        = player->spawn_y - player->h + PLAYER_FLOOR_SINK;
    player->vx       = 0.0f;
    player->vy       = 0.0f;   /* start stationary; gravity will pull down   */
    player->speed    = 160.0f; /* horizontal speed: 160 logical px per second */
    player->on_ground = 1;     /* starts on the floor                         */

    /* Animation — start in idle state, first frame, facing right */
    player->anim_state       = ANIM_IDLE;
    player->anim_frame_index = 0;
    player->anim_timer_ms    = 0;
    player->facing_left      = 0;

    /* Not climbing at startup */
    player->on_vine      = 0;
    player->vine_index   = 0;
    player->climb_source = 0;
    player->jump_held   = 0;
    player->coyote_timer = PLAYER_COYOTE_TIME;
    player->jump_buffer_timer = 0.0f;
    player->move_dir       = 0;   /* no direction pressed at startup */
    player->is_running     = 0;   /* not running at startup          */
    player->air_is_running = 0;   /* starts on the ground, no run momentum */

    /*
     * Physics fields — initialised from the #define defaults above.
     * level_load may override these after player_init if the LevelDef
     * specifies non-zero physics values for finetuning per level.
     */
    player->walk_max_speed       = WALK_MAX_SPEED;
    player->run_max_speed        = RUN_MAX_SPEED;
    player->walk_ground_accel    = WALK_GROUND_ACCEL;
    player->run_ground_accel     = RUN_GROUND_ACCEL;
    player->ground_friction      = GROUND_FRICTION;
    player->ground_counter_accel = GROUND_COUNTER_ACCEL;
    player->air_accel_walk       = AIR_ACCEL_WALK;
    player->air_accel_run        = AIR_ACCEL_RUN;
    player->air_friction         = AIR_FRICTION;

    /* Not hurt at startup; timer counts down to 0 during invincibility */
    player->hurt_timer = 0.0f;
}

/*
 * player_render — Draw the player sprite at its current position.
 *
 * While hurt_timer > 0 the sprite blinks on/off every 100 ms to give visual
 * feedback that the player was hit and is temporarily invincible.
 */
void player_render(Player *player, SDL_Renderer *renderer, int cam_x)
{
    if (player->hurt_timer > 0.0f) {
        int interval = (int)(player->hurt_timer * 1000.0f) / 100;
        if (interval % 2 != 0) return;
    }

    /* Cast float world position to integer screen pixels at render time. */
    SDL_Rect dst = {
        .x = (int)player->x - cam_x,
        .y = (int)player->y,
        .w = player->w,
        .h = player->h
    };

    SDL_RendererFlip flip = player->facing_left ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
    SDL_RenderCopyEx(renderer, player->texture, &player->frame, &dst,
                     0.0, NULL, flip);
}

/*
 * player_get_hitbox — Return the player's inset physics hitbox.
 *
 * The hitbox trims transparent sprite padding so enemy and hazard collision
 * matches the visible character instead of the full 48×48 frame.
 */
SDL_Rect player_get_hitbox(const Player *player)
{
    SDL_Rect r;
    r.x = (int)(player->x) + PLAYER_PHYS_PAD_X;
    r.y = (int)(player->y) + PLAYER_PHYS_PAD_TOP;
    r.w = player->w - 2 * PLAYER_PHYS_PAD_X;
    r.h = player->h - PLAYER_PHYS_PAD_TOP - PLAYER_FLOOR_SINK;
    return r;
}

/*
 * player_reset — Reset position, velocity, and animation without reloading.
 *
 * Called when the player loses all hearts and spends a life.  Texture and
 * tunable physics fields stay intact because they were configured by init and
 * level_load().
 */
void player_reset(Player *player)
{
    player->x = player->spawn_x + (TILE_SIZE - player->w) / 2.0f;
    player->y = player->spawn_y - player->h + PLAYER_FLOOR_SINK;
    player->vx = 0.0f;
    player->vy = 0.0f;
    player->on_ground = 1;

    player->anim_state = ANIM_IDLE;
    player->anim_frame_index = 0;
    player->anim_timer_ms = 0;
    player->facing_left = 0;
    player->on_vine = 0;
    player->vine_index = 0;
    player->climb_source = 0;
    player->jump_held = 0;
    player->coyote_timer = PLAYER_COYOTE_TIME;
    player->jump_buffer_timer = 0.0f;
    player->move_dir = 0;
    player->is_running = 0;
    player->air_is_running = 0;
    player->hurt_timer = 0.0f;

    player->frame.x = 0;
    player->frame.y = 0;
}

/*
 * player_cleanup — Release GPU memory held by the player's texture.
 *
 * Must run before the renderer is destroyed, because SDL_Texture objects are
 * owned by the renderer that created them.
 */
void player_cleanup(Player *player)
{
    if (player->texture) {
        SDL_DestroyTexture(player->texture);
        player->texture = NULL;
    }
}
