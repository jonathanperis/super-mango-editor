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

#include <SDL.h>       /* SDL_Rect, SDL_SetRenderDrawColor, SDL_RenderFillRect */
#include <stdio.h>     /* snprintf */

#include "editor.h"    /* EditorState, EntityType, EditorTool, CANVAS_W, etc.  */
#include "entity_meta.h" /* shared palette names/categories                    */
#include "ui.h"        /* UIState, ui_panel, ui_label_color, UI_* colours      */

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
 * scroll_y — vertical scroll offset in pixels for the palette content.
 *
 * When the palette has more entries than can fit in the visible area,
 * the user scrolls with the mouse wheel.  scroll_y tracks how many
 * pixels of content have been scrolled past the top edge.
 *
 * Static (file-scope) because this state must persist between frames
 * but is private to the palette module — no other file needs it.
 */
static int scroll_y = 0;

/*
 * category_open — tracks which categories are expanded (1) or collapsed (0).
 * All start collapsed.  Clicking a category header toggles it.
 */
static int category_open[EDITOR_ENTITY_CATEGORY_COUNT] = { 0 };

/*
 * palette_scroll — Adjust the palette scroll offset by a pixel delta.
 * Called from the editor's mouse wheel handler when the cursor is over
 * the palette area.  The clamp in palette_render keeps it in range.
 */
void palette_scroll(int delta) {
    scroll_y += delta;
    if (scroll_y < 0) scroll_y = 0;
}

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
/* palette_render                                                      */
/* ------------------------------------------------------------------ */

/*
 * palette_render — Draw the categorised entity palette and handle input.
 *
 * This function does three things every frame:
 *
 *   1. Compute the panel geometry (position, height) based on whether
 *      an entity is selected on the canvas.
 *
 *   2. Handle mouse-wheel scrolling when the cursor is over the panel
 *      and the content is taller than the visible area.
 *
 *   3. Draw the panel background, title, category headers, and entity
 *      rows.  Detect clicks on rows and update es->palette_type and
 *      es->tool accordingly.
 *
 * The drawing uses a "cursor_y" approach: we start at the top of the
 * panel and move downward row by row.  The scroll offset shifts the
 * cursor upward so earlier content scrolls off the top edge.  We skip
 * drawing any row whose Y falls outside the visible clip region.
 */
