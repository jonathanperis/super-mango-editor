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
#define CAMPAIGN_BUTTONS_Y     (TOOLBAR_H + CANVAS_H - 40)
#define CAMPAIGN_BUTTON_W      96
#define CAMPAIGN_BUTTON_STEP   (CAMPAIGN_BUTTON_W + 8)
/* Buttons, left to right: Up, Down, Remove, Add level..., Link in order,
 * Save, Close.  Button i starts at CAMPAIGN_VIEW_X + i * CAMPAIGN_BUTTON_STEP. */
#define CAMPAIGN_NAME_FIELD_ID 7000                /* + row index          */

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

/* Free the view's memory (editor shutdown). */
void editor_campaign_free(EditorState *es);

/* Rerun the game's campaign rules on the entries as edited. */
void editor_campaign_check(EditorState *es);

/* 1 when the entries, their order or a level's name or next_phase differ
 * from the files on disk. */
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
 * levels changed. */
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
