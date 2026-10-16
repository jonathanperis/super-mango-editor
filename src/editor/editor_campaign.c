/*
 * editor_campaign.c — The Campaign view (see editor_campaign.h).
 *
 * The view keeps the manifest's entries as a CampaignCatalog, the same
 * structure the game's start menu loads, so every verdict it shows comes
 * from the game's own rules (campaign_catalog_check in
 * src/levels/campaign_catalog.c).  Next to each entry it remembers what
 * the level file held when it was read (name, next_phase, fingerprint), to
 * know which files a Save must rewrite and that nobody else changed them.
 *
 * Every edit is also noted in a small history (CampaignStep below), so
 * Undo and Redo (the buttons, or Ctrl+Z and Ctrl+Y) walk back and forth
 * through them.  A step records what was done, not a copy of the level it
 * was done to: a LevelDef is far too big to keep one per step, and its file
 * is still on disk to read again.
 */

#include "editor_campaign.h"

#include <stdio.h>    /* snprintf */
#include <stdlib.h>   /* realloc, free, calloc */
#include <string.h>   /* memmove, memcpy, strcmp, strrchr */

#include "editor_files.h"    /* editor_path_for_display */
#include "editor_session.h"  /* editor_set_status, editor_finish_field_edit */
#include "file_dialog.h"     /* file_dialog_open */
#include "../levels/campaign_catalog.h"
#include "../levels/level_path.h"   /* level_resolve_path */
#include "../shared/serializer.h"   /* level_save_toml_checked */
#include "../shared/ui.h"

/* What one entry's level file held when the view read or last wrote it. */
typedef struct {
    char disk_name[LEVEL_NAME_CAPACITY];
    char disk_next[sizeof(((LevelDef *)0)->next_phase)];
    SerializerFileFingerprint fingerprint;
} CampaignDisk;

/* One line of the manifest file as the view read or last wrote it. */
typedef struct {
    char path[CAMPAIGN_LEVEL_PATH_SIZE];
} CampaignListed;

/*
 * What "Link in order" did to one row, kept so the relink can be undone and
 * redone.  The link it wrote is kept too, not worked out again from the row
 * below: once a row has left the history for good (see skip_blocked_step),
 * the row below is no longer the one this row was pointed at.
 */
typedef struct {
    char next[sizeof(((LevelDef *)0)->next_phase)];  /* its next_phase before */
    char to[sizeof(((LevelDef *)0)->next_phase)];    /* ...and after          */
    int relinked;                                    /* 1 when it was changed */
} CampaignOldLink;

/* The edits the view can undo.  s_step_kinds below is indexed by these. */
typedef enum {
    STEP_ADD,      /* a level was appended to the list              */
    STEP_REMOVE,   /* a row was taken out                           */
    STEP_MOVE,     /* a row changed places with its neighbour       */
    STEP_RENAME,   /* a level's name was changed                    */
    STEP_LINK,     /* "Link in order" rewrote the next_phase chain  */
    STEP_KIND_COUNT
} CampaignStepKind;

/*
 * CampaignStep — one undoable edit, small enough to keep
 * CAMPAIGN_HISTORY_MAX of them inside the view.
 *
 * Undo replays the newest step backwards and redo replays it forwards, so
 * whenever a step is replayed the list is exactly as the edit left (or
 * found) it, and `row` still names the same row.  A row that left the list
 * (an undone Add, a Remove) comes back by reading its level file again
 * from `path`.  What the designer had changed in that row without saving
 * is not in the file, so a Remove keeps it here to put back on top.
 *
 * The one thing that rewrites a recorded step is skipping a Remove that can
 * no longer be undone (skip_blocked_step): the older steps then get the row
 * numbers they would have had if that row had never been listed, which
 * keeps the sentence above true for the list as it is from then on.
 */
typedef struct {
    CampaignStepKind kind;
    int row;                 /* the row the edit acted on                  */
    int to_row;              /* STEP_MOVE: where that row went             */
    char path[CAMPAIGN_LEVEL_PATH_SIZE];   /* the row's level file ("" for */
                                           /* a relink, which has no row)  */
    int loaded;              /* add, remove: did the file load back then?  */

    /* STEP_RENAME: the name before and after.  STEP_REMOVE: old_name and
     * `next` are the name and next_phase the row showed, and the two flags
     * say which of them differed from its file (were not saved yet). */
    char old_name[LEVEL_NAME_CAPACITY];
    char new_name[LEVEL_NAME_CAPACITY];
    char next[sizeof(((LevelDef *)0)->next_phase)];
    int renamed;
    int relinked;

    /* STEP_LINK: what the relink did, one per row of the list, and how
     * many rows that was.  The step owns this block; it is NULL for every
     * other kind. */
    CampaignOldLink *old_links;
    int rows;
} CampaignStep;

/*
 * CampaignHistory — the steps, oldest first.  steps[0 .. applied) are in
 * effect and can be undone; steps[applied .. count) were undone and can be
 * redone.  Undo and redo only move `applied`, so neither needs memory.
 */
typedef struct {
    CampaignStep steps[CAMPAIGN_HISTORY_MAX];
    int count;
    int applied;
} CampaignHistory;

struct EditorCampaign {
    char manifest_path[EDITOR_PATH_MAX];
    SerializerFileFingerprint manifest_fingerprint;  /* as read or saved  */
    CampaignCatalog catalog;   /* the entries, in the order shown        */
    CampaignDisk *disk;        /* disk[i] belongs to catalog.levels[i]   */
    CampaignListed *listed;    /* the manifest file's own list           */
    size_t listed_count;
    size_t capacity;           /* room in all three arrays               */
    CampaignHistory history;   /* the edits Undo and Redo replay         */
    int undo_blocked;          /* Undo refused the newest step: its level */
                               /* file did not load (see undo_refused)    */
    int selected;              /* highlighted row, -1 for none           */
    int scroll;                /* first row shown                        */
    int close_armed;           /* Close was refused once for unsaved work */
    int revert_armed;          /* Revert was refused once, likewise       */
    char problem[160];         /* campaign-wide problem, "" when none    */
};

/* ------------------------------------------------------------------ */
/* Entries                                                             */
/* ------------------------------------------------------------------ */

/*
 * Make room for `count` entries in all three arrays.  `listed` is only
 * written by a Save, but it grows with the others so that a Save always
 * has room to note the list it wrote: it must not run out of memory after
 * the files are already on disk.
 */
static int reserve(EditorCampaign *c, size_t count)
{
    CampaignLevel *levels;
    CampaignDisk *disk;
    CampaignListed *listed;
    size_t capacity = c->capacity ? c->capacity : 8;

    if (count <= c->capacity) return 0;
    while (capacity < count) capacity *= 2;
    levels = realloc(c->catalog.levels, capacity * sizeof(*levels));
    if (!levels) return -1;
    c->catalog.levels = levels;
    disk = realloc(c->disk, capacity * sizeof(*disk));
    if (!disk) return -1;
    c->disk = disk;
    listed = realloc(c->listed, capacity * sizeof(*listed));
    if (!listed) return -1;
    c->listed = listed;
    c->capacity = capacity;
    return 0;
}

/* Note what entry's file holds now: after reading it, or after Save. */
static void remember_disk(const CampaignLevel *entry, CampaignDisk *disk)
{
    char resolved[EDITOR_PATH_MAX];

    memset(disk, 0, sizeof(*disk));
    if (!entry->loaded) return;
    memcpy(disk->disk_name, entry->level.name, sizeof(disk->disk_name));
    memcpy(disk->disk_next, entry->level.next_phase, sizeof(disk->disk_next));
    if (level_resolve_path(entry->path, resolved, sizeof(resolved)) == 0)
        (void)serializer_fingerprint_utf8(resolved, &disk->fingerprint);
}

