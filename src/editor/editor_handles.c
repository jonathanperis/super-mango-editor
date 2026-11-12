/*
 * editor_handles.c — Patrol ranges and resize handles on the canvas.
 *
 * See editor_handles.h for what a range and a handle are.  This file has
 * three parts:
 *
 *   1. where things are: a patroller's range, an entity's handles;
 *   2. what a handle means: turning a pointer position into a new patrol
 *      end or a new number of tiles;
 *   3. the gesture: press, motion, release and Esc.
 *
 * Nothing here knows about screen pixels except the two places that
 * divide a pixel distance by the zoom.
 */

#include "editor_handles.h"

#include <math.h>    /* fabsf, roundf */
#include <string.h>  /* memset */

#include "editor_session.h" /* editor_set_status, editor_refresh_dirty */
#include "entity_meta.h"    /* display sizes, editor_snapshot_entity      */
#include "hit_test.h"       /* editor_entity_bounds, EditorRect           */
#include "snapshot_equal.h" /* placement_same                             */
#include "tools.h"          /* editor_edit_kept_level, editor_snap_active */
#include "undo.h"           /* Command, undo_push                         */
#include "../game_constants.h" /* TILE_SIZE                               */

/* The same travel a move needs before it counts (tools_drag.c): a click on
 * a handle that wobbles less than this, in canvas pixels, changes nothing. */
#define RESIZE_THRESHOLD_PX 3.0f

/*
 * The most tiles a handle ever asks for.  It only bounds the search in
 * editor_resize_drag; the real limits are level validation's, which are
 * all smaller (the longest thing made of tiles is a rail, MAX_RAIL_TILES
 * = 128 in src/surfaces/rail.h).
 */
#define RESIZE_MAX_TILES 128

/* The errors the level had when the current resize began (one resize at a
 * time): every step and the release are measured against them. */
static EditorIssueSnapshot resize_issues;

/* ================================================================== */
/* 1. Where things are                                                 */
/* ================================================================== */

/*
 * PatrolFields — the three stored numbers of a patroller, wherever its
 * placement struct keeps them, and two widths that depend on its type.
 *
 * sprite_w : the narrowest range validation accepts (patrol_x1 - patrol_x0
 *            must be at least this): the sprite's frame width for the
 *            enemies, 0 for a saw.
 * reach    : how far past patrol_x1 the sprite is drawn.  0 for the
 *            enemies, whose right edge stops at patrol_x1; one saw width
 *            for a saw, whose left edge stops there.
 */
typedef struct {
    float *x;
    float *x0;
    float *x1;
    float  sprite_w;
    float  reach;
} PatrolFields;

/* Fill *out for a patrolling type and return 1; return 0 for any other. */
static int patrol_fields(EntityType type, PlacementData *pd, PatrolFields *out)
{
    switch (type) {
    case ENT_SPIDER:
        out->x = &pd->spider.x;
        out->x0 = &pd->spider.patrol_x0;
        out->x1 = &pd->spider.patrol_x1;
        out->sprite_w = (float)SPIDER_FRAME_W;
        out->reach = 0.0f;
        return 1;
    case ENT_JUMPING_SPIDER:
        out->x = &pd->jumping_spider.x;
        out->x0 = &pd->jumping_spider.patrol_x0;
        out->x1 = &pd->jumping_spider.patrol_x1;
        out->sprite_w = (float)SPIDER_FRAME_W;
        out->reach = 0.0f;
        return 1;
    case ENT_BIRD:
    case ENT_FASTER_BIRD:
        out->x = &pd->bird.x;
        out->x0 = &pd->bird.patrol_x0;
        out->x1 = &pd->bird.patrol_x1;
        out->sprite_w = (float)BIRD_FRAME_W;
        out->reach = 0.0f;
        return 1;
    case ENT_FISH:
    case ENT_FASTER_FISH:
        out->x = &pd->fish.x;
        out->x0 = &pd->fish.patrol_x0;
        out->x1 = &pd->fish.patrol_x1;
        out->sprite_w = (float)FISH_FRAME_W;
        out->reach = 0.0f;
        return 1;
    case ENT_CIRCULAR_SAW:
        out->x = &pd->circular_saw.x;
        out->x0 = &pd->circular_saw.patrol_x0;
        out->x1 = &pd->circular_saw.patrol_x1;
        out->sprite_w = 0.0f;
        out->reach = (float)SAW_DISPLAY_W;
        return 1;
    default:
        return 0;
    }
}

