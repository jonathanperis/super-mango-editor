/*
 * editor_minimap.c — The level overview strip (see editor_minimap.h).
 *
 * Plain DrawRectangle calls every frame, no render texture: a level holds a
 * few hundred entities at most, and each is one rectangle, so redrawing is
 * cheaper than keeping a texture in step with every edit and undo.
 */

#include "editor_minimap.h"

#include <math.h> /* floorf, truncf */

#include "canvas.h"      /* canvas_clamp_camera */
#include "entity_meta.h" /* editor_entity_count, editor_entity_category, editor_world_width */
#include "hit_test.h"    /* editor_entity_bounds */

/*
 * Entity colours, by palette category. They are level content, not UI, so
 * they stay the same in the high-contrast style; the strip's background and
 * the viewport frame come from the UI style. Documented in level-editor.md.
 */
static const Color s_category_colors[EDITOR_ENTITY_CATEGORY_COUNT] = {
    [EDITOR_ENTITY_CATEGORY_WORLD]        = {150, 160, 185, 255}, /* rails, checkpoints, the player spawn */
    [EDITOR_ENTITY_CATEGORY_COLLECTIBLES] = {240, 200,  90, 255}, /* coins and stars: gold */
    [EDITOR_ENTITY_CATEGORY_ENEMIES]      = {235,  85,  85, 255}, /* red */
    [EDITOR_ENTITY_CATEGORY_HAZARDS]      = {255, 150,  40, 255}, /* orange */
    [EDITOR_ENTITY_CATEGORY_SURFACES]     = { 80, 190, 200, 255}, /* platforms, bridges, float platforms, pads: cyan */
    [EDITOR_ENTITY_CATEGORY_DECORATIONS]  = { 90, 170,  70, 255}, /* vines, ladders, ropes: green */
};
static const Color s_floor_color = {120, 90, 60, 255};

int editor_minimap_contains(int sx, int sy)
{
    return sx >= EDITOR_MINIMAP_X && sx < EDITOR_MINIMAP_X + EDITOR_MINIMAP_WIDTH &&
           sy >= EDITOR_MINIMAP_Y && sy < EDITOR_MINIMAP_Y + EDITOR_MINIMAP_HEIGHT;
}

float editor_minimap_world_x(const EditorState *es, int sx)
{
    float world_w = editor_world_width(&es->level);
    float across = (float)(sx - EDITOR_MINIMAP_X) / (float)EDITOR_MINIMAP_WIDTH;
    if (across < 0.0f) across = 0.0f;
    if (across > 1.0f) across = 1.0f;
    return across * world_w;
}

void editor_minimap_jump(EditorState *es, int sx)
{
    float zoom = es->camera.zoom > 0.0f ? es->camera.zoom : 1.0f;
    es->camera.x = editor_minimap_world_x(es, sx) - (float)CANVAS_W / (2.0f * zoom);
    canvas_clamp_camera(es);
}

/* World rectangle to strip pixels: x by the level's width, y by the
 * world's 300 px height, never thinner than one pixel so a coin shows. */
/* A strip coordinate, clipped to [0, max] while still a float: converting
 * an out-of-range float to int is undefined, and a hand-edited level can
 * hold any number. NaN counts as 0. */
static float strip_clamp(float v, int max)
{
    if (!(v > 0.0f)) return 0.0f;
    return v > (float)max ? (float)max : v;
}

