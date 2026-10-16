/*
 * palette.c — Entity palette panel for the Super Mango level editor.
 *
 * Renders a scrollable, categorised list of all placeable entity types
 * on the right side of the editor window.  The palette lets designers pick
 * which entity type to stamp onto the canvas when the Place tool is active.
 *
 * Layout:
 *   - The panel sits at x = CANVAS_W (896), y = TOOLBAR_H (32).
 *   - Width is PANEL_W (384).
 *   - Height depends on whether an entity is selected on the canvas:
 *       No selection  -> full column height (CANVAS_H = 656).
 *       Selection     -> top half only (CANVAS_H / 2 = 328), so the
 *                         properties inspector can use the bottom half.
 *
 * Content:
 *   - A "PALETTE" title row at the top.
 *   - Six category sections, each with a header label and entity rows.
 *   - The currently selected palette entry is highlighted with the accent
 *     colour so the designer knows which type will be placed next.
 *   - Mouse-wheel scrolling when the content is taller than the visible area.
 *
 * This module follows the project's immediate-mode UI pattern: every frame
 * we re-draw all palette content and re-check for mouse interaction.  There
 * is no persistent widget tree.
 */

#include "palette.h"

#include <stdio.h>     /* snprintf */

#include "editor.h"    /* EditorState, EntityType, EditorTool, CANVAS_W, etc.  */
#include "entity_meta.h" /* shared palette names/categories                    */
#include "../shared/ui.h" /* UIState and shared widgets */

/* ------------------------------------------------------------------ */
/* Layout constants                                                    */
/* ------------------------------------------------------------------ */

/*
 * ROW_H — height in pixels of one entity entry row.
 * Sized to fit the 13-px font with comfortable padding above and below.
 */
#define ROW_H           22

/*
 * CATEGORY_H — height in pixels of a category header row.
 * Slightly taller than entity rows to give categories visual weight
 * and separate groups with breathing room.
 */
#define CATEGORY_H      28

/*
 * TITLE_H — height of the "PALETTE" title bar at the top of the panel.
 * Matches category header height for visual consistency.
 */
#define TITLE_H         28

/*
 * PAD_X — horizontal padding in pixels from the panel's left edge to
 * the start of text labels.  Keeps text from touching the panel border.
 */
#define PAD_X           12

/* ------------------------------------------------------------------ */
/* Scroll state — retained across frames                               */
/* ------------------------------------------------------------------ */


/*
 * category_open — tracks which categories are expanded (1) or collapsed (0).
 * All start collapsed.  Clicking a category header toggles it.
 */
static int category_open[EDITOR_ENTITY_CATEGORY_COUNT] = { 0 };


/* ------------------------------------------------------------------ */
/* Internal helpers — forward declarations                             */
/* ------------------------------------------------------------------ */

/*
 * point_in_rect — test whether pixel (px, py) lies inside a rectangle.
 *
 * Duplicated here as a static function to keep palette.c self-contained
 * (ui.c has its own static copy).  In C, static functions are private
 * to their translation unit, so identically named statics in different
 * .c files do not conflict at link time.
 */
static int point_in_rect(int px, int py, int rx, int ry, int rw, int rh);

/* ------------------------------------------------------------------ */
/* Helper implementation                                               */
/* ------------------------------------------------------------------ */

static int point_in_rect(int px, int py, int rx, int ry, int rw, int rh)
{
    return px >= rx && px < rx + rw &&
           py >= ry && py < ry + rh;
}

/* ------------------------------------------------------------------ */
/* Pieces of the palette, top to bottom                                */
/* ------------------------------------------------------------------ */

/*
 * palette_content_height — Height of everything below and including the
 * title: the title, every category header, and a row per entry of each
 * expanded category.  Collapsed categories contribute only their header.
 */
static int palette_content_height(void)
{
    int total_content_h = TITLE_H;
    for (int cat = 0; cat < EDITOR_ENTITY_CATEGORY_COUNT; cat++) {
        EditorEntityCategory category = (EditorEntityCategory)cat;
        total_content_h += CATEGORY_H;
        if (category_open[cat]) {
            int entry_count = editor_entity_palette_entry_count();
            for (int i = 0; i < entry_count; i++) {
                EntityType type = editor_entity_palette_entry_type(i);
                if (editor_entity_category(type) == category)
                    total_content_h += ROW_H;
            }
        }
    }
    return total_content_h;
}

