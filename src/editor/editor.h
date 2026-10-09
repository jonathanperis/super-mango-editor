/*
 * editor.h — Central header for the Super Mango level editor.
 *
 * The level editor is a standalone executable that shares data types with the
 * game (LevelDef, placement structs) but has its own window, main loop, and
 * raylib render target. It lets designers visually place entities on a scrollable
 * canvas, then save / load LevelDef files that the game engine can consume.
 *
 * This header defines:
 *   - EntityType enum — one value per placeable entity kind.
 *   - EditorTool enum — the current mouse interaction mode.
 *   - Selection, EntityTextures, EditorCamera structs.
 *   - EditorState — the single container for all editor resources.
 *   - Constants for the editor window layout.
 *   - Function declarations for the editor lifecycle.
 *
 * Include this header in every editor .c file.
 */

/*
 * #pragma once is a non-standard but universally supported header guard.
 * It tells the compiler: "include this file only once per translation unit,
 * even if #included multiple times".  Prevents duplicate-definition errors.
 */
#pragma once

#include "../shared/text.h"
#include "../shared/platform.h"
#include "../input/input_backend.h"
#include <stdint.h>        /* uint64_t — document save-point fingerprint */
#include "../levels/level.h" /* LevelDef — the data-driven level definition we edit */
#include "../shared/ui.h"  /* UIState — shared immediate-mode widgets */
#include "undo.h"          /* UndoStack, PlacementData — undo system + clipboard */
#include "editor_validation.h" /* EditorValidationReport                         */
#include "../shared/serializer_io.h" /* SerializerFileFingerprint */

/* ------------------------------------------------------------------ */
/* Constants — editor window layout                                    */
/* ------------------------------------------------------------------ */

/*
 * EDITOR_W / EDITOR_H — the editor window size in pixels.
 *
 * The editor needs more screen real estate than the game (800x600) because
 * it displays both the level canvas and a side panel with the entity palette
 * and property inspector.  1280x720 is a common 16:9 resolution that fits
 * comfortably on most modern displays.
 */
#define EDITOR_W     1280
#define EDITOR_H      720

/*
 * CANVAS_W — width of the level-preview area on the left side of the window.
 * PANEL_W  — width of the entity palette / inspector panel on the right.
 *
 * These two must sum to EDITOR_W so they tile the window horizontally
 * with no gap:  896 + 384 = 1280.
 *
 * The canvas displays the scrollable level at zoom; the panel shows the
 * entity palette, selection properties, and file operations.
 */
#define CANVAS_W      896
#define PANEL_W       384

/*
 * TOOLBAR_H — height of the top toolbar strip (tool selector, file buttons).
 * STATUS_H  — height of the bottom status bar (cursor coords, zoom level).
 * CANVAS_H  — remaining vertical space for the level canvas.
 *
 * The vertical layout stacks as:
 *   [    toolbar   ]  TOOLBAR_H (32 px)
 *   [              ]
 *   [    canvas    ]  CANVAS_H  (656 px)
 *   [              ]
 *   [  status bar  ]  STATUS_H  (32 px)
 *                     ──────────
 *                      720 px total
 */
#define TOOLBAR_H      32
#define STATUS_H       32
#define CANVAS_H      (EDITOR_H - TOOLBAR_H - STATUS_H)
#define EDITOR_PATH_MAX 1024
#define EDITOR_MAX_RECOVERY_ENTRIES 32
/*
 * Most entities one selection (and so one group move, delete, copy or
 * duplicate) can hold.  Every one costs a placement snapshot in the drag
 * and the clipboard, which live inside EditorState, so the limit keeps the
 * struct small enough to sit on a stack.
 */
#define EDITOR_MAX_SELECTION 64

typedef struct {
    uint64_t id;
    uint64_t timestamp;
    unsigned long owner_pid;  /* process that wrote it; 0 = unknown (old file) */
    char source_path[EDITOR_PATH_MAX];
    char snapshot_path[EDITOR_PATH_MAX];
    char metadata_path[EDITOR_PATH_MAX];
} EditorRecoveryEntry;

typedef enum {
    EDITOR_SOURCE_UNKNOWN = 0,
    EDITOR_SOURCE_EXPECTED_MISSING,
    EDITOR_SOURCE_EXPECTED_EXISTING
} EditorSourceState;

