/*
 * editor_campaign.h — The Campaign view: edit levels/campaigns/main.toml.
 *
 * The campaign manifest lists the levels the game's start menu offers, in
 * order, and each level's [last_star].next_phase must name the next one
 * (the last names none).  The view lists the entries with the game's own
 * verdict on each (campaign_catalog_check), and lets the designer reorder,
 * add and remove entries, rename a level (its `name` is the menu's label),
 * and relink every next_phase to match the order.  Save writes the changed
 * level files and then the manifest, each atomically.
 *
 * The view has an undo history of its own (the Undo and Redo buttons, or
 * Ctrl+Z / Ctrl+Y while it is open), apart from the level's: each of those
 * edits is one step.  Revert goes back to the files on disk in one go.
 *
 * While the view is open it covers the canvas; the level being edited is
 * untouched (unless Save rewrites that very file, which is then reloaded).
 */
#pragma once

#include "editor.h"   /* EditorState */
#include "../levels/campaign_catalog.h"  /* CampaignCatalog */

/* Layout of the view over the canvas, for drawing and for the UI test. */
#define CAMPAIGN_VIEW_X        16
#define CAMPAIGN_ROWS_Y        (TOOLBAR_H + 72)   /* first entry row      */
#define CAMPAIGN_ROW_H         24
#define CAMPAIGN_NAME_X        330                 /* the level name field */
#define CAMPAIGN_NAME_W        220
/* Right of the rows, which end CAMPAIGN_VIEW_X before the canvas edge. */
#define CAMPAIGN_SCROLLBAR_X   (CANVAS_W - CAMPAIGN_VIEW_X + 4)
#define CAMPAIGN_BUTTONS_Y     (TOOLBAR_H + CANVAS_H - 40)
#define CAMPAIGN_BUTTON_W      80
#define CAMPAIGN_BUTTON_H      24
#define CAMPAIGN_BUTTON_STEP   (CAMPAIGN_BUTTON_W + 6)
/* How many rows fit between the header and the buttons.  The 8 pixels kept
 * free above the buttons, and what is left over of a row, hold the one-line
 * notice below. */
#define CAMPAIGN_VISIBLE_ROWS  ((CAMPAIGN_BUTTONS_Y - 8 - CAMPAIGN_ROWS_Y) / CAMPAIGN_ROW_H)
/* A one-line notice between the last row and the buttons, shown while Undo
 * is stopped at a step it cannot replay. */
#define CAMPAIGN_NOTICE_Y      (CAMPAIGN_BUTTONS_Y - 14)
#define CAMPAIGN_NAME_FIELD_ID 7000                /* + row index          */

/*
 * The buttons along the bottom, left to right: the edits of the list, then
 * its history, then the files.  Button b starts at
 * CAMPAIGN_VIEW_X + b * CAMPAIGN_BUTTON_STEP.
 */
typedef enum {
    CAMPAIGN_BUTTON_UP,
    CAMPAIGN_BUTTON_DOWN,
    CAMPAIGN_BUTTON_REMOVE,
    CAMPAIGN_BUTTON_ADD,       /* "Add level..."                           */
    CAMPAIGN_BUTTON_LINK,      /* "Link in order"                          */
    CAMPAIGN_BUTTON_UNDO,      /* disabled while there is nothing to undo  */
    CAMPAIGN_BUTTON_REDO,      /* disabled while there is nothing to redo  */
    CAMPAIGN_BUTTON_SAVE,
    CAMPAIGN_BUTTON_REVERT,
    CAMPAIGN_BUTTON_CLOSE,
    CAMPAIGN_BUTTON_COUNT
} CampaignButton;

/* The whole row must end no further right than the row highlight does,
 * inside the canvas: the side panel starts at CANVAS_W and would cover a
 * button drawn past it.  A button added to the enum above without room for
 * it stops the build. */
_Static_assert(CAMPAIGN_VIEW_X + (CAMPAIGN_BUTTON_COUNT - 1) * CAMPAIGN_BUTTON_STEP +
               CAMPAIGN_BUTTON_W <= CANVAS_W - CAMPAIGN_VIEW_X,
               "the Campaign view's buttons must fit in the canvas");
/* The notice (one line of text) must clear the last row above it, whose
 * highlight starts 2 pixels above the row, and the buttons below it. */
_Static_assert(CAMPAIGN_ROWS_Y - 2 + CAMPAIGN_VISIBLE_ROWS * CAMPAIGN_ROW_H <= CAMPAIGN_NOTICE_Y &&
               CAMPAIGN_NOTICE_Y + TEXT_FONT_SIZE <= CAMPAIGN_BUTTONS_Y,
               "the Campaign view's notice must fit between the rows and the buttons");

/*
 * CAMPAIGN_HISTORY_MAX — how many edits the view can undo.  A step is under
 * a kilobyte (it names the level file instead of copying the level), so
 * the whole history is a fixed part of the view; once it is full, the
 * oldest step is dropped for each new one.
 */
#define CAMPAIGN_HISTORY_MAX 64

/*
 * editor_campaign_open — Load manifest_path (NULL: CAMPAIGN_MANIFEST_PATH)
 * and every level it lists, and show the view.  A manifest that cannot be
 * read is refused (status bar says why).  Returns 0 or -1.
 */
int editor_campaign_open(EditorState *es, const char *manifest_path);

