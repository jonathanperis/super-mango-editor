#include "graphics.h"
#include "../input/input_backend.h"

int display_open(int width, int height, const char *title, int hidden)
{
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(hidden ? FLAG_WINDOW_HIDDEN : FLAG_VSYNC_HINT);
    InitWindow(width, height, title);
    if (!IsWindowReady()) return -1;
    SetExitKey(KEY_NULL);
    SetTargetFPS(hidden ? 0 : 60);
    return 0;
}

void display_present(RenderTexture2D target)
{
    EndTextureMode();
    ClearBackground(BLACK);
    float sx = (float)GetScreenWidth()/target.texture.width;
    float sy = (float)GetScreenHeight()/target.texture.height;
    float scale = sx < sy ? sx : sy;
    float width = target.texture.width*scale, height = target.texture.height*scale;
    Rectangle dest = {(GetScreenWidth()-width)/2, (GetScreenHeight()-height)/2, width, height};
    Rectangle source = {0, 0, (float)target.texture.width, -(float)target.texture.height};
    DrawTexturePro(target.texture, source, dest, (Vector2){0}, 0, WHITE);
    EndDrawing(); /* Exactly one event poll and frame-pacing owner. */
}
