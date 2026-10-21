/*
 * editor_undo_recovery_test.c — Direct checks of the editor's safety nets:
 * undo and redo (editor_undo_apply.c on top of undo.c), the clipboard
 * (editor_clipboard.c) and crash recovery (editor_recovery.c).
 *
 * editor-validation-test and editor-ui-test reach most of this code on the
 * way to something else. The cases here call the modules' own functions
 * on a bare EditorState (no window, no event loop) so each promise is
 * written down where it can be read on its own:
 *
 *   undo       every kind of step undoes and redoes exactly, an edit that
 *              cannot be recorded is taken back, the history keeps the
 *              newest UNDO_MAX steps;
 *   clipboard  a multi-selection pastes as one step, offset and clamped
 *              into the level, and a paste that cannot fit adds nothing;
 *   recovery   snapshots are named and found on disk, a crashed editor's
 *              copy is recovered by id, and a copy another running editor
 *              still owns is never offered.
 *
 * Only public functions are used, so the tests describe behaviour rather
 * than the shape of the code behind it.
 */
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L /* fork, pipe, waitpid under -std=c11 */
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>     /* _mkdir, _rmdir */
#include <process.h>    /* _getpid */
#else
#include <sys/stat.h>   /* mkdir */
#include <sys/wait.h>   /* waitpid */
#include <unistd.h>     /* fork, pipe, read, getpid, rmdir */
#endif

#include "shared/graphics.h"   /* KEY_X */
#include "shared/platform.h"   /* clock_millis */
#include "editor/editor.h"
#include "editor/editor_clipboard.h"
#include "editor/editor_events.h"
#include "editor/editor_files.h"
#include "editor/editor_recovery.h"
#include "editor/editor_session.h"
#include "editor/editor_undo_apply.h"
#include "editor/entity_meta.h"
#include "editor/tools.h"
#include "editor/undo.h"
#include "levels/level_validate.h"
#include "shared/serializer.h"
#include "test_paths.h"

#define CHECK(test) do { if (!(test)) { \
    fprintf(stderr, "editor_undo_recovery_test:%d: %s\n", __LINE__, #test); \
    failed = 1; goto done; } } while (0)
#define STARTS_WITH(text, prefix) (strncmp((text), (prefix), strlen(prefix)) == 0)

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

/* A new, valid, saved-and-clean document with an empty history. */
static int open_blank_document(EditorState *es)
{
    memset(es, 0, sizeof(*es));
    editor_level_init_defaults(&es->level);
    es->selection.index = -1;
    es->undo = undo_create();
    if (!es->undo) return -1;
    editor_set_document_save_point(es);
    return 0;
}

static void close_document(EditorState *es)
{
    undo_test_limit_allocations(-1);
    undo_destroy(es->undo);
    es->undo = NULL;
}

/*
 * Undo (or redo) one whole step, as Ctrl+Z does: pop an entry, then keep
 * popping while the next one belongs to the same group. Returns how many
 * entries the step had, 0 when there was nothing to undo.
 */
static int history_step(EditorState *es, int redo)
{
    static Command cmd;
    int entries = 0;
    int group;

    if (!(redo ? redo_pop(es->undo, &cmd) : undo_pop(es->undo, &cmd))) return 0;
    group = cmd.group;
    do {
        editor_apply_undo_command(es, &cmd, !redo);
        entries++;
    } while (group != 0 &&
             (redo ? redo_top_group(es->undo) : undo_top_group(es->undo)) == group &&
             (redo ? redo_pop(es->undo, &cmd) : undo_pop(es->undo, &cmd)));
    editor_refresh_dirty(es);
    return entries;
}

static int undo_step(EditorState *es) { return history_step(es, 0); }
static int redo_step(EditorState *es) { return history_step(es, 1); }

/* Change coin 0's x through the inspector's change tracking. */
static void edit_coin_x(EditorState *es, float x)
{
    editor_select_only(es, ENT_COIN, 0);
    editor_begin_change_tracking(es, EDITOR_CHANGE_ENTITY);
    editor_capture_change_before(es, -1);
    es->level.coins[0].x = x;
    editor_commit_change(es);
    editor_end_change_tracking(es);
}

/* ------------------------------------------------------------------ */
/* Undo and redo                                                       */
/* ------------------------------------------------------------------ */

/*
 * Delete takes an entity out of the middle of its array and Undo puts it
 * back in the same place, the others keeping their order; Place appends
 * and selects, and its Undo removes it. Redo repeats each exactly, and
 * undoing everything brings the document back to its saved state.
 */
static int place_and_delete_undo_in_place(void)
{
    int failed = 0;
    static EditorState es;
    CHECK(open_blank_document(&es) == 0);
    es.level.coin_count = 3;
    es.level.coins[0] = (CoinPlacement){40.0f, 100.0f};
    es.level.coins[1] = (CoinPlacement){80.0f, 100.0f};
    es.level.coins[2] = (CoinPlacement){120.0f, 100.0f};
    editor_set_document_save_point(&es);

    editor_select_only(&es, ENT_COIN, 1);
    tools_delete_selected(&es);
    CHECK(es.level.coin_count == 2 && es.level.coins[1].x == 120.0f);
    CHECK(es.modified);

    CHECK(undo_step(&es) == 1);
    CHECK(es.level.coin_count == 3);
    CHECK(es.level.coins[0].x == 40.0f && es.level.coins[1].x == 80.0f &&
          es.level.coins[2].x == 120.0f);
    CHECK(!es.modified);                       /* back at the save point */

    CHECK(redo_step(&es) == 1);
    CHECK(es.level.coin_count == 2 && es.level.coins[1].x == 120.0f);

    /* Place appends the new coin and selects it. */
    PlacementData coin = {.coin = {200.0f, 60.0f}};
    CHECK(editor_add_placement(&es, ENT_COIN, &coin, "place") == 0);
    CHECK(es.level.coin_count == 3 && es.level.coins[2].x == 200.0f);
    CHECK(es.selection.type == ENT_COIN && es.selection.index == 2);
    CHECK(undo_step(&es) == 1);
    CHECK(es.level.coin_count == 2);
    CHECK(!editor_selection_is_valid(&es) || es.selection.index < 2);
    CHECK(redo_step(&es) == 1);
    CHECK(es.level.coin_count == 3 && es.level.coins[2].y == 60.0f);

    /* Undo all of it: the saved document again, and nothing left to undo. */
    while (undo_step(&es) > 0) {}
    CHECK(es.level.coin_count == 3 && es.level.coins[1].x == 80.0f);
    CHECK(!es.modified);
    CHECK(es.undo->top == 0 && es.undo->redo_top == 2);
done:
    close_document(&es);
    return failed;
}