/*
 * editor_campaign_close — Hide the view.  With unsaved changes the first
 * call only warns ("Close again to discard"); force discards at once.
 * Returns 1 when the view is closed.
 */
int editor_campaign_close(EditorState *es, int force);

/*
 * editor_campaign_revert — Throw away the view's unsaved changes (a name
 * being typed included) by reading the manifest and its levels again.
 * Like Close, the first call with unsaved changes only warns.  The undo
 * history is cleared with them.  Returns 1 when reverted, 0 when it only
 * warned or there was nothing to revert, -1 when the manifest can no
 * longer be read (the view is then closed).
 */
int editor_campaign_revert(EditorState *es);

/*
 * editor_campaign_undo / editor_campaign_redo — Take back the newest edit
 * made in the view (a move, remove, add, rename or relink), or make the
 * edit undone last again.  A name being typed is applied first, as its own
 * step.  A new edit clears what could be redone; Save keeps the history
 * (undoing past a Save shows the campaign as unsaved again); Revert and
 * closing the view clear it.
 *
 * A row that left the list comes back by reading its level file again.
 * When that file no longer loads, the step is refused: the list is left
 * unchanged and the status bar says so.
 *
 * A refused Undo would leave every older step out of reach, so it can be
 * skipped.  The first refusal only says so; the view then shows a notice
 * and its Undo button reads "Undo...".  From the second Undo on (file still
 * not loading) the editor asks, and only the answer "Skip this step" drops
 * the step: the removed level then stays out of the list, and the older
 * steps are rewritten for a list without that row, so each of them still
 * undoes and redoes exactly what it did to the other rows.  An older step
 * that only acted on that row has nothing left to do and goes with it; the
 * status bar says what was skipped and how many such steps went.  A refused
 * Redo is not skipped: any new edit clears the steps to redo anyway.
 *
 * Return 1 when a step was replayed, 0 when there was none, -1 when the
 * step was refused (status bar says why), 2 when a refused Undo step was
 * skipped (the list is unchanged).
 */
int editor_campaign_undo(EditorState *es);
int editor_campaign_redo(EditorState *es);

/* How many steps Undo and Redo can replay right now.  For tests. */
int editor_campaign_undo_steps(const EditorState *es);
int editor_campaign_redo_steps(const EditorState *es);

/* 1 while the newest step was refused by Undo because its level file does
 * not load, and nothing has changed the history since: the next Undo that
 * is refused too offers to skip it. */
int editor_campaign_undo_blocked(const EditorState *es);

/*
 * editor_campaign_finish_name — Apply the name being typed in the view, as
 * one step of its history.  editor_finish_field_edit calls this first, so
 * that however a name is applied (Enter, a click elsewhere, Ctrl+S...) it
 * is recorded here and never in the level's undo stack.  Returns 1 when a
 * name field was active and is now settled, 0 when the active field is not
 * one of the view's.
 */
int editor_campaign_finish_name(EditorState *es);

/* Free the view's memory (editor shutdown). */
void editor_campaign_free(EditorState *es);

/* Rerun the game's campaign rules on the entries as edited. */
void editor_campaign_check(EditorState *es);

/* 1 when the entries, their order or a level's name or next_phase differ
 * from the files on disk, or a name is being typed that differs from the
 * level's name. */
int editor_campaign_unsaved(const EditorState *es);

/* Move entry `index` up (-1) or down (+1) one place.  Returns 0 or -1. */
int editor_campaign_move(EditorState *es, int index, int delta);

/* Take entry `index` out of the campaign (its file is not touched). */
int editor_campaign_remove(EditorState *es, int index);

/*
 * editor_campaign_add — Append the level at `path`: a picked file, which
 * must sit directly in levels/ (the levels/<name>.toml rule), or the
 * levels/<name>.toml text itself.  Refused, with the reason in the status
 * bar, for any other file or one already listed.  Returns 0 or -1.
 */
int editor_campaign_add(EditorState *es, const char *path);

/* Point every listed level's next_phase at the entry after it (the last
 * at none), so the chain matches the order shown.  Returns how many
 * levels changed: 0 too when there is no memory to remember the old links
 * for Undo, in which case none is changed. */
int editor_campaign_link_in_order(EditorState *es);

/*
 * editor_campaign_save — Check, then write each level file whose name or
 * next_phase changed (a checked save: a file changed by someone else is
 * not overwritten) and the manifest when the list changed.  Refused while
 * the rules find a problem a Save could fix here, or when the level open
 * in the editor would be rewritten under unsaved edits.  Returns 0 or -1.
 */
int editor_campaign_save(EditorState *es);

/* Draw the view over the canvas and handle its widgets (canvas_render
 * calls this instead of drawing the level while the view is open). */
void editor_campaign_render(EditorState *es);

/* Scroll the entry list (mouse wheel over the view). */
void editor_campaign_wheel(EditorState *es, float wheel);

/* The entries as the view holds them, and its campaign-wide problem ("" for
 * none); NULL while the view is closed.  Read-only, for tests. */
const CampaignCatalog *editor_campaign_entries(const EditorState *es);
const char *editor_campaign_problem(const EditorState *es);

#ifdef MANGO_TESTING
/* Test seam (test builds only): while `fail` is 1, the history's one
 * allocation (the old links a relink keeps for Undo) fails. */
void editor_campaign_test_fail_history_allocation(int fail);
#endif
