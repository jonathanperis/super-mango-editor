/*
 * player.c — Implementation of player init, input, physics, rendering, and cleanup.
 */

#include <SDL_image.h>  /* IMG_LoadTexture */
#include <SDL_mixer.h>  /* Mix_Chunk, Mix_PlayChannel */
#include <stdio.h>
#include <stdlib.h>

#include "player.h"
#include "game.h"   /* GAME_W, GAME_H, FLOOR_Y, GRAVITY (for physics and clamping) */

/* ------------------------------------------------------------------ */

/*
 * player_init — Load the sprite and place the player in the center of the window.
 */
/* Width and height of one sprite frame in the sheet (pixels). */
#define FRAME_W 48
#define FRAME_H 48

/*
 * FLOOR_SINK — extra pixels the player overlaps with the top of the floor tile.
 * The sprite sheet has transparent padding at the bottom of each frame, so the
 * physics edge (y + h = FLOOR_Y) leaves the character visually floating.
 * Sinking by this many pixels makes the feet appear to rest on the grass.
 */
#define FLOOR_SINK 16

/*
 * Physics-box insets — how many pixels to trim from each side of the 48×48
 * sprite frame to reach the visible character boundary.
 *
 * The character art is centred in the frame with significant transparent
 * padding, especially on the left and right. Using the raw frame size for
 * collisions creates an invisible wall well outside the art.
 *
 * PHYS_PAD_X : pixels trimmed from the LEFT and RIGHT edges (each side).
 *              Physics width  = FRAME_W − 2 × PHYS_PAD_X  = 48 − 24 = 24 px.
 * PHYS_PAD_TOP : pixels trimmed from the TOP edge.
 *              Combined with FLOOR_SINK at the bottom the physics box
 *              tracks the drawn character closely on all four sides.
 */
#define PHYS_PAD_X   12
#define PHYS_PAD_TOP  6

/*
 * Animation tables — indexed by AnimState (0=IDLE, 1=WALK, 2=JUMP, 3=FALL).
 *
 * ANIM_FRAME_COUNT : how many frames the animation has.
 * ANIM_FRAME_MS    : how long each frame is shown, in milliseconds.
 * ANIM_ROW         : which row of the sprite sheet the animation lives on.
 *
 * Frame layout confirmed from Player.png (192×288, 4 cols × 6 rows, 48×48 each):
 *   Row 0 — Idle : 4 frames  (cols 0–3)
 *   Row 1 — Walk : 4 frames  (cols 0–3)
 *   Row 2 — Jump : 2 frames  (cols 0–1, rising-phase poses)
 *   Row 3 — Fall : 1 frame   (col 0, descent pose)
 */
static const int ANIM_FRAME_COUNT[4] = { 4,   4,   2,   1   };
static const int ANIM_FRAME_MS[4]    = { 150, 100, 150, 200 };
static const int ANIM_ROW[4]         = { 0,   1,   2,   3   };

void player_init(Player *player, SDL_Renderer *renderer) {
    /*
     * IMG_LoadTexture — decode the PNG sprite sheet and upload it to the GPU.
     * The sheet is 192×288 px and contains a 4-column × 6-row grid of 48×48
     * frames. We only draw one frame at a time using a source clipping rect.
     */
    player->texture = IMG_LoadTexture(renderer, "assets/Player.png");
    if (!player->texture) {
        fprintf(stderr, "Failed to load Player.png: %s\n", IMG_GetError());
        exit(EXIT_FAILURE);
    }

    /*
     * frame — the source rectangle: which 48×48 cell to cut from the sheet.
     * {x=0, y=0} → top-left cell, which is the first idle/standing pose.
     * We keep frame.w and frame.h constant at FRAME_W/FRAME_H for now.
     */
    player->frame.x = 0;
    player->frame.y = 0;
    player->frame.w = FRAME_W;
    player->frame.h = FRAME_H;

    /* The on-screen display size matches the frame size exactly. */
    player->w = FRAME_W;
    player->h = FRAME_H;

    /*
     * Place the player horizontally centered, sitting on top of the floor.
     * FLOOR_Y is the top edge of the grass tiles. FLOOR_SINK adds a small
     * downward offset to compensate for transparent padding at the bottom of
     * the sprite frame, so the feet visually rest on the grass surface.
     */
    player->x        = (GAME_W - player->w) / 2.0f;
    player->y        = (float)(FLOOR_Y - player->h + FLOOR_SINK);
    player->vx       = 0.0f;
    player->vy       = 0.0f;   /* start stationary; gravity will pull down   */
    player->speed    = 160.0f; /* horizontal speed: 160 logical px per second */
    player->on_ground = 1;     /* starts on the floor                         */

    /* Animation — start in idle state, first frame, facing right */
    player->anim_state       = ANIM_IDLE;
    player->anim_frame_index = 0;
    player->anim_timer_ms    = 0;
    player->facing_left      = 0;

    /* Not hurt at startup; timer counts down to 0 during invincibility */
    player->hurt_timer = 0.0f;
}

