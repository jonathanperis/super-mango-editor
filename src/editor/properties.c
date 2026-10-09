/*
 * properties.c — Entity property inspector panel for the level editor.
 *
 * Renders per-entity-type editable fields (position, velocity, patrol range,
 * mode, tile counts, etc.) in the bottom-right panel when an entity is
 * selected on the canvas.  Uses the immediate-mode UI widgets from ui.h.
 *
 * The panel occupies the bottom half of the right column:
 *   x = CANVAS_W (896),  y = EDITOR_H / 2 (360)
 *   w = PANEL_W  (384),  h = EDITOR_H / 2 - STATUS_H (328)
 *
 * Each field has a unique widget ID built from the entity type enum value
 * multiplied by 100 plus a per-field offset (e.g. ENT_COIN * 100 + 1 = 301).
 * This guarantees no two fields share an ID, which the IMGUI system requires
 * to track which widget is actively being edited.
 */

#include <float.h>      /* FLT_MAX for one-sided field limits    */
#include <limits.h>     /* INT_MIN, INT_MAX for unlimited int fields */
#include <stdio.h>      /* snprintf for header label formatting */
#include <string.h>     /* strrchr for filename extraction       */

#include "properties.h"
#include "editor.h"     /* EditorState, EntityType, CANVAS_W, PANEL_W, etc. */
#include "editor_undo_apply.h" /* committed property/config command tracking */
#include "editor_files.h"  /* editor_path_for_display for recent entries   */
#include "entity_meta.h" /* editor_entity_type_name/is_singleton             */
#include "tools.h"       /* editor_set_float_platform_mode                    */
#include "../shared/ui.h" /* ui_panel, ui_label, ui_separator, ui_float_field,
                           ui_int_field, ui_dropdown                         */
#include "../levels/level.h" /* LevelDef, all *Placement structs            */
#include "../player/player.h"  /* JUMP_VY (bouncepad launch limit)           */
#include "../surfaces/rail.h"  /* MAX_RAIL_SPEED (rail rider speed limit)    */

/* ------------------------------------------------------------------ */
/* Layout constants                                                    */
/* ------------------------------------------------------------------ */

/*
 * PROP_X — left edge of the properties panel (flush with the right column).
 * PROP_Y — top edge (splits the right column in half vertically).
 * PROP_W — full width of the right column.
 * PROP_H — available height above the status bar.
 */
#define PROP_X     CANVAS_W
#define PROP_Y     (EDITOR_H / 2)
#define PROP_W     PANEL_W
#define PROP_H     (EDITOR_H / 2 - STATUS_H)

/*
 * ROW_H      — vertical space per field row (label + input).
 * LABEL_W    — horizontal space reserved for the field label text.
 * FIELD_W    — width of the input widget (int/float field or dropdown).
 * CONTENT_X  — left edge of field content (panel x + left margin).
 * FIELD_X    — left edge of the input widget (after the label).
 */
#define ROW_H      24
#define LABEL_W    80
#define FIELD_W    120
#define CONTENT_X  (PROP_X + 8)
#define FIELD_X    (CONTENT_X + LABEL_W)

/* Collapsible subsection open states — accessed by editor.c for config_h computation */
int g_plx_open  = 0;
int g_fg_open   = 0;
int g_fog_open  = 0;
int g_phys_open = 0;

/*
 * cfg_scroll_y — vertical scroll offset (px) for the Level Config content.
 *
 * When the config panel has more content than fits in the visible area,
 * scrolling shifts content upward.  Clamped to [0, max] in
 * level_config_render each frame.
 */
static int cfg_scroll_y = 0;

/*
 * cfg_scroll — Adjust the Level Config scroll offset by a pixel delta.
 * Called from editor.c's mouse wheel handler.
 */
void cfg_scroll(int delta) {
    cfg_scroll_y += delta;
    if (cfg_scroll_y < 0) cfg_scroll_y = 0;
}

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
/* Helper: unique widget ID from entity type and field offset          */
/* ------------------------------------------------------------------ */

/*
 * FIELD_ID — Generate a unique widget ID for an entity property field.
 *
 * Each EntityType has a range of 100 IDs (type * 100 + 0..99).  The field
 * offset distinguishes different fields within the same entity type.
 * Example: ENT_COIN (3) field 1 = 301, ENT_COIN field 2 = 302.
 *
 * The +1 ensures no ID is ever zero (the IMGUI system uses 0 to mean
 * "no active widget").
 */
#define FIELD_ID(type, field)  ((int)(type) * 100 + (field) + 1)

/*
 * Limits the level validator puts on a few fields.  The fields clamp typed
 * values to them, so an edit cannot turn a valid level into one that
 * refuses to save:
 *
 *   floor gap x — rounded to FLOOR_PIECE_W (game.h): the floor is drawn in
 *                 16 px pieces, so a gap edge must fall on that grid.
 *   launch_vy   — a bouncepad must launch at least as hard as a jump, so at
 *                 most JUMP_VY (player.h; negative is up).
 *   rail speed  — spike blocks and rail-mode float platforms move along
 *                 their rail at more than 0 and at most MAX_RAIL_SPEED
 *                 tiles/s (rail.h); RAIL_SPEED_MIN (tools.h) stands in
 *                 for "more than 0".
 */
#define BOUNCE_LAUNCH_VY_MAX  JUMP_VY

/*
 * option_index — Position of value in a dropdown's paths, or -1.
 *
 * -1 tells ui_dropdown the stored path is none of its options (a path
 * typed into the TOML by hand).  Mapping it to option 0 instead would make
 * picking option 0 look like "no change", so it could never be chosen.
 */
static int option_index(const char *value, const char *const *paths,
                        int count)
{
    for (int i = 0; i < count; i++) {
        if (strcmp(value, paths[i]) == 0) return i;
    }
    return -1;
}

/*
 * draw_section_title — The title bar of a collapsible right-panel section
 * (the entity properties and Level Config).
 *
 * Fills the bar, toggles *open when it is clicked, and draws the ">"/"v"
 * symbol in UI_ACCENT followed by title, which turns from UI_ACCENT to
 * UI_TEXT while hovered.  The bar is ROW_H + 4 px tall.
 */
static void draw_section_title(EditorState *es, int x, int y,
                               const char *title, int *open)
{
    DrawRectangle(x, y, PROP_W, ROW_H+4, UI_TITLE_BG);

    int hovered = (es->ui.mouse_x >= x &&
                   es->ui.mouse_x < x + PROP_W &&
                   es->ui.mouse_y >= y &&
                   es->ui.mouse_y < y + ROW_H + 4);
    if (hovered && es->ui.mouse_clicked)
        *open = !*open;

    const char *sym = *open ? "v" : ">";
    int sym_w = ui_text_width(&es->ui, sym);
    ui_label_color(&es->ui, x + 8, y + 4, sym, UI_ACCENT);
    ui_label_color(&es->ui, x + 8 + sym_w, y + 4, title,
                   hovered ? UI_TEXT : UI_ACCENT);
}

/*
 * draw_rail_properties — Fields for the selected rail: layout, position
 * (x, y in pixels), size (w, h in rail tiles) and end_cap.
 */
static void draw_rail_properties(EditorState *es, int y)
{
    RailPlacement *p = &es->level.rails[es->selection.index];

    /*
     * layout — dropdown that selects between Rect and Horiz rail types.
     * Cast the enum to int for the dropdown, then cast back on change.
     */
    int layout_sel = (int)p->layout;
    ui_label(&es->ui, CONTENT_X, y, "layout:");
    if (ui_dropdown(&es->ui, FIELD_ID(ENT_RAIL, 0),
                    FIELD_X, y, FIELD_W,
                    rail_layout_opts, 2, &layout_sel)) {
        p->layout = (RailLayout)layout_sel;
        editor_commit_change(es);
    }
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "x:");
    if (ui_int_field(&es->ui, FIELD_ID(ENT_RAIL, 1),
                     FIELD_X, y, FIELD_W, &p->x))
        editor_commit_change(es);
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "y:");
    if (ui_int_field(&es->ui, FIELD_ID(ENT_RAIL, 2),
                     FIELD_X, y, FIELD_W, &p->y))
        editor_commit_change(es);
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "w:");
    if (ui_int_field(&es->ui, FIELD_ID(ENT_RAIL, 3),
                     FIELD_X, y, FIELD_W, &p->w))
        editor_commit_change(es);
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "h:");
    if (ui_int_field(&es->ui, FIELD_ID(ENT_RAIL, 4),
                     FIELD_X, y, FIELD_W, &p->h))
        editor_commit_change(es);
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "end_cap:");
    if (ui_int_field(&es->ui, FIELD_ID(ENT_RAIL, 5),
                     FIELD_X, y, FIELD_W, &p->end_cap))
        editor_commit_change(es);
}

/*
 * One draw_<type>_properties function per entity type.  Each draws the
 * selected entity's fields starting at row y and commits every edit through
 * editor_commit_change; s_property_panels below picks the one to call.
 */
