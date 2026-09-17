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
 * The stacks are fixed-size arrays (UNDO_MAX entries).  When the undo stack
 * is full the oldest command is silently dropped -- the user loses the ability
 * to undo that ancient action but nothing else breaks.
 *
 * Usage:
 *   UndoStack *undo = undo_create();
 *   undo_push(undo, cmd);           // after every editor action
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
 * UNDO_MAX --- maximum number of commands stored in each stack.
 *
 * 256 is generous for a level editor: even rapid-fire placing one entity
 * per second gives over four minutes of undo history.
 */
#define UNDO_MAX 256

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
 * UndoStack --- holds two fixed-size arrays of Commands.
 *
 * commands[]  + top      : the undo stack (most-recent action at top-1).
 * redo_stack[] + redo_top : the redo stack (most-recently undone at redo_top-1).
 *
 * "top" and "redo_top" act like stack pointers: they index the NEXT free
 * slot, so the topmost element is at [top - 1].  When top == 0 the stack
 * is empty.
 *
 * Transfer Commands contain inline config snapshots for simple caller ownership.
 * Stored entries allocate those snapshots only for CMD_CONFIG; ordinary entity
 * actions use compact inline storage. Each config pair has exactly one owner as
 * entries move between stacks. The total number of entries stays bounded.
 */
typedef struct {
    CommandType type;
    int entity_type, entity_index;
    PlacementData before, after;
    int property_field;
    char property_text_before[256], property_text_after[256];
    LevelConfigSnapshot *config; /* owned pair: before, after; NULL for entities */
} UndoEntry;

typedef struct UndoStack {
    UndoEntry commands[UNDO_MAX];
    int     top;
    UndoEntry redo_stack[UNDO_MAX];
    int     redo_top;
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
 * SDL_DestroyTexture(NULL).
 */
void undo_destroy(UndoStack *stack);

/*
 * undo_push --- Record a new editor action on the undo stack.
 *
 * Appends cmd to the top of the undo stack and clears the redo stack
 * (because a new action invalidates any previously undone commands).
 *
 * If the stack is full (top == UNDO_MAX), the oldest command at index 0
 * is discarded by shifting the entire array left by one slot.  This
 * keeps memory bounded while preserving the most recent history.
 */
/* Returns 0 without changing history on allocation failure. The caller must
 * roll back a config edit if its snapshot cannot be recorded. */
int undo_push(UndoStack *stack, Command cmd);

/*
 * undo_pop --- Pop the most recent command from the undo stack.
 *
 * On success: writes the command into *out, moves it to the redo stack,
 * and returns 1.  The caller should then apply `out->before` to the
 * level to reverse the action.
 *
 * On failure (empty stack): returns 0 and leaves *out untouched.
 */
int undo_pop(UndoStack *stack, Command *out);

/*
 * redo_pop --- Pop the most recently undone command from the redo stack.
 *
 * On success: writes the command into *out, moves it back to the undo
 * stack, and returns 1.  The caller should then apply `out->after` to
 * the level to re-apply the action.
 *
 * On failure (empty stack): returns 0 and leaves *out untouched.
 */
int redo_pop(UndoStack *stack, Command *out);

/*
 * undo_clear --- Reset both stacks to empty.
 *
 * Called when loading a new level or starting a fresh editing session.
 * Does not free the UndoStack itself --- use undo_destroy() for that.
 */
void undo_clear(UndoStack *stack);
