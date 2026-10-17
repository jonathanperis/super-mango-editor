/*
 * editor_minimap.c — The level overview strip (see editor_minimap.h).
 *
 * Plain DrawRectangle calls every frame, no render texture: a level holds a
 * few hundred entities at most, and each is one rectangle, so redrawing is
 * cheaper than keeping a texture in step with every edit and undo.
 */

#include "editor_minimap.h"
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
static void draw_world_rect(float world_w, float x, float y, float w, float h, Color color)
{
    float sx = (float)EDITOR_MINIMAP_WIDTH / world_w, sy = (float)EDITOR_MINIMAP_HEIGHT / (float)GAME_H;
    int left = EDITOR_MINIMAP_X + (int)(x * sx), top = EDITOR_MINIMAP_Y + (int)(y * sy);
    int width = (int)(w * sx), height = (int)(h * sy);
    if (width < 1) width = 1;
    if (height < 1) height = 1;
    /* Keep everything inside the strip, whatever a hand-edited level says. */
    if (left < EDITOR_MINIMAP_X) { width -= EDITOR_MINIMAP_X - left; left = EDITOR_MINIMAP_X; }
    if (top < EDITOR_MINIMAP_Y) { height -= EDITOR_MINIMAP_Y - top; top = EDITOR_MINIMAP_Y; }
    if (left + width > EDITOR_MINIMAP_X + EDITOR_MINIMAP_WIDTH) width = EDITOR_MINIMAP_X + EDITOR_MINIMAP_WIDTH - left;
    if (top + height > EDITOR_MINIMAP_Y + EDITOR_MINIMAP_HEIGHT) height = EDITOR_MINIMAP_Y + EDITOR_MINIMAP_HEIGHT - top;
    if (width > 0 && height > 0) DrawRectangle(left, top, width, height, color);
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
