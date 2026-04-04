/*
 * undo.c --- Implementation of the command-stack undo/redo system.
 *
 * The core data structure is a pair of fixed-size arrays acting as stacks.
 * Every editor action pushes a Command onto the undo stack.  Undo pops
 * from there and pushes to the redo stack; redo does the reverse.  Any
 * new push clears the redo stack entirely (the standard UX convention).
 *
 * Memory management is simple: one malloc in undo_create, one free in
 * undo_destroy.  The Command structs are plain old data (no pointers to
 * heap memory), so there's nothing else to free.
 */

#include <stdlib.h>   /* malloc, free  --- heap allocation for UndoStack   */
#include <string.h>   /* memset, memmove --- zeroing and array shifting    */

#include "undo.h"     /* UndoStack, Command, UNDO_MAX, PlacementData      */

/* ------------------------------------------------------------------ */

/*
 * undo_create --- Allocate a new UndoStack on the heap and zero it.
 *
 * We use calloc-style initialization (memset to 0) so that both
 * stacks start empty (top == 0, redo_top == 0) and all Command
 * fields are zeroed.  This is safe because 0 is a valid default
 * for every field type (int, float, enum, pointer).
 *
 * Returns NULL if malloc fails (extremely unlikely but defensive).
 */
UndoStack *undo_create(void) {
    /*
     * malloc --- request sizeof(UndoStack) bytes from the heap.
     *
     * UndoStack is ~24 KB (two arrays of 256 Commands).  We allocate
     * it on the heap instead of the C stack because:
     *   1) Some embedded/WASM platforms have small stack limits.
     *   2) The editor owns this for its entire lifetime anyway.
     */
    UndoStack *stack = malloc(sizeof(UndoStack));
    if (!stack) return NULL;

    /*
     * memset --- fill every byte with 0.
     *
     * This zeroes top, redo_top, and every Command in both arrays.
     * It's equivalent to calling undo_clear() on a freshly allocated
     * block, but also zeroes padding bytes for good measure.
     */
    memset(stack, 0, sizeof(UndoStack));

    return stack;
}

/* ------------------------------------------------------------------ */

/*
 * undo_destroy --- Free the heap memory used by an UndoStack.
 *
 * The NULL guard follows the same convention as SDL_DestroyTexture:
 * callers can safely pass NULL without checking first.  This prevents
 * accidental double-frees and simplifies cleanup paths.
 */
void undo_destroy(UndoStack *stack) {
    if (stack) {
        /*
         * free --- return the UndoStack's memory to the heap.
         *
         * No inner pointers need freeing: Command and PlacementData
         * are plain-old-data structs (floats, ints, enums) with no
         * heap-allocated children.
         */
        free(stack);
    }
}

/* ------------------------------------------------------------------ */

/*
 * undo_push --- Append a command to the undo stack and clear redo.
 *
 * This is the heart of the system.  Every editor action (place, delete,
 * move, property change) calls this after modifying the level data.
 *
 * Three things happen:
 *   1) The redo stack is cleared --- any previously undone commands are
 *      now permanently gone, because the timeline has diverged.
 *   2) If the undo stack is full, the oldest entry (index 0) is dropped
 *      by shifting the array left.  This keeps memory bounded.
 *   3) The new command is stored at the top of the stack.
 *
 * The shift-on-overflow strategy means the user always has up to
 * UNDO_MAX - 1 recent actions available for undo, even after very
 * long editing sessions.
 */