/* draw_floor_gap_properties — Fields for the selected floor gap. */
static void draw_floor_gap_properties(EditorState *es, int y)
{
    /*
     * floor_gaps is an int array — each element is a single x coordinate.
     * We take a pointer to the array element so ui_int_field can modify it.
     */
    int *p = &es->level.floor_gaps[es->selection.index];
    int screens = es->level.screen_count > 0 ? es->level.screen_count : 4;
    ui_label(&es->ui, CONTENT_X, y, "x:");
    if (ui_int_field_limited(&es->ui, FIELD_ID(ENT_FLOOR_GAP, 0),
                             FIELD_X, y, FIELD_W, p,
                             0, screens * GAME_W - FLOOR_GAP_W, FLOOR_PIECE_W))
        editor_commit_change(es);
}

/* draw_checkpoint_properties — Fields for the selected checkpoint. */
static void draw_checkpoint_properties(EditorState *es, int y)
{
    CheckpointPlacement *p = &es->level.checkpoints[es->selection.index];
    int screen = p->x >= 0.0f && p->x <= MAX_LEVEL_SCREENS * GAME_W
               ? (int)(p->x / GAME_W) : -1;
    ui_label(&es->ui, CONTENT_X, y, "x:");
    if (ui_float_field(&es->ui, FIELD_ID(ENT_CHECKPOINT, 0),
                       FIELD_X, y, FIELD_W, &p->x))
        editor_commit_change(es);
    y += ROW_H;
    ui_label(&es->ui, CONTENT_X, y, "y:");
    if (ui_float_field(&es->ui, FIELD_ID(ENT_CHECKPOINT, 1),
                       FIELD_X, y, FIELD_W, &p->y))
        editor_commit_change(es);
    y += ROW_H;
    ui_label(&es->ui, CONTENT_X, y, "screen:");
    char screen_text[32];
    snprintf(screen_text, sizeof(screen_text), "%d (derived)", screen);
    ui_label_color(&es->ui, FIELD_X, y, screen_text, UI_TEXT_DIM);
    y += ROW_H;
    ui_label_color(&es->ui, CONTENT_X, y, "Respawn when crossed.", UI_TEXT_DIM);
}

/* draw_platform_properties — Fields for the selected platform. */
static void draw_platform_properties(EditorState *es, int y)
{
    PlatformPlacement *p = &es->level.platforms[es->selection.index];

    ui_label(&es->ui, CONTENT_X, y, "x:");
    if (ui_float_field(&es->ui, FIELD_ID(ENT_PLATFORM, 0),
                       FIELD_X, y, FIELD_W, &p->x))
        editor_commit_change(es);
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "tile_height:");
    if (ui_int_field(&es->ui, FIELD_ID(ENT_PLATFORM, 1),
                     FIELD_X, y, FIELD_W, &p->tile_height))
        editor_commit_change(es);
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "tile_width:");
    if (ui_int_field(&es->ui, FIELD_ID(ENT_PLATFORM, 2),
                     FIELD_X, y, FIELD_W, &p->tile_width))
        editor_commit_change(es);
    y += ROW_H;

    /* Tile path dropdown — select platform texture override */
    {
        static const char *platform_tile_names[] = {
            "(default)",
            "stone_platform.png",
            "leaf_platform.png"
        };
        static const char *platform_tile_paths[] = {
            "",
            "assets/sprites/levels/stone_platform.png",
            "assets/sprites/levels/leaf_platform.png"
        };
        static const int platform_tile_count = 3;

        int sel = option_index(p->tile_path, platform_tile_paths,
                               platform_tile_count);
        ui_label(&es->ui, CONTENT_X, y, "tile_path:");
        if (ui_dropdown(&es->ui, FIELD_ID(ENT_PLATFORM, 3),
                        FIELD_X, y, FIELD_W,
                        platform_tile_names, platform_tile_count, &sel)) {
            if (sel == 0) {
                p->tile_path[0] = '\0';  /* Clear to use default */
            } else {
                strncpy(p->tile_path, platform_tile_paths[sel],
                        sizeof(p->tile_path) - 1);
                p->tile_path[sizeof(p->tile_path) - 1] = '\0';
            }
            editor_commit_change(es);
        }
    }
}

/* draw_coin_properties — Fields for the selected coin. */
static void draw_coin_properties(EditorState *es, int y)
{
    CoinPlacement *p = &es->level.coins[es->selection.index];

    ui_label(&es->ui, CONTENT_X, y, "x:");
    if (ui_float_field(&es->ui, FIELD_ID(ENT_COIN, 0),
                       FIELD_X, y, FIELD_W, &p->x))
        editor_commit_change(es);
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "y:");
    if (ui_float_field(&es->ui, FIELD_ID(ENT_COIN, 1),
                       FIELD_X, y, FIELD_W, &p->y))
        editor_commit_change(es);
}

/* draw_star_yellow_properties — Fields for the selected star yellow. */
static void draw_star_yellow_properties(EditorState *es, int y)
{
    StarYellowPlacement *p =
        &es->level.star_yellows[es->selection.index];

    ui_label(&es->ui, CONTENT_X, y, "x:");
    if (ui_float_field(&es->ui, FIELD_ID(ENT_STAR_YELLOW, 0),
                       FIELD_X, y, FIELD_W, &p->x))
        editor_commit_change(es);
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "y:");
    if (ui_float_field(&es->ui, FIELD_ID(ENT_STAR_YELLOW, 1),
                       FIELD_X, y, FIELD_W, &p->y))
        editor_commit_change(es);
}

/* draw_star_green_properties — Fields for the selected star green. */
static void draw_star_green_properties(EditorState *es, int y)
{
    StarGreenPlacement *p =
        &es->level.star_greens[es->selection.index];

    ui_label(&es->ui, CONTENT_X, y, "x:");
    if (ui_float_field(&es->ui, FIELD_ID(ENT_STAR_GREEN, 0),
                       FIELD_X, y, FIELD_W, &p->x))
        editor_commit_change(es);
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "y:");
    if (ui_float_field(&es->ui, FIELD_ID(ENT_STAR_GREEN, 1),
                       FIELD_X, y, FIELD_W, &p->y))
        editor_commit_change(es);
}

/* draw_star_red_properties — Fields for the selected star red. */
static void draw_star_red_properties(EditorState *es, int y)
{
    StarRedPlacement *p =
        &es->level.star_reds[es->selection.index];

    ui_label(&es->ui, CONTENT_X, y, "x:");
    if (ui_float_field(&es->ui, FIELD_ID(ENT_STAR_RED, 0),
                       FIELD_X, y, FIELD_W, &p->x))
        editor_commit_change(es);
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "y:");
    if (ui_float_field(&es->ui, FIELD_ID(ENT_STAR_RED, 1),
                       FIELD_X, y, FIELD_W, &p->y))
        editor_commit_change(es);
}

/* draw_last_star_properties — Fields for the selected last star. */
static void draw_last_star_properties(EditorState *es, int y)
{
    /*
     * last_star is a single struct in LevelDef, not an array.
     * The selection index is always 0 for this type.
     */
    LastStarPlacement *p = &es->level.last_star;

    ui_label(&es->ui, CONTENT_X, y, "x:");
    if (ui_float_field(&es->ui, FIELD_ID(ENT_LAST_STAR, 0),
                       FIELD_X, y, FIELD_W, &p->x))
        editor_commit_change(es);
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "y:");
    if (ui_float_field(&es->ui, FIELD_ID(ENT_LAST_STAR, 1),
                       FIELD_X, y, FIELD_W, &p->y))
        editor_commit_change(es);
    y += ROW_H;

    /* Next phase path for level linking */
    ui_label(&es->ui, CONTENT_X, y, "next phase:");
    y += ROW_H;
    if (ui_text_field(&es->ui, FIELD_ID(ENT_LAST_STAR, 2),
                      CONTENT_X, y, FIELD_W * 2,
                      es->level.next_phase,
                      sizeof(es->level.next_phase)))
        editor_commit_change(es);
}

/* draw_player_spawn_properties — Fields for the selected player spawn. */
static void draw_player_spawn_properties(EditorState *es, int y)
{
    /*
     * player_start_x / player_start_y are scalar fields in LevelDef,
     * not a struct like LastStarPlacement.  The selection index is
     * always 0 because there is exactly one player spawn per level.
     */
    ui_label(&es->ui, CONTENT_X, y, "x:");
    if (ui_float_field(&es->ui, FIELD_ID(ENT_PLAYER_SPAWN, 0),
                       FIELD_X, y, FIELD_W,
                       &es->level.player_start_x))
        editor_commit_change(es);
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "y:");
    if (ui_float_field(&es->ui, FIELD_ID(ENT_PLAYER_SPAWN, 1),
                       FIELD_X, y, FIELD_W,
                       &es->level.player_start_y))
        editor_commit_change(es);
}

