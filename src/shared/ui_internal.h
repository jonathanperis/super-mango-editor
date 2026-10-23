/*
 * ui_internal.h — Helpers shared by ui.c, ui_widgets.c and ui_scroll.c.
 *
 * Not the public widget API (that is ui.h). These are the drawing, hit
 * testing and edit-buffer helpers the widget and scroll files call.
 */
#pragma once

#include <stddef.h>
#include "ui.h"

void draw_rect(int x, int y, int w, int h, Color c);
void draw_text(UIState *ui, int x, int y, const char *text, Color c);
int point_in_rect(int px, int py, int rx, int ry, int rw, int rh);
void note_widget(UIState *ui, int x, int y, int w, int h);
void field_cursor(UIState *ui, int x, int y, int w, int h, int is_active);
int pointer_over(const UIState *ui, int x, int y, int w, int h);
void notify_before_change(UIState *ui, int id);
void format_float(char *out, size_t out_size, float value);
void apply_pending_text_input(UIState *ui);
int neighbour_field(const UIState *ui, int id, int direction);
int field_drawn(UIState *ui, int id, int y);
int handle_tab(UIState *ui, int id);
int command_allows_activation(UIState *ui, int id);
void draw_control_border(UIState *ui, int x, int y, int w, int h);
void draw_field_border(UIState *ui, int x, int y, int w, int h, int is_active);
int draw_field_text(UIState *ui, int x, int y, int max_width, const char *text, Color color);
void edit_mouse(UIState *ui, int x, int y, int w, int h, int activated);
void draw_active_edit(UIState *ui, int x, int y, int w);
int is_continuation_byte(char c);
int text_width_of(UIState *ui, const char *text, size_t from, size_t to);