/*
 * The Last Star and the Player Spawn exist exactly once. "Placing" one
 * moves it, so its Undo moves it back instead of deleting it. A command
 * naming no real entity type is ignored rather than writing anywhere.
 */
static int singletons_move_and_bad_commands_are_ignored(void)
{
    int failed = 0;
    static EditorState es;
    static Command bad;
    CHECK(open_blank_document(&es) == 0);

    PlacementData star_before = editor_snapshot_entity(&es.level, ENT_LAST_STAR, 0);
    PlacementData star = {.last_star = {300.0f, 80.0f}};
    CHECK(editor_add_placement(&es, ENT_LAST_STAR, &star, "place") == 0);
    PlacementData moved = editor_snapshot_entity(&es.level, ENT_LAST_STAR, 0);
    CHECK(moved.last_star.x == 300.0f && moved.last_star.y == 80.0f);
    CHECK(undo_step(&es) == 1);
    PlacementData back = editor_snapshot_entity(&es.level, ENT_LAST_STAR, 0);
    CHECK(memcmp(&back, &star_before, sizeof(back)) == 0);
    CHECK(redo_step(&es) == 1);
    CHECK(editor_snapshot_entity(&es.level, ENT_LAST_STAR, 0).last_star.x == 300.0f);

    /* Unknown entity types and a missing command change nothing. */
    uint64_t hash = editor_document_hash(&es.level);
    memset(&bad, 0, sizeof(bad));
    bad.type = CMD_DELETE;
    bad.entity_type = -1;
    editor_apply_undo_command(&es, &bad, 1);
    bad.entity_type = ENT_COUNT;
    editor_apply_undo_command(&es, &bad, 0);
    editor_apply_undo_command(&es, NULL, 1);
    editor_apply_undo_command(NULL, &bad, 1);
    CHECK(editor_document_hash(&es.level) == hash);
done:
    close_document(&es);
    return failed;
}

/*
 * The inspector records a change only when it changed something, and
 * only for something that is selected. Level settings (name, hearts...)
 * undo and redo as one step and leave entity edits alone.
 */
static int tracked_edits_record_only_real_changes(void)
{
    int failed = 0;
    static EditorState es;
    CHECK(open_blank_document(&es) == 0);
    es.level.coin_count = 1;
    es.level.coins[0] = (CoinPlacement){40.0f, 100.0f};
    editor_set_document_save_point(&es);

    /* A field committed with its old value: no step, still clean. */
    edit_coin_x(&es, 40.0f);
    CHECK(es.undo->top == 0 && !es.modified);

    /* Nothing selected: nothing is captured, so nothing is recorded. */
    editor_select_none(&es);
    editor_begin_change_tracking(&es, EDITOR_CHANGE_ENTITY);
    editor_capture_change_before(&es, -1);
    es.level.coins[0].x = 64.0f;
    editor_commit_change(&es);
    editor_end_change_tracking(&es);
    CHECK(es.undo->top == 0);
    es.level.coins[0].x = 40.0f;

    /* Level settings: one step that brings both fields back. */
    const int hearts = es.level.initial_hearts;
    editor_begin_change_tracking(&es, EDITOR_CHANGE_CONFIG);
    editor_capture_change_before(&es, -1);
    snprintf(es.level.name, sizeof(es.level.name), "%s", "Renamed");
    es.level.initial_hearts = hearts + 2;
    editor_commit_change(&es);
    editor_end_change_tracking(&es);
    CHECK(es.undo->top == 1 && es.modified);
    edit_coin_x(&es, 96.0f);                       /* a later entity edit */
    CHECK(es.undo->top == 2);

    CHECK(undo_step(&es) == 1);                    /* the coin edit */
    CHECK(es.level.coins[0].x == 40.0f && es.level.initial_hearts == hearts + 2);
    CHECK(undo_step(&es) == 1);                    /* the settings */
    CHECK(es.level.initial_hearts == hearts && strcmp(es.level.name, "Renamed") != 0);
    CHECK(!es.modified);
    CHECK(redo_step(&es) == 1);
    CHECK(es.level.initial_hearts == hearts + 2 && strcmp(es.level.name, "Renamed") == 0);
    CHECK(es.level.coins[0].x == 40.0f);           /* the coin edit stays undone */
done:
    close_document(&es);
    return failed;
}

/*
 * An edit the history has no memory to record is taken back, with a
 * status line saying so: otherwise the level would hold a change Undo can
 * never reach. Only level settings and text edits need memory; a plain
 * entity edit is stored inline and is still recorded.
 */