void palette_render(EditorState *es, int start_y, int available_h)
{
    UIState *ui = &es->ui;

    /* ---- Step 1: Compute panel geometry ---- */

    /*
     * panel_x — the palette panel starts immediately after the canvas area.
     * panel_y / panel_h — passed in by the caller so the layout orchestrator
     * in editor.c can position the palette dynamically between the level
     * config section above and the properties section below.
     */
    int panel_x = CANVAS_W;
    int panel_y = start_y;
    int panel_h = available_h;

    /*
     * Draw the panel background — a dark rectangle that visually separates
     * the palette from the canvas on the left and provides contrast for
     * the text labels drawn on top.
     */
    ui_panel(ui, panel_x, panel_y, PANEL_W, panel_h);

    /* ---- Step 2: Calculate total content height ---- */

    /*
     * Total height accounts for headers always, but only adds row heights
     * for expanded categories.
     */
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

    /*
     * visible_h — the drawable area inside the panel, excluding the title.
     * Content below the title scrolls; the title itself stays fixed.
     */
    int visible_h = panel_h - TITLE_H;

    /*
     * scrollable_h — how much content extends beyond the visible area.
     * If this is zero or negative, all content fits and no scrolling is needed.
     * We subtract TITLE_H from total because the title doesn't scroll.
     */
    int scrollable_h = (total_content_h - TITLE_H) - visible_h;
    if (scrollable_h < 0) scrollable_h = 0;

    /* ---- Step 3: Handle mouse-wheel scrolling ---- */

    /*
     * Only process scroll input when the mouse cursor is hovering over
     * the palette panel.  This prevents accidental scrolling when the
     * designer is interacting with the canvas or other panels.
     *
     * SDL tracks the mouse wheel via SDL_MouseWheelEvent, but our
     * immediate-mode UI doesn't expose raw SDL events.  Instead we
     * read the mouse position and check SDL's wheel state directly.
     * The editor main loop typically pumps SDL_GetMouseState and wheel
     * events into UIState; here we use SDL_GetMouseState as a fallback
     * for the position check.
     *
     * For now, scroll_y is only modified externally (the editor main loop
     * should call palette_scroll or set scroll_y via a public API if
     * mouse wheel events are needed).  The clamp below keeps it in range.
     */

    /* Clamp scroll_y to valid range [0, scrollable_h]. */
    if (scroll_y < 0)             scroll_y = 0;
    if (scroll_y > scrollable_h)  scroll_y = scrollable_h;

    /* ---- Step 4: Draw the title bar ---- */

    /*
     * The title sits at the very top of the panel and does NOT scroll.
     * We draw it with a slightly lighter background (UI_TITLE_BG) to
     * distinguish it from the scrolling content below.
     */
    /* ---- Collapsible title bar ---- */
    {
        SDL_Color title_bg = UI_TITLE_BG;
        SDL_SetRenderDrawColor(ui->renderer,
                               title_bg.r, title_bg.g, title_bg.b, title_bg.a);
        SDL_Rect title_rect = { panel_x, panel_y, PANEL_W, TITLE_H };
        SDL_RenderFillRect(ui->renderer, &title_rect);

        /* Click header to toggle expand/collapse */
        int hdr_hovered = (ui->mouse_x >= panel_x &&
                           ui->mouse_x < panel_x + PANEL_W &&
                           ui->mouse_y >= panel_y &&
                           ui->mouse_y < panel_y + TITLE_H);
        if (hdr_hovered && ui->mouse_clicked)
            es->palette_open = !es->palette_open;

        const char *pal_sym = es->palette_open ? "v" : ">";
        int pal_sym_w = ui_text_width(ui, pal_sym);
        ui_label_color(ui, panel_x + PAD_X, panel_y + 6, pal_sym, UI_ACCENT);
        ui_label_color(ui, panel_x + PAD_X + pal_sym_w, panel_y + 6,
                       " PALETTE",
                       hdr_hovered ? UI_TEXT : UI_TEXT_DIM);
    }

    /* If collapsed, just show the header bar */
    if (!es->palette_open) {
        return;
    }

    /* ---- Draw scrolling content (categories + entries) ---- */

    int content_top = panel_y + TITLE_H;

    SDL_Rect pal_clip = { panel_x, content_top, PANEL_W, panel_h - TITLE_H };
    SDL_RenderSetClipRect(ui->renderer, &pal_clip);

    /*
     * clip_bottom — the Y coordinate of the panel's bottom edge.
     * Rows whose Y exceeds this value are outside the visible area and
     * should not be drawn or tested for clicks.
     */
    int clip_bottom = panel_y + panel_h;

    /*
     * cursor_y — a running Y position that advances downward as we lay
     * out each category header and entity row.  The scroll offset shifts
     * it upward so earlier content scrolls off the top.
     *
     * Starting value: content_top minus the scroll offset.  As scroll_y
     * increases, cursor_y starts further above the visible area, causing
     * the top entries to disappear and later entries to slide into view.
     */
    int cursor_y = content_top - scroll_y;

    /*
     * Iterate through categories in order (0..5).  For each category,
     * draw its header, then draw every entry that belongs to it.
     */
    for (int cat = 0; cat < EDITOR_ENTITY_CATEGORY_COUNT; cat++) {
        EditorEntityCategory category = (EditorEntityCategory)cat;

        /* ---- Category header ---- */

        /*
         * Only draw the header if it falls within the visible clip region.
         * The header must overlap the vertical band [content_top, clip_bottom)
         * to be at least partially visible.
         */
        if (cursor_y + CATEGORY_H > content_top && cursor_y < clip_bottom) {
            /* Click on category header toggles open/closed */
            int hdr_hovered = point_in_rect(ui->mouse_x, ui->mouse_y,
                                            panel_x, cursor_y,
                                            PANEL_W, CATEGORY_H);
            if (hdr_hovered && ui->mouse_clicked) {
                category_open[cat] = !category_open[cat];
            }

            /* Draw expand/collapse indicator and category name */
            const char *cat_sym = category_open[cat] ? "v" : ">";
            int cat_sym_w = ui_text_width(ui, cat_sym);
            ui_label_color(ui, panel_x + PAD_X, cursor_y + 7,
                           cat_sym, UI_ACCENT);
            ui_label_color(ui, panel_x + PAD_X + cat_sym_w, cursor_y + 7,
                           editor_entity_category_name(category),
                           hdr_hovered ? UI_TEXT : UI_TEXT_DIM);

            ui_separator(ui,
                         panel_x + PAD_X,
                         cursor_y + CATEGORY_H - 2,
                         PANEL_W - PAD_X * 2);
        }

        /* Advance cursor past the category header. */
        cursor_y += CATEGORY_H;

        /* ---- Entity rows for this category (only if expanded) ---- */

        if (!category_open[cat]) continue;

        int entry_count = editor_entity_palette_entry_count();
        for (int i = 0; i < entry_count; i++) {
            EntityType type = editor_entity_palette_entry_type(i);
            if (editor_entity_category(type) != category) continue;

            /*
             * Determine if this row is within the visible vertical clip
             * region.  Rows entirely above content_top (scrolled past)
             * or entirely below clip_bottom (below the panel) are skipped.
             */
            int row_visible = (cursor_y + ROW_H > content_top &&
                               cursor_y < clip_bottom);

            if (row_visible) {

                /*
                 * Check if this entry is the currently selected palette type.
                 * An entry is "selected" when its EntityType matches the one
                 * stored in es->palette_type AND the active tool is TOOL_PLACE.
                 *
                 * We highlight the selected row with the accent colour
                 * (#4A90D9) so the designer immediately sees which entity
                 * type will be placed on the next canvas click.
                 */
                int is_selected = (type == es->palette_type &&
                                   es->tool == TOOL_PLACE);

                /*
                 * Check if the mouse is hovering over this row.
                 * We test against the full row rectangle (panel-wide, ROW_H tall).
                 */
                int hovered = point_in_rect(ui->mouse_x, ui->mouse_y,
                                            panel_x, cursor_y,
                                            PANEL_W, ROW_H);

                /* ---- Draw row background ---- */

                if (is_selected) {
                    /*
                     * Selected row — draw with the accent colour to make it
                     * stand out.  UI_ACCENT is a blue (#4A90D9) that contrasts
                     * well against the dark panel background.
                     */
                    SDL_Color accent = UI_ACCENT;
                    SDL_SetRenderDrawColor(ui->renderer,
                                           accent.r, accent.g, accent.b,
                                           accent.a);
                    SDL_Rect row_rect = { panel_x, cursor_y, PANEL_W, ROW_H };
                    SDL_RenderFillRect(ui->renderer, &row_rect);

                } else if (hovered) {
                    /*
                     * Hovered row — draw with the button-hover colour to
                     * provide visual feedback before the click.
                     */
                    SDL_Color hot = UI_BTN_HOT;
                    SDL_SetRenderDrawColor(ui->renderer,
                                           hot.r, hot.g, hot.b, hot.a);
                    SDL_Rect row_rect = { panel_x, cursor_y, PANEL_W, ROW_H };
                    SDL_RenderFillRect(ui->renderer, &row_rect);
                }
                /* Else: no background drawn — the panel's UI_BG shows through. */

                /*
                 * Draw the entity name label.
                 *
                 * Text colour is bright white (UI_TEXT) regardless of selection
                 * state — the accent background provides enough contrast for
                 * selected rows, and dim text would be hard to read.
                 *
                 * The +4 vertical offset centres the 13-px font within the
                 * 22-px row height: (22 - 13) / 2 ~ 4.
                 */
                ui_label_color(ui,
                               panel_x + PAD_X + 8,  /* extra indent under category */
                               cursor_y + 4,
                                editor_entity_palette_name(type),
                                UI_TEXT);

                /* ---- Handle click on this row ---- */

                /*
                 * If the user clicked on this row, select this entity type
                 * for placement.  We set palette_type to the entry's type
                 * and switch the tool to TOOL_PLACE so the next canvas click
                 * stamps this entity.
                 *
                 * mouse_clicked is 1 only on the frame the button went down
                 * (not while held), preventing repeated selection changes
                 * from a single press.
                 */
                if (hovered && ui->mouse_clicked) {
                    es->palette_type = type;
                    es->tool         = TOOL_PLACE;
                }
            }

            /* Advance cursor past this entity row. */
            cursor_y += ROW_H;
        }
    }

    /* Remove clip rect so other panels are not affected */
    SDL_RenderSetClipRect(ui->renderer, NULL);
}