/* ------------------------------------------------------------------ */

/*
 * player_handle_input — Sample the keyboard and set the player's velocity.
 *
 * Called once per frame, before player_update.
 *
 * We use SDL_GetKeyboardState instead of key-press events because
 * it tells us which keys are held RIGHT NOW, giving smooth, continuous
 * movement rather than one-shot movement on the moment of press.
 */
/*
 * AXIS_DEAD_ZONE — minimum absolute value an analog axis must exceed before
 * it is treated as intentional input.
 *
 * SDL reports axis values in the range [-32768, +32767].  Physical sticks
 * produce small non-zero readings even when untouched (electrical noise,
 * mechanical centre offset).  Ignoring anything below this threshold
 * prevents the player from drifting without touching the controller.
 *
 * 8000 ≈ 24% of the full range — safe for all DualSense / DS4 / Xbox sticks.
 * Raise this value if a specific controller drifts; lower it for more
 * sensitivity at the cost of accidental movement.
 */
#define AXIS_DEAD_ZONE  8000

void player_handle_input(Player *player, Mix_Chunk *snd_jump,
                         SDL_GameController *ctrl) {
    /*
     * SDL_GetKeyboardState returns a pointer to an array of key states.
     * Each element is 1 if that key is currently held, 0 if not.
     * Indexed by SDL_SCANCODE_* values (hardware-based, layout-independent).
     * Passing NULL means "use SDL's internal state array".
     */
    const Uint8 *keys = SDL_GetKeyboardState(NULL);

    /*
     * Horizontal movement: left/right arrow keys and A/D both work.
     * Reset vx to 0 each frame so the player stops when keys are released.
     */
    player->vx = 0.0f;
    if (keys[SDL_SCANCODE_LEFT]  || keys[SDL_SCANCODE_A]) {
        player->vx -= player->speed;
        player->facing_left = 1;   /* mirror sprite so character faces left */
    }
    if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) {
        player->vx += player->speed;
        player->facing_left = 0;
    }

    /*
     * Jump: Space only — only allowed while standing on the floor.
     * Setting on_ground = 0 ensures this block only fires once per jump;
     * subsequent frames while the key is held skip it because on_ground is 0.
     * vy is set to a negative value (up is negative in SDL screen-space).
     */
    if (player->on_ground && keys[SDL_SCANCODE_SPACE]) {
        player->vy        = -500.0f;  /* upward impulse; reaches ~156 px, clearing the tall pillar */
        player->on_ground  = 0;
        if (snd_jump) Mix_PlayChannel(-1, snd_jump, 0);
    }

    /* ----------------------------------------------------------------
     * Gamepad input — only runs when a controller is connected (ctrl != NULL).
     *
     * SDL_GameController maps every supported device (DualSense, DualShock 4,
     * Xbox Series / One / 360) to a unified button/axis layout regardless of
     * the physical label on the device.  We read two input sources:
     *
     *   1. D-Pad buttons — digital on/off, perfect for precision platforming.
     *   2. Left analog stick X axis — analog range; requires a dead zone check
     *      to filter electrical noise when the stick is at rest.
     *
     * Both sources can be active simultaneously; the velocity accumulates.
     * Keyboard and gamepad also work at the same time — no mode switching.
     * ---------------------------------------------------------------- */
    if (ctrl) {
        /*
         * D-Pad left / right — SDL_GameControllerGetButton returns 1 if the
         * named button is currently pressed, 0 if not.
         *
         * SDL_CONTROLLER_BUTTON_DPAD_LEFT  → D-Pad left  on all controllers
         * SDL_CONTROLLER_BUTTON_DPAD_RIGHT → D-Pad right on all controllers
         */
        if (SDL_GameControllerGetButton(ctrl, SDL_CONTROLLER_BUTTON_DPAD_LEFT)) {
            player->vx -= player->speed;
            player->facing_left = 1;
        }
        if (SDL_GameControllerGetButton(ctrl, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) {
            player->vx += player->speed;
            player->facing_left = 0;
        }

        /*
         * Left analog stick horizontal axis.
         *
         * SDL_GameControllerGetAxis returns a Sint16 in [-32768, +32767].
         * Negative = left, positive = right.
         * We compare the raw value against AXIS_DEAD_ZONE to ignore noise.
         *
         * SDL_CONTROLLER_AXIS_LEFTX maps to:
         *   DualSense / DS4 → left stick horizontal
         *   Xbox             → left stick horizontal
         */
        Sint16 axis_x = SDL_GameControllerGetAxis(ctrl, SDL_CONTROLLER_AXIS_LEFTX);
        if (axis_x < -AXIS_DEAD_ZONE) {
            player->vx -= player->speed;
            player->facing_left = 1;
        } else if (axis_x > AXIS_DEAD_ZONE) {
            player->vx += player->speed;
            player->facing_left = 0;
        }

        /*
         * Jump button — A (Xbox) / Cross × (DualSense / DS4).
         *
         * SDL maps both to SDL_CONTROLLER_BUTTON_A in the unified layout.
         * Same one-shot guard as the keyboard: on_ground prevents re-triggering
         * while the button is held after takeoff.
         */
        if (player->on_ground &&
            SDL_GameControllerGetButton(ctrl, SDL_CONTROLLER_BUTTON_A)) {
            player->vy        = -500.0f;
            player->on_ground = 0;
            if (snd_jump) Mix_PlayChannel(-1, snd_jump, 0);
        }
    }
}