/*
 * palette_title_bar — The fixed "PALETTE" title at the top of the panel.
 *
 * It does not scroll and is drawn with a slightly lighter background
 * (UI_COLOR_TITLE) than the content below.  Clicking it collapses or expands
 * the whole palette.
 */
static void palette_title_bar(EditorState *es, int panel_x, int panel_y)
{
    UIState *ui = &es->ui;

    DrawRectangle(panel_x, panel_y, PANEL_W, TITLE_H, ui_color(ui, UI_COLOR_TITLE));

    /* Click header to toggle expand/collapse */
    int hdr_hovered = (ui->mouse_x >= panel_x &&
                       ui->mouse_x < panel_x + PANEL_W &&
                       ui->mouse_y >= panel_y &&
                       ui->mouse_y < panel_y + TITLE_H);
    if (hdr_hovered && ui->mouse_clicked)
        es->palette_open = !es->palette_open;

    const char *pal_sym = es->palette_open ? "v" : ">";
    int pal_sym_w = ui_text_width(ui, pal_sym);
    ui_label_color(ui, panel_x + PAD_X, panel_y + 6, pal_sym, ui_color(ui, UI_COLOR_ACCENT));
    ui_label_color(ui, panel_x + PAD_X + pal_sym_w, panel_y + 6,
                   " PALETTE",
                   hdr_hovered ? ui_color(ui, UI_COLOR_TEXT) : ui_color(ui, UI_COLOR_TEXT_DIM));
}

/*
 * palette_category_header — One category header at row_y ("> Enemies").
 * Clicking it opens or closes that category.
 */
static void palette_category_header(UIState *ui, int cat, int panel_x, int row_y)
{
    int hdr_hovered = point_in_rect(ui->mouse_x, ui->mouse_y,
                                    panel_x, row_y,
                                    PANEL_W, CATEGORY_H);
    if (hdr_hovered && ui->mouse_clicked) {
        category_open[cat] = !category_open[cat];
    }

    /* Draw expand/collapse indicator and category name */
    const char *cat_sym = category_open[cat] ? "v" : ">";
    int cat_sym_w = ui_text_width(ui, cat_sym);
    ui_label_color(ui, panel_x + PAD_X, row_y + 7,
                   cat_sym, ui_color(ui, UI_COLOR_ACCENT));
    ui_label_color(ui, panel_x + PAD_X + cat_sym_w, row_y + 7,
                   editor_entity_category_name((EditorEntityCategory)cat),
                   hdr_hovered ? ui_color(ui, UI_COLOR_TEXT) : ui_color(ui, UI_COLOR_TEXT_DIM));

    ui_separator(ui,
                 panel_x + PAD_X,
                 row_y + CATEGORY_H - 2,
                 PANEL_W - PAD_X * 2);
}

/*
 * palette_entry_row — One entity row at row_y, and its click.
 *
 * The entry the next canvas click will place (palette_type while the Place
 * tool is active) has an accent background; a hovered row gets the button
 * hover colour.  Clicking a row picks that type and switches to Place.
 */
static void palette_entry_row(EditorState *es, EntityType type,
                              int panel_x, int row_y)
{
    UIState *ui = &es->ui;
    int is_selected = (type == es->palette_type && es->tool == TOOL_PLACE);
    int hovered = point_in_rect(ui->mouse_x, ui->mouse_y,
                                panel_x, row_y, PANEL_W, ROW_H);

    /* ---- Draw row background ---- */
    if (is_selected) {
        /* The selection colour carries the text colour in every style (the
         * default one is the accent blue). */
        DrawRectangle(panel_x, row_y, PANEL_W, ROW_H, ui_color(ui, UI_COLOR_SELECTED));
    } else if (hovered) {
        /* Feedback before the click. */
        DrawRectangle(panel_x, row_y, PANEL_W, ROW_H, ui_color(ui, UI_COLOR_HOT));
    }
    /* Else: no background drawn — the panel colour shows through. */

    /*
     * The name is the text colour (UI_COLOR_TEXT) whether or not the row is
     * selected: the selection background gives enough contrast, and dim text
     * would be hard to read.  The +4 vertical offset centres the 13-px
     * font within the 22-px row height: (22 - 13) / 2 ~ 4.
     */
    ui_label_color(ui,
                   panel_x + PAD_X + 8,  /* extra indent under category */
                   row_y + 4,
                   editor_entity_palette_name(type),
                   ui_color(ui, UI_COLOR_TEXT));

    /*
     * mouse_clicked is 1 only on the frame the button went down (not while
     * held), so one press changes the selection once.
     */
    if (hovered && ui->mouse_clicked) {
        es->palette_type = type;
        es->tool         = TOOL_PLACE;
    }
}

