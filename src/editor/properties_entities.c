/*
 * properties_entities.c — The fields of each entity type in the
 * Properties panel.
 *
 * Most types are a table of rows drawn by one function (draw_table_properties,
 * see "Field rows" below); the few whose panel has a button, a file
 * dropdown or mode-dependent fields have a draw_<type>_properties function.
 * s_property_panels in properties.c picks one or the other. The field ids,
 * row layout and label helper are shared (properties_internal.h) so a
 * field keeps the id the editor's focus path already knows.
 */

#include <float.h>      /* FLT_MAX for one-sided field limits    */
#include <limits.h>     /* INT_MIN, INT_MAX for unlimited int fields */
#include <stddef.h>     /* offsetof: where a field row's value lives */
#include <stdio.h>      /* snprintf for header label formatting */
#include <string.h>     /* strrchr for filename extraction       */

#include "properties.h"
#include "asset_choices.h" /* the files each asset dropdown offers */
#include "../screens/hud.h"  /* MAX_HEARTS */
#include "editor.h"     /* EditorState, EntityType, CANVAS_W, PANEL_W, etc. */
#include "editor_undo_apply.h" /* committed property/config command tracking */
#include "editor_files.h"  /* editor_path_for_display for recent entries   */
#include "entity_meta.h" /* editor_entity_type_name/is_singleton             */
#include "tools.h"       /* editor_set_float_platform_mode                    */
#include "editor_panels.h" /* editor_focus_validation_issue                     */
#include "editor_session.h" /* editor_set_status (show_field_note)              */
#include "editor_playtest.h" /* editor_play_test_from (checkpoint "Playtest here") */
#include "../shared/ui.h" /* ui_panel, ui_label, ui_separator, ui_float_field,
                           ui_int_field, ui_dropdown                         */
#include "../levels/level.h" /* LevelDef, all *Placement structs            */
#include "../player/player.h"  /* JUMP_VY (bouncepad launch limit)           */
#include "../surfaces/rail.h"  /* MAX_RAIL_SPEED (rail rider speed limit)    */
#include "../game_constants.h" /* MAX_PATROL_SPEED (enemy vx limit)          */
#include "properties_internal.h"

const int patrol_speed_limit_px = MAX_PATROL_SPEED;

/* ------------------------------------------------------------------ */
/* Dropdown option arrays                                              */
/* ------------------------------------------------------------------ */

/*
 * Rail layout options — maps to RailLayout enum (RAIL_LAYOUT_RECT = 0,
 * RAIL_LAYOUT_HORIZ = 1).  The dropdown selected index is cast to/from
 * the enum when reading and writing the RailPlacement.layout field.
 */
static const char *rail_layout_opts[] = { "Rect", "Horiz" };

/*
 * Axe trap mode options — maps to AxeTrapMode enum (AXE_MODE_PENDULUM = 0,
 * AXE_MODE_SPIN = 1).
 */
static const char *axe_mode_opts[] = { "Pendulum", "Spin" };

/*
 * Float platform mode options — maps to FloatPlatformMode enum
 * (FLOAT_PLATFORM_STATIC = 0, FLOAT_PLATFORM_CRUMBLE = 1,
 *  FLOAT_PLATFORM_RAIL = 2).
 */
static const char *fplat_mode_opts[] = { "Static", "Crumble", "Rail" };

/*
 * Vine type options — maps to VineType enum (VINE_GREEN = 0, VINE_BROWN = 1).
 */
static const char *vine_type_opts[] = { "Green", "Brown" };

/* ------------------------------------------------------------------ */
/* Field rows: one table instead of one function per entity            */
/* ------------------------------------------------------------------ */

/*
 * Most entities are edited the same way: a column of labelled number
 * fields, one per struct member, perhaps with a limit or a dropdown. That
 * used to be a hand-written function per entity type, most of them copies
 * of each other with the names changed (the bird and the faster bird were
 * identical line for line). Here each of those entities is a short table
 * of PropertyRow, and one function, draw_rows, draws any of them.
 *
 * A row says which key it edits (the label and tooltip come from the key,
 * see field_label), which widget id it has, what kind of field it is, and
 * where the value lives: `offset` bytes into the entity's placement
 * struct (offsetof gives that number).
 *
 * The widget id is part of the row, not its position: a validation message
 * focuses a field by id, so a field keeps the id it always had even when
 * rows are reordered (the axe trap's rows are numbered 0, 3, 1).
 *
 * The same tables let several selected entities of one type be edited
 * together (properties_apply_to_selection): a row knows where its value
 * is in ANY entity of that type, so a value typed once can be copied to
 * all of them.
 *
 * Entities whose panel does more than this keep a function of their own
 * below: a button, a path dropdown, fields that depend on a mode.
 */