/* ------------------------------------------------------------------ */

/*
 * player_animate — Choose the correct animation state and advance its frame.
 *
 * Called once per frame from player_update, after physics are resolved.
 * Uses the player's velocity and on_ground flag to determine which
 * animation should be playing, then advances the frame timer.
 *
 * On state transitions the frame index is reset to 0 so animations
 * always start from the beginning rather than mid-sequence.
 */
static void player_animate(Player *player, Uint32 dt_ms) {
    /* Determine the target animation from current physics state */
    AnimState target;
    if (!player->on_ground) {
        /*
         * In the air: negative vy means moving upward (SDL y-axis is inverted),
         * positive vy means falling down under gravity.
         */
        target = (player->vy < 0.0f) ? ANIM_JUMP : ANIM_FALL;
    } else if (player->vx != 0.0f) {
        target = ANIM_WALK;
    } else {
        target = ANIM_IDLE;
    }

    /* Reset the frame counter whenever the animation state changes */
    if (target != player->anim_state) {
        player->anim_state       = target;
        player->anim_frame_index = 0;
        player->anim_timer_ms    = 0;
    }

    /*
     * Advance the frame timer. When it exceeds the per-frame duration,
     * subtract the duration (rather than resetting to 0) so any leftover
     * time carries into the next frame — keeping animation speed accurate.
     */
    player->anim_timer_ms += dt_ms;
    Uint32 frame_duration = (Uint32)ANIM_FRAME_MS[player->anim_state];
    if (player->anim_timer_ms >= frame_duration) {
        player->anim_timer_ms -= frame_duration;
        player->anim_frame_index =
            (player->anim_frame_index + 1) % ANIM_FRAME_COUNT[player->anim_state];
    }

    /*
     * Update the source rectangle to point at the correct cell on the sheet.
     *   frame.x = column × FRAME_W  (horizontal offset into the sheet)
     *   frame.y = row    × FRAME_H  (vertical  offset into the sheet)
     */
    player->frame.x = player->anim_frame_index * FRAME_W;
    player->frame.y = ANIM_ROW[player->anim_state] * FRAME_H;
}

/* ------------------------------------------------------------------ */