static int unrecordable_edits_are_taken_back(void)
{
    int failed = 0;
    static EditorState es;
    CHECK(open_blank_document(&es) == 0);
    es.level.coin_count = 1;
    es.level.coins[0] = (CoinPlacement){40.0f, 100.0f};
    editor_set_document_save_point(&es);
    const int volume = es.level.music_volume;
    char next_phase[sizeof(es.level.next_phase)];
    memcpy(next_phase, es.level.next_phase, sizeof(next_phase));

    undo_test_limit_allocations(0);                /* every allocation fails */

    editor_begin_change_tracking(&es, EDITOR_CHANGE_CONFIG);
    editor_capture_change_before(&es, -1);
    es.level.music_volume = volume + 10;
    editor_commit_change(&es);
    editor_end_change_tracking(&es);
    CHECK(es.level.music_volume == volume);
    CHECK(strcmp(es.status_message, "Config edit cancelled: cannot allocate undo history") == 0);
    CHECK(es.undo->top == 0 && !es.modified);

    /* The Last Star's next-phase text is the one entity field with text. */
    editor_select_only(&es, ENT_LAST_STAR, 0);
    editor_begin_change_tracking(&es, EDITOR_CHANGE_ENTITY);
    editor_capture_change_before(&es, EDITOR_LAST_STAR_NEXT_PHASE_WIDGET);
    snprintf(es.level.next_phase, sizeof(es.level.next_phase), "%s", "levels/next.toml");
    editor_commit_change(&es);
    editor_end_change_tracking(&es);
    CHECK(memcmp(es.level.next_phase, next_phase, sizeof(next_phase)) == 0);
    CHECK(strcmp(es.status_message, "Edit cancelled: cannot allocate undo history") == 0);
    CHECK(es.undo->top == 0 && !es.modified);

    /* A plain field needs no allocation: it is recorded as usual. */
    edit_coin_x(&es, 64.0f);
    CHECK(es.level.coins[0].x == 64.0f && es.undo->top == 1);
    undo_test_limit_allocations(-1);
    CHECK(undo_step(&es) == 1 && es.level.coins[0].x == 40.0f);
done:
    close_document(&es);
    return failed;
}

/*
 * The history keeps the newest UNDO_MAX steps. After more edits than that,
 * undoing everything stops at the oldest step it still has (the earlier
 * ones are gone), and redoing everything returns to the newest edit.
 */
static int history_keeps_the_newest_steps(void)
{
    int failed = 0;
    static EditorState es;
    const int edits = UNDO_MAX + 44;
    CHECK(open_blank_document(&es) == 0);
    es.level.coin_count = 1;
    es.level.coins[0] = (CoinPlacement){0.0f, 100.0f};
    editor_set_document_save_point(&es);

    for (int i = 1; i <= edits; i++) edit_coin_x(&es, (float)i);
    CHECK(es.undo->top == UNDO_MAX && es.level.coins[0].x == (float)edits);

    int undone = 0;
    while (undo_step(&es) > 0) undone++;
    CHECK(undone == UNDO_MAX);
    /* The oldest kept step changed 44 into 45, so Undo stops at 44. */
    CHECK(es.level.coins[0].x == (float)(edits - UNDO_MAX));
    CHECK(es.modified);                    /* the saved x = 0 is out of reach */

    int redone = 0;
    while (redo_step(&es) > 0) redone++;
    CHECK(redone == UNDO_MAX && es.level.coins[0].x == (float)edits);
done:
    close_document(&es);
    return failed;
}

/*
 * undo.c on its own: a group may push more entries than it reserved (the
 * step grows), a growth that cannot get memory leaves the step as it was,
 * and undo_take undoes the newest entry and forgets it, so Redo cannot
 * bring it back.
 */
static int undo_stack_grows_groups_and_forgets_taken_entries(void)
{
    int failed = 0;
    static Command cmd, out;
    UndoStack *stack = undo_create();
    CHECK(stack != NULL);
    memset(&cmd, 0, sizeof(cmd));
    cmd.type = CMD_MOVE;
    cmd.entity_type = ENT_COIN;

    CHECK(undo_group_begin(stack, UNDO_GROUP_MAX + 1) == 0);  /* too big */
    /* Reserve room for one entry, then push three: the second moves the
     * step from its inline slot to the heap, the third grows that array. */
    int group = undo_group_begin(stack, 1);
    CHECK(group != 0);
    cmd.entity_index = 0;
    CHECK(undo_push(stack, &cmd) == 1);
    /* Without memory to grow, the second push is refused, history kept. */
    undo_test_limit_allocations(0);
    cmd.entity_index = 1;
    CHECK(undo_push(stack, &cmd) == 0);
    undo_test_limit_allocations(-1);
    CHECK(stack->count == 1 && stack->top == 1);
    for (int i = 1; i < 3; i++) {
        cmd.entity_index = i;
        CHECK(undo_push(stack, &cmd) == 1);
    }
    undo_group_end(stack);
    CHECK(stack->count == 1 && stack->top == 1);   /* the group is one step */
    CHECK(undo_top_group(stack) == group);

    /* Undo meets the three entries newest first. */
    for (int i = 2; i >= 0; i--) {
        CHECK(undo_pop(stack, &out) == 1);
        CHECK(out.entity_index == i && out.group == group);
    }
    CHECK(undo_pop(stack, &out) == 0);
    for (int i = 0; i < 3; i++) CHECK(redo_pop(stack, &out) == 1 && out.entity_index == i);

    /* undo_take: the newest entry comes out and does not come back. */
    CHECK(undo_take(stack, &out) == 1 && out.entity_index == 2);
    CHECK(stack->redo_top == 0 && stack->top == 1);
    CHECK(redo_pop(stack, &out) == 0);
    CHECK(undo_take(stack, &out) == 1 && out.entity_index == 1);
    CHECK(undo_take(stack, &out) == 1 && out.entity_index == 0);
    CHECK(stack->count == 0 && undo_take(stack, &out) == 0);
done:
    undo_test_limit_allocations(-1);
    undo_destroy(stack);
    return failed;
}