int editor_patrol_range(const LevelDef *level, EntityType type, int index,
                        EditorPatrolRange *out)
{
    PlacementData pd;
    PatrolFields fields;
    EditorRect bounds;

    if (!level || !out) return 0;
    if (!editor_entity_bounds(level, type, index, &bounds)) return 0;
    pd = editor_snapshot_entity(level, type, index);
    if (!patrol_fields(type, &pd, &fields)) return 0;

    out->x0 = *fields.x0;
    out->x1 = *fields.x1 + fields.reach;
    out->y = bounds.y + bounds.h / 2.0f;
    return 1;
}

/* A handle of the given kind centred on (x, y). */
static EditorHandle handle_at_point(EditorHandleKind kind, float x, float y)
{
    EditorHandle handle;
    handle.kind = kind;
    handle.x = x;
    handle.y = y;
    return handle;
}

int editor_entity_handles(const LevelDef *level, EntityType type, int index,
                          EditorHandle *out)
{
    EditorRect r;
    EditorPatrolRange range;
    /* The middle of the right edge, the middle of the bottom edge... */
    float right, bottom, middle_x, middle_y;

    if (!level || !out) return 0;
    if (!editor_entity_bounds(level, type, index, &r)) return 0;
    right = r.x + r.w;
    bottom = r.y + r.h;
    middle_x = r.x + r.w / 2.0f;
    middle_y = r.y + r.h / 2.0f;

    /* Patrollers: one handle on each end of the range line. */
    if (editor_patrol_range(level, type, index, &range)) {
        out[0] = handle_at_point(EDITOR_HANDLE_PATROL_START, range.x0, range.y);
        out[1] = handle_at_point(EDITOR_HANDLE_PATROL_END, range.x1, range.y);
        return 2;
    }

    switch (type) {
    /* A ground pillar stands on the floor and grows upward, so its height
     * handle is on its top edge; its width grows to the right. */
    case ENT_PLATFORM:
        out[0] = handle_at_point(EDITOR_HANDLE_WIDTH, right, middle_y);
        out[1] = handle_at_point(EDITOR_HANDLE_HEIGHT, middle_x, r.y);
        return 2;
    /* Rows of tiles grow to the right. */
    case ENT_SPIKE_ROW:
    case ENT_SPIKE_PLATFORM:
    case ENT_FLOAT_PLATFORM:
    case ENT_BRIDGE:
        out[0] = handle_at_point(EDITOR_HANDLE_WIDTH, right, middle_y);
        return 1;
    /* Climbables hang from their top and grow downward. */
    case ENT_VINE:
    case ENT_LADDER:
    case ENT_ROPE:
        out[0] = handle_at_point(EDITOR_HANDLE_HEIGHT, middle_x, bottom);
        return 1;
    /* A rail keeps its top-left corner; the opposite corner sizes it. */
    case ENT_RAIL:
        out[0] = handle_at_point(EDITOR_HANDLE_CORNER, right, bottom);
        return 1;
    default:
        return 0;
    }
}

int editor_handles_shown(const EditorState *es)
{
    if (!es || es->campaign || es->playing) return 0;
    if (es->tool != TOOL_SELECT) return 0;
    if (editor_selection_count(es) != 1) return 0;
    /* While the selected entity is being moved its handles would only be
     * in the way; while one of them is being dragged they stay. */
    if (es->dragging && !es->resizing) return 0;
    return 1;
}

EditorHandleKind editor_handle_at(const EditorState *es, float world_x, float world_y)
{
    EditorHandle handles[EDITOR_HANDLE_MAX];
    float zoom, reach;
    int count;

    if (!editor_handles_shown(es)) return EDITOR_HANDLE_NONE;
    count = editor_entity_handles(&es->level, es->selection.type,
                                  es->selection.index, handles);
    /* The grab distance is in canvas pixels; in world pixels it shrinks as
     * the canvas zooms in, so a handle is as easy to hit at 1/4x as at 5x. */
    zoom = es->camera.zoom > 0.0f ? es->camera.zoom : 1.0f;
    reach = EDITOR_HANDLE_GRAB_PX / zoom;
    for (int i = 0; i < count; i++) {
        if (fabsf(world_x - handles[i].x) <= reach &&
            fabsf(world_y - handles[i].y) <= reach)
            return handles[i].kind;
    }
    return EDITOR_HANDLE_NONE;
}