/* ------------------------------------------------------------------ */
/* EntityType — identifies every kind of placeable level entity         */
/* ------------------------------------------------------------------ */

/*
 * EntityType — one value per entity kind that can appear in a level.
 *
 * The palette UI iterates from 0 to ENT_COUNT-1 to display all available
 * entity types.  The numeric values also serve as indices into lookup
 * tables for entity names, thumbnail textures, and default sizes.
 *
 * ENT_COUNT is not a real entity — it is a sentinel that equals the total
 * number of entity types.  This is a common C pattern: placing a _COUNT
 * value at the end of an enum gives you the array size automatically,
 * and it updates itself whenever you add new values above it.
 */
typedef enum {
    ENT_FLOOR_GAP = 0,     /* hole in the ground floor (exposes water)      */
    ENT_CHECKPOINT,        /* authored respawn marker                       */
    ENT_RAIL,              /* rail path (spike blocks / platforms ride on)  */
    ENT_PLATFORM,          /* ground pillar (static collision surface)      */
    ENT_COIN,              /* score pickup; score thresholds grant lives */
    ENT_STAR_YELLOW,       /* health-restoring star pickup                  */
    ENT_STAR_GREEN,        /* green health-restoring star pickup             */
    ENT_STAR_RED,          /* red health-restoring star pickup               */
    ENT_LAST_STAR,         /* end-of-level star (triggers level complete)   */
    ENT_SPIDER,            /* ground-patrol enemy (walks back and forth)    */
    ENT_JUMPING_SPIDER,    /* spider variant that leaps across sea gaps     */
    ENT_BIRD,              /* slow sine-wave sky patrol enemy               */
    ENT_FASTER_BIRD,       /* fast sine-wave sky patrol enemy               */
    ENT_FISH,              /* slow jumping water-lane enemy                 */
    ENT_FASTER_FISH,       /* fast jumping water-lane enemy                 */
    ENT_AXE_TRAP,          /* swinging / spinning axe hazard                */
    ENT_CIRCULAR_SAW,      /* fast rotating patrol hazard                   */
    ENT_SPIKE_ROW,         /* static ground spike strip                     */
    ENT_SPIKE_PLATFORM,    /* elevated spike hazard surface                 */
    ENT_SPIKE_BLOCK,       /* rail-riding spike block                       */
    ENT_BLUE_FLAME,        /* erupting fire hazard from sea gaps            */
    ENT_FIRE_FLAME,        /* erupting fire hazard (fire variant)           */
    ENT_FLOAT_PLATFORM,    /* hovering / crumble / rail platform            */
    ENT_BRIDGE,            /* tiled crumble walkway                         */
    ENT_BOUNCEPAD_SMALL,   /* green bouncepad (small jump)                  */
    ENT_BOUNCEPAD_MEDIUM,  /* wood bouncepad (medium jump)                  */
    ENT_BOUNCEPAD_HIGH,    /* red bouncepad (high jump)                     */
    ENT_VINE,              /* hanging climbable vine                        */
    ENT_LADDER,            /* climbable ladder                              */
    ENT_ROPE,              /* climbable rope                                */
    ENT_PLAYER_SPAWN,      /* player start position (single, always one)    */
    ENT_COUNT              /* sentinel — total number of entity types       */
} EntityType;

/* ------------------------------------------------------------------ */
/* EditorTool — the current mouse interaction mode                     */
/* ------------------------------------------------------------------ */

/*
 * EditorTool — determines what happens when the user clicks on the canvas.
 *
 * TOOL_SELECT : click an existing entity to select and inspect it.
 * TOOL_PLACE  : click empty space to stamp a new entity of the palette type.
 * TOOL_DELETE : click an existing entity to remove it from the level.
 *
 * The active tool is shown highlighted in the top toolbar.
 */
typedef enum {
    TOOL_SELECT = 0,
    TOOL_PLACE,
    TOOL_DELETE
} EditorTool;

/* ------------------------------------------------------------------ */
/* Selection — which entity (if any) is currently selected             */
/* ------------------------------------------------------------------ */

