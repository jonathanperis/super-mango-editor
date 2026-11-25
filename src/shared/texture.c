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
    Texture2D value = LoadTexture(path);
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
    UnloadTexture(*texture);
    free(texture);
#ifdef MANGO_TESTING
    s_test_live--;
#endif
}

/* The texture of the most recent sprite or text draw, and how many times
 * a draw has used another one since texture_take_switches last ran. */
static unsigned int s_last_texture_id;
static int s_texture_switches;

void texture_note_use(unsigned int texture_id)
{
    if (texture_id == s_last_texture_id) return;
    s_last_texture_id = texture_id;
    s_texture_switches++;
}

int texture_take_switches(void)
{
    int switches = s_texture_switches;
    s_texture_switches = 0;
    /* The next frame's first draw counts as a switch too: it starts a batch. */
    s_last_texture_id = 0;
    return switches;
}
