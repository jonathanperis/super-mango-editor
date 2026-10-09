/*
 * undo.h --- Command-stack undo/redo system for the level editor.
 *
 * Implements the classic "command pattern": every editor action (place, delete,
 * move, change a property) is recorded as a Command that knows how to reverse
 * itself.  Commands are pushed onto an undo stack; when the user hits Ctrl+Z
 * the most recent command is popped and its "before" state is restored.
 * Ctrl+Shift+Z (redo) pops from the redo stack and re-applies the "after"
 * state.  Any NEW action clears the redo stack (you can't redo after making
 * a change -- that's the universal UX convention).
 *
 * The history holds at most UNDO_MAX steps.  When it is full the oldest step
 * is silently dropped -- the user loses the ability to undo that ancient
 * action but nothing else breaks.
 *
 * Groups: one user action can change several entities (moving or deleting a
 * multi-selection, pasting a group).  Each entity still gets its own entry,
 * but entries pushed between undo_group_begin and undo_group_end share a
 * group number and are stored together in ONE step, so a 64-entity move
 * costs one of the UNDO_MAX slots, not 64.  The editor undoes or redoes a
 * whole group in one go.  Group 0 means "a step of its own".
 *
 * Usage:
 *   UndoStack *undo = undo_create();
 *   undo_push(undo, &cmd);          // after every editor action
 *   if (undo_pop(undo, &cmd)) ...   // Ctrl+Z
 *   if (redo_pop(undo, &cmd)) ...   // Ctrl+Shift+Z
 *   undo_destroy(undo);             // on editor shutdown
 */
#pragma once

#include "../levels/level.h"   /* All Placement structs (CoinPlacement, etc.) */

/* ------------------------------------------------------------------ */
/* Constants                                                           */
/* ------------------------------------------------------------------ */

/*
 * UNDO_MAX --- maximum number of steps (user actions) the history keeps.
 *
 * 256 is generous for a level editor: even rapid-fire placing one entity
 * per second gives over four minutes of undo history.
 */
#define UNDO_MAX 256

/*
 * UNDO_GROUP_MAX --- most entries one step may hold.  A group never touches
 * more entities than a selection can name (editor.h checks that), and this
 * cap is what keeps the history's memory bounded: at worst UNDO_MAX steps of
 * UNDO_GROUP_MAX entries each.
 */
#define UNDO_GROUP_MAX 64

/* ------------------------------------------------------------------ */
/* CommandType --- what kind of editor action was performed             */
/* ------------------------------------------------------------------ */

/*
 * CMD_PLACE    : a new entity was added to the level.
 * CMD_DELETE   : an existing entity was removed.
 * CMD_MOVE     : an entity was dragged to a new position.
 * CMD_PROPERTY : a non-positional field was changed (e.g. patrol range).
 *
 * The undo/redo handler inspects this enum to decide how to apply or
 * reverse the command.  For example, undoing a CMD_PLACE means deleting
 * the entity; undoing a CMD_DELETE means re-inserting it.
 */
typedef enum {
    CMD_PLACE,
    CMD_DELETE,
    CMD_MOVE,
    CMD_PROPERTY,
    CMD_CONFIG
} CommandType;

/* ------------------------------------------------------------------ */
/* PlacementData --- generic storage for any entity placement struct    */
/* ------------------------------------------------------------------ */

/*
 * PlacementData --- a tagged union that can hold ANY of the game's
 * placement structs.  This is what makes the undo system entity-agnostic:
 * every command stores a "before" and "after" snapshot using this union
 * so we don't need a separate undo struct for every entity type.
 *
 * The active member is determined by the Command's entity_type field.
 * The tag identifies the meaningful member. C permits some union type-punning,
 * but interpreting a placement through another member does not preserve its
 * semantic meaning and can produce invalid values.
 *
 * Memory cost: the union is as large as its biggest member, including alignment.
 * Measure sizeof(PlacementData) rather than assuming a fixed byte count.
 */
typedef union {
    CoinPlacement           coin;
    StarYellowPlacement     star_yellow;
    StarGreenPlacement      star_green;
    StarRedPlacement        star_red;
    LastStarPlacement       last_star;
    SpiderPlacement         spider;
    JumpingSpiderPlacement  jumping_spider;
    BirdPlacement           bird;
    FishPlacement           fish;
    AxeTrapPlacement        axe_trap;
    CircularSawPlacement    circular_saw;
    SpikeRowPlacement       spike_row;
    SpikePlatformPlacement  spike_platform;
    SpikeBlockPlacement     spike_block;
    BlueFlamePlacement      blue_flame;
    FireFlamePlacement      fire_flame;
    FloatPlatformPlacement  float_platform;
    BridgePlacement         bridge;
    BouncepadPlacement      bouncepad;
    PlatformPlacement       platform;
    VinePlacement           vine;
    LadderPlacement         ladder;
    RopePlacement           rope;
    RailPlacement           rail;
    int                     floor_gap;
    CheckpointPlacement     checkpoint;
} PlacementData;

