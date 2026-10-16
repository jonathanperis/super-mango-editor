/* Window creation and final presentation. Individual render modules draw in
 * logical coordinates; this file maps their completed canvas to the window. */
#include "graphics.h"
#include "../input/input_backend.h"

/* The player's first sprite frame (48x48 at the top-left of the sheet; see
 * PLAYER_FRAME_W in src/player/player_internal.h) doubles as the window and
 * taskbar icon on Windows and Linux. macOS takes an application's icon from
 * its bundle and GLFW refuses to set one there; a browser tab shows the
 * page's favicon. Neither of those tries. */
#if !defined(__EMSCRIPTEN__) && !defined(__APPLE__)
#define DISPLAY_ICON_PATH "assets/sprites/player/player.png"
#define DISPLAY_ICON_SIZE 48

static void display_set_icon(void)
{
    /* A missing or damaged sheet costs only the icon: the window keeps the
     * system's default one. FileExists first keeps a missing file quiet. */
    if (!FileExists(DISPLAY_ICON_PATH)) return;
    Image sheet = LoadImage(DISPLAY_ICON_PATH);
    if (!IsImageValid(sheet)) return;
    if (sheet.width >= DISPLAY_ICON_SIZE && sheet.height >= DISPLAY_ICON_SIZE) {
        ImageCrop(&sheet, (Rectangle){0, 0, DISPLAY_ICON_SIZE, DISPLAY_ICON_SIZE});
        /* GLFW accepts only 8-bit RGBA, whatever the PNG decoded to, and
         * copies the pixels before SetWindowIcon returns. */
        ImageFormat(&sheet, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
        SetWindowIcon(sheet);
    }
    UnloadImage(sheet);
}
#endif

int display_open(int width, int height, const char *title, int hidden)
{
    SetTraceLogLevel(LOG_WARNING);
    unsigned int flags = hidden ? FLAG_WINDOW_HIDDEN : FLAG_VSYNC_HINT;
#ifdef __APPLE__
    /* Without this flag a Retina Mac gets a framebuffer at half its pixel
     * density, which the compositor then stretches, blurring the pixel art.
     * With it the framebuffer is full density while sizes stay in points:
     * GetScreenWidth and glfwGetCursorPos both report points, and raylib's
     * screen-scale matrix (applied by BeginDrawing and EndTextureMode)
     * turns display_present's point rectangle into device pixels. So the
     * letterbox below and input_pointer_to_logical keep using one unit.
     *
     * Not on Windows or X11: there raylib honours the flag by enlarging the
     * window by the monitor's scale and calling SetMouseScale. Our GLFW
     * cursor callbacks in input_backend.c read raw cursor positions, which
     * SetMouseScale does not reach, so clicks would land in the wrong place.
     * Those systems already give the window one device pixel per unit. */
    flags |= FLAG_WINDOW_HIGHDPI;
#endif
    SetConfigFlags(flags);
    InitWindow(width, height, title);
    if (!IsWindowReady()) return -1;
    /* Escape belongs to pause/cancel/menu actions, not automatic raylib exit. */
    SetExitKey(KEY_NULL);
#if !defined(__EMSCRIPTEN__) && !defined(__APPLE__)
    /* Hidden test and smoke windows are never seen; skip the decode. */
    if (!hidden) display_set_icon();
#endif
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