typedef enum {
    ROW_FLOAT,          /* any number                                        */
    ROW_FLOAT_NONZERO,  /* a speed: -limit..limit, never 0 (`hi` is limit)   */
    ROW_FLOAT_LIMITED,  /* lo..hi                                            */
    ROW_FLOAT_BELOW,    /* at most (the float at other_offset) - gap         */
    ROW_FLOAT_ABOVE,    /* at least (the float at other_offset) + gap        */
    ROW_INT,            /* any whole number                                  */
    ROW_CHOICE          /* an int-sized member picked from a dropdown        */
} PropertyRowKind;

typedef struct {
    const char     *key;          /* the TOML key: label, tooltip, focus    */
    int             id;           /* FIELD_ID(type, id) is the widget id    */
    PropertyRowKind kind;
    size_t          offset;       /* where the value is in the placement    */
    float           lo, hi;       /* ROW_FLOAT_LIMITED / ROW_FLOAT_NONZERO  */
    size_t          other_offset; /* ROW_FLOAT_BELOW / ABOVE: the other end */
    float           gap;          /* ... and how far apart they must stay   */
    const char    **options;      /* ROW_CHOICE: the dropdown's entries     */
    int             option_count;
} PropertyRow;

#define ROW_COUNT(rows) ((int)(sizeof(rows) / sizeof((rows)[0])))

/* Shorthand for the row kinds. T is the placement struct, m its member. */
#define FLOAT_ROW(key, id, T, m) \
    { key, id, ROW_FLOAT, offsetof(T, m), 0, 0, 0, 0, NULL, 0 }
#define INT_ROW(key, id, T, m) \
    { key, id, ROW_INT, offsetof(T, m), 0, 0, 0, 0, NULL, 0 }
#define SPEED_ROW(key, id, T, m, limit) \
    { key, id, ROW_FLOAT_NONZERO, offsetof(T, m), 0, limit, 0, 0, NULL, 0 }
#define LIMITED_ROW(key, id, T, m, lo, hi) \
    { key, id, ROW_FLOAT_LIMITED, offsetof(T, m), lo, hi, 0, 0, NULL, 0 }
/* The two ends of a patrol: the left one stays `gap` below the right one. */
#define PATROL_LEFT_ROW(key, id, T, m, right_member, gap) \
    { key, id, ROW_FLOAT_BELOW, offsetof(T, m), 0, 0, offsetof(T, right_member), gap, NULL, 0 }
#define PATROL_RIGHT_ROW(key, id, T, m, left_member, gap) \
    { key, id, ROW_FLOAT_ABOVE, offsetof(T, m), 0, 0, offsetof(T, left_member), gap, NULL, 0 }
#define CHOICE_ROW(key, id, T, m, options) \
    { key, id, ROW_CHOICE, offsetof(T, m), 0, 0, 0, 0, options, ROW_COUNT(options) }

/* A choice is stored through an int; the enums it is used for are that size. */
_Static_assert(sizeof(RailLayout) == sizeof(int) && sizeof(AxeTrapMode) == sizeof(int),
               "ROW_CHOICE reads and writes the member as an int");

/* The float `offset` bytes into the entity at `item`. */
static float float_at(const void *item, size_t offset)
{
    float value;
    memcpy(&value, (const char *)item + offset, sizeof(value));
    return value;
}

/*
 * draw_row — Draw one row for the entity at `item` and return 1 when the
 * designer changed its value this frame. The widgets write straight into
 * the entity through the pointer, as the hand-written panels did.
 */
static int draw_row(EditorState *es, EntityType type, void *item, const PropertyRow *row, int y)
{
    char *member = (char *)item + row->offset;
    int id = FIELD_ID(type, row->id);

    field_label(es, y, row->key, id);
    switch (row->kind) {
    case ROW_FLOAT:
        return ui_float_field(&es->ui, id, FIELD_X, y, FIELD_W, (float *)(void *)member);
    case ROW_FLOAT_NONZERO:
        return ui_float_field_nonzero(&es->ui, id, FIELD_X, y, FIELD_W,
                                      (float *)(void *)member, row->hi);
    case ROW_FLOAT_LIMITED:
        return ui_float_field_limited(&es->ui, id, FIELD_X, y, FIELD_W,
                                      (float *)(void *)member, row->lo, row->hi);
    case ROW_FLOAT_BELOW:
        return ui_float_field_limited(&es->ui, id, FIELD_X, y, FIELD_W, (float *)(void *)member,
                                      -FLT_MAX, float_at(item, row->other_offset) - row->gap);
    case ROW_FLOAT_ABOVE:
        return ui_float_field_limited(&es->ui, id, FIELD_X, y, FIELD_W, (float *)(void *)member,
                                      float_at(item, row->other_offset) + row->gap, FLT_MAX);
    case ROW_INT:
        return ui_int_field(&es->ui, id, FIELD_X, y, FIELD_W, (int *)(void *)member);
    case ROW_CHOICE:
        return ui_dropdown(&es->ui, id, FIELD_X, y, FIELD_W, row->options, row->option_count,
                           (int *)(void *)member);
    }
    return 0;
}

