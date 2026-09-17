/* Bounded command history. Only configuration edits own heap snapshots.
 * Moving an entry between stacks transfers ownership; copying it to the caller
 * returns a value snapshot, never a borrowed pointer into history. */
#include "undo.h"
#include <stdlib.h>
#include <string.h>

UndoStack *undo_create(void)
{
    return calloc(1, sizeof(UndoStack));
}

static void release_entries(UndoEntry *entries, int count)
{
    for (int i = 0; i < count; i++) {
        free(entries[i].config);
        entries[i].config = NULL;
    }
}

void undo_clear(UndoStack *stack)
{
    if (!stack) return;
    release_entries(stack->commands, stack->top);
    release_entries(stack->redo_stack, stack->redo_top);
    stack->top = stack->redo_top = 0;
}

void undo_destroy(UndoStack *stack)
{
    if (!stack) return;
    undo_clear(stack);
    free(stack); /* The caller must clear its owning pointer after this call. */
}

static void append_entry(UndoEntry *entries, int *count, UndoEntry entry)
{
    if (*count == UNDO_MAX) {
        free(entries[0].config);
        memmove(entries, entries + 1, (UNDO_MAX - 1) * sizeof(*entries));
        (*count)--;
    }
    entries[(*count)++] = entry;
}

int undo_push(UndoStack *stack, Command cmd)
{
    if (!stack) return 0;
    UndoEntry entry = {
        .type = cmd.type, .entity_type = cmd.entity_type,
        .entity_index = cmd.entity_index, .before = cmd.before, .after = cmd.after,
        .property_field = cmd.property_field
    };
    memcpy(entry.property_text_before, cmd.property_text_before, sizeof(entry.property_text_before));
    memcpy(entry.property_text_after, cmd.property_text_after, sizeof(entry.property_text_after));
    if (cmd.type == CMD_CONFIG) {
        entry.config = malloc(2 * sizeof(*entry.config));
        if (!entry.config) return 0; /* History remains intact. */
        entry.config[0] = cmd.config_before;
        entry.config[1] = cmd.config_after;
    }
    release_entries(stack->redo_stack, stack->redo_top);
    stack->redo_top = 0;
    append_entry(stack->commands, &stack->top, entry);
    return 1;
}

static int transfer(UndoEntry *from, int *from_count,
                    UndoEntry *to, int *to_count, Command *out)
{
    if (!out || *from_count == 0) return 0;
    UndoEntry entry = from[--*from_count];
    from[*from_count].config = NULL;
    memset(out, 0, sizeof(*out));
    out->type = entry.type;
    out->entity_type = entry.entity_type;
    out->entity_index = entry.entity_index;
    out->before = entry.before;
    out->after = entry.after;
    out->property_field = entry.property_field;
    memcpy(out->property_text_before, entry.property_text_before, sizeof(out->property_text_before));
    memcpy(out->property_text_after, entry.property_text_after, sizeof(out->property_text_after));
    if (entry.config) {
        out->config_before = entry.config[0];
        out->config_after = entry.config[1];
    }
    append_entry(to, to_count, entry);
    return 1;
}

int undo_pop(UndoStack *stack, Command *out)
{
    return stack ? transfer(stack->commands, &stack->top, stack->redo_stack, &stack->redo_top, out) : 0;
}

int redo_pop(UndoStack *stack, Command *out)
{
    return stack ? transfer(stack->redo_stack, &stack->redo_top, stack->commands, &stack->top, out) : 0;
}