/* Does entry i's level differ from its file (a renamed or relinked level)? */
static int level_changed(const EditorCampaign *c, size_t i)
{
    const CampaignLevel *entry = &c->catalog.levels[i];
    return entry->loaded &&
           (strcmp(entry->level.name, c->disk[i].disk_name) != 0 ||
            strcmp(entry->level.next_phase, c->disk[i].disk_next) != 0);
}

/* Note the list the manifest file holds now: after reading it, or after
 * Save wrote it.  The array always has the room (see reserve). */
static void remember_list(EditorCampaign *c)
{
    for (size_t i = 0; i < c->catalog.count; i++)
        memcpy(c->listed[i].path, c->catalog.levels[i].path, sizeof(c->listed[i].path));
    c->listed_count = c->catalog.count;
}

/*
 * Does the list (which levels, in which order) differ from the manifest
 * file?  Comparing with the file's own list, rather than raising a flag on
 * every edit, is what lets Undo take the unsaved mark away again: a list
 * undone back to the saved order is simply no different from the file.
 */
static int list_changed(const EditorCampaign *c)
{
    if (c->catalog.count != c->listed_count) return 1;
    for (size_t i = 0; i < c->catalog.count; i++)
        if (strcmp(c->catalog.levels[i].path, c->listed[i].path) != 0) return 1;
    return 0;
}

/* Any edit means a refused Close or Revert must be asked again. */
static void edited(EditorCampaign *c)
{
    c->close_armed = 0;
    c->revert_armed = 0;
}

/* The entry whose name field is being typed in, or -1. */
static int name_being_typed(const EditorState *es)
{
    int row = es->ui.active_id - CAMPAIGN_NAME_FIELD_ID;
    if (!es->campaign || es->ui.active_id < CAMPAIGN_NAME_FIELD_ID ||
        (size_t)row >= es->campaign->catalog.count) return -1;
    return row;
}

/* Is a name being typed that differs from the one its row holds?  It is
 * not an edit yet, but the next command will apply it as one. */
static int typed_name_differs(const EditorState *es)
{
    int row = name_being_typed(es);
    return row >= 0 &&
           strcmp(es->ui.edit_buf, es->campaign->catalog.levels[row].level.name) != 0;
}

/* Settle a half-typed name before the list changes under its field. */
static int finish_edit(EditorState *es)
{
    return editor_finish_field_edit(es);
}

void editor_campaign_check(EditorState *es)
{
    EditorCampaign *c = es ? es->campaign : NULL;
    if (!c) return;
    (void)campaign_catalog_check(&c->catalog, c->problem, sizeof(c->problem));
}

const CampaignCatalog *editor_campaign_entries(const EditorState *es)
{
    return es && es->campaign ? &es->campaign->catalog : NULL;
}

const char *editor_campaign_problem(const EditorState *es)
{
    return es && es->campaign ? es->campaign->problem : NULL;
}