/*
 * The pointer casts above ((float *)(void *)member) say "this address holds
 * a float". They are sound because `item` points at a real placement
 * struct and `offset` came from offsetof on a float (or int) member of
 * that struct, so the address is that member's own and is aligned for it.
 */

/* How many bytes a row's value takes in the placement struct. */
static size_t row_size(const PropertyRow *row)
{
    return row->kind == ROW_INT || row->kind == ROW_CHOICE ? sizeof(int) : sizeof(float);
}

/*
 * draw_rows — Draw every row of one entity. With several entities of this
 * type selected, a changed row is copied to all of them as one undo step
 * (properties_apply_to_selection); with one selected it is an ordinary
 * property edit.
 *
 * Enter and Tab end the edit here, while the row draws.  Every other way
 * of leaving the field (a click on the canvas or on another field, Ctrl+S,
 * Undo...) ends it in editor_finish_field_edit before the panel draws
 * again, so the row being typed into is remembered in es->shared_field_*
 * for that path to copy the value the same way.
 */
static void draw_rows(EditorState *es, EntityType type, void *item,
                      const PropertyRow *rows, int count, int y)
{
    int shared = editor_selection_count(es) > 1;

    for (int i = 0; i < count; i++, y += ROW_H) {
        int changed = draw_row(es, type, item, &rows[i], y);
        if (shared && es->ui.active_id == FIELD_ID(type, rows[i].id)) {
            es->shared_field_id = FIELD_ID(type, rows[i].id);
            es->shared_field_type = (int)type;
            es->shared_field_key = rows[i].key;
            es->shared_field_offset = rows[i].offset;
            es->shared_field_size = row_size(&rows[i]);
        }
        if (!changed) continue;
        if (shared)
            properties_apply_to_selection(es, type, rows[i].key, rows[i].offset,
                                          row_size(&rows[i]));
        else
            editor_commit_change(es);
    }
}

/* ---- The tables ---------------------------------------------------- */

static const PropertyRow s_rail_rows[] = {
    CHOICE_ROW("layout", 0, RailPlacement, layout, rail_layout_opts),
    INT_ROW("x", 1, RailPlacement, x),
    INT_ROW("y", 2, RailPlacement, y),
    INT_ROW("w", 3, RailPlacement, w),
    INT_ROW("h", 4, RailPlacement, h),
    INT_ROW("end_cap", 5, RailPlacement, end_cap),
};

/* Coins and the three star colours are all "float x, y". */
static const PropertyRow s_xy_rows[] = {
    FLOAT_ROW("x", 0, CoinPlacement, x),
    FLOAT_ROW("y", 1, CoinPlacement, y),
};
_Static_assert(sizeof(CoinPlacement) == sizeof(StarYellowPlacement) &&
               sizeof(CoinPlacement) == sizeof(StarGreenPlacement) &&
               sizeof(CoinPlacement) == sizeof(StarRedPlacement) &&
               offsetof(CoinPlacement, y) == offsetof(StarYellowPlacement, y) &&
               offsetof(CoinPlacement, y) == offsetof(StarGreenPlacement, y) &&
               offsetof(CoinPlacement, y) == offsetof(StarRedPlacement, y),
               "coins and stars share s_xy_rows: keep their layouts the same");

/* The player spawn is two members of LevelDef itself. */
static const PropertyRow s_player_spawn_rows[] = {
    FLOAT_ROW("x", 0, LevelDef, player_start_x),
    FLOAT_ROW("y", 1, LevelDef, player_start_y),
};

static const PropertyRow s_spider_rows[] = {
    FLOAT_ROW("x", 0, SpiderPlacement, x),
    SPEED_ROW("vx", 1, SpiderPlacement, vx, MAX_PATROL_SPEED),
    PATROL_LEFT_ROW("patrol_x0", 2, SpiderPlacement, patrol_x0, patrol_x1, SPIDER_FRAME_W),
    PATROL_RIGHT_ROW("patrol_x1", 3, SpiderPlacement, patrol_x1, patrol_x0, SPIDER_FRAME_W),
    INT_ROW("frame_index", 4, SpiderPlacement, frame_index),
};