/* ------------------------------------------------------------------ */
/* Clipboard                                                           */
/* ------------------------------------------------------------------ */

/*
 * Copying a selection of several entities and pasting adds one copy of
 * each, 24 px right and down, as a single undo step; the copies become
 * the selection. One Undo removes them all and one Redo brings them back.
 */
static int multi_selection_pastes_as_one_step(void)
{
    int failed = 0;
    static EditorState es;
    CHECK(open_blank_document(&es) == 0);
    es.level.coin_count = 1;
    es.level.coins[0] = (CoinPlacement){100.0f, 100.0f};
    es.level.bird_count = 1;
    es.level.birds[0] = (BirdPlacement){200.0f, 100.0f, 45.0f, 150.0f, 300.0f, 0};
    editor_set_document_save_point(&es);

    const Selection both[2] = {{ENT_COIN, 0}, {ENT_BIRD, 0}};
    CHECK(editor_select_items(&es, both, 2) == 2);
    editor_copy_selected(&es);
    CHECK(es.clipboard_count == 2);
    CHECK(strcmp(es.status_message, "Copied 2 entities") == 0);

    editor_paste_clipboard(&es);
    CHECK(strcmp(es.status_message, "Pasted 2 entities") == 0);
    CHECK(es.level.coin_count == 2 && es.level.bird_count == 2);
    CHECK(es.level.coins[1].x == 124.0f && es.level.coins[1].y == 124.0f);
    /* A bird carries its patrol along. */
    CHECK(es.level.birds[1].x == 224.0f && es.level.birds[1].base_y == 124.0f);
    CHECK(es.level.birds[1].patrol_x0 == 174.0f && es.level.birds[1].patrol_x1 == 324.0f);
    CHECK(editor_selection_count(&es) == 2);
    CHECK(editor_is_selected(&es, ENT_COIN, 1) && editor_is_selected(&es, ENT_BIRD, 1));
    CHECK(!editor_is_selected(&es, ENT_COIN, 0));
    CHECK(es.undo->top == 1);

    CHECK(undo_step(&es) == 2);
    CHECK(es.level.coin_count == 1 && es.level.bird_count == 1 && !es.modified);
    CHECK(redo_step(&es) == 2);
    CHECK(es.level.coin_count == 2 && es.level.birds[1].x == 224.0f);

    /* Pasting again steps one more offset from the last copies. */
    editor_paste_clipboard(&es);
    CHECK(es.level.coin_count == 3 && es.level.coins[2].x == 148.0f);
done:
    close_document(&es);
    return failed;
}

/*
 * Ctrl+X copies the selection and deletes it (one undoable step), so a
 * later paste puts a copy back, offset like any other paste.
 */
static int cut_copies_then_deletes(void)
{
    int failed = 0;
    static EditorState es;
    InputEvent cut;
    CHECK(open_blank_document(&es) == 0);
    es.level.coin_count = 2;
    es.level.coins[0] = (CoinPlacement){100.0f, 100.0f};
    es.level.coins[1] = (CoinPlacement){300.0f, 50.0f};
    editor_set_document_save_point(&es);

    editor_select_only(&es, ENT_COIN, 1);
    memset(&cut, 0, sizeof(cut));
    cut.type = INPUT_KEY_DOWN;
    cut.key = KEY_X;
    cut.mods = INPUT_CTRL;
    editor_handle_event(&es, &cut);
    CHECK(es.level.coin_count == 1 && es.level.coins[0].x == 100.0f);
    CHECK(es.clipboard_count == 1);

    editor_paste_clipboard(&es);
    CHECK(es.level.coin_count == 2);
    CHECK(es.level.coins[1].x == 324.0f && es.level.coins[1].y == 74.0f);
    CHECK(undo_step(&es) == 1 && es.level.coin_count == 1);   /* the paste */
    CHECK(undo_step(&es) == 1 && es.level.coin_count == 2);   /* the cut */
    CHECK(es.level.coins[1].x == 300.0f && !es.modified);
done:
    close_document(&es);
    return failed;
}

/*
 * The clipboard outlives the document it was filled in. A copy taken near
 * the end of a wide level and pasted into a narrower one is pulled back
 * inside the new level's width instead of being refused or left outside.
 */
static int paste_clamps_into_a_narrower_level(void)
{
    int failed = 0;
    static EditorState es;
    CHECK(open_blank_document(&es) == 0);
    es.level.screen_count = 4;
    es.level.coin_count = 1;
    es.level.coins[0] = (CoinPlacement){1580.0f, 100.0f};
    editor_select_only(&es, ENT_COIN, 0);
    editor_copy_selected(&es);

    editor_reset_new_level(&es);
    es.level.screen_count = 1;
    es.level.coin_count = 0;
    CHECK(es.clipboard_count == 1);
    editor_paste_clipboard(&es);
    CHECK(es.level.coin_count == 1);
    CHECK(es.level.coins[0].x == (float)GAME_W);       /* the world's right edge */
    CHECK(es.level.coins[0].y == 124.0f);
    CHECK(level_validate_preview(&es.level, NULL, 0) == 0);
done:
    close_document(&es);
    return failed;
}

/*
 * A paste that cannot add every copy adds none, and leaves the history
 * alone, Redo included. Here the coin array has room for one copy of the
 * two copied coins.
 */