/* ------------------------------------------------------------------ */
/* palette_render                                                      */
/* ------------------------------------------------------------------ */

/*
 * palette_render — Draw the categorised entity palette and handle input.
 *
 * Every frame this:
 *
 *   1. Draws the panel background at the position the layout code in
 *      editor_panels.c passes in (between Level Config and properties).
 *
 *   2. Clamps the scroll offset to the content height, which depends on
 *      which categories are open.
 *
 *   3. Draws the title, then each category header and, for open
 *      categories, their entity rows, detecting clicks as it goes.
 *
 * The drawing uses a "cursor_y" approach: we start at the top of the
 * panel and move downward row by row.  The scroll offset shifts the
 * cursor upward so earlier content scrolls off the top edge.  We skip
 * drawing any row whose Y falls outside the visible clip region.
 */
void palette_render(EditorState *es, int start_y, int available_h)
{
    int panel_x = CANVAS_W;
    int panel_y = start_y;
    int panel_h = available_h;

    ui_panel(&es->ui, panel_x, panel_y, PANEL_W, panel_h);

    palette_title_bar(es, panel_x, panel_y);

    /* If collapsed, just show the header bar */
    if (!es->palette_open) {
        return;
    }

    /* ---- Draw scrolling content (categories + entries) ---- */

    int content_top = panel_y + TITLE_H;

    /*
     * Only the content below the title scrolls (es->palette_scroll, moved by
     * the wheel in editor_panels.c). ui_scroll_begin keeps the offset within
     * the content, clips drawing to the view and returns where the content's
     * top goes: above content_top once earlier entries have scrolled away.
     */
    int view_h = panel_h - TITLE_H;
    int content_h = palette_content_height() - TITLE_H;
    /* The scrollbar takes its clicks before the rows below can see them. */
    (void)ui_scrollbar_input(&es->ui, &es->palette_scroll, panel_x + PANEL_W - SCROLLBAR_W,
                             content_top, SCROLLBAR_W, view_h, view_h, content_h);
    int cursor_y = ui_scroll_begin(&es->ui, &es->palette_scroll, panel_x, content_top, PANEL_W,
                                   view_h, content_h);

    /*
     * clip_bottom — the Y coordinate of the panel's bottom edge.
     * Rows whose Y exceeds this value are outside the visible area and
     * should not be drawn or tested for clicks.
     */
    int clip_bottom = panel_y + panel_h;

    /* cursor_y advances downward as each category header and entity row is
     * laid out. */
    for (int cat = 0; cat < EDITOR_ENTITY_CATEGORY_COUNT; cat++) {
        EditorEntityCategory category = (EditorEntityCategory)cat;

        /* A header or row is drawn (and clickable) only if some of it
         * overlaps the visible band [content_top, clip_bottom). */
        if (cursor_y + CATEGORY_H > content_top && cursor_y < clip_bottom)
            palette_category_header(&es->ui, cat, panel_x, cursor_y);
        cursor_y += CATEGORY_H;

        if (!category_open[cat]) continue;

        int entry_count = editor_entity_palette_entry_count();
        for (int i = 0; i < entry_count; i++) {
            EntityType type = editor_entity_palette_entry_type(i);
            if (editor_entity_category(type) != category) continue;

            if (cursor_y + ROW_H > content_top && cursor_y < clip_bottom)
                palette_entry_row(es, type, panel_x, cursor_y);
            cursor_y += ROW_H;
        }
    }

    /* Remove clip rect so other panels are not affected */
    ui_scroll_end(&es->ui);
    ui_scrollbar_draw(&es->ui, &es->palette_scroll, panel_x + PANEL_W - SCROLLBAR_W,
                      content_top, SCROLLBAR_W, view_h, view_h, content_h);
}