static const PropertyRow s_jumping_spider_rows[] = {
    FLOAT_ROW("x", 0, JumpingSpiderPlacement, x),
    SPEED_ROW("vx", 1, JumpingSpiderPlacement, vx, MAX_PATROL_SPEED),
    PATROL_LEFT_ROW("patrol_x0", 2, JumpingSpiderPlacement, patrol_x0, patrol_x1, SPIDER_FRAME_W),
    PATROL_RIGHT_ROW("patrol_x1", 3, JumpingSpiderPlacement, patrol_x1, patrol_x0, SPIDER_FRAME_W),
};

/* Birds and faster birds are both BirdPlacement: one table, two arrays. */
static const PropertyRow s_bird_rows[] = {
    FLOAT_ROW("x", 0, BirdPlacement, x),
    FLOAT_ROW("base_y", 1, BirdPlacement, base_y),
    SPEED_ROW("vx", 2, BirdPlacement, vx, MAX_PATROL_SPEED),
    PATROL_LEFT_ROW("patrol_x0", 3, BirdPlacement, patrol_x0, patrol_x1, BIRD_FRAME_W),
    PATROL_RIGHT_ROW("patrol_x1", 4, BirdPlacement, patrol_x1, patrol_x0, BIRD_FRAME_W),
    INT_ROW("frame_index", 5, BirdPlacement, frame_index),
};

/* Fish and faster fish are both FishPlacement. */
static const PropertyRow s_fish_rows[] = {
    FLOAT_ROW("x", 0, FishPlacement, x),
    SPEED_ROW("vx", 1, FishPlacement, vx, MAX_PATROL_SPEED),
    PATROL_LEFT_ROW("patrol_x0", 2, FishPlacement, patrol_x0, patrol_x1, FISH_FRAME_W),
    PATROL_RIGHT_ROW("patrol_x1", 3, FishPlacement, patrol_x1, patrol_x0, FISH_FRAME_W),
};

static const PropertyRow s_axe_trap_rows[] = {
    FLOAT_ROW("pillar_x", 0, AxeTrapPlacement, pillar_x),
    FLOAT_ROW("y", 3, AxeTrapPlacement, y),
    CHOICE_ROW("mode", 1, AxeTrapPlacement, mode, axe_mode_opts),
};

static const PropertyRow s_circular_saw_rows[] = {
    FLOAT_ROW("x", 0, CircularSawPlacement, x),
    FLOAT_ROW("y", 4, CircularSawPlacement, y),
    FLOAT_ROW("patrol_x0", 1, CircularSawPlacement, patrol_x0),
    FLOAT_ROW("patrol_x1", 2, CircularSawPlacement, patrol_x1),
    INT_ROW("direction", 3, CircularSawPlacement, direction),
};

static const PropertyRow s_spike_row_rows[] = {
    FLOAT_ROW("x", 0, SpikeRowPlacement, x),
    INT_ROW("count", 1, SpikeRowPlacement, count),
};

static const PropertyRow s_spike_platform_rows[] = {
    FLOAT_ROW("x", 0, SpikePlatformPlacement, x),
    FLOAT_ROW("y", 1, SpikePlatformPlacement, y),
    INT_ROW("tile_count", 2, SpikePlatformPlacement, tile_count),
};

static const PropertyRow s_spike_block_rows[] = {
    INT_ROW("rail_index", 0, SpikeBlockPlacement, rail_index),
    FLOAT_ROW("t_offset", 1, SpikeBlockPlacement, t_offset),
    LIMITED_ROW("speed", 2, SpikeBlockPlacement, speed, RAIL_SPEED_MIN, MAX_RAIL_SPEED),
};

static const PropertyRow s_bridge_rows[] = {
    FLOAT_ROW("x", 0, BridgePlacement, x),
    FLOAT_ROW("y", 1, BridgePlacement, y),
    INT_ROW("brick_count", 2, BridgePlacement, brick_count),
};

static const PropertyRow s_vine_rows[] = {
    FLOAT_ROW("x", 0, VinePlacement, x),
    FLOAT_ROW("y", 1, VinePlacement, y),
    INT_ROW("tile_count", 2, VinePlacement, tile_count),
    CHOICE_ROW("vine_type", 3, VinePlacement, vine_type, vine_type_opts),
};

/* Ladders and ropes are both "float x, y; int tile_count". */
static const PropertyRow s_climbable_rows[] = {
    FLOAT_ROW("x", 0, LadderPlacement, x),
    FLOAT_ROW("y", 1, LadderPlacement, y),
    INT_ROW("tile_count", 2, LadderPlacement, tile_count),
};
_Static_assert(sizeof(LadderPlacement) == sizeof(RopePlacement) &&
               offsetof(LadderPlacement, y) == offsetof(RopePlacement, y) &&
               offsetof(LadderPlacement, tile_count) == offsetof(RopePlacement, tile_count),
               "ladders and ropes share s_climbable_rows: keep their layouts the same");

