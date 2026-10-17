/*
 * dialog_choice.h — The editor's in-window button prompts.
 *
 * dialog_choice (declared in file_dialog.h, beside the native file
 * pickers, where its callers already find it) asks a two- or three-button
 * question inside the editor window: the last full frame dimmed behind a
 * box with the title, the message and the buttons.  It blocks like the
 * native dialogs it replaces, running its own small frame loop until a
 * button is chosen.  This header is for the editor that hosts it and for
 * tests.
 */
#pragma once

#include "editor.h"  /* EditorState */

/* Longest message a prompt shows, in wrapped lines; more are cut off. */
#define DIALOG_MAX_LINES 16

/* Where a prompt puts everything, in logical editor pixels. */
typedef struct {
    int box_x, box_y, box_w, box_h;
    int title_h;                       /* the title bar at the box's top  */
    int text_x, text_y, line_h;
    int line_count;
    int line_start[DIALOG_MAX_LINES];  /* byte offset into the message    */
    int line_length[DIALOG_MAX_LINES];
    int button_count;
    int button_x[3], button_w[3];
    int button_y, button_h;
} DialogLayout;

/*
 * dialog_choice_attach — Give the prompts an editor to draw over: its
 * window, font, style, frame target and the backdrop copy of its last
 * frame.  editor_init attaches; editor_cleanup passes NULL.  With nothing
 * attached, dialog_choice fails (returns -1) without asking.
 */
void dialog_choice_attach(EditorState *es);

/*
 * dialog_choice_keep_frame — Copy the finished frame (es->frame_target)
 * into es->prompt_backdrop.  A prompt opened while the next frame is half
 * drawn shows this complete one behind it.  Call after the frame's
 * drawing, before presenting it; it ends texture mode.
 */
void dialog_choice_keep_frame(EditorState *es);

#ifdef MANGO_TESTING
/* Frames the prompts have drawn since the program started, and the layout
 * of the last prompt opened (a test can then click its buttons). */
int dialog_choice_test_frames(void);
const DialogLayout *dialog_choice_test_layout(void);
/* Called in each prompt frame right after its events are collected, where
 * a real window could deliver anything (another dropped file, say).  NULL
 * stops it. */
void dialog_choice_test_set_poll_hook(void (*hook)(void));
#endif
