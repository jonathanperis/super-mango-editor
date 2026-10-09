/* Bounded command history: one timeline of steps (see UndoStack in undo.h).
 *
 * Memory: a lone entry lives inside its step, a group's entries in one heap
 * array per step, and only configuration edits own heap snapshots.  Undo and
 * redo never move an entry, so they never allocate and never hand a pointer
 * from one owner to another; copying an entry to the caller returns a value
 * snapshot, never a borrowed pointer into history.  The only hand-over is the
 * room undo_group_begin reserves, which the group's step takes on its first
 * push. */
#include "undo.h"
#include <stdlib.h>
#include <string.h>

#ifdef MANGO_TESTING
static int s_test_allocations_left = -1;   /* < 0: no limit */

void undo_test_limit_allocations(int count)
{
    s_test_allocations_left = count;
}

/* Spend one allocation from the test budget; 0 once it is used up. */
static int test_allocation_allowed(void)
{
    if (s_test_allocations_left == 0) return 0;
    if (s_test_allocations_left > 0) s_test_allocations_left--;
    return 1;
}
#else
static int test_allocation_allowed(void) { return 1; }
#endif

/* Every history allocation goes through these two, so the test seam above
 * can make any of them fail. */
static void *history_malloc(size_t size)
{
    return test_allocation_allowed() ? malloc(size) : NULL;
}

static void *history_realloc(void *block, size_t size)
{
    return test_allocation_allowed() ? realloc(block, size) : NULL;
}

UndoStack *undo_create(void)
{
    return calloc(1, sizeof(UndoStack));
}

/* The step's entries, wherever they are stored, and how many fit there. */
static UndoEntry *step_entries(UndoStep *step)
{
    return step->many ? step->many : &step->one;
}

static int step_capacity(const UndoStep *step)
{
    return step->many ? step->capacity : 1;
}

/* Free what entries [from, step->count) own and forget them. */
static void release_entries_from(UndoStep *step, int from)
{
    UndoEntry *entries = step_entries(step);
    for (int i = from; i < step->count; i++) {
        free(entries[i].config);
        entries[i].config = NULL;
    }
    step->count = from;
    if (step->applied > from) step->applied = from;
}

/* Forget steps[index] and everything it owns, closing the gap. */
static void remove_step(UndoStack *stack, int index)
{
    UndoStep *step = &stack->steps[index];
    release_entries_from(step, 0);
    free(step->many);
    memmove(step, step + 1, (size_t)(stack->count - index - 1) * sizeof(*step));
    stack->count--;
}

/*
 * recount --- Refresh top and redo_top after the timeline changed.
 *
 * Undo always lowers the newest step that still has applied entries and
 * redo raises the oldest one that has undone entries, so the timeline is
 * always: fully applied steps, at most one partly applied step, then fully
 * undone steps.  Counting from each end finds the two boundaries.
 */
static void recount(UndoStack *stack)
{
    int top = stack->count;
    int first_undone = 0;

    while (top > 0 && stack->steps[top - 1].applied == 0) top--;
    while (first_undone < stack->count &&
           stack->steps[first_undone].applied == stack->steps[first_undone].count)
        first_undone++;
    stack->top = top;
    stack->redo_top = stack->count - first_undone;
}

void undo_clear(UndoStack *stack)
{
    if (!stack) return;
    while (stack->count > 0) remove_step(stack, stack->count - 1);
    recount(stack);
}

void undo_destroy(UndoStack *stack)
{
    if (!stack) return;
    undo_clear(stack);
    free(stack->reserve);
    free(stack); /* The caller must clear its owning pointer after this call. */
}

/*
 * discard_redo --- A new edit makes everything undone unreachable: drop the
 * fully undone steps and the undone tail of a partly undone one.
 */
