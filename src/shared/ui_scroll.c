/*
 * ui_scroll.c — Scrolled panels and their scrollbars.
 *
 * A panel calls ui_scroll_begin / ui_scroll_end around the rows it draws.
 * The thumb arithmetic is private to this file; hit-testing uses the same
 * pointer helper as the widgets (ui_internal.h).
 */

#include "platform.h"

#include "ui.h"
#include "ui_internal.h"

/* ------------------------------------------------------------------ */
/* Scrolling                                                           */
/* ------------------------------------------------------------------ */

int ui_scroll_wheel(UIScroll *scroll, float wheel, float units_per_notch)
{
    int units;

    if (!scroll) return 0;
    scroll->carry += -wheel * units_per_notch;
    units = (int)scroll->carry;            /* toward 0, sign kept */
    scroll->carry -= (float)units;
    scroll->offset += units;
    if (scroll->offset < 0) scroll->offset = 0;
    return units;
}

void ui_scroll_clamp(UIScroll *scroll, int content, int view)
{
    int max_offset = content - view;

    if (!scroll) return;
    if (max_offset < 0) max_offset = 0;
    if (scroll->offset > max_offset) scroll->offset = max_offset;
    if (scroll->offset < 0) scroll->offset = 0;
}

void ui_clip_begin(UIState *ui, int x, int y, int w, int h)
{
    if (ui) {
        ui->clip_active = 1;
        ui->clip_x = x;
        ui->clip_y = y;
        ui->clip_w = w;
        ui->clip_h = h;
    }
    if (IsWindowReady()) BeginScissorMode(x, y, w, h);
}

void ui_clip_end(UIState *ui)
{
    if (ui) ui->clip_active = 0;
    if (IsWindowReady()) EndScissorMode();
}

int ui_scroll_begin(UIState *ui, UIScroll *scroll, int x, int y, int w, int view_h,
                    int content_h)
{
    ui_scroll_clamp(scroll, content_h, view_h);
    ui_clip_begin(ui, x, y, w, view_h);
    return y - (scroll ? scroll->offset : 0);
}

void ui_scroll_end(UIState *ui)
{
    ui_clip_end(ui);
}

void ui_scroll_reveal(UIScroll *scroll, int top, int bottom,
                      int view_top, int view_bottom, int margin)
{
    if (!scroll) return;
    if (top < view_top) scroll->offset -= view_top - top + margin;
    else if (bottom > view_bottom) scroll->offset += bottom - view_bottom + margin;
    if (scroll->offset < 0) scroll->offset = 0;
}

/* The thumb's place and length on a track of h pixels; 0 when it all fits. */
static int scrollbar_thumb(int y, int h, int view, int content, int offset,
                           int *thumb_y, int *thumb_h)
{
    if (content <= view || view <= 0 || h <= 0) return 0;
    int length = (int)((int64_t)h * view / content);
    if (length < 12) length = 12;
    if (length > h) length = h;
    int max_offset = content - view;
    if (offset < 0) offset = 0;
    if (offset > max_offset) offset = max_offset;
    *thumb_h = length;
    *thumb_y = y + (int)((int64_t)(h - length) * offset / max_offset);
    return 1;
}

int ui_scrollbar_input(UIState *ui, UIScroll *scroll, int x, int y, int w, int h,
                       int view, int content)
{
    int thumb_y, thumb_h, before;

    if (!ui || !scroll) return 0;
    before = scroll->offset;
    if (!scrollbar_thumb(y, h, view, content, scroll->offset, &thumb_y, &thumb_h)) {
        if (ui->scroll_drag == scroll) ui->scroll_drag = NULL;
        return 0;
    }
    if (ui->scroll_drag == scroll) {
        if (!ui->mouse_down) {
            ui->scroll_drag = NULL;
        } else {
            ui->scroll_drag_seen = 1;
            /* Put the thumb where the pointer holds it, as a share of the
             * track it can travel, rounded to the nearest unit. */
            int travel = h - thumb_h;
            int top = ui->mouse_y - ui->scroll_grab - y;
            if (top < 0) top = 0;
            if (top > travel) top = travel;
            scroll->offset = travel > 0
                ? (int)(((int64_t)top * (content - view) * 2 + travel) / (2 * (int64_t)travel)) : 0;
        }
    } else if (ui->mouse_clicked && pointer_over(ui, x, y, w, h)) {
        if (ui->mouse_y >= thumb_y && ui->mouse_y < thumb_y + thumb_h) {
            ui->scroll_drag = scroll;
            ui->scroll_grab = ui->mouse_y - thumb_y;
            ui->scroll_drag_seen = 1;
        } else {
            scroll->offset += ui->mouse_y < thumb_y ? -view : view;
        }
        ui->mouse_clicked = 0;   /* the bar used this click */
    }
    ui_scroll_clamp(scroll, content, view);
    return scroll->offset != before;
}

void ui_scrollbar_draw(UIState *ui, const UIScroll *scroll, int x, int y, int w, int h,
                       int view, int content)
{
    int thumb_y, thumb_h;

    if (!ui || !scroll ||
        !scrollbar_thumb(y, h, view, content, scroll->offset, &thumb_y, &thumb_h)) return;
    draw_rect(x, y, w, h, ui_color(ui, UI_COLOR_TITLE));
    draw_rect(x, thumb_y, w, thumb_h,
              ui_color(ui, ui->scroll_drag == scroll ? UI_COLOR_ACCENT : UI_COLOR_TEXT_DIM));
}