/*
 * player_update -- Apply gravity and velocity to position, handle floor and
 *                  one-way platform collisions.
 *
 * dt (delta time) is the time in seconds since the last frame (e.g. 0.016).
 * Multiplying velocity (px/s) by dt (s) gives displacement in pixels.
 * This makes movement speed identical regardless of frame rate.
 *
 * One-way platforms:
 *   Only the TOP SURFACE of each platform triggers a landing.  The player
 *   can jump through from below; collision is only checked when the player
 *   is moving downward (vy >= 0) and their bottom edge crosses the platform
 *   top surface this frame (crossing test prevents tunnelling at high speed).
 */
void player_update(Player *player, float dt,
                   const Platform *platforms, int platform_count) {
    /*
     * Reset on_ground every frame so the player immediately starts falling
     * when they walk off the edge of a platform.  Collision checks below
     * will set it back to 1 if the player is resting on a surface.
     */
    player->on_ground = 0;

    /*
     * Sample the physics bottom BEFORE this frame's movement so the
     * platform crossing test can compare "where was the player last frame"
     * vs "where is the player now".
     *
     * Why capture it here instead of back-projecting later:
     *   back-projection computes  prev = (y_new + 32) - vy_new * dt
     *   which is mathematically   (y_old + vy_new*dt + 32) - vy_new*dt = y_old + 32.
     *   In IEEE 754 single-precision, (a + b) - b can differ from a by up
     *   to 1 ULP due to rounding during the addition.  When the player is
     *   snapped to a platform at y=plat->y, that 1-ULP error makes
     *   prev_bottom > plat->y, the crossing test fails, and the player
     *   silently sinks through the platform every frame until they hit the
     *   floor.  Capturing the true absolute value before the update avoids
     *   all cancellation error.
     */
    const float prev_bottom = player->y + player->h - FLOOR_SINK;

    /*
     * Gravity: accelerate downward each frame.
     * Because on_ground was just cleared, gravity always runs here; the
     * floor/platform snap below cancels the tiny fall each frame while the
     * player stands still, keeping the character rock-solid on the ground.
     */
    player->vy += GRAVITY * dt;

    player->x += player->vx * dt;   /* move horizontally */
    player->y += player->vy * dt;   /* move vertically   */

    /*
     * Floor collision -- snap to the grass surface and stop falling.
     * FLOOR_Y is the top edge of the grass tiles in logical coordinates.
     */
    const float ground_snap = (float)(FLOOR_Y - player->h + FLOOR_SINK);
    if (player->y >= ground_snap) {
        player->y         = ground_snap;
        player->vy        = 0.0f;
        player->on_ground = 1;
    }

    /*
     * One-way platform collision -- top surface only.
     *
     * We only test when:
     *   1. The player is not already on the floor (avoid double-snap).
     *   2. The player is moving downward (vy >= 0), so upward jumps pass through.
     *
     * Crossing test: compare where the player's bottom was BEFORE this frame's
     * movement (prev_bottom, captured above) with where it is NOW (bottom).
     * A landing is detected when the edge crossed the platform's top Y from
     * above to below.  This is frame-rate-independent and handles any fall
     * speed correctly.
     *
     * The "physics bottom" strips the FLOOR_SINK visual offset so contact
     * lands the sprite at the same apparent depth as on the main floor.
     */
    if (!player->on_ground && player->vy >= 0.0f) {
        const float bottom = player->y + player->h - FLOOR_SINK;

        for (int i = 0; i < platform_count; i++) {
            const Platform *plat = &platforms[i];

            /* Horizontal overlap: use the inset physics box, not the full sprite */
            int h_overlap = (player->x + player->w - PHYS_PAD_X > plat->x) &&
                            (player->x + PHYS_PAD_X < plat->x + plat->w);
            if (!h_overlap) continue;

            /* Vertical crossing: bottom was at or above surface, now below */
            if (prev_bottom <= plat->y && bottom >= plat->y) {
                player->y         = plat->y - player->h + FLOOR_SINK;
                player->vy        = 0.0f;
                player->on_ground = 1;
                break;   /* first platform wins */
            }
        }
    }

    /*
     * Horizontal clamp — keep the PHYSICS body inside the logical canvas.
     * We clamp the inset edge (x + PHYS_PAD_X), not the sprite left edge,
     * so the transparent side-padding can slide off-screen while the visible
     * character stays flush with the border instead of stopping early.
     */
    if (player->x + PHYS_PAD_X < 0.0f)
        player->x = -(float)PHYS_PAD_X;
    if (player->x + player->w - PHYS_PAD_X > WORLD_W)
        player->x = (float)(WORLD_W - player->w + PHYS_PAD_X);

    /*
     * Ceiling clamp — stop upward movement when the physics top hits the
     * canvas ceiling.  PHYS_PAD_TOP lets the transparent head-room of the
     * sprite frame slide above y=0 before the physics edge triggers.
     */
    if (player->y + PHYS_PAD_TOP < 0.0f) {
        player->y  = -(float)PHYS_PAD_TOP;
        player->vy = 0.0f;
    }

    /*
     * Advance the sprite animation based on the resolved physics state.
     * Convert dt (seconds) to milliseconds for the frame timer.
     */
    player_animate(player, (Uint32)(dt * 1000.0f));
}

