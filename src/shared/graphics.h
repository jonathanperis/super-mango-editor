/* Small drawing helpers that keep integer gameplay geometry separate from
 * raylib's floating-point drawing rectangles. These are project functions. */
#pragma once

#include <raylib.h>
#include <stdlib.h>
#include "geometry.h"

/* Open the process's one window at width x height. hidden: an unseen
 * window for tests and smoke runs. resizable: the user may resize it, down
 * to width x height (the editor); the canvas keeps its size and
 * display_present letterboxes it at a whole-number scale. */
int display_open(int width, int height, const char *title, int hidden, int resizable);

/*
 * display_frame_cap — How many frames a second to draw on a display that
 * refreshes refresh_hz times a second: that same number, so every refresh
 * shows a new frame.  A rate the system cannot report (0), or one outside
 * DISPLAY_FRAME_CAP_MIN..DISPLAY_FRAME_CAP_MAX, gives
 * DISPLAY_FRAME_CAP_DEFAULT.  display_open applies it to a visible native
 * window, and display_follow_monitor applies it again when that window is
 * moved to another display; the browser paces itself and tests run unpaced.
 */
#define DISPLAY_FRAME_CAP_DEFAULT 60
#define DISPLAY_FRAME_CAP_MIN     30
#define DISPLAY_FRAME_CAP_MAX     360
int display_frame_cap(int refresh_hz);

/*
 * display_follow_monitor — Keep the frame cap on the window's display.
 *
 * Call it once a frame. A window dragged from a 60 Hz laptop screen to a
 * 144 Hz monitor would otherwise keep drawing 60 frames a second there
 * (and one going the other way would draw frames nobody sees). When the
 * window's monitor has changed since the cap was set, the cap is set again
 * from the new display's refresh rate. Asking which monitor the window is
 * on is cheap; the refresh rate is only asked for after a change.
 *
 * Does nothing for a window that is not paced: a hidden one, the browser
 * build, and the Memory test backend.
 */
void display_follow_monitor(void);

/*
 * display_monitor_changed — The decision display_follow_monitor makes,
 * written out so a test can check it without a second display.
 * paced_monitor is the monitor the current cap came from, or -1 when the
 * window is not paced; current_monitor is the one it is on now. 1 means
 * "set the cap again".
 */
int display_monitor_changed(int paced_monitor, int current_monitor);

/* Finish the frame: end texture mode, draw the canvas into the window
 * (display_viewport) and call EndDrawing. Returns clock_cpu_seconds() taken when
 * every draw had been handed to the renderer but before EndDrawing, which
 * waits for the display; the game subtracts its start time from it to show
 * how long drawing took. Callers that do not time frames ignore it. */
double display_present(RenderTexture2D target);
/* Where display_present draws a canvas_w x canvas_h canvas inside a
 * window_w x window_h window, in window units. At 1x or more it is the
 * largest whole-number scale that fits, centred on whole-pixel offsets;
 * a smaller window gets a fractional fit. All-zero for empty sizes.
 * input_pointer_to_logical inverts it, so clicks match what is drawn. */
Rectangle display_viewport(int window_w, int window_h, int canvas_w, int canvas_h);

/* Room a window leaves around itself on a monitor for its title bar,
 * borders and the system's taskbar or menu bar, in window units. */
#define DISPLAY_DECORATION_MARGIN 80

/* The largest whole-number scale from 1 to requested whose window (canvas
 * times scale) fits a monitor_w x monitor_h monitor with the decoration
 * margin to spare. Never below 1; an unknown (zero) monitor size leaves
 * requested as it is. */
int display_scale_fit(int requested, int canvas_w, int canvas_h, int monitor_w, int monitor_h);

/* Where to put window (its client area) so that it lies on monitor, with
 * room above it for the title bar: the same rectangle moved as little as
 * possible. A window larger than the monitor keeps its top-left corner on
 * it. Only x and y change. */
IntRect display_keep_on_monitor(IntRect window, IntRect monitor);

/* Asset slots own heap-stable raylib values. Borrowers never unload a slot.
 * Texture2D itself is a small handle, not the image pixels. Allocating a slot
 * preserves explicit NULL-on-failure and shared-pointer ownership throughout
 * the entity code; it does not allocate another copy of the GPU image.
 * The three functions below are defined in texture.c. */

/* Decode an image file and upload it. Returns an owned handle, or NULL when
 * the file is missing or is not an image. */
Texture2D *texture_load(const char *path);

/* Put a valid raylib texture into an owned handle, with the nearest-neighbour
 * filter pixel art needs. Every handle is made here; texture_load passes the
 * file it decoded. When memory runs out it unloads value and returns NULL. */
Texture2D *texture_own(Texture2D value);

/* Release the GPU image and the handle. NULL is ignored. */
void texture_unload(Texture2D *texture);

/*
 * Texture switches — a cheap stand-in for "draw calls".
 *
 * raylib collects quads into one batch and starts a new draw call each time
 * the texture changes (rlgl.h, rlSetTexture). So the number of times one
 * draw uses a different texture from the draw before it is roughly the
 * number of draw calls a frame costs. sprite_draw and font_draw report the
 * texture they use; the game takes the count once per frame for the debug
 * overlay and `make bench`. Shapes (DrawRectangle and friends) are not
 * counted, so the real number of batches is a little higher.
 * Defined in texture.c.
 */