static void discard_redo(UndoStack *stack)
{
    while (stack->count > 0 && stack->steps[stack->count - 1].applied == 0)
        remove_step(stack, stack->count - 1);
    if (stack->count > 0) {
        UndoStep *last = &stack->steps[stack->count - 1];
        release_entries_from(last, last->applied);
    }
}

/*
 * make_room --- Make sure `step` can take `needed` entries.  Room reserved
 * by undo_group_begin means this does nothing for a well-sized group; a
 * caller that pushes more than it reserved grows the array here, which can
 * fail.  On failure the step is unchanged.
 */
static int make_room(UndoStep *step, int needed)
{
    UndoEntry *grown;
    int capacity;

    if (needed <= step_capacity(step)) return 1;
    if (needed > UNDO_GROUP_MAX) return 0;
    capacity = step_capacity(step) * 2;
    if (capacity < needed) capacity = needed;
    if (capacity > UNDO_GROUP_MAX) capacity = UNDO_GROUP_MAX;
    if (step->many) {
        grown = history_realloc(step->many, (size_t)capacity * sizeof(*grown));
        if (!grown) return 0;
    } else {
        /* Move the inline entry into the new array. */
        grown = history_malloc((size_t)capacity * sizeof(*grown));
        if (!grown) return 0;
        if (step->count > 0) grown[0] = step->one;
        memset(&step->one, 0, sizeof(step->one));
    }
    step->many = grown;
    step->capacity = capacity;
    return 1;
}

int undo_group_begin(UndoStack *stack, int size)
{
    if (!stack || size > UNDO_GROUP_MAX) return 0;
    /* Any room left from an earlier group that pushed nothing goes first. */
    free(stack->reserve);
    stack->reserve = NULL;
    stack->reserve_capacity = 0;
    /* One entry fits inside the step itself; only a real group needs room. */
    if (size > 1) {
        stack->reserve = history_malloc((size_t)size * sizeof(*stack->reserve));
        if (!stack->reserve) return 0;
        stack->reserve_capacity = size;
    }
    stack->last_group++;
    if (stack->last_group <= 0) stack->last_group = 1;  /* after a wrap */
    stack->open_group = stack->last_group;
    return stack->open_group;
}

void undo_group_end(UndoStack *stack)
{
    if (!stack) return;
    free(stack->reserve);   /* unused room: the group pushed nothing */
    stack->reserve = NULL;
    stack->reserve_capacity = 0;
    stack->open_group = 0;
}

int undo_top_group(const UndoStack *stack)
{
    return stack && stack->top > 0 ? stack->steps[stack->top - 1].group : 0;
}

int redo_top_group(const UndoStack *stack)
{
    /* The oldest step with undone entries is the next to come back. */
    return stack && stack->redo_top > 0
        ? stack->steps[stack->count - stack->redo_top].group : 0;
}

int undo_amend_after(UndoStack *stack, int group, int entity_type,
                     int entity_index, const PlacementData *after)
{
    UndoStep *step;
    UndoEntry *entries;

    if (!stack || group == 0 || !after || stack->top == 0) return 0;
    step = &stack->steps[stack->top - 1];
    if (step->group != group) return 0;
    entries = step_entries(step);
    for (int i = step->applied - 1; i >= 0; i--) {
        if (entries[i].entity_type == entity_type && entries[i].entity_index == entity_index) {
            entries[i].after = *after;
            return 1;
        }
    }
    return 0;
}

/*
 * A Command is about 13 KB (two full placement snapshots plus config
 * snapshots), so it is passed by const pointer: copying it onto the stack for
 * every call would be wasted work, and const promises the caller's command is
 * only read.
 */