static int paste_that_cannot_fit_adds_nothing(void)
{
    int failed = 0;
    static EditorState es;
    CHECK(open_blank_document(&es) == 0);
    es.level.coin_count = MAX_COINS - 2;
    for (int i = 0; i < es.level.coin_count; i++)
        es.level.coins[i] = (CoinPlacement){20.0f + 20.0f * (float)i, 60.0f};
    editor_set_document_save_point(&es);

    /* An edit, undone, so there is a Redo step to keep. */
    edit_coin_x(&es, 30.0f);
    CHECK(undo_step(&es) == 1 && es.undo->redo_top == 1);

    PlacementData one_more = {.coin = {500.0f, 60.0f}};
    CHECK(editor_add_placement(&es, ENT_COIN, &one_more, "place") == 0);
    CHECK(es.level.coin_count == MAX_COINS - 1);
    CHECK(undo_step(&es) == 1 && es.level.coin_count == MAX_COINS - 2);
    CHECK(es.undo->redo_top == 1);                       /* place, undone */

    const Selection pair[2] = {{ENT_COIN, 0}, {ENT_COIN, 1}};
    CHECK(editor_select_items(&es, pair, 2) == 2);
    editor_copy_selected(&es);
    es.level.coins[es.level.coin_count++] = (CoinPlacement){900.0f, 60.0f};
    uint64_t hash = editor_document_hash(&es.level);
    const int top = es.undo->top, redo_top = es.undo->redo_top;

    editor_paste_clipboard(&es);
    CHECK(es.level.coin_count == MAX_COINS - 1);
    CHECK(editor_document_hash(&es.level) == hash);
    CHECK(es.undo->top == top && es.undo->redo_top == redo_top);
    CHECK(es.status_message[0] != '\0');
done:
    close_document(&es);
    return failed;
}

/*
 * The refusals explain themselves: an empty clipboard, a level with
 * errors (pasting or duplicating would only add to them), nothing
 * selected to duplicate, and the one-per-level Last Star.
 */
static int paste_and_duplicate_refusals_explain_why(void)
{
    int failed = 0;
    static EditorState es;
    CHECK(open_blank_document(&es) == 0);

    editor_paste_clipboard(&es);
    CHECK(strcmp(es.status_message, "Nothing to paste: copy an entity with Ctrl+C first") == 0);
    editor_duplicate_selection(&es);
    CHECK(strcmp(es.status_message, "Nothing to duplicate: select an entity first") == 0);
    editor_select_only(&es, ENT_LAST_STAR, 0);
    editor_duplicate_selection(&es);
    CHECK(strstr(es.status_message, "is unique; it cannot be duplicated") != NULL);

    es.level.coin_count = 1;
    es.level.coins[0] = (CoinPlacement){100.0f, 100.0f};
    editor_select_only(&es, ENT_COIN, 0);
    editor_copy_selected(&es);
    es.level.coin_count = MAX_COINS + 1;               /* a broken document */
    editor_paste_clipboard(&es);
    CHECK(STARTS_WITH(es.status_message, "Paste blocked: fix level errors first ("));
    editor_duplicate_selection(&es);
    CHECK(STARTS_WITH(es.status_message, "Duplicate blocked: fix level errors first ("));
    CHECK(es.level.coin_count == MAX_COINS + 1 && es.undo->top == 0);
done:
    close_document(&es);
    return failed;
}

/*
 * A float platform riding a rail is placed by its rail, so a pasted or
 * duplicated copy steps 24 px (1.5 rail tiles) along the same rail
 * instead of moving in x and y.
 */
static int rail_platform_copies_step_along_the_rail(void)
{
    int failed = 0;
    static EditorState es;
    CHECK(open_blank_document(&es) == 0);
    es.level.rail_count = 1;
    es.level.rails[0] = (RailPlacement){RAIL_LAYOUT_RECT, 32, 32, 8, 4, 0};
    es.level.float_platform_count = 1;
    es.level.float_platforms[0] = (FloatPlatformPlacement){
        FLOAT_PLATFORM_RAIL, 0.0f, 0.0f, 3, 0, 2.0f, 1.0f};
    CHECK(level_validate_preview(&es.level, NULL, 0) == 0);
    editor_set_document_save_point(&es);

    editor_select_only(&es, ENT_FLOAT_PLATFORM, 0);
    editor_copy_selected(&es);
    editor_paste_clipboard(&es);
    CHECK(es.level.float_platform_count == 2);
    CHECK(es.level.float_platforms[1].mode == FLOAT_PLATFORM_RAIL);
    CHECK(es.level.float_platforms[1].rail_index == 0);
    CHECK(es.level.float_platforms[1].t_offset == 3.5f);

    /* Duplicate steps from the selected copy in the same way. */
    editor_duplicate_selection(&es);
    CHECK(strcmp(es.status_message, "Duplicated 1 entity") == 0);
    CHECK(es.level.float_platform_count == 3);
    CHECK(es.level.float_platforms[2].rail_index == 0);
    CHECK(es.level.float_platforms[2].t_offset == 5.0f);
    CHECK(es.clipboard[0].data.float_platform.t_offset == 3.5f);  /* clipboard untouched */
done:
    close_document(&es);
    return failed;
}

/* ------------------------------------------------------------------ */
/* Recovery                                                            */
/* ------------------------------------------------------------------ */

/* A private preference folder for one test: TEST_OUT/undo_recovery_<pid>. */
static int make_preference_root(char *root, size_t size)
{
#ifdef _WIN32
    unsigned long pid = (unsigned long)_getpid();
#else
    unsigned long pid = (unsigned long)getpid();
#endif
    int written = snprintf(root, size, TEST_OUT "undo_recovery_%lu", pid);
    if (written < 0 || (size_t)written >= size) return -1;
#ifdef _WIN32
    (void)_mkdir(MANGO_TEST_OUTDIR);
    (void)_mkdir(root);
#else
    (void)mkdir(MANGO_TEST_OUTDIR, 0755);
    (void)mkdir(root, 0755);
#endif
    return 0;
}

