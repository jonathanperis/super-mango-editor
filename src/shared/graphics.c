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

int display_open(int width, int height, const char *title, int hidden, int resizable)
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
    if (resizable) flags |= FLAG_WINDOW_RESIZABLE;
    SetConfigFlags(flags);
    InitWindow(width, height, title);
    if (!IsWindowReady()) return -1;
    /* A resizable window may grow, and display_present letterboxes the
     * fixed canvas into it, but never shrinks below one whole canvas:
     * smaller would shrink the canvas by a fraction and drop pixel rows,
     * leaving text and one-pixel lines unreadable. */
    if (resizable) SetWindowMinSize(width, height);
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

Rectangle display_viewport(int window_w, int window_h, int canvas_w, int canvas_h)
{
    if (window_w <= 0 || window_h <= 0 || canvas_w <= 0 || canvas_h <= 0) return (Rectangle){0};
    /* The smaller ratio keeps the aspect ratio; the rest stays black
     * (letterboxing). Integer division picks the largest whole multiple
     * that fits, so every canvas pixel covers the same square of window
     * pixels: at 2.5x, nearest-neighbour sampling would draw alternate
     * columns 2 and 3 pixels wide and the art would shimmer as it scrolls. */
    int scale = window_w / canvas_w < window_h / canvas_h ? window_w / canvas_w : window_h / canvas_h;
    if (scale >= 1) {
        int width = canvas_w*scale, height = canvas_h*scale;
        /* Whole-pixel offsets keep each canvas pixel on the window's grid. */
        return (Rectangle){(float)((window_w-width)/2), (float)((window_h-height)/2),
                           (float)width, (float)height};
    }
    /* A window smaller than the canvas cannot fit even 1x. Shrinking by a
     * fraction drops some rows and columns, but it still shows everything,
     * which beats cropping the HUD or the editor's panels. */
    float sx = (float)window_w / canvas_w, sy = (float)window_h / canvas_h;
    float fit = sx < sy ? sx : sy;
    float width = canvas_w*fit, height = canvas_h*fit;
    return (Rectangle){(window_w-width)/2, (window_h-height)/2, width, height};
}

int display_scale_fit(int requested, int canvas_w, int canvas_h, int monitor_w, int monitor_h)
{
    if (requested < 1) return 1;
    if (monitor_w <= 0 || monitor_h <= 0 || canvas_w <= 0 || canvas_h <= 0) return requested;
    int scale = requested;
    while (scale > 1 && (canvas_w*scale > monitor_w - DISPLAY_DECORATION_MARGIN ||
                         canvas_h*scale > monitor_h - DISPLAY_DECORATION_MARGIN)) scale--;
    return scale;
}

/* One axis of display_keep_on_monitor: the window spans [pos, pos+size);
 * lead is the room kept free before it (the title bar on the y axis). */
static int keep_on_axis(int pos, int size, int start, int length, int lead)
{
    int last = start + length - size;  /* rightmost/lowest position that fits */
    if (pos > last) pos = last;
    if (pos < start + lead) pos = start + lead;
    return pos;
}

IntRect display_keep_on_monitor(IntRect window, IntRect monitor)
{
    if (monitor.w <= 0 || monitor.h <= 0) return window;
    /* GLFW positions the client area; its title bar sits above that. Half
     * the margin covers a title bar on every desktop the game supports. */
    window.x = keep_on_axis(window.x, window.w, monitor.x, monitor.w, 0);
    window.y = keep_on_axis(window.y, window.h, monitor.y, monitor.h, DISPLAY_DECORATION_MARGIN/2);
    return window;
}

void display_present(RenderTexture2D target)
{
    /* Return from the logical render target to the real window framebuffer. */
    EndTextureMode();
    ClearBackground(BLACK);
    /* Input applies the inverse of this same mapping. */
    Rectangle dest = display_viewport(GetScreenWidth(), GetScreenHeight(),
                                      target.texture.width, target.texture.height);
    /* Render textures have the opposite Y orientation to ordinary textures.
     * A negative source height flips this completed image for presentation. */
    Rectangle source = {0, 0, (float)target.texture.width, -(float)target.texture.height};
    DrawTexturePro(target.texture, source, dest, (Vector2){0}, 0, WHITE);
    EndDrawing(); /* Exactly one event poll and frame-pacing owner. */
}
