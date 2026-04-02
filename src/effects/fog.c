/*
 * fog.c — Atmospheric fog/mist overlay that pans sky layers across the screen.
 *
 * Two semi-transparent cloud assets (Sky_Background_1 and 2) are chosen at
 * random and panned slowly across the logical canvas. Each wave lasts 6–10 s.
 * The image is rendered at 2× the canvas width so its edges are always outside
 * the viewport — you only ever see a cropped interior portion drifting past.
 * The next wave always starts 1 s before the current one ends.
 */

#include <SDL_image.h>  /* IMG_LoadTexture                  */
#include <stdio.h>
#include <stdlib.h>     /* rand(), srand()                  */

#include "fog.h"
#include "game.h"       /* GAME_W, GAME_H (logical canvas)  */

/* ------------------------------------------------------------------ */
/* Tuning constants                                                    */
/* ------------------------------------------------------------------ */

/* Total pan duration range for one fog wave, in seconds */
#define FOG_DURATION_MIN  6.0f
#define FOG_DURATION_MAX  10.0f

/*
 * Maximum alpha applied to the fog overlay (0 = invisible, 255 = opaque).
 * 180 / 255 ≈ 71% opacity — enough to be visible without hiding the game.
 */
#define FOG_ALPHA_MAX  120

/*
 * FOG_FADE_RATIO — fraction of the total duration used for fade-in and fade-out.
 * 0.4 means the first 40% of the animation fades in and the last 40% fades out.
 * Because each instance has a random duration, the actual fade time in seconds
 * is computed per-instance at render time: fade_time = duration × FOG_FADE_RATIO.
 */
#define FOG_FADE_RATIO  0.4f

/*
 * FOG_W — render width of each fog layer in logical pixels.
 *
 * Setting this to 2×GAME_W means the image is always wider than the screen.
 * The image pans within the range [-(FOG_W − GAME_W), 0], so its left and
 * right edges are always outside the viewport and never visibly cut in.
 * The fade-in/fade-out ensure the boundary pixels are transparent at the
 * exact moment they would reach the screen edge.
 */
#define FOG_W  (GAME_W * 2)

/*
 * How many seconds before the end of a wave the next wave is spawned.
 * 1.0 s overlap ensures there is always at least one visible fog layer.
 */
#define FOG_OVERLAP  1.0f

/* ------------------------------------------------------------------ */

/*
 * fog_spawn — activate a free slot with a randomly chosen texture,
 * direction, and duration.
 *
 * Called from fog_init (first wave) and from fog_update (overlap waves).
 * If all slots are occupied, the spawn is silently skipped — this should
 * not happen with FOG_MAX = 4 and the durations used here.
 */
static void fog_spawn(FogSystem *fog) {
    /* Find the first inactive slot in the pool */
    for (int i = 0; i < FOG_MAX; i++) {
        if (fog->instances[i].active) continue;

        FogInstance *inst = &fog->instances[i];

        /* Random texture: 0 or 1 (maps to Sky_Background_1 and Sky_Background_2) */
        inst->tex_index = rand() % FOG_TEX_COUNT;

        /* Random direction: +1 (left→right) or -1 (right→left) */
        inst->dir = (rand() % 2) ? +1 : -1;

        /*
         * Random duration between FOG_DURATION_MIN and FOG_DURATION_MAX.
         * rand() % 1001 gives an integer 0..1000; dividing by 1000.0f maps
         * it to the float range 0.0..1.0, then we scale to the desired range.
         */
        float t      = (float)(rand() % 1001) / 1000.0f;
        inst->duration = FOG_DURATION_MIN + t * (FOG_DURATION_MAX - FOG_DURATION_MIN);

        /*
         * Starting x position for the pan.
         *
         * The image is FOG_W = 2×GAME_W wide. It pans within the range
         * [-(FOG_W - GAME_W), 0] = [-GAME_W, 0], keeping both edges off-screen.
         *   dir = +1 → pan right: start at -GAME_W (left edge off-screen left)
         *   dir = -1 → pan left:  start at  0       (right edge off-screen right)
         */
        inst->x            = (inst->dir > 0) ? -(float)(FOG_W - GAME_W) : 0.0f;
        inst->elapsed      = 0.0f;
        inst->spawned_next = 0;
        inst->active       = 1;
        return;
    }
    /* All slots full — skip this spawn (no crash, no visual glitch) */
}