/*
 * Selection — tracks the currently selected entity in the level.
 *
 * type  : which EntityType array the selected entity belongs to.
 * index : position within that array, or -1 when nothing is selected.
 *
 * The inspector panel reads this to display editable properties.
 * Setting index to -1 is the "no selection" sentinel; every operation
 * that reads the selection must check for -1 first.
 */
typedef struct {
    EntityType type;
    int        index;  /* -1 = nothing selected */
} Selection;

/*
 * EditorClipboardItem — one copied entity (Ctrl+C copies the whole
 * selection, so the clipboard holds a list of these).
 *
 * A rail rider stores only a rail *index*, which goes stale when rails
 * are deleted or the copy is pasted into another level.  rail_index
 * follows the copied rider's rail in this document: removing or
 * re-inserting an earlier rail renumbers it, moving or resizing the rail
 * leaves it alone, and deleting that rail (or opening another document)
 * sets it to -1.  Paste uses it while it is valid.  rail keeps the rail's
 * shape for a paste into another level, where the rider attaches to a rail
 * with the same shape and position; has_rail drops to 0 once the rail is
 * deleted here, so the copy cannot attach to a look-alike rail instead.
 */
typedef struct {
    EntityType    type;
    PlacementData data;
    int           rail_index;  /* a rider's rail; a copied rail's own index */
    int           has_rail;
    RailPlacement rail;
    int           rail_item;   /* clipboard slot of the rail it rides when
                                  that rail was copied too, else -1 */
} EditorClipboardItem;

/* ------------------------------------------------------------------ */
/* EntityTextures — preloaded GPU textures for all entity thumbnails   */
/* ------------------------------------------------------------------ */

/*
 * EntityTextures — one owned raylib texture slot per visual entity type.
 *
 * Loaded once at editor startup and shared across the palette panel,
 * canvas entity rendering, and tooltip previews.  Each texture is the
 * same sprite sheet used by the game engine, so WYSIWYG: what designers
 * see in the editor is exactly what appears in-game.
 *
 * texture_load creates a GPU texture, released by texture_unload before
 * closing the graphics context.
 * All pointers are NULL until editor_init runs; cleanup sets them back to NULL.
 */
typedef struct {
    Texture2D *sky;
    Texture2D *floor_tile;
    Texture2D *water;
    Texture2D *platform;
    Texture2D *platform_stone;
    Texture2D *platform_leaf;
    Texture2D *spider;
    Texture2D *jumping_spider;
    Texture2D *bird;
    Texture2D *faster_bird;
    Texture2D *fish;
    Texture2D *faster_fish;
    Texture2D *coin;
    Texture2D *star_yellow;
    Texture2D *star_green;
    Texture2D *star_red;
    Texture2D *last_star;
    Texture2D *axe_trap;
    Texture2D *circular_saw;
    Texture2D *blue_flame;
    Texture2D *fire_flame;
    Texture2D *spike;
    Texture2D *spike_platform;
    Texture2D *spike_block;
    Texture2D *float_platform;
    Texture2D *bridge;
    Texture2D *bouncepad_small;
    Texture2D *bouncepad_medium;
    Texture2D *bouncepad_high;
    Texture2D *vine_green;
    Texture2D *vine_brown;
    Texture2D *ladder;
    Texture2D *rope;
    Texture2D *rail;
    Texture2D *player;
} EntityTextures;

/* ------------------------------------------------------------------ */
/* EditorCamera — viewport position and zoom within the level canvas   */
/* ------------------------------------------------------------------ */

/*
 * EditorCamera — controls what portion of the level is visible on the canvas.
 *
 * x    : horizontal scroll offset in world-space logical pixels.
 *         x = 0 shows the left edge of the level; increasing x pans right.
 * y    : vertical scroll offset in world-space logical pixels.  At 1x and
 *         2x the 300-px-tall world fits in the canvas and y stays 0; at 3x
 *         and 5x it does not, and y lets the designer pan down to the floor.
 * zoom : scale factor for rendering (1.0 = native logical size, 2.0 = 2x).
 *         The canvas maps world coordinates to screen coordinates via:
 *           screen_x = (world_x - camera.x) * zoom
 *           screen_y = (world_y - camera.y) * zoom + TOOLBAR_H
 *
 * All fields use float because the camera can pan smoothly at sub-pixel
 * granularity (same reason the game uses float for entity positions).
 * canvas_clamp_camera keeps x and y inside the world.
 */
