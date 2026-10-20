/*
 * editor_chrome.h — Toolbar and status bar rendering helpers.
 */

#ifndef EDITOR_CHROME_H
#define EDITOR_CHROME_H

#include <stddef.h> /* size_t */
#include "editor.h" /* EditorState */

/* Where the status bar's validation summary starts (it is a link). */
#define STATUS_SUMMARY_X 380

void editor_render_toolbar(EditorState *es);
void editor_render_status_bar(EditorState *es);
void editor_render_play_overlay(EditorState *es);
/* The status bar's tool label: "Tool: Select", or "Place: Coin" while
 * placing (what a click adds). */
void editor_status_tool_text(const EditorState *es, char *out, size_t out_size);

#endif /* EDITOR_CHROME_H */