/*
 * s_row_tables — which entity types are drawn from a table, and where
 * their placement array starts in LevelDef. item_size 0 marks a singleton
 * stored directly in LevelDef (the player spawn), whose "array" is the
 * LevelDef itself.
 */
typedef struct {
    const PropertyRow *rows;
    int                count;
    size_t             items_offset;  /* offsetof(LevelDef, <array>)  */
    size_t             item_size;     /* sizeof one placement, or 0   */
} PropertyRowTable;

#define ROWS_FOR(rows, array_member) \
    { rows, ROW_COUNT(rows), offsetof(LevelDef, array_member), \
      sizeof(((LevelDef *)0)->array_member[0]) }

static const PropertyRowTable s_row_tables[ENT_COUNT] = {
    [ENT_RAIL]           = ROWS_FOR(s_rail_rows, rails),
    [ENT_COIN]           = ROWS_FOR(s_xy_rows, coins),
    [ENT_STAR_YELLOW]    = ROWS_FOR(s_xy_rows, star_yellows),
    [ENT_STAR_GREEN]     = ROWS_FOR(s_xy_rows, star_greens),
    [ENT_STAR_RED]       = ROWS_FOR(s_xy_rows, star_reds),
    [ENT_PLAYER_SPAWN]   = { s_player_spawn_rows, ROW_COUNT(s_player_spawn_rows), 0, 0 },
    [ENT_SPIDER]         = ROWS_FOR(s_spider_rows, spiders),
    [ENT_JUMPING_SPIDER] = ROWS_FOR(s_jumping_spider_rows, jumping_spiders),
    [ENT_BIRD]           = ROWS_FOR(s_bird_rows, birds),
    [ENT_FASTER_BIRD]    = ROWS_FOR(s_bird_rows, faster_birds),
    [ENT_FISH]           = ROWS_FOR(s_fish_rows, fish),
    [ENT_FASTER_FISH]    = ROWS_FOR(s_fish_rows, faster_fish),
    [ENT_AXE_TRAP]       = ROWS_FOR(s_axe_trap_rows, axe_traps),
    [ENT_CIRCULAR_SAW]   = ROWS_FOR(s_circular_saw_rows, circular_saws),
    [ENT_SPIKE_ROW]      = ROWS_FOR(s_spike_row_rows, spike_rows),
    [ENT_SPIKE_PLATFORM] = ROWS_FOR(s_spike_platform_rows, spike_platforms),
    [ENT_SPIKE_BLOCK]    = ROWS_FOR(s_spike_block_rows, spike_blocks),
    [ENT_BRIDGE]         = ROWS_FOR(s_bridge_rows, bridges),
    [ENT_VINE]           = ROWS_FOR(s_vine_rows, vines),
    [ENT_LADDER]         = ROWS_FOR(s_climbable_rows, ladders),
    [ENT_ROPE]           = ROWS_FOR(s_climbable_rows, ropes),
};

int properties_type_has_rows(EntityType type)
{
    return (int)type >= 0 && type < ENT_COUNT && s_row_tables[type].rows != NULL;
}

void draw_table_properties(EditorState *es, int y)
{
    EntityType type = es->selection.type;
    const PropertyRowTable *table;
    char *item;

    if (!properties_type_has_rows(type)) return;
    table = &s_row_tables[type];
    /* The selected entity: its array's start plus index elements, or the
     * LevelDef itself for a singleton kept there. */
    item = (char *)&es->level + table->items_offset +
           (size_t)es->selection.index * table->item_size;
    draw_rows(es, type, item, table->rows, table->count, y);
}

/* ------------------------------------------------------------------ */
/* Entities with a panel of their own                                  */
/* ------------------------------------------------------------------ */

/*
 * One draw_<type>_properties function per entity type.  Each draws the
 * selected entity's fields starting at row y and commits every edit through
 * editor_commit_change; s_property_panels below picks the one to call.
 */
/* draw_floor_gap_properties — Fields for the selected floor gap. */
void draw_floor_gap_properties(EditorState *es, int y)
{
    /*
     * floor_gaps is an int array — each element is a single x coordinate.
     * We take a pointer to the array element so ui_int_field can modify it.
     */
    int *p = &es->level.floor_gaps[es->selection.index];
    int screens = level_screen_count(&es->level);
    field_label(es, y, "x", FIELD_ID(ENT_FLOOR_GAP, 0));
    if (ui_int_field_limited(&es->ui, FIELD_ID(ENT_FLOOR_GAP, 0),
                             FIELD_X, y, FIELD_W, p,
                             0, screens * GAME_W - FLOOR_GAP_W, FLOOR_PIECE_W))
        editor_commit_change(es);
}