/* draw_spider_properties — Fields for the selected spider. */
static void draw_spider_properties(EditorState *es, int y)
{
    SpiderPlacement *p = &es->level.spiders[es->selection.index];

    ui_label(&es->ui, CONTENT_X, y, "x:");
    if (ui_float_field(&es->ui, FIELD_ID(ENT_SPIDER, 0),
                       FIELD_X, y, FIELD_W, &p->x))
        editor_commit_change(es);
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "vx:");
    if (ui_float_field(&es->ui, FIELD_ID(ENT_SPIDER, 1),
                       FIELD_X, y, FIELD_W, &p->vx))
        editor_commit_change(es);
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "patrol_x0:");
    if (ui_float_field_limited(&es->ui, FIELD_ID(ENT_SPIDER, 2),
                               FIELD_X, y, FIELD_W, &p->patrol_x0,
                               -FLT_MAX, p->patrol_x1 - SPIDER_FRAME_W))
        editor_commit_change(es);
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "patrol_x1:");
    if (ui_float_field_limited(&es->ui, FIELD_ID(ENT_SPIDER, 3),
                               FIELD_X, y, FIELD_W, &p->patrol_x1,
                               p->patrol_x0 + SPIDER_FRAME_W, FLT_MAX))
        editor_commit_change(es);
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "frame_index:");
    if (ui_int_field(&es->ui, FIELD_ID(ENT_SPIDER, 4),
                     FIELD_X, y, FIELD_W, &p->frame_index))
        editor_commit_change(es);
}

/* draw_jumping_spider_properties — Fields for the selected jumping spider. */
static void draw_jumping_spider_properties(EditorState *es, int y)
{
    JumpingSpiderPlacement *p =
        &es->level.jumping_spiders[es->selection.index];

    ui_label(&es->ui, CONTENT_X, y, "x:");
    if (ui_float_field(&es->ui, FIELD_ID(ENT_JUMPING_SPIDER, 0),
                       FIELD_X, y, FIELD_W, &p->x))
        editor_commit_change(es);
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "vx:");
    if (ui_float_field(&es->ui, FIELD_ID(ENT_JUMPING_SPIDER, 1),
                       FIELD_X, y, FIELD_W, &p->vx))
        editor_commit_change(es);
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "patrol_x0:");
    if (ui_float_field_limited(&es->ui, FIELD_ID(ENT_JUMPING_SPIDER, 2),
                               FIELD_X, y, FIELD_W, &p->patrol_x0,
                               -FLT_MAX, p->patrol_x1 - SPIDER_FRAME_W))
        editor_commit_change(es);
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "patrol_x1:");
    if (ui_float_field_limited(&es->ui, FIELD_ID(ENT_JUMPING_SPIDER, 3),
                               FIELD_X, y, FIELD_W, &p->patrol_x1,
                               p->patrol_x0 + SPIDER_FRAME_W, FLT_MAX))
        editor_commit_change(es);
}

/* draw_bird_properties — Fields for the selected bird. */
static void draw_bird_properties(EditorState *es, int y)
{
    BirdPlacement *p = &es->level.birds[es->selection.index];

    ui_label(&es->ui, CONTENT_X, y, "x:");
    if (ui_float_field(&es->ui, FIELD_ID(ENT_BIRD, 0),
                       FIELD_X, y, FIELD_W, &p->x))
        editor_commit_change(es);
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "base_y:");
    if (ui_float_field(&es->ui, FIELD_ID(ENT_BIRD, 1),
                       FIELD_X, y, FIELD_W, &p->base_y))
        editor_commit_change(es);
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "vx:");
    if (ui_float_field(&es->ui, FIELD_ID(ENT_BIRD, 2),
                       FIELD_X, y, FIELD_W, &p->vx))
        editor_commit_change(es);
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "patrol_x0:");
    if (ui_float_field_limited(&es->ui, FIELD_ID(ENT_BIRD, 3),
                               FIELD_X, y, FIELD_W, &p->patrol_x0,
                               -FLT_MAX, p->patrol_x1 - BIRD_FRAME_W))
        editor_commit_change(es);
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "patrol_x1:");
    if (ui_float_field_limited(&es->ui, FIELD_ID(ENT_BIRD, 4),
                               FIELD_X, y, FIELD_W, &p->patrol_x1,
                               p->patrol_x0 + BIRD_FRAME_W, FLT_MAX))
        editor_commit_change(es);
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "frame_index:");
    if (ui_int_field(&es->ui, FIELD_ID(ENT_BIRD, 5),
                     FIELD_X, y, FIELD_W, &p->frame_index))
        editor_commit_change(es);
}

/* draw_faster_bird_properties — Fields for the selected faster bird. */
static void draw_faster_bird_properties(EditorState *es, int y)
{
    /*
     * Faster birds use the same BirdPlacement struct and the same
     * fields as regular birds — they just live in a separate array.
     */
    BirdPlacement *p = &es->level.faster_birds[es->selection.index];

    ui_label(&es->ui, CONTENT_X, y, "x:");
    if (ui_float_field(&es->ui, FIELD_ID(ENT_FASTER_BIRD, 0),
                       FIELD_X, y, FIELD_W, &p->x))
        editor_commit_change(es);
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "base_y:");
    if (ui_float_field(&es->ui, FIELD_ID(ENT_FASTER_BIRD, 1),
                       FIELD_X, y, FIELD_W, &p->base_y))
        editor_commit_change(es);
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "vx:");
    if (ui_float_field(&es->ui, FIELD_ID(ENT_FASTER_BIRD, 2),
                       FIELD_X, y, FIELD_W, &p->vx))
        editor_commit_change(es);
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "patrol_x0:");
    if (ui_float_field_limited(&es->ui, FIELD_ID(ENT_FASTER_BIRD, 3),
                               FIELD_X, y, FIELD_W, &p->patrol_x0,
                               -FLT_MAX, p->patrol_x1 - BIRD_FRAME_W))
        editor_commit_change(es);
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "patrol_x1:");
    if (ui_float_field_limited(&es->ui, FIELD_ID(ENT_FASTER_BIRD, 4),
                               FIELD_X, y, FIELD_W, &p->patrol_x1,
                               p->patrol_x0 + BIRD_FRAME_W, FLT_MAX))
        editor_commit_change(es);
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "frame_index:");
    if (ui_int_field(&es->ui, FIELD_ID(ENT_FASTER_BIRD, 5),
                     FIELD_X, y, FIELD_W, &p->frame_index))
        editor_commit_change(es);
}

/* draw_fish_properties — Fields for the selected fish. */
static void draw_fish_properties(EditorState *es, int y)
{
    FishPlacement *p = &es->level.fish[es->selection.index];

    ui_label(&es->ui, CONTENT_X, y, "x:");
    if (ui_float_field(&es->ui, FIELD_ID(ENT_FISH, 0),
                       FIELD_X, y, FIELD_W, &p->x))
        editor_commit_change(es);
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "vx:");
    if (ui_float_field(&es->ui, FIELD_ID(ENT_FISH, 1),
                       FIELD_X, y, FIELD_W, &p->vx))
        editor_commit_change(es);
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "patrol_x0:");
    if (ui_float_field_limited(&es->ui, FIELD_ID(ENT_FISH, 2),
                               FIELD_X, y, FIELD_W, &p->patrol_x0,
                               -FLT_MAX, p->patrol_x1 - FISH_FRAME_W))
        editor_commit_change(es);
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "patrol_x1:");
    if (ui_float_field_limited(&es->ui, FIELD_ID(ENT_FISH, 3),
                               FIELD_X, y, FIELD_W, &p->patrol_x1,
                               p->patrol_x0 + FISH_FRAME_W, FLT_MAX))
        editor_commit_change(es);
}

/* draw_faster_fish_properties — Fields for the selected faster fish. */
static void draw_faster_fish_properties(EditorState *es, int y)
{
    /*
     * Faster fish use the same FishPlacement struct and the same
     * fields as regular fish — they just live in a separate array.
     */
    FishPlacement *p = &es->level.faster_fish[es->selection.index];

    ui_label(&es->ui, CONTENT_X, y, "x:");
    if (ui_float_field(&es->ui, FIELD_ID(ENT_FASTER_FISH, 0),
                       FIELD_X, y, FIELD_W, &p->x))
        editor_commit_change(es);
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "vx:");
    if (ui_float_field(&es->ui, FIELD_ID(ENT_FASTER_FISH, 1),
                       FIELD_X, y, FIELD_W, &p->vx))
        editor_commit_change(es);
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "patrol_x0:");
    if (ui_float_field_limited(&es->ui, FIELD_ID(ENT_FASTER_FISH, 2),
                               FIELD_X, y, FIELD_W, &p->patrol_x0,
                               -FLT_MAX, p->patrol_x1 - FISH_FRAME_W))
        editor_commit_change(es);
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "patrol_x1:");
    if (ui_float_field_limited(&es->ui, FIELD_ID(ENT_FASTER_FISH, 3),
                               FIELD_X, y, FIELD_W, &p->patrol_x1,
                               p->patrol_x0 + FISH_FRAME_W, FLT_MAX))
        editor_commit_change(es);
}

