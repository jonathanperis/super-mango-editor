/*
 * texture_after_free.c — Debugging lab: using a texture after freeing it.
 *
 * In the game, texture_load() in src/shared/graphics.h mallocs a small
 * Texture2D handle and texture_unload() frees it. The owner (for example
 * the TextureResources table in src/core/game_resources.c) must clear its
 * pointer after the free, so nothing can reach the old memory again.
 *
 * Here a cut-down texture owner forgets that. Two ways to run it:
 *
 *   ./out/labs/texture_after_free          the owner keeps the stale pointer:
 *                                          a use-after-free (needs a sanitizer
 *                                          to see it; otherwise it "works")
 *   ./out/labs/texture_after_free --clear  the owner clears its pointer, so the
 *                                          late draw dereferences NULL and the
 *                                          program crashes on that exact line
 *
 * This file is NOT part of the game build. Compile it on its own from the
 * repository root, once with sanitizers and once plain for the debugger:
 *
 *   mkdir -p out/labs
 *   cc -std=c11 -g -O0 -fsanitize=address,undefined -fno-omit-frame-pointer \
 *      labs/debugging/texture_after_free.c -o out/labs/texture_after_free
 *   cc -std=c11 -g -O0 labs/debugging/texture_after_free.c \
 *      -o out/labs/texture_after_free_plain
 *
 * The walkthrough is in docs/wiki/debugging-c.md.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Stands in for raylib's Texture2D: a GPU id plus a size. */
typedef struct {
    unsigned int id;
    int width;
    int height;
} FakeTexture;

typedef struct {
    FakeTexture *coin; /* owning pointer: this struct must free it once */
} TextureResources;

static FakeTexture *texture_load(int width, int height)
{
    FakeTexture *texture = malloc(sizeof(*texture));
    if (!texture) return NULL;
    texture->id = 7;
    texture->width = width;
    texture->height = height;
    return texture;
}

static void texture_unload(FakeTexture *texture)
{
    free(texture);
}

static void coin_draw(const FakeTexture *texture, int x)
{
    /* Reading texture->width is where a stale pointer is caught. */
    printf("draw coin at x=%d (%dx%d)\n", x, texture->width, texture->height);
}

static void resources_cleanup(TextureResources *res, int clear_pointer)
{
    texture_unload(res->coin);
    if (clear_pointer) res->coin = NULL; /* the fix the real owner applies */
}

int main(int argc, char **argv)
{
    int clear_pointer = argc > 1 && strcmp(argv[1], "--clear") == 0;

    TextureResources res = { 0 };
    res.coin = texture_load(16, 16);
    if (!res.coin) return 1;

    coin_draw(res.coin, 32);
    resources_cleanup(&res, clear_pointer);

    /* BUG: one more frame is drawn after cleanup, as if a screen kept
     * rendering after its resources were released. */
    coin_draw(res.coin, 48);
    return 0;
}