int undo_push(UndoStack *stack, const Command *cmd)
{
    UndoStep *step = NULL;
    UndoEntry entry;
    int group;

    if (!stack || !cmd) return 0;
    group = cmd->group ? cmd->group : stack->open_group;
    /* Join the newest applied step when it is this command's group. */
    if (group != 0 && stack->top > 0 && stack->steps[stack->top - 1].group == group) {
        step = &stack->steps[stack->top - 1];
        /* After discard_redo the step holds exactly its applied entries. */
        if (!make_room(step, step->applied + 1)) return 0; /* History intact. */
    }

    memset(&entry, 0, sizeof(entry));
    entry.type = cmd->type;
    entry.entity_type = cmd->entity_type;
    entry.entity_index = cmd->entity_index;
    entry.before = cmd->before;
    entry.after = cmd->after;
    entry.property_field = cmd->property_field;
    memcpy(entry.property_text_before, cmd->property_text_before, sizeof(entry.property_text_before));
    memcpy(entry.property_text_after, cmd->property_text_after, sizeof(entry.property_text_after));
    if (cmd->type == CMD_CONFIG) {
        entry.config = history_malloc(2 * sizeof(*entry.config));
        if (!entry.config) return 0; /* History remains intact. */
        entry.config[0] = cmd->config_before;
        entry.config[1] = cmd->config_after;
    }

    /* Nothing can fail from here on, so history may change. */
    discard_redo(stack);
    if (!step) {
        if (stack->count == UNDO_MAX) remove_step(stack, 0); /* oldest, whole */
        step = &stack->steps[stack->count++];
        memset(step, 0, sizeof(*step));
        step->group = group;
        /* The open group's first push takes the room begin set aside. */
        if (group != 0 && group == stack->open_group && stack->reserve) {
            step->many = stack->reserve;
            step->capacity = stack->reserve_capacity;
            stack->reserve = NULL;
            stack->reserve_capacity = 0;
        }
    }
    step_entries(step)[step->count++] = entry;
    step->applied = step->count;
    recount(stack);
    return 1;
}

/* Copy a stored entry out as a Command (config snapshots by value). */
static void entry_to_command(const UndoEntry *entry, int group, Command *out)
{
    memset(out, 0, sizeof(*out));
    out->type = entry->type;
    out->entity_type = entry->entity_type;
    out->entity_index = entry->entity_index;
    out->group = group;
    out->before = entry->before;
    out->after = entry->after;
    out->property_field = entry->property_field;
    memcpy(out->property_text_before, entry->property_text_before, sizeof(out->property_text_before));
    memcpy(out->property_text_after, entry->property_text_after, sizeof(out->property_text_after));
    if (entry->config) {
        out->config_before = entry->config[0];
        out->config_after = entry->config[1];
    }
}

int undo_pop(UndoStack *stack, Command *out)
{
    UndoStep *step;

    if (!stack || !out || stack->top == 0) return 0;
    step = &stack->steps[stack->top - 1];
    step->applied--;   /* the newest applied entry becomes the newest undone */
    entry_to_command(&step_entries(step)[step->applied], step->group, out);
    recount(stack);
    return 1;
}

int redo_pop(UndoStack *stack, Command *out)
{
    UndoStep *step;

    if (!stack || !out || stack->redo_top == 0) return 0;
    step = &stack->steps[stack->count - stack->redo_top];
    entry_to_command(&step_entries(step)[step->applied], step->group, out);
    step->applied++;   /* the oldest undone entry is applied again */
    recount(stack);
    return 1;
}

int undo_take(UndoStack *stack, Command *out)
{
    int index;
    UndoStep *step;
    UndoEntry *entries;

    if (!stack || stack->top == 0) return 0;
    index = stack->top - 1;
    if (!undo_pop(stack, out)) return 0;
    /* The entry just undone sits at `applied`: forget it, closing the gap. */
    step = &stack->steps[index];
    entries = step_entries(step);
    free(entries[step->applied].config);
    memmove(entries + step->applied, entries + step->applied + 1,
            (size_t)(step->count - step->applied - 1) * sizeof(*entries));
    step->count--;
    if (step->count == 0) remove_step(stack, index);
    recount(stack);
    return 1;
}