/* draw_axe_trap_properties — Fields for the selected axe trap. */
static void draw_axe_trap_properties(EditorState *es, int y)
{
    AxeTrapPlacement *p = &es->level.axe_traps[es->selection.index];

    ui_label(&es->ui, CONTENT_X, y, "pillar_x:");
    if (ui_float_field(&es->ui, FIELD_ID(ENT_AXE_TRAP, 0),
                       FIELD_X, y, FIELD_W, &p->pillar_x))
        editor_commit_change(es);
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "y:");
    if (ui_float_field(&es->ui, FIELD_ID(ENT_AXE_TRAP, 3),
                       FIELD_X, y, FIELD_W, &p->y))
        editor_commit_change(es);
    y += ROW_H;

    int mode_sel = (int)p->mode;
    ui_label(&es->ui, CONTENT_X, y, "mode:");
    if (ui_dropdown(&es->ui, FIELD_ID(ENT_AXE_TRAP, 1),
                    FIELD_X, y, FIELD_W,
                    axe_mode_opts, 2, &mode_sel)) {
        p->mode = (AxeTrapMode)mode_sel;
        editor_commit_change(es);
    }
}

/* draw_circular_saw_properties — Fields for the selected circular saw. */
static void draw_circular_saw_properties(EditorState *es, int y)
{
    CircularSawPlacement *p =
        &es->level.circular_saws[es->selection.index];

    ui_label(&es->ui, CONTENT_X, y, "x:");
    if (ui_float_field(&es->ui, FIELD_ID(ENT_CIRCULAR_SAW, 0),
                       FIELD_X, y, FIELD_W, &p->x))
        editor_commit_change(es);
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "y:");
    if (ui_float_field(&es->ui, FIELD_ID(ENT_CIRCULAR_SAW, 4),
                       FIELD_X, y, FIELD_W, &p->y))
        editor_commit_change(es);
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "patrol_x0:");
    if (ui_float_field(&es->ui, FIELD_ID(ENT_CIRCULAR_SAW, 1),
                       FIELD_X, y, FIELD_W, &p->patrol_x0))
        editor_commit_change(es);
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "patrol_x1:");
    if (ui_float_field(&es->ui, FIELD_ID(ENT_CIRCULAR_SAW, 2),
                       FIELD_X, y, FIELD_W, &p->patrol_x1))
        editor_commit_change(es);
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "direction:");
    if (ui_int_field(&es->ui, FIELD_ID(ENT_CIRCULAR_SAW, 3),
                     FIELD_X, y, FIELD_W, &p->direction))
        editor_commit_change(es);
}

/* draw_spike_row_properties — Fields for the selected spike row. */
static void draw_spike_row_properties(EditorState *es, int y)
{
    SpikeRowPlacement *p =
        &es->level.spike_rows[es->selection.index];

    ui_label(&es->ui, CONTENT_X, y, "x:");
    if (ui_float_field(&es->ui, FIELD_ID(ENT_SPIKE_ROW, 0),
                       FIELD_X, y, FIELD_W, &p->x))
        editor_commit_change(es);
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "count:");
    if (ui_int_field(&es->ui, FIELD_ID(ENT_SPIKE_ROW, 1),
                     FIELD_X, y, FIELD_W, &p->count))
        editor_commit_change(es);
}

/* draw_spike_platform_properties — Fields for the selected spike platform. */
static void draw_spike_platform_properties(EditorState *es, int y)
{
    SpikePlatformPlacement *p =
        &es->level.spike_platforms[es->selection.index];

    ui_label(&es->ui, CONTENT_X, y, "x:");
    if (ui_float_field(&es->ui, FIELD_ID(ENT_SPIKE_PLATFORM, 0),
                       FIELD_X, y, FIELD_W, &p->x))
        editor_commit_change(es);
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "y:");
    if (ui_float_field(&es->ui, FIELD_ID(ENT_SPIKE_PLATFORM, 1),
                       FIELD_X, y, FIELD_W, &p->y))
        editor_commit_change(es);
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "tile_count:");
    if (ui_int_field(&es->ui, FIELD_ID(ENT_SPIKE_PLATFORM, 2),
                     FIELD_X, y, FIELD_W, &p->tile_count))
        editor_commit_change(es);
}

/* draw_spike_block_properties — Fields for the selected spike block. */
static void draw_spike_block_properties(EditorState *es, int y)
{
    SpikeBlockPlacement *p =
        &es->level.spike_blocks[es->selection.index];

    ui_label(&es->ui, CONTENT_X, y, "rail_index:");
    if (ui_int_field(&es->ui, FIELD_ID(ENT_SPIKE_BLOCK, 0),
                     FIELD_X, y, FIELD_W, &p->rail_index))
        editor_commit_change(es);
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "t_offset:");
    if (ui_float_field(&es->ui, FIELD_ID(ENT_SPIKE_BLOCK, 1),
                       FIELD_X, y, FIELD_W, &p->t_offset))
        editor_commit_change(es);
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "speed:");
    if (ui_float_field_limited(&es->ui, FIELD_ID(ENT_SPIKE_BLOCK, 2),
                               FIELD_X, y, FIELD_W, &p->speed,
                               RAIL_SPEED_MIN, MAX_RAIL_SPEED))
        editor_commit_change(es);
}

/* draw_blue_flame_properties — Fields for the selected blue flame. */
static void draw_blue_flame_properties(EditorState *es, int y)
{
    BlueFlamePlacement *p =
        &es->level.blue_flames[es->selection.index];

    ui_label(&es->ui, CONTENT_X, y, "x:");
    if (ui_float_field(&es->ui, FIELD_ID(ENT_BLUE_FLAME, 0),
                       FIELD_X, y, FIELD_W, &p->x)) {
        /* A typed x lands on the nearest gap, as a click would. */
        p->x = editor_nearest_floor_gap(&es->level, p->x);
        editor_commit_change(es);
    }
}

/* draw_fire_flame_properties — Fields for the selected fire flame. */
static void draw_fire_flame_properties(EditorState *es, int y)
{
    FireFlamePlacement *p =
        &es->level.fire_flames[es->selection.index];

    ui_label(&es->ui, CONTENT_X, y, "x:");
    if (ui_float_field(&es->ui, FIELD_ID(ENT_FIRE_FLAME, 0),
                       FIELD_X, y, FIELD_W, &p->x)) {
        /* A typed x lands on the nearest gap, as a click would. */
        p->x = editor_nearest_floor_gap(&es->level, p->x);
        editor_commit_change(es);
    }
}

/* draw_float_platform_properties — Fields for the selected float platform. */
static void draw_float_platform_properties(EditorState *es, int y)
{
    FloatPlatformPlacement *p =
        &es->level.float_platforms[es->selection.index];

    /*
     * mode — dropdown selecting Static, Crumble, or Rail.
     * Cast FloatPlatformMode to int for the dropdown widget.
     */
    int mode_sel = (int)p->mode;
    ui_label(&es->ui, CONTENT_X, y, "mode:");
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

    ui_label(&es->ui, CONTENT_X, y, "x:");
    if (ui_float_field(&es->ui, FIELD_ID(ENT_FLOAT_PLATFORM, 1),
                       FIELD_X, y, FIELD_W, &p->x))
        editor_commit_change(es);
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "y:");
    if (ui_float_field(&es->ui, FIELD_ID(ENT_FLOAT_PLATFORM, 2),
                       FIELD_X, y, FIELD_W, &p->y))
        editor_commit_change(es);
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "tile_count:");
    if (ui_int_field(&es->ui, FIELD_ID(ENT_FLOAT_PLATFORM, 3),
                     FIELD_X, y, FIELD_W, &p->tile_count))
        editor_commit_change(es);
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "rail_index:");
    /* A rail rider's index must name an existing rail; other modes ignore
     * it, and the switch to Rail checks it then. */
    if (ui_int_field_limited(&es->ui, FIELD_ID(ENT_FLOAT_PLATFORM, 4),
                             FIELD_X, y, FIELD_W, &p->rail_index,
                             p->mode == FLOAT_PLATFORM_RAIL ? 0 : INT_MIN,
                             p->mode == FLOAT_PLATFORM_RAIL && es->level.rail_count > 0
                                 ? es->level.rail_count - 1 : INT_MAX, 1))
        editor_commit_change(es);
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "t_offset:");
    if (ui_float_field(&es->ui, FIELD_ID(ENT_FLOAT_PLATFORM, 5),
                       FIELD_X, y, FIELD_W, &p->t_offset))
        editor_commit_change(es);
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "speed:");
    /* Only a rail rider's speed has to lie in the rail range. */
    if (ui_float_field_limited(&es->ui, FIELD_ID(ENT_FLOAT_PLATFORM, 6),
                               FIELD_X, y, FIELD_W, &p->speed,
                               p->mode == FLOAT_PLATFORM_RAIL ? RAIL_SPEED_MIN : -FLT_MAX,
                               p->mode == FLOAT_PLATFORM_RAIL ? MAX_RAIL_SPEED : FLT_MAX))
        editor_commit_change(es);
}