/* Delete what an editor left in the folder, then the folder itself. */
static void remove_preference_root(const char *root, EditorState *const *editors, int count)
{
    for (int e = 0; e < count; e++) {
        EditorState *es = editors[e];
        if (editor_discover_recoveries(es) == 0)
            for (int i = 0; i < es->recovery_entry_count; i++) {
                remove(es->recovery_entries[i].metadata_path);
                remove(es->recovery_entries[i].snapshot_path);
            }
        remove(es->autosave_path);
        remove(es->playtest_path);
        remove(es->recent_path);
        remove(es->settings_path);
    }
#ifdef _WIN32
    (void)_rmdir(root);
#else
    (void)rmdir(root);
#endif
}

/* An editor with its preference folder at root and a dirty document. */
static int open_editor_in(EditorState *es, const char *root, const char *name,
                          const char *file_path)
{
    if (open_blank_document(es) != 0) return -1;
    snprintf(es->level.name, sizeof(es->level.name), "%s", name);
    snprintf(es->file_path, sizeof(es->file_path), "%s", file_path);
    if (editor_set_preference_root(es, root) != 0 ||
        editor_init_persistence_paths(es) != 0) return -1;
    es->modified = 1;
    return 0;
}

/* Write a snapshot now, whatever the 30 s timer says. */
static void autosave_now(EditorState *es)
{
    es->last_autosave_ms = (uint32_t)clock_millis() - 30001u;
    editor_maybe_autosave(es);
}

/*
 * Recovery snapshots are editor_recovery_<16 hex digits>.toml, in any
 * folder; anything else is an ordinary level the editor may save over.
 */
static int recovery_file_names_are_recognised(void)
{
    int failed = 0;
    CHECK(editor_path_is_recovery("editor_recovery_0123456789abcdef.toml"));
    CHECK(editor_path_is_recovery("some/dir/editor_recovery_0123456789ABCDEF.toml"));
    CHECK(editor_path_is_recovery("C:\\prefs\\editor_recovery_00000000000000ff.toml"));
    CHECK(!editor_path_is_recovery("editor_recovery_0123456789abcdef.meta"));
    CHECK(!editor_path_is_recovery("editor_recovery_0123456789abcde.toml"));   /* 15 */
    CHECK(!editor_path_is_recovery("editor_recovery_0123456789abcdeg.toml"));  /* g */
    CHECK(!editor_path_is_recovery("editor_recovery_0123456789abcdef.toml/x.toml"));
    CHECK(!editor_path_is_recovery("levels/level_01.toml"));
    CHECK(!editor_path_is_recovery(""));
    CHECK(!editor_path_is_recovery(NULL));
done:
    return failed;
}

/*
 * Each document gets its own recovery id and snapshot path inside the
 * preference folder. Autosave writes the snapshot and its .meta (naming
 * the source file and this process); a save or discard retires both.
 */
static int each_document_gets_its_own_snapshot(void)
{
    int failed = 0;
    static EditorState es;
    char root[EDITOR_PATH_MAX] = {0};
    char first[EDITOR_PATH_MAX];
    const char *source = TEST_OUT "undo_recovery_source.toml";
    CHECK(make_preference_root(root, sizeof(root)) == 0);
    CHECK(open_editor_in(&es, root, "Autosaved", source) == 0);
    CHECK(es.recovery_entry_count == 0);
    CHECK(editor_path_is_recovery(es.autosave_path));
    CHECK(strncmp(es.autosave_path, root, strlen(root)) == 0);

    const uint64_t id = es.recovery_document_id;
    memcpy(first, es.autosave_path, sizeof(first));
    CHECK(editor_set_recovery_document(&es, NULL) == 0);
    CHECK(es.recovery_document_id != id && strcmp(es.autosave_path, first) != 0);
    CHECK(editor_path_is_recovery(es.autosave_path));

    autosave_now(&es);
    CHECK(strcmp(es.status_message, "Autosaved recovery copy") == 0);
    CHECK(es.recovery_entry_count == 1);
    const EditorRecoveryEntry *entry = &es.recovery_entries[0];
    CHECK(entry->id == es.recovery_document_id);
    CHECK(strcmp(entry->source_path, source) == 0);
    CHECK(strcmp(entry->snapshot_path, es.autosave_path) == 0);
    CHECK(editor_file_exists(entry->snapshot_path) && editor_file_exists(entry->metadata_path));
#ifndef _WIN32
    CHECK(entry->owner_pid == (unsigned long)getpid());
#endif
    /* The snapshot is a normal level file holding the document. */
    static LevelDef loaded;
    CHECK(level_load_toml(entry->snapshot_path, &loaded) == 0);
    CHECK(strcmp(loaded.name, "Autosaved") == 0);

    /* Saving another file leaves this document's copy alone ... */
    char metadata[EDITOR_PATH_MAX];
    memcpy(metadata, entry->metadata_path, sizeof(metadata));
    editor_retire_matching_recovery(&es, TEST_OUT "some_other_level.toml");
    CHECK(editor_file_exists(es.autosave_path) && editor_file_exists(metadata));
    /* ... saving its own file retires both halves. */
    editor_retire_matching_recovery(&es, source);
    CHECK(!editor_file_exists(es.autosave_path) && !editor_file_exists(metadata));
    CHECK(es.recovery_entry_count == 0);

    /* A document path too long for the editor gets no recovery document. */
    char too_long[EDITOR_PATH_MAX + 8];
    memset(too_long, 'a', sizeof(too_long) - 1);
    too_long[sizeof(too_long) - 1] = '\0';
    CHECK(editor_set_recovery_document(&es, too_long) == -1);
done:
    remove_preference_root(root, (EditorState *[]){&es}, 1);
    close_document(&es);
    return failed;
}

