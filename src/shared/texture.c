/*
 * texture.c — Owned texture handles: load an image file, release it.
 *
 * graphics.h declares these three functions. They live in their own file,
 * apart from the window code in graphics.c, so the session test can link a
 * build of this one file that counts loads (MANGO_TESTING).
 */
#include "graphics.h"
#include <rlgl.h>  /* rlgl after raylib.h, which defines its Matrix type */

#ifdef MANGO_TESTING
/* Test builds count image loads and live handles (see graphics.h). */
static int s_test_loads;
static int s_test_live;
int texture_test_loads(void) { return s_test_loads; }
int texture_test_live(void) { return s_test_live; }
#endif

Texture2D *texture_own(Texture2D value)
{
    Texture2D *texture = malloc(sizeof(*texture));
    if (!texture) {
        UnloadTexture(value);
        return NULL;
    }
    *texture = value;
    /* Nearest-neighbor sampling keeps pixel-art edges sharp when scaled. */
    SetTextureFilter(value, TEXTURE_FILTER_POINT);
#ifdef MANGO_TESTING
    s_test_live++;
#endif
    return texture;
}

Texture2D *texture_load(const char *path)
{
    /* Decode to memory first: the 2.5D view (depth.h) keeps a note of which
     * pixels are opaque, and the GPU copy cannot be read back on the Web. */
    Image image = LoadImage(path);
    if (!IsImageValid(image)) return NULL;
    Texture2D value = LoadTextureFromImage(image);
    if (IsTextureValid(value)) depth_mask_set(value.id, &image);
    UnloadImage(image);
    if (!IsTextureValid(value)) return NULL;
#ifdef MANGO_TESTING
    s_test_loads++;
#endif
    return texture_own(value);
}

void texture_unload(Texture2D *texture)
{
    if (!texture) return;
    /* A UI cache eviction can occur before EndDrawing flushes queued draws. */
    rlDrawRenderBatchActive();
    depth_mask_forget(texture->id);
    UnloadTexture(*texture);
    free(texture);
#ifdef MANGO_TESTING
    s_test_live--;
#endif
}