int editor_minimap_strip_rect(float world_w, float x, float y, float w, float h,
                              int *left, int *top, int *width, int *height)
{
    float sx = (float)EDITOR_MINIMAP_WIDTH / world_w, sy = (float)EDITOR_MINIMAP_HEIGHT / (float)GAME_H;
    /* At least a pixel each way, so the smallest entity still shows. */
    float pw = w * sx, ph = h * sy;
    pw = pw >= 1.0f ? floorf(pw) : 1.0f;
    ph = ph >= 1.0f ? floorf(ph) : 1.0f;
    float x0 = truncf(x * sx), y0 = truncf(y * sy);
    /* Keep everything inside the strip, whatever a hand-edited level says. */
    int l = (int)strip_clamp(x0, EDITOR_MINIMAP_WIDTH), r = (int)strip_clamp(x0 + pw, EDITOR_MINIMAP_WIDTH);
    int t = (int)strip_clamp(y0, EDITOR_MINIMAP_HEIGHT), b = (int)strip_clamp(y0 + ph, EDITOR_MINIMAP_HEIGHT);
    *left = EDITOR_MINIMAP_X + l;
    *top = EDITOR_MINIMAP_Y + t;
    *width = r - l;
    *height = b - t;
    return *width > 0 && *height > 0;
}

static void draw_world_rect(float world_w, float x, float y, float w, float h, Color color)
{
    int left, top, width, height;
    if (editor_minimap_strip_rect(world_w, x, y, w, h, &left, &top, &width, &height))
        DrawRectangle(left, top, width, height, color);
}

void editor_minimap_render(EditorState *es)
{
    if (es->campaign) { es->minimap_dragging = 0; return; }
    float world_w = editor_world_width(&es->level);
    if (!(world_w > 0.0f)) return;

    /* A press on the strip scrolls there and keeps scrolling while held,
     * so a drag slides the view along the level. The press is the strip's,
     * like a button's: nothing else on the status bar acts on it. */
    if (es->ui.mouse_clicked && editor_minimap_contains(es->ui.mouse_x, es->ui.mouse_y)) {
        /* Jump on the press itself: a quick click can be released within
         * the same frame (likely at the unfocused 10 fps), and must still
         * move the view. Holding the button then drags. */
        editor_minimap_jump(es, es->ui.mouse_x);
        es->minimap_dragging = 1;
        es->ui.mouse_clicked = 0;
    }
    if (!es->ui.mouse_down) es->minimap_dragging = 0;
    if (es->minimap_dragging) editor_minimap_jump(es, es->ui.mouse_x);

    DrawRectangle(EDITOR_MINIMAP_X, EDITOR_MINIMAP_Y, EDITOR_MINIMAP_WIDTH, EDITOR_MINIMAP_HEIGHT,
                  ui_color(&es->ui, UI_COLOR_INPUT));
    /* The floor runs the whole level; its gaps are cut out of it below. */
    draw_world_rect(world_w, 0.0f, (float)FLOOR_Y, world_w, (float)(GAME_H - FLOOR_Y), s_floor_color);
    for (int t = 0; t < ENT_COUNT; t++) {
        EntityType type = (EntityType)t;
        int count = editor_entity_count(&es->level, type);
        Color color = type == ENT_FLOOR_GAP ? ui_color(&es->ui, UI_COLOR_INPUT)
                                            : s_category_colors[editor_entity_category(type)];
        for (int i = 0; i < count; i++) {
            EditorRect r;
            if (editor_entity_bounds(&es->level, type, i, &r))
                draw_world_rect(world_w, r.x, r.y, r.w, r.h, color);
        }
    }
    /* The part of the level the canvas shows. */
    float zoom = es->camera.zoom > 0.0f ? es->camera.zoom : 1.0f;
    float sx = (float)EDITOR_MINIMAP_WIDTH / world_w;
    int left = EDITOR_MINIMAP_X + (int)(es->camera.x * sx);
    int width = (int)((float)CANVAS_W / zoom * sx + 0.5f);
    if (width < 3) width = 3;
    if (left + width > EDITOR_MINIMAP_X + EDITOR_MINIMAP_WIDTH) width = EDITOR_MINIMAP_X + EDITOR_MINIMAP_WIDTH - left;
    DrawRectangleLines(left, EDITOR_MINIMAP_Y, width, EDITOR_MINIMAP_HEIGHT, ui_color(&es->ui, UI_COLOR_ACCENT));
}