/* draw_checkpoint_properties — Fields for the selected checkpoint. */
void draw_checkpoint_properties(EditorState *es, int y)
{
    CheckpointPlacement *p = &es->level.checkpoints[es->selection.index];
    int screen = p->x >= 0.0f && p->x <= MAX_LEVEL_SCREENS * GAME_W
               ? (int)(p->x / GAME_W) : -1;
    field_label(es, y, "x", FIELD_ID(ENT_CHECKPOINT, 0));
    if (ui_float_field(&es->ui, FIELD_ID(ENT_CHECKPOINT, 0),
                       FIELD_X, y, FIELD_W, &p->x))
        editor_commit_change(es);
    y += ROW_H;
    field_label(es, y, "y", FIELD_ID(ENT_CHECKPOINT, 1));
    if (ui_float_field(&es->ui, FIELD_ID(ENT_CHECKPOINT, 1),
                       FIELD_X, y, FIELD_W, &p->y))
        editor_commit_change(es);
    y += ROW_H;
    ui_label(&es->ui, CONTENT_X, y, "screen:");
    char screen_text[32];
    snprintf(screen_text, sizeof(screen_text), "%d (derived)", screen);
    ui_label_color(&es->ui, FIELD_X, y, screen_text, ui_color(&es->ui, UI_COLOR_TEXT_DIM));
    y += ROW_H;
    ui_label_color(&es->ui, CONTENT_X, y, "Respawn when crossed.", ui_color(&es->ui, UI_COLOR_TEXT_DIM));
    y += ROW_H;
    /* Start a playtest standing on this checkpoint (the game's
     * --start-checkpoint), like Shift+F5 with it selected. */
    if (ui_button(&es->ui, CONTENT_X, y, 160, 20, "Playtest from here")) {
        LevelStart start = { LEVEL_START_AT_CHECKPOINT, 0.0f, es->selection.index };
        editor_play_test_from(es, &start);
    }
}

/* draw_platform_properties — Fields for the selected platform. */
void draw_platform_properties(EditorState *es, int y)
{
    PlatformPlacement *p = &es->level.platforms[es->selection.index];

    field_label(es, y, "x", FIELD_ID(ENT_PLATFORM, 0));
    if (ui_float_field(&es->ui, FIELD_ID(ENT_PLATFORM, 0),
                       FIELD_X, y, FIELD_W, &p->x))
        editor_commit_change(es);
    y += ROW_H;

    field_label(es, y, "tile_height", FIELD_ID(ENT_PLATFORM, 1));
    if (ui_int_field(&es->ui, FIELD_ID(ENT_PLATFORM, 1),
                     FIELD_X, y, FIELD_W, &p->tile_height))
        editor_commit_change(es);
    y += ROW_H;

    field_label(es, y, "tile_width", FIELD_ID(ENT_PLATFORM, 2));
    if (ui_int_field(&es->ui, FIELD_ID(ENT_PLATFORM, 2),
                     FIELD_X, y, FIELD_W, &p->tile_width))
        editor_commit_change(es);
    y += ROW_H;

    /* Tile path dropdown — select platform texture override */
    {
        const AssetChoices *tiles = editor_asset_choices(ASSET_CHOICES_PLATFORM_TILES);

        int sel = option_index(p->tile_path, tiles->paths, tiles->count);
        field_label(es, y, "tile_path", FIELD_ID(ENT_PLATFORM, 3));
        if (ui_dropdown_unlisted(&es->ui, FIELD_ID(ENT_PLATFORM, 3),
                                 FIELD_X, y, FIELD_W, tiles->names, tiles->count, &sel,
                                 editor_asset_unlisted_name(p->tile_path))) {
            if (sel == 0) {
                p->tile_path[0] = '\0';  /* Clear to use default */
            } else {
                strncpy(p->tile_path, tiles->paths[sel],
                        sizeof(p->tile_path) - 1);
                p->tile_path[sizeof(p->tile_path) - 1] = '\0';
            }
            editor_commit_change(es);
        }
    }
}