/*
 * LevelConfigSnapshot — focused snapshot for level-wide editor settings.
 * Entity arrays intentionally excluded: config undo must not restore
 * unrelated entity edits.
 */
typedef struct {
    char name[LEVEL_NAME_CAPACITY];
    char description[LEVEL_DESCRIPTION_CAPACITY];
    char generated_by[LEVEL_AUTHOR_CAPACITY];
    int screen_count;
    char next_phase[256];

    struct {
        char path[64];
        float speed;
    } background_layers[MAX_BACKGROUND_LAYERS];
    int background_layer_count;

    struct {
        char path[64];
        float speed;
    } foreground_layers[MAX_BACKGROUND_LAYERS];
    int foreground_layer_count;

    struct {
        char path[64];
        float speed;
    } fog_layers[MAX_FOG_TEXTURES];
    int fog_layer_count;

    char music_path[64];
    int music_volume;
    char floor_tile_path[64];
    int initial_hearts;
    int initial_lives;
    int score_per_life;
    int coin_score;

    struct {
        float walk_max_speed;
        float run_max_speed;
        float walk_ground_accel;
        float run_ground_accel;
        float ground_friction;
        float ground_counter_accel;
        float air_accel_walk;
        float air_accel_run;
        float air_friction;
        float cam_lookahead_vx_factor;
        float cam_lookahead_max;
    } physics;
} LevelConfigSnapshot;

/* ------------------------------------------------------------------ */
/* Command --- one recorded editor action                              */
/* ------------------------------------------------------------------ */

/*
 * Command --- captures everything needed to undo or redo a single action.
 *
 * type         : what happened (place / delete / move / property change).
 * entity_type  : which kind of entity this command affects; selects which
 *                member of the PlacementData union to read.
 * entity_index : index into the corresponding array in LevelDef.  For
 *                CMD_PLACE this is where the entity was inserted; for
 *                CMD_DELETE it's where the entity was before removal.
 * before       : the entity's state BEFORE the action (used by undo).
 * after        : the entity's state AFTER  the action (used by redo).
 *
 * For CMD_PLACE:  "before" is unused (nothing existed); "after" holds
 *                 the newly placed entity's data.
 * For CMD_DELETE: "before" holds the deleted entity's data; "after" unused.
 * For CMD_MOVE / CMD_PROPERTY: both "before" and "after" are meaningful.
 */
typedef struct {
    CommandType   type;
    int           entity_type;    /* EntityType enum value from the editor */
    int           entity_index;
    int           group;          /* 0, or the step this entry belongs to */
    PlacementData before;
    PlacementData after;
    int           property_field;
    char          property_text_before[256];
    char          property_text_after[256];
    LevelConfigSnapshot config_before;
    LevelConfigSnapshot config_after;
} Command;

/* ------------------------------------------------------------------ */
/* UndoStack --- the paired undo + redo stacks                         */
/* ------------------------------------------------------------------ */

/*
 * UndoEntry --- how the history stores one Command.
 *
 * Transfer Commands contain inline config snapshots for simple caller
 * ownership.  Stored entries allocate those snapshots only for CMD_CONFIG;
 * ordinary entity actions use compact inline storage.  A config pair is owned
 * by the entry that recorded it and freed when that entry is forgotten.
 */
typedef struct {
    CommandType type;
    int entity_type, entity_index;
    PlacementData before, after;
    int property_field;
    char property_text_before[256], property_text_after[256];
    LevelConfigSnapshot *config; /* owned pair: before, after; NULL for entities */
} UndoEntry;

/*
 * UndoStep --- one user action: a lone entry, or every entry of a group.
 *
 * A lone entry lives inline in `one`, so the common single-entity edit never
 * allocates.  A group's entries live in the heap array `many` (oldest
 * first), reserved by undo_group_begin before the action changes anything.
 *
 * applied : how many of the entries are in effect in the level right now.
 *           Undo lowers it one entry at a time, redo raises it again.
 */
typedef struct {
    int        group;     /* 0, or the group number of the action */
    int        count;     /* entries recorded in this step         */
    int        applied;   /* entries currently applied (<= count)  */
    int        capacity;  /* entries `many` has room for            */
    UndoEntry  one;       /* storage while `many` is NULL           */
    UndoEntry *many;      /* heap storage for a group, or NULL      */
} UndoStep;

/*
 * UndoStack --- the undo and redo stacks, sharing one timeline of steps.
 *
 * steps[0 .. count) are every recorded step, oldest first.  The applied
 * entries form the undo stack (newest at the end) and the undone ones the
 * redo stack (the one undone last comes back first).  Undo and redo only
 * move a step's `applied` count -- entries never change place -- so moving
 * history between the two stacks needs no memory and cannot fail.
 *
 * top and redo_top count the steps with something to undo and to redo.
 * They are kept up to date for callers to read; only undo.c writes them.
 */