/* ================================================================== */
/* 2. What a handle means                                              */
/* ================================================================== */

/* Keep n between lo and hi. */
static int clamp_int(int n, int lo, int hi)
{
    if (n > hi) n = hi;
    if (n < lo) n = lo;
    return n;
}

/*
 * A float as a whole number, for counting tiles.  The pointer can be far
 * outside the level (the button is held while it leaves the window), and
 * converting a float too big for an int is undefined behaviour in C, so
 * the float is limited first.
 */
static int whole_tiles(float value)
{
    if (!(value > -1000.0f)) return -1000;   /* also catches NaN */
    if (value > 1000.0f) return 1000;
    return (int)roundf(value);
}

/*
 * get_sizes — The tile counts a handle can change: `across` (the count
 * along x) and `down` (the count along y).  A type that has only one of
 * them reports 1 for the other.  Returns 0 for a type with no tile count.
 */
static int get_sizes(EntityType type, const PlacementData *pd, int *across, int *down)
{
    *across = 1;
    *down = 1;
    switch (type) {
    case ENT_PLATFORM:
        /* tile_width 0 is an old way to write "one tile wide". */
        *across = pd->platform.tile_width > 0 ? pd->platform.tile_width : 1;
        *down = pd->platform.tile_height;
        return 1;
    case ENT_SPIKE_ROW:      *across = pd->spike_row.count;           return 1;
    case ENT_SPIKE_PLATFORM: *across = pd->spike_platform.tile_count; return 1;
    case ENT_FLOAT_PLATFORM: *across = pd->float_platform.tile_count; return 1;
    case ENT_BRIDGE:         *across = pd->bridge.brick_count;        return 1;
    case ENT_VINE:           *down = pd->vine.tile_count;             return 1;
    case ENT_LADDER:         *down = pd->ladder.tile_count;           return 1;
    case ENT_ROPE:           *down = pd->rope.tile_count;             return 1;
    case ENT_RAIL:
        *across = pd->rail.w;
        /* A horizontal rail is one tile tall whatever its h says. */
        *down = pd->rail.layout == RAIL_LAYOUT_RECT ? pd->rail.h : 1;
        return 1;
    default:
        return 0;
    }
}

/* Write tile counts back; the reverse of get_sizes. */
static void set_sizes(EntityType type, PlacementData *pd, int across, int down)
{
    int old_across, old_down;

    /* Leave a count that did not change exactly as stored (a platform's
     * tile_width 0 stays 0, a horizontal rail keeps its unused h). */
    if (!get_sizes(type, pd, &old_across, &old_down)) return;
    switch (type) {
    case ENT_PLATFORM:
        if (across != old_across) pd->platform.tile_width = across;
        if (down != old_down) pd->platform.tile_height = down;
        break;
    case ENT_SPIKE_ROW:      pd->spike_row.count = across;           break;
    case ENT_SPIKE_PLATFORM: pd->spike_platform.tile_count = across; break;
    case ENT_FLOAT_PLATFORM: pd->float_platform.tile_count = across; break;
    case ENT_BRIDGE:         pd->bridge.brick_count = across;        break;
    case ENT_VINE:           pd->vine.tile_count = down;             break;
    case ENT_LADDER:         pd->ladder.tile_count = down;           break;
    case ENT_ROPE:           pd->rope.tile_count = down;             break;
    case ENT_RAIL:
        if (across != old_across) pd->rail.w = across;
        if (down != old_down) pd->rail.h = down;
        break;
    default:
        break;
    }
}

/*
 * tiles_for_length — How many tiles make a run `length` pixels long, when
 * the first tile covers `first` pixels and each further one adds `step`
 * (vines overlap: 32 px for the first tile, 19 px for each one after).
 * Rounded to the nearest whole tile, never fewer than one.
 */
static int tiles_for_length(float length, float first, float step)
{
    int tiles = whole_tiles((length - first) / step) + 1;
    return clamp_int(tiles, 1, RESIZE_MAX_TILES);
}

/*
 * wanted_sizes — The tile counts that would put the grabbed handle at
 * world point (x, y).  `shown` is the entity as it is in the level now and
 * `bounds` its rectangle; a count the handle does not change keeps the
 * value it has in `shown`.
 */