/* draw_last_star_properties — Fields for the selected last star. */
void draw_last_star_properties(EditorState *es, int y)
{
    /*
     * last_star is a single struct in LevelDef, not an array.
     * The selection index is always 0 for this type.
     */
    LastStarPlacement *p = &es->level.last_star;

    field_label(es, y, "x", FIELD_ID(ENT_LAST_STAR, 0));
    if (ui_float_field(&es->ui, FIELD_ID(ENT_LAST_STAR, 0),
                       FIELD_X, y, FIELD_W, &p->x))
        editor_commit_change(es);
    y += ROW_H;

    field_label(es, y, "y", FIELD_ID(ENT_LAST_STAR, 1));
    if (ui_float_field(&es->ui, FIELD_ID(ENT_LAST_STAR, 1),
                       FIELD_X, y, FIELD_W, &p->y))
        editor_commit_change(es);
    y += ROW_H;

    /* Next phase path for level linking */
    ui_label(&es->ui, CONTENT_X, y, "next phase:");
    y += ROW_H;
    if (ui_text_field_hint(&es->ui, FIELD_ID(ENT_LAST_STAR, 2),
                           CONTENT_X, y, FIELD_W * 2,
                           es->level.next_phase,
                           (int)sizeof(es->level.next_phase), NEXT_PHASE_HINT, 0))
        editor_commit_change(es);
}

/* draw_blue_flame_properties — Fields for the selected blue flame. */
void draw_blue_flame_properties(EditorState *es, int y)
{
    BlueFlamePlacement *p =
        &es->level.blue_flames[es->selection.index];

    field_label(es, y, "x", FIELD_ID(ENT_BLUE_FLAME, 0));
    if (ui_float_field(&es->ui, FIELD_ID(ENT_BLUE_FLAME, 0),
                       FIELD_X, y, FIELD_W, &p->x)) {
        /* A typed x lands on the nearest gap, as a click would. */
        p->x = editor_nearest_floor_gap(&es->level, p->x);
        editor_commit_change(es);
    }
}

/* draw_fire_flame_properties — Fields for the selected fire flame. */
void draw_fire_flame_properties(EditorState *es, int y)
{
    FireFlamePlacement *p =
        &es->level.fire_flames[es->selection.index];

    field_label(es, y, "x", FIELD_ID(ENT_FIRE_FLAME, 0));
    if (ui_float_field(&es->ui, FIELD_ID(ENT_FIRE_FLAME, 0),
                       FIELD_X, y, FIELD_W, &p->x)) {
        /* A typed x lands on the nearest gap, as a click would. */
        p->x = editor_nearest_floor_gap(&es->level, p->x);
        editor_commit_change(es);
    }
}

/* draw_float_platform_properties — Fields for the selected float platform. */
void draw_float_platform_properties(EditorState *es, int y)
{
    FloatPlatformPlacement *p =
        &es->level.float_platforms[es->selection.index];

    /*
     * mode — dropdown selecting Static, Crumble, or Rail.
     * Cast FloatPlatformMode to int for the dropdown widget.
     */
    int mode_sel = (int)p->mode;
    field_label(es, y, "mode", FIELD_ID(ENT_FLOAT_PLATFORM, 0));
    if (ui_dropdown(&es->ui, FIELD_ID(ENT_FLOAT_PLATFORM, 0),
                    FIELD_X, y, FIELD_W,
                    fplat_mode_opts, 3, &mode_sel)) {
        /* The switch to Rail re-checks the stored rail (see tools.c).  A
         * refused switch changes nothing, so the commit records nothing. */
        (void)editor_set_float_platform_mode(es, es->selection.index,
                                             (FloatPlatformMode)mode_sel);
        editor_commit_change(es);
    }
    y += ROW_H;

    field_label(es, y, "x", FIELD_ID(ENT_FLOAT_PLATFORM, 1));
    if (ui_float_field(&es->ui, FIELD_ID(ENT_FLOAT_PLATFORM, 1),
                       FIELD_X, y, FIELD_W, &p->x))
        editor_commit_change(es);
    y += ROW_H;

    field_label(es, y, "y", FIELD_ID(ENT_FLOAT_PLATFORM, 2));
    if (ui_float_field(&es->ui, FIELD_ID(ENT_FLOAT_PLATFORM, 2),
                       FIELD_X, y, FIELD_W, &p->y))
        editor_commit_change(es);
    y += ROW_H;

    field_label(es, y, "tile_count", FIELD_ID(ENT_FLOAT_PLATFORM, 3));
    if (ui_int_field(&es->ui, FIELD_ID(ENT_FLOAT_PLATFORM, 3),
                     FIELD_X, y, FIELD_W, &p->tile_count))
        editor_commit_change(es);
    y += ROW_H;

    field_label(es, y, "rail_index", FIELD_ID(ENT_FLOAT_PLATFORM, 4));
    /* A rail rider's index must name an existing rail; other modes ignore
     * it, and the switch to Rail checks it then. */
    if (ui_int_field_limited(&es->ui, FIELD_ID(ENT_FLOAT_PLATFORM, 4),
                             FIELD_X, y, FIELD_W, &p->rail_index,
                             p->mode == FLOAT_PLATFORM_RAIL ? 0 : INT_MIN,
                             p->mode == FLOAT_PLATFORM_RAIL && es->level.rail_count > 0
                                 ? es->level.rail_count - 1 : INT_MAX, 1))
        editor_commit_change(es);
    y += ROW_H;

    field_label(es, y, "t_offset", FIELD_ID(ENT_FLOAT_PLATFORM, 5));
    if (ui_float_field(&es->ui, FIELD_ID(ENT_FLOAT_PLATFORM, 5),
                       FIELD_X, y, FIELD_W, &p->t_offset))
        editor_commit_change(es);
    y += ROW_H;

    field_label(es, y, "speed", FIELD_ID(ENT_FLOAT_PLATFORM, 6));
    /* Only a rail rider's speed has to lie in the rail range. */
    if (ui_float_field_limited(&es->ui, FIELD_ID(ENT_FLOAT_PLATFORM, 6),
                               FIELD_X, y, FIELD_W, &p->speed,
                               p->mode == FLOAT_PLATFORM_RAIL ? RAIL_SPEED_MIN : -FLT_MAX,
                               p->mode == FLOAT_PLATFORM_RAIL ? MAX_RAIL_SPEED : FLT_MAX))
        editor_commit_change(es);
}