/* ------------------------------------------------------------------ */

/*
 * player_render — Draw the player sprite at its current position.
 *
 * While hurt_timer > 0 the sprite blinks on/off every 100 ms to give
 * visual feedback that the player was hit and is temporarily invincible.
 */
void player_render(Player *player, SDL_Renderer *renderer, int cam_x) {
    /*
     * Blink effect: during invincibility, convert the remaining hurt time
     * into a 100-ms cadence.  On odd intervals the sprite is hidden,
     * creating a flash / blink pattern.
     */
    if (player->hurt_timer > 0.0f) {
        int interval = (int)(player->hurt_timer * 1000.0f) / 100;
        if (interval % 2 == 1) return;   /* skip this frame — blink off */
    }

    /*
     * dst — where on screen the sprite will appear.
     * x/y are cast from float to int because SDL works in whole pixels.
     * w/h match the frame size (FRAME_W × FRAME_H = 48×48).
     */
    SDL_Rect dst = {
        .x = (int)player->x - cam_x,  /* world → screen: subtract camera offset */
        .y = (int)player->y,
        .w = player->w,
        .h = player->h
    };

    /*
     * SDL_RenderCopyEx — like SDL_RenderCopy but supports rotation and flipping.
     * We pass angle=0 and center=NULL (no rotation), and flip the texture
     * horizontally when the player last moved left, so the character always
     * faces the correct direction regardless of which animation is playing.
     */
    SDL_RendererFlip flip = player->facing_left ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
    SDL_RenderCopyEx(renderer, player->texture, &player->frame, &dst,
                     0.0, NULL, flip);
}

/* ------------------------------------------------------------------ */

/*
 * player_get_hitbox — Return the player's physics hitbox as an SDL_Rect.
 *
 * The hitbox is inset from the full 48×48 sprite frame on every side
 * to match the visible character art.  Used by game.c for collision
 * checks against enemies.
 */
SDL_Rect player_get_hitbox(const Player *player) {
    SDL_Rect r;
    r.x = (int)(player->x) + PHYS_PAD_X;
    r.y = (int)(player->y) + PHYS_PAD_TOP;
    r.w = player->w - 2 * PHYS_PAD_X;
    r.h = player->h - PHYS_PAD_TOP - FLOOR_SINK;
    return r;
}

/* ------------------------------------------------------------------ */

/*
 * player_reset — Reset position, velocity, and animation to the initial
 *                state without reloading the texture from disk.
 *
 * Called when the player loses all hearts and a life is consumed.
 * The texture and display dimensions (w/h/speed) are left untouched
 * because they were set once in player_init and don't change.
 */
void player_reset(Player *player) {
    player->x         = (GAME_W - player->w) / 2.0f;
    player->y         = (float)(FLOOR_Y - player->h + FLOOR_SINK);
    player->vx        = 0.0f;
    player->vy        = 0.0f;
    player->on_ground = 1;

    player->anim_state       = ANIM_IDLE;
    player->anim_frame_index = 0;
    player->anim_timer_ms    = 0;
    player->facing_left      = 0;
    player->hurt_timer       = 0.0f;

    player->frame.x = 0;
    player->frame.y = 0;
}

/* ------------------------------------------------------------------ */

/*
 * player_cleanup — Release GPU memory held by the player's texture.
 *
 * Must be called before the renderer is destroyed, because SDL_Texture
 * objects are owned by the renderer that created them.
 */
void player_cleanup(Player *player) {
    if (player->texture) {
        SDL_DestroyTexture(player->texture);
        player->texture = NULL;   /* guard against accidental double-free */
    }
}