/*
 * After a crash the next editor finds the dead editor's copy and loads it
 * by id as an unsaved document of the original file. Its own snapshot
 * is retired first, so the folder does not keep two copies of one
 * session. An id that is no longer on disk is refused, and recovering the
 * same copy again through the document's own snapshot path works too.
 */
static int a_crashed_editors_copy_is_recovered_by_id(void)
{
    int failed = 0;
    EditorState *crashed = calloc(1, sizeof(*crashed));
    EditorState *restarted = calloc(1, sizeof(*restarted));
    char root[EDITOR_PATH_MAX] = {0};
    char own_snapshot[EDITOR_PATH_MAX];
    const char *source = TEST_OUT "undo_recovery_crashed.toml";
    CHECK(crashed && restarted);
    CHECK(make_preference_root(root, sizeof(root)) == 0);

    /* The "crashed" editor wrote a copy, then stopped without retiring it. */
    CHECK(open_editor_in(crashed, root, "Crashed work", source) == 0);
    autosave_now(crashed);
    CHECK(crashed->recovery_entry_count == 1);
    const uint64_t crashed_id = crashed->recovery_document_id;

    CHECK(open_editor_in(restarted, root, "Fresh start", "") == 0);
    CHECK(restarted->recovery_entry_count == 1);   /* finds the crashed copy */
    autosave_now(restarted);                       /* and writes its own */
    CHECK(restarted->recovery_entry_count == 2);
    memcpy(own_snapshot, restarted->autosave_path, sizeof(own_snapshot));

    CHECK(editor_recover_entry_by_id(restarted, crashed_id + 1000u) == -1);
    CHECK(strcmp(restarted->status_message, "Recovery copy no longer exists") == 0);
    CHECK(strcmp(restarted->level.name, "Fresh start") == 0);

    CHECK(editor_recover_entry_by_id(restarted, crashed_id) == 0);
    CHECK(strcmp(restarted->status_message, "Recovered unsaved changes") == 0);
    CHECK(strcmp(restarted->level.name, "Crashed work") == 0);
    CHECK(strcmp(restarted->file_path, source) == 0 && restarted->modified);
    CHECK(restarted->recovery_document_id == crashed_id);
    CHECK(strcmp(restarted->autosave_path, crashed->autosave_path) == 0);
    CHECK(!editor_file_exists(own_snapshot));      /* its own copy retired */
    CHECK(restarted->recovery_entry_count == 1);

    /* The document's own snapshot, found through the index, loads again. */
    snprintf(restarted->level.name, sizeof(restarted->level.name), "%s", "Edited since");
    CHECK(editor_recover_autosave(restarted) == 0);
    CHECK(strcmp(restarted->level.name, "Crashed work") == 0);

    /* Once retired, there is nothing to recover. */
    editor_retire_current_recovery(restarted);
    CHECK(restarted->recovery_entry_count == 0);
    CHECK(editor_recover_autosave(restarted) == -1);
    CHECK(strcmp(restarted->status_message, "Recovery copy not found") == 0);
done:
    if (crashed && restarted)
        remove_preference_root(root, (EditorState *[]){crashed, restarted}, 2);
    if (crashed) close_document(crashed);
    if (restarted) close_document(restarted);
    free(crashed);
    free(restarted);
    return failed;
}

/* Write a recovery pair by hand: a version-1 .meta (no owner) and a .toml. */
static int write_old_recovery_pair(const char *root, unsigned long long id)
{
    char path[EDITOR_PATH_MAX + 64];
    char line[96];
    FILE *fp;

    snprintf(path, sizeof(path), "%s/editor_recovery_%016llx.toml", root, id);
    if (!(fp = fopen(path, "w"))) return -1;
    fputs("name = \"left over\"\n", fp);
    fclose(fp);
    snprintf(path, sizeof(path), "%s/editor_recovery_%016llx.meta", root, id);
    snprintf(line, sizeof(line), "1\t%016llx\t%llu\t-\n", id, 1700000000ull + id);
    if (!(fp = fopen(path, "w"))) return -1;
    fputs(line, fp);
    fclose(fp);
    return 0;
}

/*
 * The picker walks the copies: Next moves to the following copy and wraps
 * after the last, Back asks about the same copy again, Recover picks the
 * one on show (its id waits in pending_recovery_id), Cancel picks none.
 */