/* draw_bridge_properties — Fields for the selected bridge. */
static void draw_bridge_properties(EditorState *es, int y)
{
    BridgePlacement *p = &es->level.bridges[es->selection.index];

    ui_label(&es->ui, CONTENT_X, y, "x:");
    if (ui_float_field(&es->ui, FIELD_ID(ENT_BRIDGE, 0),
                       FIELD_X, y, FIELD_W, &p->x))
        editor_commit_change(es);
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "y:");
    if (ui_float_field(&es->ui, FIELD_ID(ENT_BRIDGE, 1),
                       FIELD_X, y, FIELD_W, &p->y))
        editor_commit_change(es);
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "brick_count:");
    if (ui_int_field(&es->ui, FIELD_ID(ENT_BRIDGE, 2),
                     FIELD_X, y, FIELD_W, &p->brick_count))
        editor_commit_change(es);
}

/* draw_bouncepad_small_properties — Fields for the selected bouncepad small. */
static void draw_bouncepad_small_properties(EditorState *es, int y)
{
    BouncepadPlacement *p =
        &es->level.bouncepads_small[es->selection.index];

    /*
     * BouncepadType is fixed per array (BOUNCEPAD_GREEN for small),
     * so we just show a read-only label instead of a dropdown.
     */
    ui_label(&es->ui, CONTENT_X, y, "type: Small (Green)");
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "x:");
    if (ui_float_field(&es->ui, FIELD_ID(ENT_BOUNCEPAD_SMALL, 0),
                       FIELD_X, y, FIELD_W, &p->x))
        editor_commit_change(es);
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "launch_vy:");
    if (ui_float_field_limited(&es->ui, FIELD_ID(ENT_BOUNCEPAD_SMALL, 1),
                               FIELD_X, y, FIELD_W, &p->launch_vy,
                               -MAX_LEVEL_MOTION, BOUNCE_LAUNCH_VY_MAX))
        editor_commit_change(es);
}

/* draw_bouncepad_medium_properties — Fields for the selected bouncepad medium. */
static void draw_bouncepad_medium_properties(EditorState *es, int y)
{
    BouncepadPlacement *p =
        &es->level.bouncepads_medium[es->selection.index];

    ui_label(&es->ui, CONTENT_X, y, "type: Medium (Wood)");
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "x:");
    if (ui_float_field(&es->ui, FIELD_ID(ENT_BOUNCEPAD_MEDIUM, 0),
                       FIELD_X, y, FIELD_W, &p->x))
        editor_commit_change(es);
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "launch_vy:");
    if (ui_float_field_limited(&es->ui, FIELD_ID(ENT_BOUNCEPAD_MEDIUM, 1),
                               FIELD_X, y, FIELD_W, &p->launch_vy,
                               -MAX_LEVEL_MOTION, BOUNCE_LAUNCH_VY_MAX))
        editor_commit_change(es);
}

/* draw_bouncepad_high_properties — Fields for the selected bouncepad high. */
static void draw_bouncepad_high_properties(EditorState *es, int y)
{
    BouncepadPlacement *p =
        &es->level.bouncepads_high[es->selection.index];

    ui_label(&es->ui, CONTENT_X, y, "type: High (Red)");
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "x:");
    if (ui_float_field(&es->ui, FIELD_ID(ENT_BOUNCEPAD_HIGH, 0),
                       FIELD_X, y, FIELD_W, &p->x))
        editor_commit_change(es);
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "launch_vy:");
    if (ui_float_field_limited(&es->ui, FIELD_ID(ENT_BOUNCEPAD_HIGH, 1),
                               FIELD_X, y, FIELD_W, &p->launch_vy,
                               -MAX_LEVEL_MOTION, BOUNCE_LAUNCH_VY_MAX))
        editor_commit_change(es);
}

/* draw_vine_properties — Fields for the selected vine. */
static void draw_vine_properties(EditorState *es, int y)
{
    VinePlacement *p = &es->level.vines[es->selection.index];

    ui_label(&es->ui, CONTENT_X, y, "x:");
    if (ui_float_field(&es->ui, FIELD_ID(ENT_VINE, 0),
                       FIELD_X, y, FIELD_W, &p->x))
        editor_commit_change(es);
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "y:");
    if (ui_float_field(&es->ui, FIELD_ID(ENT_VINE, 1),
                       FIELD_X, y, FIELD_W, &p->y))
        editor_commit_change(es);
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "tile_count:");
    if (ui_int_field(&es->ui, FIELD_ID(ENT_VINE, 2),
                     FIELD_X, y, FIELD_W, &p->tile_count))
        editor_commit_change(es);
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "vine_type:");
    if (ui_dropdown(&es->ui, FIELD_ID(ENT_VINE, 3),
                    FIELD_X, y, FIELD_W,
                    vine_type_opts, 2, &p->vine_type))
        editor_commit_change(es);
}

/* draw_ladder_properties — Fields for the selected ladder. */
static void draw_ladder_properties(EditorState *es, int y)
{
    LadderPlacement *p = &es->level.ladders[es->selection.index];

    ui_label(&es->ui, CONTENT_X, y, "x:");
    if (ui_float_field(&es->ui, FIELD_ID(ENT_LADDER, 0),
                       FIELD_X, y, FIELD_W, &p->x))
        editor_commit_change(es);
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "y:");
    if (ui_float_field(&es->ui, FIELD_ID(ENT_LADDER, 1),
                       FIELD_X, y, FIELD_W, &p->y))
        editor_commit_change(es);
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "tile_count:");
    if (ui_int_field(&es->ui, FIELD_ID(ENT_LADDER, 2),
                     FIELD_X, y, FIELD_W, &p->tile_count))
        editor_commit_change(es);
}

/* draw_rope_properties — Fields for the selected rope. */
static void draw_rope_properties(EditorState *es, int y)
{
    RopePlacement *p = &es->level.ropes[es->selection.index];

    ui_label(&es->ui, CONTENT_X, y, "x:");
    if (ui_float_field(&es->ui, FIELD_ID(ENT_ROPE, 0),
                       FIELD_X, y, FIELD_W, &p->x))
        editor_commit_change(es);
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "y:");
    if (ui_float_field(&es->ui, FIELD_ID(ENT_ROPE, 1),
                       FIELD_X, y, FIELD_W, &p->y))
        editor_commit_change(es);
    y += ROW_H;

    ui_label(&es->ui, CONTENT_X, y, "tile_count:");
    if (ui_int_field(&es->ui, FIELD_ID(ENT_ROPE, 2),
                     FIELD_X, y, FIELD_W, &p->tile_count))
        editor_commit_change(es);
}


/* ------------------------------------------------------------------ */
/* Which function draws each type's fields                             */
/* ------------------------------------------------------------------ */

/*
 * s_property_panels — one draw_<type>_properties function per EntityType,
 * in the enum order of editor.h.  The table is sized by its rows, so the
 * _Static_assert fails the build when a type added at the end of the enum
 * has no panel yet.  Every function takes the panel's first row y.
 */
typedef void (*PropertyPanelFn)(EditorState *es, int y);

static const PropertyPanelFn s_property_panels[] = {
    [ENT_FLOOR_GAP]        = draw_floor_gap_properties,
    [ENT_CHECKPOINT]       = draw_checkpoint_properties,
    [ENT_RAIL]             = draw_rail_properties,
    [ENT_PLATFORM]         = draw_platform_properties,
    [ENT_COIN]             = draw_coin_properties,
    [ENT_STAR_YELLOW]      = draw_star_yellow_properties,
    [ENT_STAR_GREEN]       = draw_star_green_properties,
    [ENT_STAR_RED]         = draw_star_red_properties,
    [ENT_LAST_STAR]        = draw_last_star_properties,
    [ENT_SPIDER]           = draw_spider_properties,
    [ENT_JUMPING_SPIDER]   = draw_jumping_spider_properties,
    [ENT_BIRD]             = draw_bird_properties,
    [ENT_FASTER_BIRD]      = draw_faster_bird_properties,
    [ENT_FISH]             = draw_fish_properties,
    [ENT_FASTER_FISH]      = draw_faster_fish_properties,
    [ENT_AXE_TRAP]         = draw_axe_trap_properties,
    [ENT_CIRCULAR_SAW]     = draw_circular_saw_properties,
    [ENT_SPIKE_ROW]        = draw_spike_row_properties,
    [ENT_SPIKE_PLATFORM]   = draw_spike_platform_properties,
    [ENT_SPIKE_BLOCK]      = draw_spike_block_properties,
    [ENT_BLUE_FLAME]       = draw_blue_flame_properties,
    [ENT_FIRE_FLAME]       = draw_fire_flame_properties,
    [ENT_FLOAT_PLATFORM]   = draw_float_platform_properties,
    [ENT_BRIDGE]           = draw_bridge_properties,
    [ENT_BOUNCEPAD_SMALL]  = draw_bouncepad_small_properties,
    [ENT_BOUNCEPAD_MEDIUM] = draw_bouncepad_medium_properties,
    [ENT_BOUNCEPAD_HIGH]   = draw_bouncepad_high_properties,
    [ENT_VINE]             = draw_vine_properties,
    [ENT_LADDER]           = draw_ladder_properties,
    [ENT_ROPE]             = draw_rope_properties,
    [ENT_PLAYER_SPAWN]     = draw_player_spawn_properties,
};