void texture_note_use(unsigned int texture_id);
/* Switches counted since the previous call; starts the next count at 0. */
int texture_take_switches(void);

/*
 * image_file_is_opaque — 1 when every pixel of the image file has alpha 255.
 *
 * An opaque image hides whatever is behind it, so it can be drawn without
 * blending: blending a pixel of alpha 255 gives the source colour anyway,
 * and reading the destination to find that out is wasted work. Decodes
 * the file, so call it at load time, not per frame. 0 for a file that
 * cannot be read: blending stays on, which draws exactly as before.
 * Defined in texture.c.
 */
int image_file_is_opaque(const char *path);

/*
 * image_file_visible_rows — Which rows of an image file show anything.
 *
 * Sets *first_row to the first row that has a pixel with alpha above 0 and
 * *row_count to the number of rows from there to the last such row (0 for
 * a fully transparent image). Rows above and below that band are see-through
 * from edge to edge, so drawing them changes nothing on screen but still
 * costs the renderer a visit to every one of their pixels. Decodes the
 * file, so call it at load time, not per frame. Returns 0, and leaves both
 * outputs alone, for a file that cannot be read.
 * Defined in texture.c.
 */
int image_file_visible_rows(const char *path, int *first_row, int *row_count);

#ifdef MANGO_TESTING
/* Test builds only. texture_test_loads: image files texture_load has decoded
 * so far. texture_test_live: handles that exist right now, so a test can
 * check that every one made during a session was unloaded exactly once. */
int texture_test_loads(void);
int texture_test_live(void);
#endif

/*
 * DESTROY_TEX — unload a texture slot and clear it, in one statement.
 *
 * texture_unload already ignores NULL; setting the slot to NULL afterwards
 * makes a second cleanup of the same slot a harmless no-op. The resource
 * tables in game_resources.c and the editor's texture cleanup use it.
 * Slot owners clear pointers after unloading; borrowers never call this.
 */
#define DESTROY_TEX(tex) \
    do { texture_unload(tex); (tex) = NULL; } while (0)

enum { SPRITE_NORMAL = 0, SPRITE_FLIP_X = 1, SPRITE_FLIP_Y = 2 };

static inline Rectangle rect_to_raylib(IntRect r)
{
    return (Rectangle){(float)r.x, (float)r.y, (float)r.w, (float)r.h};
}

/* The destination is top-left based; raylib positions a rotated sprite by its
 * pivot instead. Keep that conversion in one place for all atlas consumers. */
static inline void sprite_draw_pivot(const Texture2D *texture, const IntRect *source,
                                     const IntRect *dest, float angle, int flip,
                                     Vector2 pivot, Color tint)
{
    if (!texture || !dest || dest->w <= 0 || dest->h <= 0) return;
    texture_note_use(texture->id);
    Rectangle src = source ? rect_to_raylib(*source) :
        (Rectangle){0, 0, (float)texture->width, (float)texture->height};
    /* Negative source dimensions mirror the sampled image. WHITE leaves
     * its original color/alpha unchanged; another tint affects this draw only. */
    if (flip & SPRITE_FLIP_X) src.width = -src.width;
    if (flip & SPRITE_FLIP_Y) src.height = -src.height;
    Rectangle dst = rect_to_raylib(*dest);
    dst.x += pivot.x;
    dst.y += pivot.y;
    /* angle is in degrees. dst locates the pivot; pivot is an offset from the
     * sprite's top-left, so add it once to retain the caller's top-left layout. */
    DrawTexturePro(*texture, src, dst, pivot, angle, tint);
}

static inline void sprite_draw(const Texture2D *texture, const IntRect *source,
                               const IntRect *dest, float angle, int flip, Color tint)
{
    if (!dest) return;
    Vector2 pivot = {dest->w / 2.0f, dest->h / 2.0f};
    sprite_draw_pivot(texture, source, dest, angle, flip, pivot, tint);
}

/*
 * sprite_draw_rows — Draw only rows [first_row, first_row + row_count) of a
 * texture, exactly where they land when the whole texture is stretched over
 * dest.
 *
 * A background layer stretched from 216 to 300 pixels is mostly transparent
 * sky; its visible band (image_file_visible_rows) may be a fifth of it. The
 * band's place on screen is worked out in floating point, so the band lines
 * up with the full stretch. The rows it covers come out the same, except
 * that a pixel row lying exactly on the border between two texture rows can
 * pick the neighbouring one: the renderer steps through texture rows by
 * adding a small amount per pixel row, and a quad that starts at a
 * different row rounds those sums differently. See the optimization
 * lessons page in the manual. texture and dest must be valid; a band of
 * no rows draws nothing.
 */
static inline void sprite_draw_rows(const Texture2D *texture, int first_row, int row_count,
                                    const IntRect *dest, Color tint)
{
    if (row_count <= 0) return;
    texture_note_use(texture->id);
    /* Screen pixels per texture row, e.g. 300 / 216 for a background layer. */
    float scale_y = (float)dest->h / (float)texture->height;
    Rectangle src = {0.0f, (float)first_row, (float)texture->width, (float)row_count};
    Rectangle dst = {(float)dest->x, (float)dest->y + (float)first_row * scale_y,
                     (float)dest->w, (float)row_count * scale_y};
    DrawTexturePro(*texture, src, dst, (Vector2){0.0f, 0.0f}, 0.0f, tint);
}