void undo_push(UndoStack *stack, Command cmd) {
    if (!stack) return;

    /*
     * Step 1: Clear the redo stack.
     *
     * When the user performs a new action after undoing, the "alternate
     * future" of redo commands no longer makes sense.  Every undo/redo
     * system discards it here.
     */
    stack->redo_top = 0;

    /*
     * Step 2: Handle overflow --- drop the oldest command if full.
     *
     * memmove --- like memcpy, but safe when source and destination
     * regions overlap (which they do here: we're sliding the array
     * one slot to the left).
     *
     * We move (UNDO_MAX - 1) commands from index 1..254 to 0..253,
     * then decrement top so the new command lands at the last slot.
     */
    if (stack->top >= UNDO_MAX) {
        memmove(
            &stack->commands[0],            /* destination: start of array  */
            &stack->commands[1],            /* source: second element       */
            (UNDO_MAX - 1) * sizeof(Command) /* byte count: all but first  */
        );
        stack->top = UNDO_MAX - 1;
    }

    /*
     * Step 3: Store the command at the current top and advance.
     *
     * After this, stack->top points to the NEXT free slot.
     * The most-recent command is always at commands[top - 1].
     */
    stack->commands[stack->top] = cmd;
    stack->top++;
}

/* ------------------------------------------------------------------ */

/*
 * undo_pop --- Pop the most recent command and move it to the redo stack.
 *
 * The caller receives a copy of the command in *out and is responsible
 * for applying out->before to the level data (restoring the old state).
 *
 * Returns 1 if a command was available, 0 if the undo stack was empty.
 */
int undo_pop(UndoStack *stack, Command *out) {
    if (!stack || stack->top <= 0) return 0;

    /*
     * Decrement top first --- remember, top is the NEXT free slot,
     * so top-1 is the most recent command.
     */
    stack->top--;

    /*
     * Copy the command to the caller's output parameter.
     *
     * We copy by value (struct assignment) --- Command is small and
     * contains no pointers, so a shallow copy is correct and complete.
     */
    *out = stack->commands[stack->top];

    /*
     * Push the same command onto the redo stack so the user can
     * re-apply it with Ctrl+Shift+Z.
     *
     * We don't check for redo overflow here because the redo stack
     * can never exceed UNDO_MAX: it only grows by one per undo_pop,
     * and the undo stack itself is capped at UNDO_MAX entries.
     */
    if (stack->redo_top < UNDO_MAX) {
        stack->redo_stack[stack->redo_top] = *out;
        stack->redo_top++;
    }

    return 1;
}

/* ------------------------------------------------------------------ */

/*
 * redo_pop --- Pop the most recently undone command and move it back.
 *
 * The caller receives a copy of the command in *out and is responsible
 * for applying out->after to the level data (re-applying the action).
 *
 * Returns 1 if a command was available, 0 if the redo stack was empty.
 */
int redo_pop(UndoStack *stack, Command *out) {
    if (!stack || stack->redo_top <= 0) return 0;

    /*
     * Decrement redo_top --- same indexing convention as the undo stack.
     */
    stack->redo_top--;

    /*
     * Copy the command to the caller.
     */
    *out = stack->redo_stack[stack->redo_top];

    /*
     * Push back onto the undo stack so the user can undo it again.
     *
     * We go through the same overflow logic as undo_push, but we
     * must NOT clear the redo stack here --- otherwise a single redo
     * would wipe the remaining redo history.  This is the key
     * difference between redo_pop and undo_push.
     */
    if (stack->top >= UNDO_MAX) {
        memmove(
            &stack->commands[0],
            &stack->commands[1],
            (UNDO_MAX - 1) * sizeof(Command)
        );
        stack->top = UNDO_MAX - 1;
    }
    stack->commands[stack->top] = *out;
    stack->top++;

    return 1;
}

/* ------------------------------------------------------------------ */

/*
 * undo_clear --- Reset both stacks to empty without freeing memory.
 *
 * Called when the editor loads a new level file --- the old undo history
 * is meaningless for the new level, so we discard it.
 *
 * memset zeroes every byte in the struct, which resets both top counters
 * to 0 and clears all stored Command data.  This is safe because all
 * fields are plain-old-data (no heap pointers to leak).
 */
void undo_clear(UndoStack *stack) {
    if (!stack) return;

    /*
     * memset --- fill the entire UndoStack with zero bytes.
     *
     * This is slightly more work than just setting top = redo_top = 0
     * (which would be sufficient for correctness), but it ensures no
     * stale data remains in the arrays --- useful for debugging and
     * for any future code that might inspect the stack contents.
     */
    memset(stack, 0, sizeof(UndoStack));
}