_Static_assert(sizeof(s_property_panels) / sizeof(s_property_panels[0]) == ENT_COUNT,
               "s_property_panels needs one draw function per EntityType");

/* The function that draws `type`'s fields, or NULL for an invalid type. */
static PropertyPanelFn property_panel(EntityType type)
{
    if (type < 0 || type >= ENT_COUNT) return NULL;
    return s_property_panels[type];
}

/* ------------------------------------------------------------------ */
/* properties_render                                                   */
/* ------------------------------------------------------------------ */

void properties_render(EditorState *es, int start_y, int available_h)
{
    if (!es) return;
    editor_selection_reconcile(es);
    if (!editor_selection_is_valid(es))
        return;

    /*
     * Use caller-supplied position instead of the old fixed PROP_Y / PROP_H
     * constants.  The layout orchestrator in editor.c computes where this
     * section starts and how tall it is based on the sections above it.
     */
    int prop_x = PROP_X;
    int prop_y = start_y;
    int prop_h = available_h;

    /* Panel background, then the "Coin #3" title bar that collapses it.
     * Singletons (Last Star, Player Spawn) have no index to show. */
    char header[64];
    if (editor_entity_type_is_singleton(es->selection.type)) {
        snprintf(header, sizeof(header), " %s",
                 editor_entity_type_name(es->selection.type));
    } else {
        snprintf(header, sizeof(header), " %s #%d",
                 editor_entity_type_name(es->selection.type),
                 es->selection.index);
    }
    ui_panel(&es->ui, prop_x, prop_y, PROP_W, prop_h);
    draw_section_title(es, prop_x, prop_y, header, &es->panel_open);

    if (!es->panel_open) return;

    editor_begin_change_tracking(es, 1);
    es->ui.before_change = editor_before_change;
    es->ui.before_change_context = es;

    /* Clip content below the title bar */
    BeginScissorMode(prop_x, prop_y+ROW_H+4, PROP_W, prop_h-ROW_H-4);

    int y = prop_y + ROW_H + 8;

    /* ---- Per-type field rendering ----------------------------------- */

    /*
     * The selected type's draw_<type>_properties function (table above)
     * draws its fields by pointer into the placement, so the ui_*_field
     * widgets read and write the value in place, and commits each edit
     * as one undoable property command.
     */
    PropertyPanelFn draw_fields = property_panel(es->selection.type);
    if (draw_fields) draw_fields(es, y);

    EndScissorMode();
    editor_end_change_tracking(es);
}

/* ================================================================== */
/* level_config_render — Level-wide configuration panel                 */
/* ================================================================== */

/*
 * The Level Config panel is drawn top to bottom by the config_* helpers
 * below, one per group of settings.  Each takes the running row position y
 * and returns where the next group starts, so level_config_render reads as
 * the list of groups in panel order.
 *
 * Widget IDs use the 9000+ range to avoid collisions with entity fields
 * (FIELD_ID stays below 9000).  Every ID is a literal next to its widget,
 * so moving a field between helpers cannot change which ID it has.
 */

/* Is the mouse over the 18-px header row of a subsection at y? */
static int config_row_hovered(const EditorState *es, int x, int y)
{
    return es->ui.mouse_x >= x &&
           es->ui.mouse_x < x + PROP_W &&
           es->ui.mouse_y >= y &&
           es->ui.mouse_y < y + 18;
}

/*
 * config_subsection_header — Draw a ">"/"v" subsection header such as
 * "Physics" or "Background Layers (2)" and toggle *open when it is clicked.
 * The caller decides the hover area (the fog header uses a narrower one).
 */
static void config_subsection_header(EditorState *es, int x, int y,
                                     const char *label, int *open, int hovered)
{
    if (hovered && es->ui.mouse_clicked) *open = !*open;

    const char *sym = *open ? "v" : ">";
    int sym_w = ui_text_width(&es->ui, sym);
    ui_label_color(&es->ui, x + 8, y, sym, UI_ACCENT);
    ui_label_color(&es->ui, x + 8 + sym_w, y, label,
                   hovered ? UI_TEXT : UI_TEXT_DIM);
}

/* Validation result and every error/warning message. */
static int config_validation(EditorState *es, int x, int y)
{
    ui_label_color(&es->ui, x + 8, y,
                   editor_validation_summary(&es->validation_report),
                   es->validation_report.error_count > 0 ?
                   (Color){0xFF,0x70,0x70,0xFF} : UI_TEXT_DIM);
    y += 20;
    for (int i = 0; i < es->validation_report.message_count; i++) {
        Color msg_color = i < es->validation_report.error_count
                            ? (Color){0xFF,0x70,0x70,0xFF}
                            : (Color){0xFF,0xC0,0x60,0xFF};
        ui_label_color(&es->ui, x + 16, y,
                       es->validation_report.messages[i], msg_color);
        y += 18;
    }
    ui_separator(&es->ui, x + 4, y, PROP_W - 8);
    return y + 8;
}

/* The recent-file list with its Ctrl+1..5 shortcuts (nothing when empty). */
static int config_recent_files(EditorState *es, int x, int y)
{
    if (es->recent_file_count <= 0) return y;

    ui_label_color(&es->ui, x + 8, y, "Recent files (Ctrl+1..5):", UI_TEXT_DIM);
    y += 18;
    for (int i = 0; i < es->recent_file_count; i++) {
        /* Keep each entry to one panel-width line: the file name at the
         * end of the path is the part that tells entries apart. */
        char shown_path[48];
        char recent[64];
        editor_path_for_display(es->recent_files[i], shown_path, sizeof(shown_path));
        snprintf(recent, sizeof(recent), "%d: %s", i + 1, shown_path);
        ui_label_color(&es->ui, x + 16, y, recent, UI_TEXT_DIM);
        y += 18;
    }
    ui_separator(&es->ui, x + 4, y, PROP_W - 8);
    return y + 8;
}

/* Name, description, author, world width and the next-phase path. */
static int config_level_fields(EditorState *es, int x, int y)
{
    ui_label(&es->ui, x + 8, y, "Name:");
    if (ui_text_field(&es->ui, 9000, x + 55, y, 310, es->level.name,
                      (int)sizeof(es->level.name)))
        editor_commit_change(es);
    y += 24;

    ui_label(&es->ui, x + 8, y, "Desc:");
    if (ui_text_field(&es->ui, 9001, x + 55, y, 310, es->level.description,
                      (int)sizeof(es->level.description)))
        editor_commit_change(es);
    y += 24;

    ui_label(&es->ui, x + 8, y, "By:");
    if (ui_text_field(&es->ui, 9002, x + 55, y, 310, es->level.generated_by,
                      (int)sizeof(es->level.generated_by)))
        editor_commit_change(es);
    y += 24;

    /* ---- World Width (screen count) ---- */
    ui_label(&es->ui, x + 8, y, "Screens:");
    /* The limits live in the field, so a value applied by clicking away
     * (the Apply choice) is clamped exactly like one confirmed with Return. */
    if (ui_int_field_limited(&es->ui, 9011, x + 80, y, 50,
                             &es->level.screen_count, 1, 99, 1))
        editor_commit_change(es);
    {
        char width_text[32];
        snprintf(width_text, sizeof(width_text), "= %lldpx",
                 (long long)es->level.screen_count * GAME_W);
        ui_label_color(&es->ui, x + 140, y, width_text, UI_TEXT_DIM);
    }
    y += 24;

    /* ---- Next phase path ---- */
    ui_label(&es->ui, x + 8, y, "Next:");
    if (ui_text_field(&es->ui, 9031, x + 55, y, 310, es->level.next_phase,
                      (int)sizeof(es->level.next_phase)))
        editor_commit_change(es);
    return y + 24;
}

/* Background sound: which loop plays, and how loud. */
static int config_music(EditorState *es, int x, int y)
{
    static const char *music_names[] = {
        "(none)", "water.wav", "lava.wav", "winds.wav"
    };
    static const char *music_paths[] = {
        "",
        "assets/sounds/levels/water.wav",
        "assets/sounds/levels/lava.wav",
        "assets/sounds/levels/winds.wav"
    };
    static const int music_count = 4;

    ui_separator(&es->ui, x + 4, y, PROP_W - 8);
    y += 6;
    ui_label(&es->ui, x + 8, y, "Background Sound:");
    {
        int sel = option_index(es->level.music_path, music_paths, music_count);
        if (ui_dropdown(&es->ui, 9009, x + 160, y, 210,
                         music_names, music_count, &sel)) {
            strncpy(es->level.music_path, music_paths[sel],
                    sizeof(es->level.music_path) - 1);
            es->level.music_path[sizeof(es->level.music_path) - 1] = '\0';
            editor_commit_change(es);
        }
    }
    y += 22;
    ui_label(&es->ui, x + 8, y, "vol:");
    /* The same 0..128 range the validator and the TOML format accept. */
    if (ui_int_field_limited(&es->ui, 9003, x + 50, y, 80,
                             &es->level.music_volume, 0,
                             LEVEL_MUSIC_VOLUME_MAX, 1))
        editor_commit_change(es);
    return y + 24;
}