typedef struct {
    float x;     /* horizontal scroll in world-space logical pixels */
    float y;     /* vertical scroll in world-space logical pixels   */
    float zoom;  /* render scale factor (1.0 = 1:1, 2.0 = double size) */
} EditorCamera;

/* ------------------------------------------------------------------ */
/* EditorState — the single source of truth for the entire editor      */
/* ------------------------------------------------------------------ */

/*
 * EditorState — all data the editor owns, passed by pointer everywhere.
 *
 * Mirrors the game's design pattern: one struct holds the window, renderer,
 * fonts, textures, level data, and UI state.  Every editor function takes
 * a pointer to EditorState so it can read and modify shared state without
 * global variables.
 *
 * In C, passing a struct by pointer (EditorState *es) lets the called
 * function modify the original struct in place.  If we passed by value,
 * the function would receive a copy and changes would be lost.
 */
typedef struct {
    /* Editor owns its process-wide context and logical render target. */
    RenderTexture2D frame_target;
    TextFont *font;

    /* ---- Level data (the document being edited) ---------------------- */
    /*
     * level — a mutable copy of a LevelDef that the editor modifies.
     * When saving, this struct is serialised to disk.  When loading, the
     * file contents are deserialised into this struct.  Stored by value
     * (not pointer) so the editor owns the data and can freely mutate it.
     */
    LevelDef      level;

    /* ---- Preloaded textures for entity rendering --------------------- */
    EntityTextures textures;

    /*
     * Path each level-config preview texture (sky, floor, water) was last
     * loaded from; "" until a level's path has been loaded.  A config edit
     * such as renaming the level then reloads only textures whose path
     * actually changed.  64 bytes matches the LevelDef path fields.
     */
    char           preview_sky_path[64];
    char           preview_floor_path[64];
    char           preview_water_path[64];

    /* ---- Viewport ---------------------------------------------------- */
    EditorCamera   camera;

    /* ---- Tool and selection state ------------------------------------- */
    EditorTool     tool;          /* active interaction mode (select/place/delete)  */
    /*
     * The selection.  `selection` is the primary entity: the one the
     * properties panel shows and a drag grabbed (-1 = nothing selected).
     * selection_more lists the other selected entities of a multi-selection
     * (Shift+click, box select).  Code that changes the selection goes
     * through the editor_select_* helpers in entity_meta.c, which keep the
     * two consistent; index shifts after inserts and removals update both.
     */
    Selection      selection;
    Selection      selection_more[EDITOR_MAX_SELECTION - 1];
    int            selection_more_count;
    EntityType     palette_type;  /* entity type chosen in the palette for placing  */

    /* ---- Undo / redo ------------------------------------------------- */
    /*
     * undo — pointer to a heap-allocated UndoStack (defined in undo.h).
     * Using a pointer here instead of embedding by value keeps EditorState
     * small and allows undo.c to manage its own memory layout.  The editor
     * allocates this at init and frees it at cleanup.
     */
    UndoStack     *undo;

    /* ---- Clipboard (copy/paste) ---------------------------------------- */
    /*
     * clipboard — snapshots of the entities the last Ctrl+C copied (0 = the
     * clipboard is empty).  Ctrl+V adds a copy of each, offset from the
     * originals; see EditorClipboardItem for how rail riders keep their rail.
     */
    EditorClipboardItem clipboard[EDITOR_MAX_SELECTION];
    int            clipboard_count;

    /* ---- File I/O ----------------------------------------------------- */
    /*
     * file_path — the path to the currently open level file.
     * A fixed 1024-byte buffer avoids heap allocation for a simple file path.
     * An empty string (file_path[0] == '\0') means "untitled, never saved".
     */
    char           file_path[EDITOR_PATH_MAX];
    /*
     * file_target — the real file read when file_path was opened.  It
     * differs from file_path only when file_path is a symlink.  Save follows
     * that link only while it still points here: the user loaded this file,
     * so updating it is expected; any other target is a file they never saw.
     * Empty when the document did not come from reading file_path.
     */
    char           file_target[EDITOR_PATH_MAX];
    char           autosave_path[EDITOR_PATH_MAX];
    char           playtest_path[EDITOR_PATH_MAX];
    char           recovery_original_path[EDITOR_PATH_MAX];
    char           preference_root[EDITOR_PATH_MAX];
    char           recovery_root_path[EDITOR_PATH_MAX];
    uint64_t       recovery_document_id;
    uint64_t       pending_recovery_id;
    EditorRecoveryEntry recovery_entries[EDITOR_MAX_RECOVERY_ENTRIES];
    int            recovery_entry_count;
    char           recent_path[EDITOR_PATH_MAX];
    char           recent_files[5][EDITOR_PATH_MAX];
    int            recent_file_count;
    char           status_message[160];
    uint32_t       status_set_ms;    /* clock_millis() when status_message was set */
    uint32_t       last_autosave_ms; /* last autosave *attempt*, success or not   */

    /*
     * The most recent version of this document that passed validation.
     * Recovery snapshots must be loadable, and loading validates, so while
     * the live level has errors autosave writes this copy instead.  The id
     * ties it to one document: a copy from a previously open file is never
     * written into another file's recovery slot.
     */
    LevelDef       last_valid_level;
    uint64_t       last_valid_document_id;
    int            last_valid_level_set;

    /* Save-point tracking.  Hash covers document content, not editor UI. */
    uint64_t       saved_document_hash;
    int            saved_document_hash_valid;
    SerializerFileFingerprint source_fingerprint;
    EditorSourceState source_state;

    /*
     * modified — dirty flag, set to 1 whenever the level changes.
     * Cleared to 0 on save.  The title bar shows an asterisk (*) and the
     * quit handler prompts to save when modified is true.
     */
    int            modified;

    /* One UI commit in flight; owned by editor_undo_apply.c. */
    int            change_tracking_kind;
    int            pending_change_valid;
    int            pending_widget_id;
    int            pending_entity_type;
    int            pending_entity_index;
    PlacementData  pending_entity_before;
    char           pending_text_before[256];
    LevelConfigSnapshot pending_config_before;

    EditorValidationReport validation_report;
    EditorLoadReport load_report;   /* why the last Open failed */
    uint64_t validated_document_hash;
    uint32_t last_validation_ms;
    int validation_cache_valid;

    /* ---- UI toggles --------------------------------------------------- */
    int            show_grid;     /* 1 = draw grid lines on the canvas         */
    /* 1 = placing and dragging snap to the TILE_SIZE grid (key S).  Holding
     * Shift inverts it for one click or drag.  input_mods are the modifier
     * keys of the latest mouse event, which tools and the ghost read. */
    int            snap_to_grid;
    int            input_mods;
    int            running;       /* 1 = main loop active, 0 = exit requested  */
    int            panel_scroll;  /* scroll offset (px) for the right panel    */
    /*
     * Trackpads report the wheel in fractions of a notch.  These keep the
     * part that has not yet added up to a whole step: panel pixels for the
     * side panels, notches for Ctrl+wheel zoom.
     */
    float          panel_wheel_accum;
    float          zoom_wheel_accum;
    int            panel_open;    /* 1 = properties panel expanded, 0 = collapsed  */
    int            config_open;   /* 1 = level config panel expanded, 0 = collapsed */
    int            palette_open;  /* 1 = palette panel expanded, 0 = collapsed      */
    int            debug_play;   /* 1 = launch game with --debug when playing      */

    /* ---- Mouse state (updated every frame from input commands) -------- */
    int            mouse_x;       /* current cursor x in window pixels         */
    int            mouse_y;       /* current cursor y in window pixels         */
    int            mouse_down;    /* 1 while left mouse button is held         */
    int            mouse_right_down; /* 1 while right mouse button is held     */

    /* ---- Drag state (moving one entity with the Select tool) ---------- */
    /*
     * A drag starts at mouse-down on an entity.  The entity is remembered by
     * (drag_type, drag_index) and its original placement is kept in
     * drag_before, so every motion event recomputes the position from the
     * original (no drift) and mouse-up can record an exact undo "before".
     *
     * drag_grab_x/y is the cursor's offset from the entity's top-left corner
     * at mouse-down, so the entity does not jump to put its corner under
     * the cursor.  drag_moved stays 0 until the cursor travels a few pixels,
     * so a plain click selects without changing anything.
     */
    int            dragging;      /* 1 while a drag operation is in progress   */
    int            drag_moved;    /* 1 once the cursor passed the threshold    */
    EntityType     drag_type;     /* entity grabbed (its corner snaps)          */
    int            drag_index;
    /* Everything the drag moves (the grabbed entity alone, or the whole
     * multi-selection) and each one's placement at mouse-down. */
    int            drag_count;
    Selection      drag_items[EDITOR_MAX_SELECTION];
    PlacementData  drag_befores[EDITOR_MAX_SELECTION];
    float          drag_start_x;  /* entity top-left (world) at mouse-down     */
    float          drag_start_y;
    float          drag_grab_x;   /* cursor minus entity top-left at mouse-down */
    float          drag_grab_y;
    float          drag_mouse_x;  /* cursor world position at mouse-down       */
    float          drag_mouse_y;
    /*
     * Where the last Select click landed (world px) when it selected
     * something without moving it.  A second click on that same spot steps
     * to the next entity underneath, like Alt+click does.
     */
    int            last_click_valid;
    float          last_click_x;
    float          last_click_y;

    /* ---- Box (rubber-band) selection ---------------------------------- */
    /*
     * A Select-tool press on empty canvas starts a box from (box_x0, box_y0)
     * to the cursor (box_x1, box_y1), in world px.  On release every entity
     * the box touches is selected; with Shift held they are added to the
     * current selection (box_additive).
     */
    int            box_selecting;
    int            box_additive;
    float          box_x0, box_y0, box_x1, box_y1;

    /*
     * Arrow-key nudges.  A quick run of nudges of the same selection is one
     * undo step: nudge_group is the undo group of the last nudge, nudge_ms
     * when it happened, and nudge_selection_key which entities it moved.
     */
    int            nudge_group;
    uint32_t       nudge_ms;
    uint64_t       nudge_selection_key;

    /* ---- Play-test state ---------------------------------------------- */
    /*
     * playing — 1 while the game is running as a child process.
     * play_pid — PID of the game process (POSIX) for kill/waitpid.
     *
     * When playing == 1 the editor hides its window and shows only a
     * minimal "Stop" overlay.  The game runs in its own window.  When
     * the user clicks Stop or the game exits, playing returns to 0 and
     * the editor window is restored.
     */
    int            playing;
#ifndef _WIN32
    int            play_pid;       /* pid_t stored as int for portability */
#else
    intptr_t       play_process;   /* process handle while playtest runs */
#endif

    /* ---- Immediate-mode UI state ------------------------------------- */
    /*
     * ui — per-frame input snapshot and retained state for the IMGUI widgets.
     *
     * Stores the renderer/font pointers (set once at init), per-frame input
     * (mouse position, clicks, key events, text input), and retained state
     * for active text fields and open dropdowns.  Every widget function
     * reads from this struct to decide what to draw and whether the user
     * interacted with it.
     */
    UIState        ui;
} EditorState;