int editor_campaign_unsaved(const EditorState *es)
{
    const EditorCampaign *c = es ? es->campaign : NULL;
    if (!c) return 0;
    if (list_changed(c)) return 1;
    /* A name typed but not yet applied is work too: quitting must ask. */
    if (typed_name_differs(es)) return 1;
    for (size_t i = 0; i < c->catalog.count; i++)
        if (level_changed(c, i)) return 1;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Rows                                                                */
/* ------------------------------------------------------------------ */

/*
 * The three ways the list changes shape.  The edits below and the history
 * that replays them share these, so an undone edit goes through the very
 * code the edit itself went through.
 */

/* Rows a and b change places, each with what the view knows of its file. */
static void swap_rows(EditorCampaign *c, int a, int b)
{
    CampaignLevel entry = c->catalog.levels[a];
    CampaignDisk disk = c->disk[a];

    c->catalog.levels[a] = c->catalog.levels[b];
    c->catalog.levels[b] = entry;
    c->disk[a] = c->disk[b];
    c->disk[b] = disk;
}

/* Take row `row` out of the list; the rows below it move up one. */
static void take_out(EditorCampaign *c, int row)
{
    size_t after = c->catalog.count - (size_t)row - 1;

    memmove(&c->catalog.levels[row], &c->catalog.levels[row + 1],
            after * sizeof(c->catalog.levels[0]));
    memmove(&c->disk[row], &c->disk[row + 1], after * sizeof(c->disk[0]));
    c->catalog.count--;
    if (c->selected >= (int)c->catalog.count) c->selected = (int)c->catalog.count - 1;
}

/*
 * put_in — Make the level file `path` row `row`: the rows from there on
 * move down one and the file is read into the gap.  Returns the new row's
 * entry (loaded or not, as campaign_entry_load found the file), or NULL
 * with the list unchanged when there is no memory for one more row.
 */
static CampaignLevel *put_in(EditorCampaign *c, int row, const char *path)
{
    size_t after = c->catalog.count - (size_t)row;
    CampaignLevel *entry;

    if (reserve(c, c->catalog.count + 1) != 0) return NULL;
    memmove(&c->catalog.levels[row + 1], &c->catalog.levels[row],
            after * sizeof(c->catalog.levels[0]));
    memmove(&c->disk[row + 1], &c->disk[row], after * sizeof(c->disk[0]));
    c->catalog.count++;
    entry = &c->catalog.levels[row];
    memset(entry, 0, sizeof(*entry));
    memcpy(entry->path, path, strlen(path) + 1);
    campaign_entry_load(entry);
    remember_disk(entry, &c->disk[row]);
    return entry;
}

/* The next_phase that makes row `row` follow the order shown: the level in
 * the row below it, or none ("") for the last row. */
static const char *next_in_order(const EditorCampaign *c, size_t row)
{
    return row + 1 < c->catalog.count ? c->catalog.levels[row + 1].path : "";
}

/* ------------------------------------------------------------------ */
/* History                                                             */
/* ------------------------------------------------------------------ */

#ifdef MANGO_TESTING
static int s_test_history_allocation_fails;

void editor_campaign_test_fail_history_allocation(int fail)
{
    s_test_history_allocation_fails = fail;
}
#endif

/* The one allocation the history makes (a relink's old links) goes through
 * here, so a test can make it fail. */
static void *history_alloc(size_t count, size_t size)
{
#ifdef MANGO_TESTING
    if (s_test_history_allocation_fails) return NULL;
#endif
    return calloc(count, size);
}

/* Forget steps[from .. count) and free what they own. */
static void forget_steps(CampaignHistory *history, int from)
{
    for (int i = from; i < history->count; i++) free(history->steps[i].old_links);
    history->count = from;
    if (history->applied > from) history->applied = from;
}

/*
 * record — Note an edit that was just made.  The step is copied in; a
 * relink's old_links block now belongs to the history.  Recording never
 * fails: the steps live inside the view, and a full history drops its
 * oldest step to make room.
 */
static void record(EditorCampaign *c, const CampaignStep *step)
{
    CampaignHistory *history = &c->history;

    /* A new edit starts a new future: the undone steps describe a list
     * that will never exist again, so they can no longer be redone. */
    forget_steps(history, history->applied);
    if (history->count == CAMPAIGN_HISTORY_MAX) {
        free(history->steps[0].old_links);
        memmove(&history->steps[0], &history->steps[1],
                (CAMPAIGN_HISTORY_MAX - 1) * sizeof(history->steps[0]));
        history->count--;
    }
    history->steps[history->count++] = *step;
    history->applied = history->count;
    c->undo_blocked = 0;   /* the newest step is another one now */
}

/* Note that row `row` was renamed from old_name (LEVEL_NAME_CAPACITY
 * bytes) to the name it holds now. */
static void record_rename(EditorCampaign *c, int row, const char *old_name)
{
    const CampaignLevel *entry = &c->catalog.levels[row];
    CampaignStep step = {0};

    step.kind = STEP_RENAME;
    step.row = row;
    memcpy(step.path, entry->path, sizeof(step.path));
    memcpy(step.old_name, old_name, sizeof(step.old_name));
    memcpy(step.new_name, entry->level.name, sizeof(step.new_name));
    record(c, &step);
}

/*
 * What a replay function answers.  A refusal leaves the list unchanged and
 * the reason in the status bar.  REPLAY_FILE_GONE is the one refusal the
 * designer can get past from inside the view (see undo_refused).
 */
enum {
    REPLAY_DONE      =  0,
    REPLAY_REFUSED   = -1,   /* no memory for one more row                  */
    REPLAY_FILE_GONE = -2    /* the row's level file no longer loads        */
};

/*
 * put_back — Return the row a step describes to the list by reading its
 * level file again.  A row that showed a loaded level must load again:
 * when its file is gone or broken the list is left exactly as it was and
 * the status bar says why (REPLAY_FILE_GONE), rather than hand back a row
 * that has quietly lost its level.  A row that never loaded comes back as
 * it was.
 */
static int put_back(EditorState *es, EditorCampaign *c, const CampaignStep *step, int redo)
{
    CampaignLevel *entry = put_in(c, step->row, step->path);

    if (!entry) {
        editor_set_status(es, "Cannot %s: out of memory", redo ? "redo" : "undo");
        return REPLAY_REFUSED;
    }
    if (step->loaded && !entry->loaded) {
        take_out(c, step->row);
        /* The status bar shows only the start of a long message, so the
         * file comes first and the way out last. */
        if (redo)
            editor_set_status(es, "Cannot redo: %s no longer loads. Put the file back, "
                              "or use Revert", step->path);
        else
            editor_set_status(es, "Cannot undo: %s no longer loads. Put the file back, "
                              "or Undo again to skip this step", step->path);
        return REPLAY_FILE_GONE;
    }
    /* Only what the designer had changed goes on top of the file, so a
     * name or link the file gained in the meantime is not written over. */
    if (entry->loaded && step->renamed)
        memcpy(entry->level.name, step->old_name, sizeof(entry->level.name));
    if (entry->loaded && step->relinked)
        memcpy(entry->level.next_phase, step->next, sizeof(entry->level.next_phase));
    c->selected = step->row;
    return REPLAY_DONE;
}

/* Take the row a step describes out again, leaving the highlight where
 * the Remove button leaves it: on the row that moved into its place. */
static int take_back(EditorCampaign *c, const CampaignStep *step)
{
    c->selected = step->row;
    take_out(c, step->row);
    return REPLAY_DONE;
}

/*
 * The replay functions: one per kind of step, each able to run its edit
 * backwards (redo == 0) and forwards again (redo == 1).  They return
 * REPLAY_DONE, or one of the refusals above with the list unchanged and the
 * reason in the status bar.
 */
static int replay_add(EditorState *es, EditorCampaign *c, const CampaignStep *step, int redo)
{
    return redo ? put_back(es, c, step, redo) : take_back(c, step);
}

static int replay_remove(EditorState *es, EditorCampaign *c, const CampaignStep *step, int redo)
{
    return redo ? take_back(c, step) : put_back(es, c, step, redo);
}

static int replay_move(EditorState *es, EditorCampaign *c, const CampaignStep *step, int redo)
{
    (void)es;
    swap_rows(c, step->row, step->to_row);
    c->selected = redo ? step->to_row : step->row;
    return REPLAY_DONE;
}

static int replay_rename(EditorState *es, EditorCampaign *c, const CampaignStep *step, int redo)
{
    CampaignLevel *entry = &c->catalog.levels[step->row];

    (void)es;
    memcpy(entry->level.name, redo ? step->new_name : step->old_name,
           sizeof(entry->level.name));
    c->selected = step->row;
    return REPLAY_DONE;
}

static int replay_link(EditorState *es, EditorCampaign *c, const CampaignStep *step, int redo)
{
    (void)es;
    /* Only the rows the relink changed are touched, in either direction.
     * The list has the rows the step describes, so old_links[i] is row i. */
    for (int i = 0; i < step->rows; i++) {
        const CampaignOldLink *link = &step->old_links[i];

        if (!link->relinked) continue;
        memcpy(c->catalog.levels[i].level.next_phase, redo ? link->to : link->next,
               sizeof(link->next));
    }
    return REPLAY_DONE;
}

/* What each kind of step is called in the status bar, and how to replay it. */
static const struct {
    const char *done;
    int (*replay)(EditorState *es, EditorCampaign *c, const CampaignStep *step, int redo);
} s_step_kinds[STEP_KIND_COUNT] = {
    [STEP_ADD]    = {"added",                        replay_add},
    [STEP_REMOVE] = {"removed",                      replay_remove},
    [STEP_MOVE]   = {"moved",                        replay_move},
    [STEP_RENAME] = {"renamed",                      replay_rename},
    [STEP_LINK]   = {"linked the levels in order",   replay_link},
};

/*
 * skip_blocked_step — Drop the newest step in effect, a Remove that Undo
 * cannot replay, so that the steps before it can be undone again.  Returns
 * how many of those older steps were dropped with it.
 *
 * The list on screen does not change: the removed row stays out.  What
 * changes is the past.  The older steps were recorded while that row was
 * still listed, so their row numbers count it: undoing "moved row 3 to row
 * 2" on a list that is one row shorter would move the wrong level, or reach
 * past the end of the list.  Each older step is therefore rewritten as the
 * same edit on a list that never had the row.
 *
 * `gone` follows the missing row back through time: it is the place the
 * row had right after the step being looked at.  A step that touched other
 * rows keeps its meaning and only has the rows below the missing one
 * renumbered.  A step that acted on the missing row alone (its rename, a
 * swap with it, the relink of it and nothing else) has nothing left to do
 * and is dropped.  The walk ends at the Add that brought the row in, if the
 * history still holds it: before that the row was not there to be counted.
 */
static int skip_blocked_step(EditorCampaign *c)
{
    CampaignHistory *history = &c->history;
    int blocked = history->applied - 1;
    int gone = history->steps[blocked].row;
    int drop[CAMPAIGN_HISTORY_MAX] = {0};
    int dropped = 0;
    int kept = 0;

    drop[blocked] = 1;
    for (int i = blocked - 1; i >= 0 && gone >= 0; i--) {
        CampaignStep *step = &history->steps[i];

        switch (step->kind) {
        case STEP_ADD:
            /* A level is added as the last row, so the missing row is
             * either this very level or one above it. */
            if (step->row == gone) {
                drop[i] = 1;
                gone = -1;
            } else {
                step->row--;
            }
            break;
        case STEP_REMOVE:
            /* Before this step the list had one more row, at step->row.
             * It was either above the missing row, which then stood one
             * place lower, or below it. */
            if (step->row <= gone) gone++;
            else step->row--;
            break;
        case STEP_MOVE:
            /* Two neighbours changed places.  If the missing row was one
             * of them, the other one never moved past anything that is
             * still there. */
            if (step->to_row == gone) {
                drop[i] = 1;
                gone = step->row;
            } else if (step->row == gone) {
                drop[i] = 1;
                gone = step->to_row;
            } else if (step->row > gone) {
                step->row--;
                step->to_row--;
            }
            break;
        case STEP_RENAME:
            if (step->row == gone) drop[i] = 1;
            else if (step->row > gone) step->row--;
            break;
        case STEP_LINK: {
            /* Take the missing row's entry out of the block.  The row that
             * was pointed at it keeps that link (old_links[].to), exactly
             * as the list shows it now. */
            int relinked = 0;

            memmove(&step->old_links[gone], &step->old_links[gone + 1],
                    (size_t)(step->rows - gone - 1) * sizeof(step->old_links[0]));
            step->rows--;
            for (int row = 0; row < step->rows; row++) relinked |= step->old_links[row].relinked;
            if (!relinked) drop[i] = 1;
            break;
        }
        case STEP_KIND_COUNT:
            break;
        }
    }

    /* Close the gaps.  Every dropped step was in effect, so `applied`
     * comes down by as many; the undone steps after them move along
     * untouched: they describe the list as it is now. */
    for (int i = 0; i < history->count; i++) {
        if (drop[i]) {
            free(history->steps[i].old_links);
            dropped++;
            continue;
        }
        history->steps[kept++] = history->steps[i];
    }
    history->count = kept;
    history->applied -= dropped;
    c->undo_blocked = 0;
    return dropped - 1;   /* not counting the blocked step itself */
}

/*
 * undo_refused — Undo could not put a removed row back because its level
 * file no longer loads (put_back has said so in the status bar).
 *
 * Left at that, the step would stand in the way of every older one until
 * the file came back.  So the step can be skipped, but never by accident:
 * the first refusal only tells, because a designer holding Ctrl+Z to walk
 * back several steps must not lose one on the way.  From the second on the
 * editor asks, and only the answer "Skip this step" drops it.  Returns -1
 * while the step is kept, 2 once it is skipped.
 */
static int undo_refused(EditorState *es, EditorCampaign *c)
{
    char path[CAMPAIGN_LEVEL_PATH_SIZE];
    char refusal[sizeof(es->status_message)];
    int dropped;

    if (!c->undo_blocked) {
        c->undo_blocked = 1;
        return -1;
    }
    /* The step is about to go; its file's name is needed after that. */
    memcpy(path, c->history.steps[c->history.applied - 1].path, sizeof(path));
    memcpy(refusal, es->status_message, sizeof(refusal));
    if (!editor_confirm_skip_undo_step(es, path)) {
        /* A dialog that could not be shown has said so itself; a "Keep
         * the step" leaves the refusal there, so say what was decided. */
        if (strcmp(es->status_message, refusal) == 0)
            editor_set_status(es, "Undo step kept: %s no longer loads. Put the file back, "
                              "then Undo", path);
        return -1;
    }
    dropped = skip_blocked_step(c);
    if (dropped == 0)
        editor_set_status(es, "Skipped: removed %s (it stays out of the campaign)", path);
    else
        editor_set_status(es, "Skipped: removed %s (it stays out of the campaign), and %d "
                          "older step%s that changed nothing else", path, dropped,
                          dropped == 1 ? "" : "s");
    return 2;
}

/* Undo the newest edit in effect, or redo the one undone last. */
static int walk_history(EditorState *es, int redo)
{
    EditorCampaign *c = es ? es->campaign : NULL;
    CampaignHistory *history;
    const CampaignStep *step;
    int result;

    /* A name being typed is applied first, as an edit of its own: undo
     * then takes that very name back, and it can be redone. */
    if (!c || !finish_edit(es)) return -1;
    history = &c->history;
    if (redo ? history->applied == history->count : history->applied == 0) {
        editor_set_status(es, "Campaign: nothing to %s", redo ? "redo" : "undo");
        return 0;
    }
    step = &history->steps[redo ? history->applied : history->applied - 1];
    result = s_step_kinds[step->kind].replay(es, c, step, redo);
    if (result == REPLAY_FILE_GONE && !redo) return undo_refused(es, c);
    if (result != REPLAY_DONE) return -1;
    history->applied += redo ? 1 : -1;
    c->undo_blocked = 0;   /* the newest step is another one now */
    edited(c);
    editor_campaign_check(es);
    editor_set_status(es, "%s: %s%s%s", redo ? "Redid" : "Undid", s_step_kinds[step->kind].done,
                      step->path[0] ? " " : "", step->path);
    return 1;
}

int editor_campaign_undo(EditorState *es)
{
    return walk_history(es, 0);
}

int editor_campaign_redo(EditorState *es)
{
    return walk_history(es, 1);
}

int editor_campaign_undo_steps(const EditorState *es)
{
    return es && es->campaign ? es->campaign->history.applied : 0;
}

int editor_campaign_redo_steps(const EditorState *es)
{
    const CampaignHistory *history = es && es->campaign ? &es->campaign->history : NULL;
    return history ? history->count - history->applied : 0;
}

int editor_campaign_undo_blocked(const EditorState *es)
{
    return es && es->campaign ? es->campaign->undo_blocked : 0;
}

int editor_campaign_finish_name(EditorState *es)
{
    int row = name_being_typed(es);
    char before[LEVEL_NAME_CAPACITY];
    int result;

    if (row < 0) return 0;
    memcpy(before, es->campaign->catalog.levels[row].level.name, sizeof(before));
    result = ui_apply_active_edit(&es->ui);
    if (result == 2) {
        record_rename(es->campaign, row, before);
        edited(es->campaign);
        editor_campaign_check(es);
    }
    return result != 0;
}

/* ------------------------------------------------------------------ */
/* Opening and closing                                                 */
/* ------------------------------------------------------------------ */

int editor_campaign_open(EditorState *es, const char *manifest_path)
{
    EditorCampaign *c;

    if (!es) return -1;
    if (es->campaign) return 0;
    if (!finish_edit(es)) return -1;
    if (!manifest_path) manifest_path = CAMPAIGN_MANIFEST_PATH;
    if (strlen(manifest_path) >= sizeof(c->manifest_path)) {
        editor_set_status(es, "Campaign: path too long");
        return -1;
    }

    c = calloc(1, sizeof(*c));
    if (!c) {
        editor_set_status(es, "Campaign: out of memory");
        return -1;
    }
    memcpy(c->manifest_path, manifest_path, strlen(manifest_path) + 1);
    /* Fingerprint first, read second: if another program changes the
     * manifest in between, Save sees a difference and refuses, rather than
     * overwriting a change the view never showed. */
    (void)serializer_fingerprint_utf8(manifest_path, &c->manifest_fingerprint);
    /* The parse half of the game's load: a campaign the start menu would
     * refuse (no playable level) still opens here, to be repaired. */
    if (campaign_catalog_read(manifest_path, &c->catalog) != 0) {
        free(c);
        editor_set_status(es, "Campaign: cannot read %s (see the terminal for why)",
                          manifest_path);
        return -1;
    }
    c->capacity = c->catalog.count;
    c->disk = calloc(c->capacity ? c->capacity : 1, sizeof(*c->disk));
    c->listed = calloc(c->capacity ? c->capacity : 1, sizeof(*c->listed));
    if (!c->disk || !c->listed) {
        campaign_catalog_cleanup(&c->catalog);
        free(c->disk);
        free(c->listed);
        free(c);
        editor_set_status(es, "Campaign: out of memory");
        return -1;
    }
    for (size_t i = 0; i < c->catalog.count; i++)
        remember_disk(&c->catalog.levels[i], &c->disk[i]);
    remember_list(c);
    c->selected = c->catalog.count > 0 ? 0 : -1;
    es->campaign = c;
    editor_campaign_check(es);
    editor_set_status(es, "Campaign: %zu levels in %s (Esc returns to the level)",
                      c->catalog.count, manifest_path);
    return 0;
}

void editor_campaign_free(EditorState *es)
{
    if (!es || !es->campaign) return;
    forget_steps(&es->campaign->history, 0);
    campaign_catalog_cleanup(&es->campaign->catalog);
    free(es->campaign->disk);
    free(es->campaign->listed);
    free(es->campaign);
    es->campaign = NULL;
}

int editor_campaign_close(EditorState *es, int force)
{
    EditorCampaign *c = es ? es->campaign : NULL;

    if (!c) return 1;
    if (!force && !c->close_armed && editor_campaign_unsaved(es)) {
        c->close_armed = 1;
        editor_set_status(es, "Unsaved campaign changes: Save, or Close again to discard them");
        return 0;
    }
    /* A name being typed belongs to the view going away. */
    if (name_being_typed(es) >= 0) ui_cancel_active_edit(&es->ui);
    editor_campaign_free(es);
    editor_set_status(es, "Campaign closed");
    return 1;
}

int editor_campaign_revert(EditorState *es)
{
    EditorCampaign *c = es ? es->campaign : NULL;
    char path[EDITOR_PATH_MAX];
    int selected;
    int scroll;

    if (!c) return -1;
    if (!editor_campaign_unsaved(es)) {
        editor_set_status(es, "Campaign: nothing to revert; it matches the files on disk");
        return 0;
    }
    if (!c->revert_armed) {
        c->revert_armed = 1;
        editor_set_status(es, "Revert discards your campaign changes: Revert again to confirm");
        return 0;
    }
    /* Reading the manifest and its levels again is the way back to the
     * files in one go.  The edit history goes with the old view (its steps
     * describe a list that no longer exists), and so does a name being
     * typed. */
    if (name_being_typed(es) >= 0) ui_cancel_active_edit(&es->ui);
    memcpy(path, c->manifest_path, sizeof(path));
    selected = c->selected;
    scroll = c->scroll;
    editor_campaign_free(es);
    if (editor_campaign_open(es, path) != 0) return -1;   /* status says why */
    c = es->campaign;
    if (selected < (int)c->catalog.count) c->selected = selected;
    c->scroll = scroll;   /* editor_campaign_render clamps it */
    editor_set_status(es, "Campaign reverted to the files on disk");
    return 1;
}

/* ------------------------------------------------------------------ */
/* Editing the list                                                    */
/* ------------------------------------------------------------------ */

int editor_campaign_move(EditorState *es, int index, int delta)
{
    EditorCampaign *c = es ? es->campaign : NULL;
    int other = index + delta;
    CampaignStep step = {0};

    if (!c || index < 0 || (size_t)index >= c->catalog.count) return -1;
    if (other < 0 || (size_t)other >= c->catalog.count) return -1;
    if (!finish_edit(es)) return -1;
    step.kind = STEP_MOVE;
    step.row = index;
    step.to_row = other;
    memcpy(step.path, c->catalog.levels[index].path, sizeof(step.path));
    swap_rows(c, index, other);
    record(c, &step);
    c->selected = other;
    edited(c);
    editor_campaign_check(es);
    return 0;
}

int editor_campaign_remove(EditorState *es, int index)
{
    EditorCampaign *c = es ? es->campaign : NULL;
    const CampaignLevel *entry;
    CampaignStep step = {0};

    if (!c || index < 0 || (size_t)index >= c->catalog.count) return -1;
    if (!finish_edit(es)) return -1;
    entry = &c->catalog.levels[index];
    step.kind = STEP_REMOVE;
    step.row = index;
    step.loaded = entry->loaded;
    memcpy(step.path, entry->path, sizeof(step.path));
    /* A name or link changed here and not saved yet is in no file: the
     * step keeps it, for Undo to put back on the row (see put_back). */
    if (entry->loaded) {
        step.renamed = strcmp(entry->level.name, c->disk[index].disk_name) != 0;
        step.relinked = strcmp(entry->level.next_phase, c->disk[index].disk_next) != 0;
        memcpy(step.old_name, entry->level.name, sizeof(step.old_name));
        memcpy(step.next, entry->level.next_phase, sizeof(step.next));
    }
    editor_set_status(es, "Removed %s from the campaign (the file stays)", entry->path);
    take_out(c, index);
    record(c, &step);
    edited(c);
    editor_campaign_check(es);
    return 0;
}

int editor_campaign_add(EditorState *es, const char *path)
{
    EditorCampaign *c = es ? es->campaign : NULL;
    char candidate[CAMPAIGN_LEVEL_PATH_SIZE];
    const char *name;
    CampaignLevel *entry;
    CampaignStep step = {0};

    if (!c || !path || !path[0]) return -1;
    if (!finish_edit(es)) return -1;

    /* A manifest names levels as levels/<name>.toml relative to the game
     * folder.  A picked file is an absolute path: it qualifies when that
     * relative name leads to the very same file. */
    {
        const char *slash = strrchr(path, '/');
        const char *backslash = strrchr(path, '\\');   /* Windows pickers */
        if (backslash && (!slash || backslash > slash)) slash = backslash;
        name = slash ? slash + 1 : path;
    }
    if (snprintf(candidate, sizeof(candidate), "levels/%s", name) >= (int)sizeof(candidate) ||
        !campaign_entry_path_valid(candidate)) {
        editor_set_status(es, "Not added: a campaign lists levels/<name>.toml files only");
        return -1;
    }
    if (!serializer_file_exists_utf8(candidate) ||
        (strcmp(path, candidate) != 0 && !serializer_same_file_utf8(path, candidate))) {
        editor_set_status(es, "Not added: %s is not a file directly in levels/", name);
        return -1;
    }
    for (size_t i = 0; i < c->catalog.count; i++) {
        if (strcmp(c->catalog.levels[i].path, candidate) == 0) {
            editor_set_status(es, "Not added: %s is already in the campaign", candidate);
            return -1;
        }
    }
    step.kind = STEP_ADD;
    step.row = (int)c->catalog.count;   /* a new level goes last */
    entry = put_in(c, step.row, candidate);
    if (!entry) {
        editor_set_status(es, "Not added: out of memory");
        return -1;
    }
    step.loaded = entry->loaded;
    memcpy(step.path, entry->path, sizeof(step.path));
    record(c, &step);
    c->selected = step.row;
    edited(c);
    editor_campaign_check(es);
    editor_set_status(es, "Added %s%s", candidate,
                      entry->loaded ? "" : " (it does not load; it stays unavailable)");
    return 0;
}

int editor_campaign_link_in_order(EditorState *es)
{
    EditorCampaign *c = es ? es->campaign : NULL;
    CampaignStep step = {0};
    int changed = 0;

    if (!c || !finish_edit(es)) return 0;
    /* Undoing a relink needs each changed row's link as it was, and
     * nothing else holds those.  The memory for them is asked for before
     * any link changes: without it the links stay as they are, rather
     * than change with no way back. */
    step.kind = STEP_LINK;
    step.rows = (int)c->catalog.count;
    step.old_links = history_alloc(c->catalog.count ? c->catalog.count : 1,
                                   sizeof(*step.old_links));
    if (!step.old_links) {
        editor_set_status(es, "Not linked: out of memory");
        return 0;
    }
    for (size_t i = 0; i < c->catalog.count; i++) {
        CampaignLevel *entry = &c->catalog.levels[i];
        const char *next = next_in_order(c, i);
        if (!entry->loaded || strcmp(entry->level.next_phase, next) == 0) continue;
        memcpy(step.old_links[i].next, entry->level.next_phase,
               sizeof(step.old_links[i].next));
        step.old_links[i].relinked = 1;
        /* Paths are at most CAMPAIGN_LEVEL_PATH_SIZE - 1 bytes, which
         * next_phase (256 bytes) always holds. */
        snprintf(entry->level.next_phase, sizeof(entry->level.next_phase), "%s", next);
        memcpy(step.old_links[i].to, entry->level.next_phase, sizeof(step.old_links[i].to));
        changed++;
    }
    if (changed > 0) record(c, &step);   /* the history owns old_links now */
    else free(step.old_links);
    edited(c);
    editor_campaign_check(es);
    if (changed > 0)
        editor_set_status(es, "Linked %d level%s in campaign order (Save writes them)",
                          changed, changed == 1 ? "" : "s");
    else
        editor_set_status(es, "Every next_phase already follows the campaign order");
    return changed;
}

/* ------------------------------------------------------------------ */
/* Saving                                                              */
/* ------------------------------------------------------------------ */

/*
 * One file a Campaign Save rewrites: where it goes, the temporary file that
 * holds its new text, and which entry it belongs to (-1 for the manifest).
 */
typedef struct {
    int entry;
    char target[EDITOR_PATH_MAX];
    char temp[EDITOR_PATH_MAX];
} CampaignSaveFile;

/* Every file of one Save: a level file per changed entry, plus the
 * manifest.  Allocated per Save, since a campaign can list many levels. */
typedef struct {
    CampaignSaveFile *files;
    int count;
} CampaignSavePlan;

/* Delete the temporary files from index `from` on and free the plan. */
static void discard_temps(CampaignSavePlan *plan, int from)
{
    for (int k = from; k < plan->count; k++)
        if (plan->files[k].temp[0]) serializer_remove_temp(plan->files[k].temp);
    free(plan->files);
    plan->files = NULL;
    plan->count = 0;
}

/*
 * write_temps — Phase one of a Save: list the files to rewrite and write
 * each one's new text to a temporary sibling.  Returns 0, or -1 (status
 * set) with every temporary already written deleted again.
 */
static int write_temps(EditorState *es, EditorCampaign *c, CampaignSavePlan *plan)
{
    plan->count = 0;
    plan->files = calloc(c->catalog.count + 1, sizeof(*plan->files));
    if (!plan->files) {
        editor_set_status(es, "Campaign not saved: out of memory");
        return -1;
    }
    for (size_t i = 0; i < c->catalog.count; i++) {
        CampaignSaveFile *file = &plan->files[plan->count];
        if (!level_changed(c, i)) continue;
        file->entry = (int)i;
        if (level_resolve_path(c->catalog.levels[i].path, file->target,
                               sizeof(file->target)) != 0) {
            editor_set_status(es, "Campaign not saved: cannot find %s",
                              c->catalog.levels[i].path);
            discard_temps(plan, 0);
            return -1;
        }
        plan->count++;
        if (level_write_temp(&c->catalog.levels[i].level, file->target,
                             file->temp, sizeof(file->temp)) != 0) {
            file->temp[0] = '\0';
            editor_set_status(es, "Campaign not saved: writing %s failed; no file "
                              "was changed", c->catalog.levels[i].path);
            discard_temps(plan, 0);
            return -1;
        }
    }
    if (list_changed(c)) {
        CampaignSaveFile *file = &plan->files[plan->count++];
        file->entry = -1;
        memcpy(file->target, c->manifest_path, sizeof(file->target));
        if (campaign_manifest_write_temp(c->manifest_path, &c->catalog,
                                         file->temp, sizeof(file->temp)) != 0) {
            file->temp[0] = '\0';
            editor_set_status(es, "Campaign not saved: writing %s failed; no file "
                              "was changed", c->manifest_path);
            discard_temps(plan, 0);
            return -1;
        }
    }
    return 0;
}

/* Does every file in the plan, the manifest included, still hold what the
 * view read?  When one does not, nothing may be installed (status set,
 * returns 0): another program changed it, and its change wins. */
static int plan_still_matches_disk(EditorState *es, const EditorCampaign *c,
                                   const CampaignSavePlan *plan)
{
    for (int k = 0; k < plan->count; k++) {
        const CampaignSaveFile *file = &plan->files[k];
        const SerializerFileFingerprint *expected =
            file->entry >= 0 ? &c->disk[file->entry].fingerprint : &c->manifest_fingerprint;
        SerializerFileFingerprint now;
        if (!expected->valid || serializer_fingerprint_utf8(file->target, &now) != 1 ||
            !serializer_fingerprint_equal(&now, expected) ||
            serializer_test_take_failure(SERIALIZER_TEST_FAILURE_SOURCE_CHANGED)) {
            editor_set_status(es, "Campaign not saved: %s changed on disk; close and "
                              "reopen the Campaign view",
                              file->entry >= 0 ? c->catalog.levels[file->entry].path
                                               : c->manifest_path);
            return 0;
        }
    }
    return 1;
}

/*
 * install_temps — Phase two: put each temporary file in its target's place.
 * Returns how many level files were installed, or -1 when one file failed:
 * the files before it are written (and remembered as saved), the ones
 * after it are not, and the status names the count and the failed file.
 */
static int install_temps(EditorState *es, EditorCampaign *c, CampaignSavePlan *plan)
{
    int installed = 0;
    int levels = 0;

    for (int k = 0; k < plan->count; k++) {
        CampaignSaveFile *file = &plan->files[k];
        int result = serializer_install_temp(file->temp, file->target, 0);
        const char *name = file->entry >= 0 ? c->catalog.levels[file->entry].path
                                            : c->manifest_path;
        if (result == SERIALIZER_REPLACE_TEMP_KEPT) {
            /* Windows moved the old file aside but could not move the new
             * one in: the temporary file is the only complete copy, so it
             * stays, and the designer needs its name. */
            char lead[96];   /* the words below with two counts of up to 11 characters */
            char shown[sizeof(es->status_message)];
            int room;
            snprintf(lead, sizeof(lead), "Campaign partly saved (%d of %d files written). ",
                     installed, plan->count);
            /* The status bar holds what is left after the words around the
             * path; a longer path keeps its end, the file's own name. */
            room = (int)sizeof(es->status_message) - (int)strlen(lead) - (int)strlen(name) -
                   (int)sizeof("Save incomplete: the new  is safe in ");
            if (room < 24) room = 24;
            editor_path_for_display(file->temp, shown, (size_t)room);
            fprintf(stderr, "campaign: could not finish replacing '%s'; it is kept in '%s'\n",
                    file->target, file->temp);
            editor_report_save_failure(es, result, name, lead, shown);
            file->temp[0] = '\0';           /* never delete the kept copy */
            discard_temps(plan, k + 1);
            return -1;
        }
        if (result != 0) {
            if (installed == 0)
                editor_set_status(es, "Campaign not saved: writing %s failed; no file "
                                  "was changed", name);
            else
                editor_set_status(es, "Campaign partly saved: %d of %d files written; "
                                  "writing %s failed", installed, plan->count, name);
            discard_temps(plan, k + 1);
            return -1;
        }
        /* Written: this file now matches the view, whatever happens next. */
        if (file->entry >= 0) {
            remember_disk(&c->catalog.levels[file->entry], &c->disk[file->entry]);
            levels++;
        } else {
            remember_list(c);
            (void)serializer_fingerprint_utf8(c->manifest_path, &c->manifest_fingerprint);
        }
        installed++;
    }
    discard_temps(plan, plan->count);
    return levels;
}

int editor_campaign_save(EditorState *es)
{
    EditorCampaign *c = es ? es->campaign : NULL;
    char reload[EDITOR_PATH_MAX] = "";
    int reload_entry = -1;
    CampaignSavePlan plan;
    int written;
    int unloaded = 0;

    if (!c || !finish_edit(es)) return -1;

    /* 1. The game's rules first: a campaign-wide problem, or an entry this
     *    view can fix (its order or name), blocks the save.  A level file
     *    that does not load is the file's problem; the menu shows it
     *    disabled, so it may stay listed. */
    editor_campaign_check(es);
    if (c->problem[0]) {
        editor_set_status(es, "Campaign not saved: %s", c->problem);
        return -1;
    }
    for (size_t i = 0; i < c->catalog.count; i++) {
        const CampaignLevel *entry = &c->catalog.levels[i];
        if (!entry->loaded) { unloaded++; continue; }
        if (!entry->available) {
            editor_set_status(es, "Campaign not saved: %s: %s (Link in order fixes next_phase)",
                              entry->path, entry->problem);
            return -1;
        }
    }

    /* 2. Before writing anything: every file to rewrite must still hold
     *    what the view read (else one could be saved and the next refused,
     *    leaving half a chain), and the level open in the editor must not
     *    be rewritten under unsaved edits; when it is clean, it is reloaded
     *    after the save. */
    for (size_t i = 0; i < c->catalog.count; i++) {
        char resolved[EDITOR_PATH_MAX];
        SerializerFileFingerprint now;
        if (!level_changed(c, i)) continue;
        if (level_resolve_path(c->catalog.levels[i].path, resolved, sizeof(resolved)) != 0 ||
            serializer_fingerprint_utf8(resolved, &now) != 1 ||
            !serializer_fingerprint_equal(&now, &c->disk[i].fingerprint)) {
            editor_set_status(es, "Campaign not saved: %s changed on disk; close and reopen "
                              "the Campaign view", c->catalog.levels[i].path);
            return -1;
        }
        if (es->file_path[0] == '\0' || !serializer_same_file_utf8(es->file_path, resolved))
            continue;
        if (es->modified) {
            editor_set_status(es, "Campaign not saved: save or discard your changes to %s first",
                              c->catalog.levels[i].path);
            return -1;
        }
        memcpy(reload, es->file_path, sizeof(reload));
        reload_entry = (int)i;
    }

    /* 3. Write every file this save changes to a temporary file next to
     *    it: the changed levels, then the manifest when its list changed.
     *    Nothing is replaced yet, so any failure here leaves the campaign
     *    on disk exactly as it was. */
    if (write_temps(es, c, &plan) != 0) return -1;

    /* 4. Install them, one quick rename each.  Right before, every target
     *    must still hold what the view read: the window in which another
     *    program could slip a change in is now as short as it can be.
     *    A rename can still fail half-way (a full disk, a file locked on
     *    Windows); the status then says exactly which files were written. */
    if (!plan_still_matches_disk(es, c, &plan)) {
        discard_temps(&plan, 0);
        return -1;
    }
    written = install_temps(es, c, &plan);
    if (written < 0) {
        /* The open level may be among the files that did get written: show
         * it as it now is, and keep the failure in the status bar. */
        if (reload_entry >= 0 && !level_changed(c, (size_t)reload_entry)) {
            char failure[sizeof(es->status_message)];
            memcpy(failure, es->status_message, sizeof(failure));
            (void)editor_load_level(es, reload);
            editor_set_status(es, "%s", failure);
        }
        return -1;
    }
    c->close_armed = 0;

    /* 5. Show the level open in the editor as it now is on disk. */
    if (reload[0]) (void)editor_load_level(es, reload);
    editor_set_status(es, "Campaign saved: %zu levels, %d level file%s updated%s",
                      c->catalog.count, written, written == 1 ? "" : "s",
                      unloaded ? "; some listed files do not load" : "");
    return 0;
}

/* ------------------------------------------------------------------ */
/* The view                                                            */
/* ------------------------------------------------------------------ */

#define CAMPAIGN_RED   ((Color){0xFF, 0x70, 0x70, 0xFF})
#define CAMPAIGN_GREEN ((Color){0x70, 0xE0, 0x80, 0xFF})

void editor_campaign_wheel(EditorState *es, float wheel)
{
    EditorCampaign *c = es ? es->campaign : NULL;
    int max_scroll;

    if (!c) return;
    c->scroll -= (int)wheel;
    max_scroll = (int)c->catalog.count - CAMPAIGN_VISIBLE_ROWS;
    if (c->scroll > max_scroll) c->scroll = max_scroll;
    if (c->scroll < 0) c->scroll = 0;
}

/* One entry row: number, file, the level's name (editable), its verdict. */
static void draw_row(EditorState *es, EditorCampaign *c, int i, int y)
{
    CampaignLevel *entry = &c->catalog.levels[i];
    int right = CANVAS_W - CAMPAIGN_VIEW_X;
    int in_row = es->ui.mouse_x >= CAMPAIGN_VIEW_X - 4 && es->ui.mouse_x < right &&
                 es->ui.mouse_y >= y - 2 && es->ui.mouse_y < y - 2 + CAMPAIGN_ROW_H;
    int in_name = in_row && es->ui.mouse_x >= CAMPAIGN_NAME_X &&
                  es->ui.mouse_x < CAMPAIGN_NAME_X + CAMPAIGN_NAME_W;
    char text[CAMPAIGN_LEVEL_PATH_SIZE + 16];

    /* A click anywhere on the row selects it; outside the name field the
     * click is used up here. */
    if (in_row && es->ui.mouse_clicked) {
        c->selected = i;
        if (!in_name) es->ui.mouse_clicked = 0;
    }
    if (i == c->selected)
        DrawRectangle(CAMPAIGN_VIEW_X - 4, y - 2, right - CAMPAIGN_VIEW_X + 4,
                      CAMPAIGN_ROW_H, UI_BTN_HOT);

    snprintf(text, sizeof(text), "%d. %s", i + 1, entry->path);
    ui_label(&es->ui, CAMPAIGN_VIEW_X, y + 2, text);
    if (entry->loaded) {
        /* The field stores a new name itself (Enter, Tab); the name it
         * replaces is kept here for the history.  Typing is not an edit
         * until then, so a rename is one step however long it took. */
        char before[LEVEL_NAME_CAPACITY];
        memcpy(before, entry->level.name, sizeof(before));
        if (ui_text_field(&es->ui, CAMPAIGN_NAME_FIELD_ID + i, CAMPAIGN_NAME_X, y,
                          CAMPAIGN_NAME_W, entry->level.name,
                          (int)sizeof(entry->level.name))) {
            record_rename(c, i, before);
            edited(c);
            editor_campaign_check(es);
        }
    } else {
        ui_label_color(&es->ui, CAMPAIGN_NAME_X, y + 2, "(cannot load)", UI_TEXT_DIM);
    }
    ui_label_color(&es->ui, CAMPAIGN_NAME_X + CAMPAIGN_NAME_W + 12, y + 2,
                   entry->available ? "ok" : entry->problem,
                   entry->available ? CAMPAIGN_GREEN : CAMPAIGN_RED);
}

/*
 * While Undo is stopped at a step it cannot replay, say so in the view
 * itself, between the rows and the buttons.  The status bar said it first,
 * but it shows only the start of a long message, and the next message
 * replaces it; the way out must stay in sight for as long as it is needed.
 */
static void draw_undo_notice(EditorState *es, const EditorCampaign *c)
{
    const CampaignStep *step;
    char shown[40];   /* a longer path keeps its end, the file's own name */
    char text[sizeof(shown) + 128];   /* the words below are under 128 bytes */

    if (!c->undo_blocked) return;
    step = &c->history.steps[c->history.applied - 1];
    editor_path_for_display(step->path, shown, sizeof(shown));
    snprintf(text, sizeof(text), "Undo is stopped: %s was removed and no longer loads. "
             "Put the file back, or press Undo again to skip that step.", shown);
    ui_label_color(&es->ui, CAMPAIGN_VIEW_X, CAMPAIGN_NOTICE_Y, text, CAMPAIGN_RED);
}

/* The button row along the bottom of the view. */
static void draw_buttons(EditorState *es, EditorCampaign *c)
{
    static const char *labels[CAMPAIGN_BUTTON_COUNT] = {
        [CAMPAIGN_BUTTON_UP]     = "Up",
        [CAMPAIGN_BUTTON_DOWN]   = "Down",
        [CAMPAIGN_BUTTON_REMOVE] = "Remove",
        [CAMPAIGN_BUTTON_ADD]    = "Add level...",
        [CAMPAIGN_BUTTON_LINK]   = "Link in order",
        [CAMPAIGN_BUTTON_UNDO]   = "Undo",
        [CAMPAIGN_BUTTON_REDO]   = "Redo",
        [CAMPAIGN_BUTTON_SAVE]   = "Save",
        [CAMPAIGN_BUTTON_REVERT] = "Revert",
        [CAMPAIGN_BUTTON_CLOSE]  = "Close",
    };
    /* Undo also has work when a name is being typed: the click applies the
     * name as a step and takes it back, as Ctrl+Z does. */
    int can_undo = c->history.applied > 0 || typed_name_differs(es);
    int can_redo = c->history.applied < c->history.count;
    int y = CAMPAIGN_BUTTONS_Y;

    for (int b = 0; b < CAMPAIGN_BUTTON_COUNT; b++) {
        int x = CAMPAIGN_VIEW_X + b * CAMPAIGN_BUTTON_STEP;
        const char *label = labels[b];

        if ((b == CAMPAIGN_BUTTON_UNDO && !can_undo) ||
            (b == CAMPAIGN_BUTTON_REDO && !can_redo)) {
            ui_button_disabled(&es->ui, x, y, CAMPAIGN_BUTTON_W, CAMPAIGN_BUTTON_H, label);
            continue;
        }
        if (b == CAMPAIGN_BUTTON_UNDO && c->undo_blocked) {
            /* The next Undo may ask a question (hence the dots, as on
             * "Add level..."), and the red line ties the button to the
             * notice above it. */
            label = "Undo...";
            DrawRectangle(x, y + CAMPAIGN_BUTTON_H, CAMPAIGN_BUTTON_W, 2, CAMPAIGN_RED);
        }
        if (!ui_button(&es->ui, x, y, CAMPAIGN_BUTTON_W, CAMPAIGN_BUTTON_H, label)) continue;
        switch (b) {
        case CAMPAIGN_BUTTON_UP:     (void)editor_campaign_move(es, c->selected, -1); break;
        case CAMPAIGN_BUTTON_DOWN:   (void)editor_campaign_move(es, c->selected, +1); break;
        case CAMPAIGN_BUTTON_REMOVE: (void)editor_campaign_remove(es, c->selected); break;
        case CAMPAIGN_BUTTON_ADD: {
            char picked[EDITOR_PATH_MAX];
            if (finish_edit(es) &&
                file_dialog_open(picked, (int)sizeof(picked)) == FILE_DIALOG_SELECTED)
                (void)editor_campaign_add(es, picked);
            break;
        }
        case CAMPAIGN_BUTTON_LINK:   (void)editor_campaign_link_in_order(es); break;
        /* The very functions Ctrl+Z and Ctrl+Y call (editor_events.c). */
        case CAMPAIGN_BUTTON_UNDO:   (void)editor_campaign_undo(es); break;
        case CAMPAIGN_BUTTON_REDO:   (void)editor_campaign_redo(es); break;
        case CAMPAIGN_BUTTON_SAVE:   (void)editor_campaign_save(es); break;
        case CAMPAIGN_BUTTON_REVERT: (void)editor_campaign_revert(es); break;
        case CAMPAIGN_BUTTON_CLOSE:  (void)editor_campaign_close(es, 0); break;
        }
        /* Close and Revert free the view; nothing below may touch it. */
        return;
    }
}

void editor_campaign_render(EditorState *es)
{
    EditorCampaign *c = es ? es->campaign : NULL;
    char title[EDITOR_PATH_MAX + 64];
    int rows;

    if (!c) return;
    /* The verdicts follow every edit, typed names included. */
    editor_campaign_check(es);
    ui_panel(&es->ui, 0, TOOLBAR_H, CANVAS_W, CANVAS_H);

    snprintf(title, sizeof(title), "Campaign: %s (%zu levels)%s", c->manifest_path,
             c->catalog.count, editor_campaign_unsaved(es) ? " *" : "");
    ui_label(&es->ui, CAMPAIGN_VIEW_X, TOOLBAR_H + 10, title);
    if (c->problem[0])
        ui_label_color(&es->ui, CAMPAIGN_VIEW_X, TOOLBAR_H + 30, c->problem, CAMPAIGN_RED);
    else
        ui_label_color(&es->ui, CAMPAIGN_VIEW_X, TOOLBAR_H + 30,
                       "The start menu lists these in order; each level's next_phase "
                       "must name the next one.", UI_TEXT_DIM);
    ui_label_color(&es->ui, CAMPAIGN_VIEW_X, TOOLBAR_H + 50, "Level file", UI_TEXT_DIM);
    ui_label_color(&es->ui, CAMPAIGN_NAME_X, TOOLBAR_H + 50, "Name (menu label)", UI_TEXT_DIM);
    ui_label_color(&es->ui, CAMPAIGN_NAME_X + CAMPAIGN_NAME_W + 12, TOOLBAR_H + 50,
                   "Game's verdict", UI_TEXT_DIM);

    rows = CAMPAIGN_VISIBLE_ROWS;
    if (c->scroll > (int)c->catalog.count - rows) c->scroll = (int)c->catalog.count - rows;
    if (c->scroll < 0) c->scroll = 0;
    for (int i = c->scroll; i < (int)c->catalog.count && i < c->scroll + rows; i++)
        draw_row(es, c, i, CAMPAIGN_ROWS_Y + (i - c->scroll) * CAMPAIGN_ROW_H);

    draw_undo_notice(es, c);
    draw_buttons(es, c);
}
