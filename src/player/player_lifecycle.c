/*
 * player_lifecycle.c — Player init, rendering, reset, hitbox, and cleanup helpers.
 */

#include <stdio.h>

#include "player.h"
#include "player_internal.h"
#include "../game_constants.h" /* FLOOR_Y, TILE_SIZE — spawn/reset placement */

/* ---- Horizontal movement physics (default values) ------------------------
 *
 * All values are in logical pixels and seconds.  These are the engine
 * defaults — each can be overridden per-level via LevelDef.physics in the
 * .toml file. A negative value there (-1.0, the default) keeps the engine
 * value; 0.0 is a real override (see level_physics.c).
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

void player_apply_default_physics(Player *player)
{
    if (!player) return;

    player->walk_max_speed       = WALK_MAX_SPEED;
    player->run_max_speed        = RUN_MAX_SPEED;
    player->walk_ground_accel    = WALK_GROUND_ACCEL;
    player->run_ground_accel     = RUN_GROUND_ACCEL;
    player->ground_friction      = GROUND_FRICTION;
    player->ground_counter_accel = GROUND_COUNTER_ACCEL;
    player->air_accel_walk       = AIR_ACCEL_WALK;
    player->air_accel_run        = AIR_ACCEL_RUN;
    player->air_friction         = AIR_FRICTION;
}

/*
 * player_init — Get the sprite and place the player in the center of the window.
 */
int player_init(Player *player, AssetCache *cache)
{
    /*
     * Ask the asset cache for the sprite sheet. The first game of a session
     * decodes the PNG and uploads it to the GPU; every later game (a Replay,
     * another level) is handed that same texture and reads no file.
     *
     * Who owns it: the cache. This Player holds one reference, and
     * player_cleanup gives it back with asset_cache_texture_release. The
     * player never calls texture_unload on it. (With cache == NULL the
     * sheet is loaded for this Player alone and freed on release.)
     *
     * The sheet is 192×288 px and contains a 4-column × 6-row grid of 48×48
     * frames. We only draw one frame at a time using a source clipping rect.
     */
    player->texture = asset_cache_texture_acquire(cache, "assets/sprites/player/player.png");
    if (!player->texture) {
        fprintf(stderr, "Failed to load assets/sprites/player/player.png\n");
        return -1;
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
     * level_load may override these after player_init: every LevelDef
     * physics value of 0 or more replaces its default, while -1 keeps it
     * (level_apply_player_physics in level_physics.c).
     */
    player_apply_default_physics(player);

    /* Not hurt at startup; timer counts down to 0 during invincibility */
    player->hurt_timer = 0.0f;
    return 0;
}

/*
 * player_hurt_tint — How the sprite shows the invincibility after a hit.
 *
 * While hurt_timer > 0 the sprite normally blinks on/off every 100 ms. With
 * reduced motion that 5 Hz flicker is exactly what the player asked to
 * avoid, so the sprite stays drawn, half transparent, for the whole window.
 * Returns 0 for a frame that skips the sprite; *tint is the colour to draw
 * with otherwise. Only reads the player, so drawing never changes a run.
 */
int player_hurt_tint(const Player *player, int reduced_motion, Color *tint)
{
    *tint = WHITE;
    if (player->hurt_timer <= 0.0f) return 1;
    if (reduced_motion) {
        tint->a = 128;
        return 1;
    }
    int interval = (int)(player->hurt_timer * 1000.0f) / 100;
    return interval % 2 == 0;
}

/*
 * player_render — Draw the player sprite at its current position, blinking
 * (or, with reduced motion, translucent) while hurt (player_hurt_tint).
 */
void player_render(const Player *player, int cam_x, int reduced_motion)
{
    Color tint;
    if (!player_hurt_tint(player, reduced_motion, &tint)) return;

    /* Cast float world position to integer screen pixels at render time. */
    IntRect dst = {
        .x = (int)player->x - cam_x,
        .y = (int)player->y,
        .w = player->w,
        .h = player->h
    };

    int flip = player->facing_left ? SPRITE_FLIP_X : SPRITE_NORMAL;
    sprite_draw(player->texture, &player->frame, &dst, 0, flip, tint);
}

/*
 * player_get_hitbox — Return the player's inset physics hitbox.
 *
 * The hitbox trims transparent sprite padding so enemy and hazard collision
 * matches the visible character instead of the full 48×48 frame.
 */
IntRect player_get_hitbox(const Player *player)
{
    IntRect r;
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
 * player_cleanup — Give the sprite sheet back to the asset cache.
 *
 * Pass the same cache player_init received. The cache decides when the GPU
 * memory is freed: not now, if the next game is about to ask for the sheet
 * again. Clearing the pointer makes a second cleanup harmless.
 */
void player_cleanup(Player *player, AssetCache *cache)
{
    asset_cache_texture_release(cache, player->texture);
    player->texture = NULL;
}