typedef struct UndoStack {
    UndoStep steps[UNDO_MAX];
    int      count;          /* steps recorded                               */
    int      top;            /* steps that can be undone                     */
    int      redo_top;       /* steps that can be redone                     */
    int      open_group;     /* group stamped on pushes, 0 when none is open */
    int      last_group;     /* the last group number handed out             */
    UndoEntry *reserve;      /* room undo_group_begin set aside, or NULL     */
    int      reserve_capacity;
} UndoStack;

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

/*
 * undo_create --- Allocate and zero-initialise an UndoStack.
 *
 * Returns a heap-allocated UndoStack with both stacks empty.
 * The caller must eventually call undo_destroy() to free it.
 */
UndoStack *undo_create(void);

/*
 * undo_destroy --- Free an UndoStack previously created by undo_create.
 *
 * Safe to call with NULL (no-op), following the same convention as
 * texture_unload(NULL).
 */
void undo_destroy(UndoStack *stack);

/*
 * undo_push --- Record a new editor action on the undo stack.
 *
 * Appends cmd to the top of the undo stack and clears the redo stack
 * (because a new action invalidates any previously undone commands).
 * A command whose group (its own, or the open group's) matches the newest
 * step joins that step; any other command starts a new step.
 *
 * If the history is full (UNDO_MAX steps), the oldest step is discarded
 * whole by shifting the array left by one slot.  This keeps memory bounded
 * while preserving the most recent history.
 */
/* Returns 0 without changing history on allocation failure. The caller must
 * roll back a config edit if its snapshot cannot be recorded.  A lone entity
 * edit never allocates, and neither does a group push within the room
 * undo_group_begin reserved. */
int undo_push(UndoStack *stack, const Command *cmd);

/*
 * undo_pop --- Pop the most recent command from the undo stack.
 *
 * On success: writes the newest applied entry into *out, moves it to the
 * redo stack, and returns 1.  A group step comes back one entry at a time,
 * newest first; undo_top_group says whether more of it is left.  The caller should then apply `out->before` to the
 * level to reverse the action.
 *
 * On failure (empty stack): returns 0 and leaves *out untouched.
 */
int undo_pop(UndoStack *stack, Command *out);

/*
 * redo_pop --- Pop the most recently undone command from the redo stack.
 *
 * On success: writes the oldest undone entry into *out, moves it back to
 * the undo stack, and returns 1.  A group comes back oldest entry first.  The caller should then apply `out->after` to
 * the level to re-apply the action.
 *
 * On failure (empty stack): returns 0 and leaves *out untouched.
 */
int redo_pop(UndoStack *stack, Command *out);

/*
 * undo_group_begin / undo_group_end --- Bracket the pushes of one action
 * that touches several entities.  Every command pushed in between (whose
 * own group is 0) gets the same new group number, which begin returns, and
 * lands in the same step.
 *
 * `size` is how many entries the action may push (at most UNDO_GROUP_MAX).
 * Begin sets that much room aside, so those pushes cannot fail for lack of
 * memory.  Begin returns 0 when it cannot: call it BEFORE changing the
 * level, and refuse the action on 0 so no unrecorded edit is left behind.
 */
int undo_group_begin(UndoStack *stack, int size);
void undo_group_end(UndoStack *stack);

/* Group of the entry undo_pop / redo_pop would return next (0 when that
 * entry stands alone or the stack is empty). */
int undo_top_group(const UndoStack *stack);
int redo_top_group(const UndoStack *stack);

/*
 * undo_amend_after --- Replace the "after" snapshot of the entry for
 * (entity_type, entity_index) inside `group`, which must be the newest
 * step on the undo stack.  Used to fold a quick run of arrow-key nudges
 * into one step.  Returns 1 when such an entry was found.
 */
int undo_amend_after(UndoStack *stack, int group, int entity_type,
                     int entity_index, const PlacementData *after);

/*
 * undo_take --- Remove the newest entry from the undo stack WITHOUT moving
 * it to the redo stack (it is forgotten).  Used to roll back a group action
 * that failed half way, which must leave nothing behind to redo.
 */
int undo_take(UndoStack *stack, Command *out);

/*
 * undo_clear --- Reset both stacks to empty.
 *
 * Called when loading a new level or starting a fresh editing session.
 * Does not free the UndoStack itself --- use undo_destroy() for that.
 */
void undo_clear(UndoStack *stack);

#ifdef MANGO_TESTING
/* Test seam (test builds only): let the next `count` history allocations
 * succeed and fail every one after that; a negative count lifts the limit.
 * Lets tests prove an edit that cannot be recorded changes nothing. */
void undo_test_limit_allocations(int count);
#endif