static void wanted_sizes(EntityType type, EditorHandleKind handle,
                         const PlacementData *shown, const EditorRect *bounds,
                         float x, float y, int *across, int *down)
{
    /* How far the pointer is from the edges that stay put. */
    float length_x = x - bounds->x;
    float length_y = y - bounds->y;

    (void)get_sizes(type, shown, across, down);

    switch (type) {
    case ENT_PLATFORM:
        if (handle == EDITOR_HANDLE_WIDTH)
            *across = tiles_for_length(length_x, TILE_SIZE, TILE_SIZE);
        if (handle == EDITOR_HANDLE_HEIGHT) {
            /* The top edge moves and the foot stays: each tile raises the
             * top by TILE_SIZE from where a 0-tile pillar's top would be. */
            float top_of_none = level_platform_top_y(0);
            *down = clamp_int(whole_tiles((top_of_none - y) / TILE_SIZE),
                              1, RESIZE_MAX_TILES);
        }
        break;
    case ENT_SPIKE_ROW:
        *across = tiles_for_length(length_x, SPIKE_TILE_W, SPIKE_TILE_W);
        break;
    case ENT_SPIKE_PLATFORM:
        *across = tiles_for_length(length_x, SPIKE_PLAT_PIECE_W, SPIKE_PLAT_PIECE_W);
        break;
    case ENT_FLOAT_PLATFORM:
        /* A platform riding a rail is centred on its rail point, so it
         * grows on both sides: the pointer is half the width from the
         * middle. */
        if (shown->float_platform.mode == FLOAT_PLATFORM_RAIL)
            length_x = (x - (bounds->x + bounds->w / 2.0f)) * 2.0f;
        *across = tiles_for_length(length_x, FPLAT_PIECE_W, FPLAT_PIECE_W);
        break;
    case ENT_BRIDGE:
        *across = tiles_for_length(length_x, BRIDGE_TILE_W, BRIDGE_TILE_W);
        break;
    case ENT_VINE:
        *down = tiles_for_length(length_y, VINE_H, VINE_STEP);
        break;
    case ENT_LADDER:
        *down = tiles_for_length(length_y, LADDER_H, LADDER_STEP);
        break;
    case ENT_ROPE:
        *down = tiles_for_length(length_y, ROPE_H, ROPE_STEP);
        break;
    case ENT_RAIL:
        *across = tiles_for_length(length_x, RAIL_TILE_W, RAIL_TILE_W);
        if (shown->rail.layout == RAIL_LAYOUT_RECT)
            *down = tiles_for_length(length_y, RAIL_TILE_H, RAIL_TILE_H);
        break;
    default:
        break;
    }
}

/* Keep v between lo and hi; when the two cross, lo wins. */
static float clamp_float(float v, float lo, float hi)
{
    if (v > hi) v = hi;
    if (v < lo) v = lo;
    return v;
}

/*
 * moved_patrol_end — `before` with one end of its patrol range moved so the
 * grabbed handle sits at world x.
 *
 * The limits are level validation's (validate_patrol):
 *   - both ends inside the world, 0 .. world_w;
 *   - the entity's own x between them;
 *   - the range at least one sprite wide.
 * and one of the editor's own: the right end stays at or past the sprite's
 * right edge, so the entity is never left standing outside its range (a
 * level that already has it outside may keep that end where it is).
 */
static PlacementData moved_patrol_end(const EditorState *es, EditorHandleKind handle,
                                      float x)
{
    PlacementData pd = es->resize_before;
    PatrolFields fields;
    float world_w = editor_world_width(&es->level);

    if (!patrol_fields(es->resize_type, &pd, &fields)) return pd;

    /* Whole pixels; the nearest 48 px grid line while snapping. */
    if (editor_snap_active(es)) x = roundf(x / TILE_SIZE) * TILE_SIZE;
    else x = roundf(x);

    if (handle == EDITOR_HANDLE_PATROL_START) {
        float highest = *fields.x;
        if (*fields.x1 - fields.sprite_w < highest) highest = *fields.x1 - fields.sprite_w;
        *fields.x0 = clamp_float(x, 0.0f, highest);
    } else {
        /* The handle is drawn `reach` past the stored value (saws). */
        float stored = x - fields.reach;
        float lowest = *fields.x + fields.sprite_w;
        if (*fields.x1 < lowest) lowest = *fields.x1;
        if (*fields.x0 + fields.sprite_w > lowest) lowest = *fields.x0 + fields.sprite_w;
        *fields.x1 = clamp_float(stored, lowest, world_w);
    }
    return pd;
}