/* The floor's tileset. */
static int config_floor_tile(EditorState *es, int x, int y)
{
    /* Display names (shown in dropdown) */
    static const char *floor_tile_names[] = {
        "grass_tileset.png",
        "brick_tileset.png",
        "cloud_tileset.png",
        "grass_rock_tileset.png",
        "leaf_tileset.png",
        "stone_tileset.png"
    };
    static const char *floor_tile_paths[] = {
        "assets/sprites/levels/grass_tileset.png",
        "assets/sprites/levels/brick_tileset.png",
        "assets/sprites/levels/cloud_tileset.png",
        "assets/sprites/levels/grass_rock_tileset.png",
        "assets/sprites/levels/leaf_tileset.png",
        "assets/sprites/levels/stone_tileset.png"
    };
    static const int floor_tile_count = 6;

    ui_separator(&es->ui, x + 4, y, PROP_W - 8);
    y += 6;
    ui_label(&es->ui, x + 8, y, "Floor Tile:");
    {
        int sel = option_index(es->level.floor_tile_path, floor_tile_paths,
                               floor_tile_count);
        if (ui_dropdown(&es->ui, 9010, x + 100, y, 270,
                         floor_tile_names, floor_tile_count, &sel)) {
            strncpy(es->level.floor_tile_path, floor_tile_paths[sel],
                    sizeof(es->level.floor_tile_path) - 1);
            es->level.floor_tile_path[sizeof(es->level.floor_tile_path) - 1] = '\0';
            editor_commit_change(es);
        }
    }
    return y + 24;
}

/* Starting hearts and lives, and the score rules. */
static int config_lives_and_score(EditorState *es, int x, int y)
{
    ui_separator(&es->ui, x + 4, y, PROP_W - 8);
    y += 6;
    ui_label(&es->ui, x + 8, y, "hearts:");
    if (ui_int_field_limited(&es->ui, 9006, x + 70, y, 60,
                             &es->level.initial_hearts, 1, 3, 1))
        editor_commit_change(es);
    ui_label(&es->ui, x + 150, y, "lives:");
    if (ui_int_field_limited(&es->ui, 9007, x + 205, y, 60,
                             &es->level.initial_lives, 0, 99, 1))
        editor_commit_change(es);
    y += 22;
    ui_label(&es->ui, x + 8, y, "pts/life:");
    if (ui_int_field(&es->ui, 9008, x + 80, y, 80, &es->level.score_per_life))
        editor_commit_change(es);
    ui_label(&es->ui, x + 180, y, "coin pts:");
    if (ui_int_field(&es->ui, 9012, x + 240, y, 60, &es->level.coin_score))
        editor_commit_change(es);
    return y + 24;
}

/* Movement physics overrides — a collapsible two-column grid. */
static int config_physics(EditorState *es, int x, int y)
{
    /*
     * Two-column layout constants for the physics grid.
     * Each column holds a label and an input field side by side.
     *
     * Column 1: label at COL1_L, field at COL1_F (width PHYS_FW = 82)
     *   → field ends at x+178, leaving 18 px gap before COL2_L.
     * Column 2: label at COL2_L, field at COL2_F (width PHYS_FW = 82)
     *   → field ends at x+350, right margin = 26 px.
     *
     * The 18 px gap between left field and right label prevents the two
     * columns from visually merging into a single unreadable block.
     */
#define COL1_L  (x +  8)
#define COL1_F  (x + 96)
#define COL2_L  (x + 196)
#define COL2_F  (x + 268)
#define PHYS_FW  82

    ui_separator(&es->ui, x + 4, y, PROP_W - 8);
    y += 6;
    config_subsection_header(es, x, y, " Physics (-1 = engine default)",
                             &g_phys_open, config_row_hovered(es, x, y));
    y += 20;

    if (g_phys_open) {
        /* -- Walk / Run speeds -- */
        ui_label(&es->ui, COL1_L, y, "walk spd:");
        if (ui_float_field(&es->ui, 9020, COL1_F, y, PHYS_FW, &es->level.physics.walk_max_speed))
            editor_commit_change(es);
        ui_label(&es->ui, COL2_L, y, "run spd:");
        if (ui_float_field(&es->ui, 9021, COL2_F, y, PHYS_FW, &es->level.physics.run_max_speed))
            editor_commit_change(es);
        y += 22;

        /* -- Ground acceleration -- */
        ui_label(&es->ui, COL1_L, y, "walk accel:");
        if (ui_float_field(&es->ui, 9022, COL1_F, y, PHYS_FW, &es->level.physics.walk_ground_accel))
            editor_commit_change(es);
        ui_label(&es->ui, COL2_L, y, "run accel:");
        if (ui_float_field(&es->ui, 9023, COL2_F, y, PHYS_FW, &es->level.physics.run_ground_accel))
            editor_commit_change(es);
        y += 22;

        /* -- Ground friction / counter -- */
        ui_label(&es->ui, COL1_L, y, "friction:");
        if (ui_float_field(&es->ui, 9024, COL1_F, y, PHYS_FW, &es->level.physics.ground_friction))
            editor_commit_change(es);
        ui_label(&es->ui, COL2_L, y, "counter:");
        if (ui_float_field(&es->ui, 9025, COL2_F, y, PHYS_FW, &es->level.physics.ground_counter_accel))
            editor_commit_change(es);
        y += 22;

        /* -- Air acceleration -- */
        ui_label(&es->ui, COL1_L, y, "air walk:");
        if (ui_float_field(&es->ui, 9026, COL1_F, y, PHYS_FW, &es->level.physics.air_accel_walk))
            editor_commit_change(es);
        ui_label(&es->ui, COL2_L, y, "air run:");
        if (ui_float_field(&es->ui, 9027, COL2_F, y, PHYS_FW, &es->level.physics.air_accel_run))
            editor_commit_change(es);
        y += 22;

        /* -- Air friction -- */
        ui_label(&es->ui, COL1_L, y, "air fric:");
        if (ui_float_field(&es->ui, 9028, COL1_F, y, PHYS_FW, &es->level.physics.air_friction))
            editor_commit_change(es);
        y += 22;

        /* -- Camera lookahead -- */
        ui_label(&es->ui, COL1_L, y, "cam vx:");
        if (ui_float_field(&es->ui, 9029, COL1_F, y, PHYS_FW, &es->level.physics.cam_lookahead_vx_factor))
            editor_commit_change(es);
        ui_label(&es->ui, COL2_L, y, "cam max:");
        if (ui_float_field(&es->ui, 9030, COL2_F, y, PHYS_FW, &es->level.physics.cam_lookahead_max))
            editor_commit_change(es);
        y += 22;
    }
#undef COL1_L
#undef COL1_F
#undef COL2_L
#undef COL2_F
#undef PHYS_FW
    return y;
}

/*
 * LayerChoices — the assets offered for one kind of layer (background,
 * foreground or fog) and the widget IDs its rows use: row i's dropdown is
 * dropdown_id + i and its speed field speed_id + i.
 */
typedef struct {
    const char **names;   /* dropdown labels                */
    const char **paths;   /* the asset path for each label  */
    int          count;
    int          dropdown_id;
    int          speed_id;
} LayerChoices;

/* One "i: [asset] spd: [speed]" row of a layer list. */
static int config_layer_row(EditorState *es, int x, int y, int i,
                            const LayerChoices *choices,
                            char *path, size_t path_size, float *speed)
{
    char label[16];
    snprintf(label, sizeof(label), "%d:", i);
    ui_label(&es->ui, x + 8, y, label);

    int sel = option_index(path, choices->paths, choices->count);
    if (ui_dropdown(&es->ui, choices->dropdown_id + i, x + 28, y, 200,
                     choices->names, choices->count, &sel)) {
        strncpy(path, choices->paths[sel], path_size - 1);
        editor_commit_change(es);
    }
    ui_label(&es->ui, x + 236, y, "spd:");
    if (ui_float_field(&es->ui, choices->speed_id + i, x + 265, y, 60, speed))
        editor_commit_change(es);
    return y + 20;
}

/*
 * config_layer_buttons — "+ Add" and "- Remove Last" under a layer list.
 *
 * *count is the list's layer count and max its capacity.  next_path and
 * next_speed are the fields of the first unused slot (NULL when the list is
 * full); Add fills them with default_path and default_speed.
 */
static int config_layer_buttons(EditorState *es, int x, int y,
                                int *count, int max,
                                char *next_path, size_t path_size,
                                float *next_speed,
                                const char *default_path, float default_speed)
{
    if (*count < max) {
        if (ui_button(&es->ui, x + 8, y, 80, 20, "+ Add")) {
            editor_capture_change_before(es, -1);
            strncpy(next_path, default_path, path_size - 1);
            *next_speed = default_speed;
            (*count)++;
            editor_commit_change(es);
        }
    }
    if (*count > 0) {
        if (ui_button(&es->ui, x + 96, y, 100, 20, "- Remove Last")) {
            editor_capture_change_before(es, -1);
            (*count)--;
            editor_commit_change(es);
        }
    }
    return y + 24;
}

