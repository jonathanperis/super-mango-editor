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

/*
 * drop_oldest --- Forget the oldest step: entry 0, and with it every
 * following entry of the same group, so no half of a group is left behind
 * to be undone on its own.
 */
static void drop_oldest(UndoEntry *entries, int *count)
{
    int group = entries[0].group;
    int drop = 1;

    while (group != 0 && drop < *count && entries[drop].group == group) drop++;
    for (int i = 0; i < drop; i++) free(entries[i].config);
    memmove(entries, entries + drop, (size_t)(*count - drop) * sizeof(*entries));
    *count -= drop;
}

static void append_entry(UndoEntry *entries, int *count, const UndoEntry *entry)
{
    if (*count == UNDO_MAX) drop_oldest(entries, count);
    entries[(*count)++] = *entry;
}

int undo_group_begin(UndoStack *stack)
{
    if (!stack) return 0;
    stack->last_group++;
    if (stack->last_group <= 0) stack->last_group = 1;  /* after a wrap */
    stack->open_group = stack->last_group;
    return stack->open_group;
}

void undo_group_end(UndoStack *stack)
{
    if (stack) stack->open_group = 0;
}

int undo_top_group(const UndoStack *stack)
{
    return stack && stack->top > 0 ? stack->commands[stack->top - 1].group : 0;
}

int redo_top_group(const UndoStack *stack)
{
    return stack && stack->redo_top > 0 ? stack->redo_stack[stack->redo_top - 1].group : 0;
}

int undo_amend_after(UndoStack *stack, int group, int entity_type,
                     int entity_index, const PlacementData *after)
{
    if (!stack || group == 0 || !after) return 0;
    /* Walk down the newest step only: its entries are on top. */
    for (int i = stack->top - 1; i >= 0 && stack->commands[i].group == group; i--) {
        UndoEntry *entry = &stack->commands[i];
        if (entry->entity_type == entity_type && entry->entity_index == entity_index) {
            entry->after = *after;
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
    if (!stack || !cmd) return 0;
    UndoEntry entry = {
        .type = cmd->type, .entity_type = cmd->entity_type,
        .entity_index = cmd->entity_index, .before = cmd->before, .after = cmd->after,
        .property_field = cmd->property_field,
        .group = cmd->group ? cmd->group : stack->open_group
    };
    memcpy(entry.property_text_before, cmd->property_text_before, sizeof(entry.property_text_before));
    memcpy(entry.property_text_after, cmd->property_text_after, sizeof(entry.property_text_after));
    if (cmd->type == CMD_CONFIG) {
        entry.config = malloc(2 * sizeof(*entry.config));
        if (!entry.config) return 0; /* History remains intact. */
        entry.config[0] = cmd->config_before;
        entry.config[1] = cmd->config_after;
    }
    release_entries(stack->redo_stack, stack->redo_top);
    stack->redo_top = 0;
    append_entry(stack->commands, &stack->top, &entry);
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
    out->group = entry.group;
    out->before = entry.before;
    out->after = entry.after;
    out->property_field = entry.property_field;
    memcpy(out->property_text_before, entry.property_text_before, sizeof(out->property_text_before));
    memcpy(out->property_text_after, entry.property_text_after, sizeof(out->property_text_after));
    if (entry.config) {
        out->config_before = entry.config[0];
        out->config_after = entry.config[1];
    }
    append_entry(to, to_count, &entry);
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