/* ================================================================== */
/* 3. The gesture                                                      */
/* ================================================================== */

/* The entity being resized is still where the press found it.  (Commands
 * that add or remove entities wait while the button is held, so this only
 * fails if something unexpected changed the arrays.) */
static int resize_target_exists(const EditorState *es)
{
    return es->resize_index >= 0 &&
           es->resize_index < editor_entity_count(&es->level, es->resize_type);
}

/*
 * try_placement — Write `candidate` over the entity being resized if that
 * leaves the level as sound as it was when the resize began.  Returns 1
 * when it was written, 0 when it was refused and the entity is unchanged.
 */
static int try_placement(EditorState *es, const PlacementData *candidate)
{
    PlacementData shown = editor_snapshot_entity(&es->level, es->resize_type,
                                                 es->resize_index);

    (void)editor_entity_write(&es->level, es->resize_type, es->resize_index, candidate);
    if (editor_edit_kept_level(&es->level, &resize_issues, NULL, 0) == 0) return 1;
    (void)editor_entity_write(&es->level, es->resize_type, es->resize_index, &shown);
    return 0;
}

/* Say in the status bar what the entity measures now. */
static void show_live_value(EditorState *es)
{
    PlacementData pd = editor_snapshot_entity(&es->level, es->resize_type,
                                              es->resize_index);
    const char *name = editor_entity_type_name(es->resize_type);
    PatrolFields fields;
    int across, down;

    if (patrol_fields(es->resize_type, &pd, &fields)) {
        editor_set_status(es, "%s patrol: %.0f to %.0f (%.0f px)", name,
                          *fields.x0, *fields.x1, *fields.x1 - *fields.x0);
        return;
    }
    if (!get_sizes(es->resize_type, &pd, &across, &down)) return;
    if (es->resize_type == ENT_PLATFORM ||
        (es->resize_type == ENT_RAIL && pd.rail.layout == RAIL_LAYOUT_RECT))
        editor_set_status(es, "%s: %d x %d tiles", name, across, down);
    else if (es->resize_handle == EDITOR_HANDLE_HEIGHT)
        editor_set_status(es, "%s: %d tile%s tall", name, down, down == 1 ? "" : "s");
    else
        editor_set_status(es, "%s: %d tile%s wide", name, across, across == 1 ? "" : "s");
}

int editor_resize_begin(EditorState *es, float world_x, float world_y)
{
    EditorHandle handles[EDITOR_HANDLE_MAX];
    EditorHandleKind kind;
    int count;

    if (!es || es->resizing) return 0;
    kind = editor_handle_at(es, world_x, world_y);
    if (kind == EDITOR_HANDLE_NONE) return 0;

    es->resizing = 1;
    es->dragging = 1;       /* the gesture owns the document (see editor.h) */
    es->drag_moved = 0;
    es->drag_count = 0;     /* no entity is being moved */
    es->box_selecting = 0;
    es->last_click_valid = 0;
    es->resize_handle = (int)kind;
    es->resize_moved = 0;
    es->resize_type = es->selection.type;
    es->resize_index = es->selection.index;
    es->resize_before = editor_snapshot_entity(&es->level, es->resize_type,
                                               es->resize_index);
    es->resize_press_x = world_x;
    es->resize_press_y = world_y;
    es->resize_grab_x = 0.0f;
    es->resize_grab_y = 0.0f;
    /* Where on the handle the press landed, so its edge does not jump to
     * put its centre under the pointer. */
    count = editor_entity_handles(&es->level, es->resize_type, es->resize_index, handles);
    for (int i = 0; i < count; i++) {
        if (handles[i].kind != kind) continue;
        es->resize_grab_x = world_x - handles[i].x;
        es->resize_grab_y = world_y - handles[i].y;
    }
    editor_level_issue_snapshot(&es->level, &resize_issues);
    return 1;
}