/* draw_bouncepad_small_properties — Fields for the selected bouncepad small. */
void draw_bouncepad_small_properties(EditorState *es, int y)
{
    BouncepadPlacement *p =
        &es->level.bouncepads_small[es->selection.index];

    /*
     * BouncepadType is fixed per array (BOUNCEPAD_GREEN for small),
     * so we just show a read-only label instead of a dropdown.
     */
    ui_label(&es->ui, CONTENT_X, y, "type: Small (Green)");
    y += ROW_H;

    field_label(es, y, "x", FIELD_ID(ENT_BOUNCEPAD_SMALL, 0));
    if (ui_float_field(&es->ui, FIELD_ID(ENT_BOUNCEPAD_SMALL, 0),
                       FIELD_X, y, FIELD_W, &p->x))
        editor_commit_change(es);
    y += ROW_H;

    field_label(es, y, "launch_vy", FIELD_ID(ENT_BOUNCEPAD_SMALL, 1));
    if (ui_float_field_limited(&es->ui, FIELD_ID(ENT_BOUNCEPAD_SMALL, 1),
                               FIELD_X, y, FIELD_W, &p->launch_vy,
                               -MAX_LEVEL_MOTION, BOUNCE_LAUNCH_VY_MAX))
        editor_commit_change(es);
}

/* draw_bouncepad_medium_properties — Fields for the selected bouncepad medium. */
void draw_bouncepad_medium_properties(EditorState *es, int y)
{
    BouncepadPlacement *p =
        &es->level.bouncepads_medium[es->selection.index];

    ui_label(&es->ui, CONTENT_X, y, "type: Medium (Wood)");
    y += ROW_H;

    field_label(es, y, "x", FIELD_ID(ENT_BOUNCEPAD_MEDIUM, 0));
    if (ui_float_field(&es->ui, FIELD_ID(ENT_BOUNCEPAD_MEDIUM, 0),
                       FIELD_X, y, FIELD_W, &p->x))
        editor_commit_change(es);
    y += ROW_H;

    field_label(es, y, "launch_vy", FIELD_ID(ENT_BOUNCEPAD_MEDIUM, 1));
    if (ui_float_field_limited(&es->ui, FIELD_ID(ENT_BOUNCEPAD_MEDIUM, 1),
                               FIELD_X, y, FIELD_W, &p->launch_vy,
                               -MAX_LEVEL_MOTION, BOUNCE_LAUNCH_VY_MAX))
        editor_commit_change(es);
}

/* draw_bouncepad_high_properties — Fields for the selected bouncepad high. */
void draw_bouncepad_high_properties(EditorState *es, int y)
{
    BouncepadPlacement *p =
        &es->level.bouncepads_high[es->selection.index];

    ui_label(&es->ui, CONTENT_X, y, "type: High (Red)");
    y += ROW_H;

    field_label(es, y, "x", FIELD_ID(ENT_BOUNCEPAD_HIGH, 0));
    if (ui_float_field(&es->ui, FIELD_ID(ENT_BOUNCEPAD_HIGH, 0),
                       FIELD_X, y, FIELD_W, &p->x))
        editor_commit_change(es);
    y += ROW_H;

    field_label(es, y, "launch_vy", FIELD_ID(ENT_BOUNCEPAD_HIGH, 1));
    if (ui_float_field_limited(&es->ui, FIELD_ID(ENT_BOUNCEPAD_HIGH, 1),
                               FIELD_X, y, FIELD_W, &p->launch_vy,
                               -MAX_LEVEL_MOTION, BOUNCE_LAUNCH_VY_MAX))
        editor_commit_change(es);
}