/* ------------------------------------------------------------------ */

/*
 * fog_init — Load all three sky textures and kick off the first fog wave.
 *
 * Texture loading is non-fatal: a missing asset prints a warning and
 * leaves its pointer NULL. fog_render silently skips NULL entries so the
 * game continues to run normally without the missing layer.
 */
void fog_init(FogSystem *fog, SDL_Renderer *renderer) {
    /*
     * Seed the random number generator with the current uptime so that
     * each game session produces a different sequence of fog waves.
     */
    srand((unsigned int)SDL_GetTicks());

    /* Zero-init the instance pool so every slot starts as inactive */
    for (int i = 0; i < FOG_MAX; i++) {
        fog->instances[i].active = 0;
    }

    /* Paths for the two cloud assets used as fog layers (Sky_Background_0 excluded) */
    const char *paths[FOG_TEX_COUNT] = {
        "assets/fog_background_1.png",
        "assets/fog_background_2.png",
    };

    for (int i = 0; i < FOG_TEX_COUNT; i++) {
        /*
         * IMG_LoadTexture — decode the PNG and upload it to GPU memory.
         * Returns NULL on failure (file not found, corrupt PNG, etc.).
         */
        fog->textures[i] = IMG_LoadTexture(renderer, paths[i]);
        if (!fog->textures[i]) {
            fprintf(stderr, "Warning: fog layer %s not loaded: %s\n",
                    paths[i], IMG_GetError());
            continue;
        }

        /*
         * SDL_BLENDMODE_BLEND — enable alpha blending for this texture.
         * Without this mode, SDL_SetTextureAlphaMod has no visible effect:
         * the texture would always render as fully opaque.
         */
        SDL_SetTextureBlendMode(fog->textures[i], SDL_BLENDMODE_BLEND);
    }

    /* Spawn the very first fog wave so the effect starts immediately */
    fog_spawn(fog);
}

/* ------------------------------------------------------------------ */

/*
 * fog_update — advance every active instance by dt seconds.
 *
 * Movement math (pan, not slide):
 *   The image is FOG_W = 2×GAME_W wide, rendered starting at x.
 *   It pans across the range [-(FOG_W - GAME_W), 0] = [-GAME_W, 0].
 *   Both edges stay outside the viewport throughout the whole animation.
 *
 *   travel  = FOG_W - GAME_W  (= GAME_W = 400 px total pan distance)
 *   dir=+1: x goes from -travel → 0      (image drifts right)
 *   dir=-1: x goes from  0     → -travel (image drifts left)
 *
 *   x = start_x + dir × travel × (elapsed / duration)
 *
 * Next-wave trigger:
 *   When FOG_OVERLAP seconds remain, spawn the next wave — guarded by
 *   spawned_next so fog_spawn fires exactly once per instance.
 *
 * Deactivation:
 *   When elapsed >= duration the pan is complete; free the slot.
 */
void fog_update(FogSystem *fog, float dt) {
    for (int i = 0; i < FOG_MAX; i++) {
        FogInstance *inst = &fog->instances[i];
        if (!inst->active) continue;

        inst->elapsed += dt;

        /*
         * Pan position: linear interpolation across the travel range.
         * Dividing elapsed by duration gives progress 0.0 → 1.0 over the
         * animation, keeping movement frame-rate independent.
         */
        float travel  = (float)(FOG_W - GAME_W);
        float start_x = (inst->dir > 0) ? -travel : 0.0f;
        inst->x       = start_x + (float)inst->dir * travel * (inst->elapsed / inst->duration);

        /*
         * Trigger the next overlapping wave 1 s before this one finishes.
         * The spawned_next flag ensures we call fog_spawn exactly once per
         * instance, not every frame once the threshold is crossed.
         */
        if (!inst->spawned_next &&
            inst->elapsed >= inst->duration - FOG_OVERLAP) {
            inst->spawned_next = 1;
            fog_spawn(fog);
        }

        /* Free the slot once the image has fully left the opposite side */
        if (inst->elapsed >= inst->duration) {
            inst->active = 0;
        }
    }
}