/* Parallax background layers — a collapsible list. */
static int config_background_layers(EditorState *es, int x, int y)
{
    static const char *bg_names[] = {
        "sky_blue.png",
        "sky_blue_lightened.png",
        "castle_pillars.png",
        "forest_leafs.png",
        "clouds_bg.png",
        "glacial_mountains.png",
        "glacial_mountains_lightened.png",
        "clouds_mg_3.png",
        "clouds_mg_2.png",
        "clouds_lonely.png",
        "clouds_mg_1.png",
        "clouds_mg_1_lightened.png"
    };
    static const char *bg_paths[] = {
        "assets/sprites/backgrounds/sky_blue.png",
        "assets/sprites/backgrounds/sky_blue_lightened.png",
        "assets/sprites/backgrounds/castle_pillars.png",
        "assets/sprites/backgrounds/forest_leafs.png",
        "assets/sprites/backgrounds/clouds_bg.png",
        "assets/sprites/backgrounds/glacial_mountains.png",
        "assets/sprites/backgrounds/glacial_mountains_lightened.png",
        "assets/sprites/backgrounds/clouds_mg_3.png",
        "assets/sprites/backgrounds/clouds_mg_2.png",
        "assets/sprites/backgrounds/clouds_lonely.png",
        "assets/sprites/backgrounds/clouds_mg_1.png",
        "assets/sprites/backgrounds/clouds_mg_1_lightened.png"
    };
    static const LayerChoices choices = { bg_names, bg_paths, 12, 9200, 9100 };
    LevelDef *level = &es->level;
    char label[48];

    ui_separator(&es->ui, x + 4, y, PROP_W - 8);
    y += 6;
    snprintf(label, sizeof(label), " Background Layers (%d)",
             level->background_layer_count);
    config_subsection_header(es, x, y, label, &g_plx_open,
                             config_row_hovered(es, x, y));
    y += 18;
    if (!g_plx_open) return y;

    for (int i = 0; i < level->background_layer_count && i < MAX_BACKGROUND_LAYERS; i++)
        y = config_layer_row(es, x, y, i, &choices,
                             level->background_layers[i].path,
                             sizeof(level->background_layers[i].path),
                             &level->background_layers[i].speed);

    int next = level->background_layer_count;
    int has_room = next < MAX_BACKGROUND_LAYERS;
    return config_layer_buttons(es, x, y, &level->background_layer_count,
                                MAX_BACKGROUND_LAYERS,
                                has_room ? level->background_layers[next].path : NULL,
                                sizeof(level->background_layers[0].path),
                                has_room ? &level->background_layers[next].speed : NULL,
                                "assets/sprites/backgrounds/sky_blue.png", 0.1f);
}

/* Foreground strips (water, lava, clouds) drawn over the floor. */
static int config_foreground_layers(EditorState *es, int x, int y)
{
    static const char *fg_names[] = {
        "fog_1.png",
        "fog_2.png",
        "water.png",
        "clouds.png",
        "lava.png"
    };
    static const char *fg_paths[] = {
        "assets/sprites/foregrounds/fog_1.png",
        "assets/sprites/foregrounds/fog_2.png",
        "assets/sprites/foregrounds/water.png",
        "assets/sprites/foregrounds/clouds.png",
        "assets/sprites/foregrounds/lava.png"
    };
    static const LayerChoices choices = { fg_names, fg_paths, 5, 9300, 9400 };
    LevelDef *level = &es->level;
    char label[48];

    ui_separator(&es->ui, x + 4, y, PROP_W - 8);
    y += 6;
    snprintf(label, sizeof(label), " Foreground Layers (%d)",
             level->foreground_layer_count);
    config_subsection_header(es, x, y, label, &g_fg_open,
                             config_row_hovered(es, x, y));
    y += 18;
    if (!g_fg_open) return y;

    for (int i = 0; i < level->foreground_layer_count && i < MAX_BACKGROUND_LAYERS; i++)
        y = config_layer_row(es, x, y, i, &choices,
                             level->foreground_layers[i].path,
                             sizeof(level->foreground_layers[i].path),
                             &level->foreground_layers[i].speed);

    int next = level->foreground_layer_count;
    int has_room = next < MAX_BACKGROUND_LAYERS;
    return config_layer_buttons(es, x, y, &level->foreground_layer_count,
                                MAX_BACKGROUND_LAYERS,
                                has_room ? level->foreground_layers[next].path : NULL,
                                sizeof(level->foreground_layers[0].path),
                                has_room ? &level->foreground_layers[next].speed : NULL,
                                "assets/sprites/foregrounds/fog_1.png", 0.5f);
}

/* Fog overlays that drift across the screen. */
static int config_fog_layers(EditorState *es, int x, int y)
{
    /* Fog asset dropdown options — includes original + fire/volcanic variants */
    static const char *fog_names[] = {
        "fog_1.png",
        "fog_2.png",
        "fog_fire_1.png",
        "fog_fire_2.png",
        "smoke.png",
    };
    static const char *fog_paths[] = {
        "assets/sprites/foregrounds/fog_1.png",
        "assets/sprites/foregrounds/fog_2.png",
        "assets/sprites/foregrounds/fog_fire_1.png",
        "assets/sprites/foregrounds/fog_fire_2.png",
        "assets/sprites/foregrounds/smoke.png",
    };
    static const LayerChoices choices = { fog_names, fog_paths, 5, 9600, 9700 };
    LevelDef *level = &es->level;
    char label[48];

    /* Unlike the other headers, this one has no separator above it and
     * reacts only over its left 200 px (edges included). */
    int hovered = (es->ui.mouse_x >= x &&
                   es->ui.mouse_x <= x + 200 &&
                   es->ui.mouse_y >= y &&
                   es->ui.mouse_y <= y + 18);
    snprintf(label, sizeof(label), " Fog Layers (%d)", level->fog_layer_count);
    config_subsection_header(es, x, y, label, &g_fog_open, hovered);
    y += 18;
    if (!g_fog_open) return y;

    for (int i = 0; i < level->fog_layer_count && i < MAX_FOG_TEXTURES; i++)
        y = config_layer_row(es, x, y, i, &choices,
                             level->fog_layers[i].path,
                             sizeof(level->fog_layers[i].path),
                             &level->fog_layers[i].speed);

    int next = level->fog_layer_count;
    int has_room = next < MAX_FOG_TEXTURES;
    return config_layer_buttons(es, x, y, &level->fog_layer_count,
                                MAX_FOG_TEXTURES,
                                has_room ? level->fog_layers[next].path : NULL,
                                sizeof(level->fog_layers[0].path),
                                has_room ? &level->fog_layers[next].speed : NULL,
                                "assets/sprites/foregrounds/fog_1.png", 0.5f);
}

/*
 * level_config_render — Show editable level-wide settings.
 *
 * Draws the collapsible "Level Config" section at the top of the right
 * column: validation messages, recent files, the level's name and size,
 * sound, floor, lives and score, physics, and the background, foreground
 * and fog layer lists.  total_content_h is the height of everything when
 * fully shown (editor_layout.c); content past available_h scrolls.
 */
void level_config_render(EditorState *es, int start_y, int available_h,
                         int total_content_h) {
    editor_begin_change_tracking(es, 2);
    es->ui.before_change = editor_before_change;
    es->ui.before_change_context = es;

    int x     = PROP_X;
    int cfg_h = available_h;

    ui_panel(&es->ui, x, start_y, PROP_W, cfg_h);
    draw_section_title(es, x, start_y, " Level Config", &es->config_open);

    if (!es->config_open) {
        editor_end_change_tracking(es);
        return;
    }

    /*
     * Scroll clamping — clamp cfg_scroll_y so we never scroll past the
     * last pixel of content.  title_h is the fixed header that never
     * scrolls; content_visible_h is the drawable area below it.
     */
    int title_h          = ROW_H + 4;
    int content_visible_h = cfg_h - title_h;
    int content_total_h   = total_content_h - title_h;
    int max_scroll = content_total_h - content_visible_h;
    if (max_scroll < 0)        max_scroll = 0;
    if (cfg_scroll_y > max_scroll) cfg_scroll_y = max_scroll;

    /*
     * Clip rect — restrict all content drawing to the area below the
     * title bar.  Anything that would scroll off the top or bottom edge
     * is invisible.  We clear the clip rect at the end of this function.
     */
    int content_top = start_y + title_h;
    BeginScissorMode(x, content_top, PROP_W, content_visible_h);

    /*
     * y — the running vertical cursor for content rendering.
     * Subtracting cfg_scroll_y shifts content upward as the user scrolls.
     */
    int y = content_top + 8 - cfg_scroll_y;
    y = config_validation(es, x, y);
    y = config_recent_files(es, x, y);
    y = config_level_fields(es, x, y);
    y = config_music(es, x, y);
    y = config_floor_tile(es, x, y);
    y = config_lives_and_score(es, x, y);
    y = config_physics(es, x, y);
    y = config_background_layers(es, x, y);
    y = config_foreground_layers(es, x, y);
    (void)config_fog_layers(es, x, y);

    EndScissorMode();
    editor_end_change_tracking(es);
}