static int recovery_picker_walks_the_copies(void)
{
    int failed = 0;
    static EditorState es;
    char root[EDITOR_PATH_MAX] = {0};
    CHECK(make_preference_root(root, sizeof(root)) == 0);
    CHECK(write_old_recovery_pair(root, 0x51) == 0);
    CHECK(write_old_recovery_pair(root, 0x52) == 0);
    CHECK(write_old_recovery_pair(root, 0x53) == 0);
    CHECK(open_editor_in(&es, root, "Picker", "") == 0);
    CHECK(es.recovery_entry_count == 3);

    editor_test_set_recovery_choice(EDITOR_RECOVERY_NEXT);
    editor_test_queue_recovery_choice(EDITOR_RECOVERY_RECOVER);
    CHECK(editor_choose_recovery(&es) == 1);
    CHECK(es.pending_recovery_id == es.recovery_entries[1].id);

    editor_test_set_recovery_choice(EDITOR_RECOVERY_NEXT);
    editor_test_queue_recovery_choice(EDITOR_RECOVERY_NEXT);
    editor_test_queue_recovery_choice(EDITOR_RECOVERY_NEXT);
    editor_test_queue_recovery_choice(EDITOR_RECOVERY_RECOVER);
    CHECK(editor_choose_recovery(&es) == 0);       /* wrapped past the last */
    CHECK(es.pending_recovery_id == es.recovery_entries[0].id);

    editor_test_set_recovery_choice(EDITOR_RECOVERY_NEXT);
    editor_test_queue_recovery_choice(EDITOR_RECOVERY_BACK);
    editor_test_queue_recovery_choice(EDITOR_RECOVERY_RECOVER);
    CHECK(editor_choose_recovery(&es) == 1);

    editor_test_set_recovery_choice(EDITOR_RECOVERY_CANCEL);
    CHECK(editor_choose_recovery(&es) == -1);
    CHECK(es.pending_recovery_id == 0);
    CHECK(strcmp(es.status_message, "Recovery cancelled") == 0);
    CHECK(es.recovery_entry_count == 3);           /* nothing was deleted */
done:
    editor_test_set_recovery_choice(-1);
    remove_preference_root(root, (EditorState *[]){&es}, 1);
    close_document(&es);
    return failed;
}

#ifndef _WIN32
/*
 * A copy whose owner is another editor that is still running is that
 * editor's live work, not a crash leftover: it is never offered, and its
 * files stay. A child process stands in for the other editor and is held
 * on a gate pipe, so it is certainly alive while the editor looks; once
 * the gate closes and the child has exited, the same copy is offered.
 */
static int another_running_editors_copy_is_not_offered(void)
{
    int failed = 0;
    static EditorState es;
    char root[EDITOR_PATH_MAX] = {0};
    char path[EDITOR_PATH_MAX + 64];
    int gate[2] = {-1, -1};
    pid_t child = -1;
    const unsigned long long id = 0x6001;

    CHECK(make_preference_root(root, sizeof(root)) == 0);
    CHECK(open_editor_in(&es, root, "Watcher", "") == 0);
    CHECK(pipe(gate) == 0);
    child = fork();
    CHECK(child >= 0);
    if (child == 0) {
        char byte;
        close(gate[1]);
        while (read(gate[0], &byte, 1) > 0) {}     /* wait for the gate */
        _exit(0);
    }
    close(gate[0]);
    gate[0] = -1;

    /* The child's copy: version 3 records its pid and start time. */
    snprintf(path, sizeof(path), "%s/editor_recovery_%016llx.toml", root, id);
    FILE *fp = fopen(path, "w");
    CHECK(fp != NULL);
    fputs("name = \"live work\"\n", fp);
    fclose(fp);
    snprintf(path, sizeof(path), "%s/editor_recovery_%016llx.meta", root, id);
    fp = fopen(path, "w");
    CHECK(fp != NULL);
    fprintf(fp, "3\t%016llx\t1700000000\t%lu\t%llu\t-\n", id, (unsigned long)child,
            (unsigned long long)editor_process_start_time((unsigned long)child));
    fclose(fp);

    CHECK(editor_discover_recoveries(&es) == 0);
    CHECK(es.recovery_entry_count == 0);           /* live elsewhere: hidden */
    CHECK(editor_clean_orphan_recoveries(&es) == 0);
    CHECK(editor_file_exists(path));               /* its .meta stays ... */
    snprintf(path, sizeof(path), "%s/editor_recovery_%016llx.toml", root, id);
    CHECK(editor_file_exists(path));               /* ... and so does the copy */

    close(gate[1]);                                /* the other editor exits */
    gate[1] = -1;
    CHECK(waitpid(child, NULL, 0) == child);
    child = -1;
    CHECK(editor_discover_recoveries(&es) == 0);
    CHECK(es.recovery_entry_count == 1 && es.recovery_entries[0].id == id);
done:
    if (gate[0] >= 0) close(gate[0]);
    if (gate[1] >= 0) close(gate[1]);
    if (child > 0) waitpid(child, NULL, 0);
    remove_preference_root(root, (EditorState *[]){&es}, 1);
    close_document(&es);
    return failed;
}
#endif

/* ------------------------------------------------------------------ */

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    const struct { const char *name; int (*run)(void); } cases[] = {
#define CASE(fn) {#fn, fn}
        CASE(place_and_delete_undo_in_place),
        CASE(singletons_move_and_bad_commands_are_ignored),
        CASE(tracked_edits_record_only_real_changes),
        CASE(unrecordable_edits_are_taken_back),
        CASE(history_keeps_the_newest_steps),
        CASE(undo_stack_grows_groups_and_forgets_taken_entries),
        CASE(multi_selection_pastes_as_one_step),
        CASE(cut_copies_then_deletes),
        CASE(paste_clamps_into_a_narrower_level),
        CASE(paste_that_cannot_fit_adds_nothing),
        CASE(paste_and_duplicate_refusals_explain_why),
        CASE(rail_platform_copies_step_along_the_rail),
        CASE(recovery_file_names_are_recognised),
        CASE(each_document_gets_its_own_snapshot),
        CASE(a_crashed_editors_copy_is_recovered_by_id),
        CASE(recovery_picker_walks_the_copies),
#ifndef _WIN32
        CASE(another_running_editors_copy_is_not_offered),
#endif
#undef CASE
    };
    int failures = 0;
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        int result = cases[i].run();
        printf("undo/recovery: %s %s\n", cases[i].name, result ? "FAIL" : "PASS");
        failures += result != 0;
    }
    printf("editor_undo_recovery_test: %d failing cases\n", failures);
    return failures ? 1 : 0;
}