/* ------------------------------------------------------------------ */

/*
 * fog_render — draw every active fog instance on top of the scene.
 *
 * Alpha envelope: each wave fades in linearly over the first FOG_FADE_TIME
 * seconds and fades out over the last FOG_FADE_TIME seconds, so waves
 * dissolve smoothly instead of appearing or disappearing abruptly.
 *
 *   0 .. FOG_FADE_TIME         → alpha ramps 0 → FOG_ALPHA_MAX  (fade in)
 *   FOG_FADE_TIME .. (d-fade)  → alpha stays at FOG_ALPHA_MAX   (full)
 *   (d-fade) .. d              → alpha ramps FOG_ALPHA_MAX → 0  (fade out)
 */
void fog_render(FogSystem *fog, SDL_Renderer *renderer) {
    for (int i = 0; i < FOG_MAX; i++) {
        FogInstance *inst = &fog->instances[i];
        if (!inst->active) continue;

        SDL_Texture *tex = fog->textures[inst->tex_index];
        if (!tex) continue;   /* asset failed to load — skip silently */

        /* Fade window in seconds: 40% of this instance's total duration */
        float fade_time = inst->duration * FOG_FADE_RATIO;

        /* Calculate the alpha value for this frame */
        float remaining = inst->duration - inst->elapsed;
        float alpha_f;

        if (inst->elapsed < fade_time) {
            /* Fading in: progress = elapsed / fade_time (0.0 → 1.0) */
            alpha_f = (inst->elapsed / fade_time) * (float)FOG_ALPHA_MAX;
        } else if (remaining < fade_time) {
            /* Fading out: progress = remaining / fade_time (1.0 → 0.0) */
            alpha_f = (remaining / fade_time) * (float)FOG_ALPHA_MAX;
        } else {
            /* Fully visible middle 20% of the animation */
            alpha_f = (float)FOG_ALPHA_MAX;
        }

        /*
         * SDL_SetTextureAlphaMod — multiply every pixel's alpha by this value.
         *   255 → no change (fully opaque)
         *   128 → 50% transparent
         *     0 → fully transparent
         * The cast to Uint8 safely clamps the value to the 0..255 range.
         */
        SDL_SetTextureAlphaMod(tex, (Uint8)alpha_f);

        /*
         * Draw the fog texture at its current horizontal offset.
         * Width is FOG_W (= 2×GAME_W) so the image is always wider than the
         * screen; only a GAME_W-wide interior strip is ever visible at once.
         * Height fills the full canvas.
         */
        SDL_Rect dst = {
            .x = (int)inst->x,
            .y = 0,
            .w = FOG_W,
            .h = GAME_H,
        };
        SDL_RenderCopy(renderer, tex, NULL, &dst);
    }
}

/* ------------------------------------------------------------------ */

/*
 * fog_cleanup — release every GPU texture owned by the fog system.
 *
 * Must be called before the renderer is destroyed. Setting each pointer
 * to NULL after freeing makes accidental double-frees safe, because
 * SDL_DestroyTexture(NULL) is a documented no-op.
 */
void fog_cleanup(FogSystem *fog) {
    for (int i = 0; i < FOG_TEX_COUNT; i++) {
        if (fog->textures[i]) {
            SDL_DestroyTexture(fog->textures[i]);
            fog->textures[i] = NULL;
        }
    }
}