void editor_resize_drag(EditorState *es, float world_x, float world_y)
{
    EditorHandleKind handle;
    PlacementData shown, candidate;
    EditorRect bounds;
    float x, y;
    int across, down, want_across, want_down;

    if (!es || !es->resizing || !resize_target_exists(es)) return;
    handle = (EditorHandleKind)es->resize_handle;

    /* Ignore small wobbles until the pointer leaves the click threshold.
     * The threshold is in canvas pixels, so divide by zoom for world px. */
    if (!es->resize_moved) {
        float zoom = es->camera.zoom > 0.0f ? es->camera.zoom : 1.0f;
        float limit = RESIZE_THRESHOLD_PX / zoom;
        float dx = world_x - es->resize_press_x;
        float dy = world_y - es->resize_press_y;
        if (dx * dx + dy * dy < limit * limit) return;
        es->resize_moved = 1;
    }

    /* Where the handle's centre should be. */
    x = world_x - es->resize_grab_x;
    y = world_y - es->resize_grab_y;

    if (handle == EDITOR_HANDLE_PATROL_START || handle == EDITOR_HANDLE_PATROL_END) {
        /* A patrol end the level would not accept stays where it last was. */
        candidate = moved_patrol_end(es, handle, x);
        (void)try_placement(es, &candidate);
        show_live_value(es);
        return;
    }

    /*
     * Tile counts.  `shown` is the entity as it is now, which the level
     * accepts.  Ask for the count under the pointer; if validation refuses
     * it (too many tiles, past the edge of the world, a rail rider left
     * off its rail), try one tile nearer to what is shown, and so on.  The
     * search ends at the first count that is accepted, or leaves the entity
     * alone when it gets back to the count already shown.  Across is
     * settled first and down second, so a rail's corner can stop at a
     * different limit on each axis.
     */
    shown = editor_snapshot_entity(&es->level, es->resize_type, es->resize_index);
    if (!editor_entity_bounds(&es->level, es->resize_type, es->resize_index, &bounds)) return;
    if (!get_sizes(es->resize_type, &shown, &across, &down)) return;
    wanted_sizes(es->resize_type, handle, &shown, &bounds, x, y, &want_across, &want_down);

    while (want_across != across) {
        candidate = shown;
        set_sizes(es->resize_type, &candidate, want_across, down);
        if (try_placement(es, &candidate)) {
            across = want_across;
            shown = candidate;
            break;
        }
        want_across += want_across < across ? 1 : -1;
    }
    while (want_down != down) {
        candidate = shown;
        set_sizes(es->resize_type, &candidate, across, want_down);
        if (try_placement(es, &candidate)) {
            down = want_down;
            break;
        }
        want_down += want_down < down ? 1 : -1;
    }
    show_live_value(es);
}

/* Put the entity back as it was at the press. */
static void restore_resize(EditorState *es)
{
    if (!resize_target_exists(es)) return;
    (void)editor_entity_write(&es->level, es->resize_type, es->resize_index,
                              &es->resize_before);
}

void editor_resize_finish(EditorState *es)
{
    PlacementData after;
    Command cmd;

    if (!es || !es->resizing) return;
    es->resizing = 0;
    es->dragging = 0;
    if (!resize_target_exists(es)) return;

    after = editor_snapshot_entity(&es->level, es->resize_type, es->resize_index);
    /* A click on a handle, or a drag that ended where it began. */
    if (placement_same(es->resize_type, &after, &es->resize_before)) return;

    /*
     * Every motion kept the level sound, but never leave an unrecorded
     * change behind: if something else broke it while the button was held,
     * or the history has no room, the entity goes back as it was.
     */
    if (editor_edit_kept_level(&es->level, &resize_issues, NULL, 0) != 0) {
        restore_resize(es);
        editor_set_status(es, "Resize cancelled: level has errors");
        return;
    }
    memset(&cmd, 0, sizeof(cmd));
    cmd.type         = CMD_PROPERTY;   /* a changed value: "Undid: changed Rail" */
    cmd.entity_type  = (int)es->resize_type;
    cmd.entity_index = es->resize_index;
    cmd.before       = es->resize_before;
    cmd.after        = after;
    if (!undo_push(es->undo, &cmd)) {
        restore_resize(es);
        editor_set_status(es, "Resize cancelled: cannot allocate undo history");
        return;
    }
    editor_refresh_dirty(es);
    show_live_value(es);
}

void editor_resize_cancel(EditorState *es)
{
    if (!es || !es->resizing) return;
    es->resizing = 0;
    es->dragging = 0;
    restore_resize(es);
    editor_set_status(es, "Resize cancelled");
}
