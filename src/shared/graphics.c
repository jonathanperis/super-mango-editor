/* Window creation and final presentation. Individual render modules draw in
 * logical coordinates; this file maps their completed canvas to the window. */
#include "graphics.h"
#include "../input/input_backend.h"

int display_open(int width, int height, const char *title, int hidden)
{
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(hidden ? FLAG_WINDOW_HIDDEN : FLAG_VSYNC_HINT);
    InitWindow(width, height, title);
    if (!IsWindowReady()) return -1;
    /* Escape belongs to pause/cancel/menu actions, not automatic raylib exit. */
    SetExitKey(KEY_NULL);
#ifdef __EMSCRIPTEN__
    /* Emscripten schedules frames through requestAnimationFrame. A second,
     * blocking limiter would occupy the browser's main thread in EndDrawing.
     * Return control to the browser instead of waiting inside its callback. */
    SetTargetFPS(0);
#else
    SetTargetFPS(hidden ? 0 : 60);
#endif
    return 0;
}

void display_present(RenderTexture2D target)
{
    /* Return from the logical render target to the real window framebuffer. */
    EndTextureMode();
    ClearBackground(BLACK);
    /* Choose the smaller ratio to keep the aspect ratio. Unused space stays
     * black (letterboxing). Input applies the inverse of this same mapping. */
    float sx = (float)GetScreenWidth() / target.texture.width;
    float sy = (float)GetScreenHeight() / target.texture.height;
    float scale = sx < sy ? sx : sy;
    float width = target.texture.width*scale, height = target.texture.height*scale;
    Rectangle dest = {(GetScreenWidth()-width)/2, (GetScreenHeight()-height)/2, width, height};
    /* Render textures have the opposite Y orientation to ordinary textures.
     * A negative source height flips this completed image for presentation. */
    Rectangle source = {0, 0, (float)target.texture.width, -(float)target.texture.height};
    DrawTexturePro(target.texture, source, dest, (Vector2){0}, 0, WHITE);
    EndDrawing(); /* Exactly one event poll and frame-pacing owner. */
}