/* ------------------------------------------------------------------ */
/* Editor lifecycle functions                                           */
/* ------------------------------------------------------------------ */

/*
 * editor_init — Create the editor window, renderer, and load all textures.
 *
 * Called once at startup.  Initialises every field in EditorState, opens
 * a raylib window at EDITOR_W x EDITOR_H, loads entity textures from the
 * shared assets/ directory, and opens the default font.
 *
 * es : pointer to the EditorState to initialise (caller allocates).
 *
 * Returns 0 on success, non-zero on fatal error.
 */
int editor_init(EditorState *es, int hidden);

int editor_load_level(EditorState *es, const char *path);

/*
 * editor_loop — Run the editor's main event/render loop until exit.
 *
 * Polls input commands (mouse, keyboard, window), updates tool interactions,
 * redraws the canvas and panel, and handles file I/O commands.
 * Returns when es->running is set to 0 (user closes the window or quits).
 *
 * es : pointer to the initialised EditorState.
 */
void editor_loop(EditorState *es);

/*
 * editor_cleanup — Release all editor resources in reverse init order.
 *
 * Frees textures, fonts, undo stack, renderer, and window.  Sets freed
 * pointers to NULL to prevent double-free errors (a project-wide safety
 * rule: every pointer is NULLed after its resource is destroyed).
 *
 * es : pointer to the EditorState to tear down.
 */
void editor_cleanup(EditorState *es);
