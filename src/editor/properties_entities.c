/*
 * properties_entities.c — One draw_<type>_properties function per entity.
 *
 * s_property_panels in properties.c calls these. The field ids, row layout
 * and label helper are shared (properties_internal.h) so a field moved
 * between types keeps the id the editor's focus path already knows.
 */

#include <float.h>      /* FLT_MAX for one-sided field limits    */
#include <limits.h>     /* INT_MIN, INT_MAX for unlimited int fields */
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

/*
 * draw_rail_properties — Fields for the selected rail: layout, position
 * (x, y in pixels), size (w, h in rail tiles) and end_cap.
 */
void draw_rail_properties(EditorState *es, int y)
{
    RailPlacement *p = &es->level.rails[es->selection.index];

    /*
     * layout — dropdown that selects between Rect and Horiz rail types.
     * Cast the enum to int for the dropdown, then cast back on change.
     */
    int layout_sel = (int)p->layout;
    field_label(es, y, "layout", FIELD_ID(ENT_RAIL, 0));
    if (ui_dropdown(&es->ui, FIELD_ID(ENT_RAIL, 0),
                    FIELD_X, y, FIELD_W,
                    rail_layout_opts, 2, &layout_sel)) {
        p->layout = (RailLayout)layout_sel;
        editor_commit_change(es);
    }
    y += ROW_H;

    field_label(es, y, "x", FIELD_ID(ENT_RAIL, 1));
    if (ui_int_field(&es->ui, FIELD_ID(ENT_RAIL, 1),
                     FIELD_X, y, FIELD_W, &p->x))
        editor_commit_change(es);
    y += ROW_H;

    field_label(es, y, "y", FIELD_ID(ENT_RAIL, 2));
    if (ui_int_field(&es->ui, FIELD_ID(ENT_RAIL, 2),
                     FIELD_X, y, FIELD_W, &p->y))
        editor_commit_change(es);
    y += ROW_H;

    field_label(es, y, "w", FIELD_ID(ENT_RAIL, 3));
    if (ui_int_field(&es->ui, FIELD_ID(ENT_RAIL, 3),
                     FIELD_X, y, FIELD_W, &p->w))
        editor_commit_change(es);
    y += ROW_H;

    field_label(es, y, "h", FIELD_ID(ENT_RAIL, 4));
    if (ui_int_field(&es->ui, FIELD_ID(ENT_RAIL, 4),
                     FIELD_X, y, FIELD_W, &p->h))
        editor_commit_change(es);
    y += ROW_H;

    field_label(es, y, "end_cap", FIELD_ID(ENT_RAIL, 5));
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

/* draw_coin_properties — Fields for the selected coin. */
void draw_coin_properties(EditorState *es, int y)
{
    CoinPlacement *p = &es->level.coins[es->selection.index];

    field_label(es, y, "x", FIELD_ID(ENT_COIN, 0));
    if (ui_float_field(&es->ui, FIELD_ID(ENT_COIN, 0),
                       FIELD_X, y, FIELD_W, &p->x))
        editor_commit_change(es);
    y += ROW_H;

    field_label(es, y, "y", FIELD_ID(ENT_COIN, 1));
    if (ui_float_field(&es->ui, FIELD_ID(ENT_COIN, 1),
                       FIELD_X, y, FIELD_W, &p->y))
        editor_commit_change(es);
}

/* draw_star_yellow_properties — Fields for the selected star yellow. */
void draw_star_yellow_properties(EditorState *es, int y)
{
    StarYellowPlacement *p =
        &es->level.star_yellows[es->selection.index];

    field_label(es, y, "x", FIELD_ID(ENT_STAR_YELLOW, 0));
    if (ui_float_field(&es->ui, FIELD_ID(ENT_STAR_YELLOW, 0),
                       FIELD_X, y, FIELD_W, &p->x))
        editor_commit_change(es);
    y += ROW_H;

    field_label(es, y, "y", FIELD_ID(ENT_STAR_YELLOW, 1));
    if (ui_float_field(&es->ui, FIELD_ID(ENT_STAR_YELLOW, 1),
                       FIELD_X, y, FIELD_W, &p->y))
        editor_commit_change(es);
}

/* draw_star_green_properties — Fields for the selected star green. */
void draw_star_green_properties(EditorState *es, int y)
{
    StarGreenPlacement *p =
        &es->level.star_greens[es->selection.index];

    field_label(es, y, "x", FIELD_ID(ENT_STAR_GREEN, 0));
    if (ui_float_field(&es->ui, FIELD_ID(ENT_STAR_GREEN, 0),
                       FIELD_X, y, FIELD_W, &p->x))
        editor_commit_change(es);
    y += ROW_H;

    field_label(es, y, "y", FIELD_ID(ENT_STAR_GREEN, 1));
    if (ui_float_field(&es->ui, FIELD_ID(ENT_STAR_GREEN, 1),
                       FIELD_X, y, FIELD_W, &p->y))
        editor_commit_change(es);
}

/* draw_star_red_properties — Fields for the selected star red. */
void draw_star_red_properties(EditorState *es, int y)
{
    StarRedPlacement *p =
        &es->level.star_reds[es->selection.index];

    field_label(es, y, "x", FIELD_ID(ENT_STAR_RED, 0));
    if (ui_float_field(&es->ui, FIELD_ID(ENT_STAR_RED, 0),
                       FIELD_X, y, FIELD_W, &p->x))
        editor_commit_change(es);
    y += ROW_H;

    field_label(es, y, "y", FIELD_ID(ENT_STAR_RED, 1));
    if (ui_float_field(&es->ui, FIELD_ID(ENT_STAR_RED, 1),
                       FIELD_X, y, FIELD_W, &p->y))
        editor_commit_change(es);
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

/* draw_player_spawn_properties — Fields for the selected player spawn. */
void draw_player_spawn_properties(EditorState *es, int y)
{
    /*
     * player_start_x / player_start_y are scalar fields in LevelDef,
     * not a struct like LastStarPlacement.  The selection index is
     * always 0 because there is exactly one player spawn per level.
     */
    field_label(es, y, "x", FIELD_ID(ENT_PLAYER_SPAWN, 0));
    if (ui_float_field(&es->ui, FIELD_ID(ENT_PLAYER_SPAWN, 0),
                       FIELD_X, y, FIELD_W,
                       &es->level.player_start_x))
        editor_commit_change(es);
    y += ROW_H;

    field_label(es, y, "y", FIELD_ID(ENT_PLAYER_SPAWN, 1));
    if (ui_float_field(&es->ui, FIELD_ID(ENT_PLAYER_SPAWN, 1),
                       FIELD_X, y, FIELD_W,
                       &es->level.player_start_y))
        editor_commit_change(es);
}

/* draw_spider_properties — Fields for the selected spider. */
void draw_spider_properties(EditorState *es, int y)
{
    SpiderPlacement *p = &es->level.spiders[es->selection.index];

    field_label(es, y, "x", FIELD_ID(ENT_SPIDER, 0));
    if (ui_float_field(&es->ui, FIELD_ID(ENT_SPIDER, 0),
                       FIELD_X, y, FIELD_W, &p->x))
        editor_commit_change(es);
    y += ROW_H;

    field_label(es, y, "vx", FIELD_ID(ENT_SPIDER, 1));
    if (ui_float_field_nonzero(&es->ui, FIELD_ID(ENT_SPIDER, 1),
                               FIELD_X, y, FIELD_W, &p->vx,
                               PATROL_SPEED_LIMIT))
        editor_commit_change(es);
    y += ROW_H;

    field_label(es, y, "patrol_x0", FIELD_ID(ENT_SPIDER, 2));
    if (ui_float_field_limited(&es->ui, FIELD_ID(ENT_SPIDER, 2),
                               FIELD_X, y, FIELD_W, &p->patrol_x0,
                               -FLT_MAX, p->patrol_x1 - SPIDER_FRAME_W))
        editor_commit_change(es);
    y += ROW_H;

    field_label(es, y, "patrol_x1", FIELD_ID(ENT_SPIDER, 3));
    if (ui_float_field_limited(&es->ui, FIELD_ID(ENT_SPIDER, 3),
                               FIELD_X, y, FIELD_W, &p->patrol_x1,
                               p->patrol_x0 + SPIDER_FRAME_W, FLT_MAX))
        editor_commit_change(es);
    y += ROW_H;

    field_label(es, y, "frame_index", FIELD_ID(ENT_SPIDER, 4));
    if (ui_int_field(&es->ui, FIELD_ID(ENT_SPIDER, 4),
                     FIELD_X, y, FIELD_W, &p->frame_index))
        editor_commit_change(es);
}

/* draw_jumping_spider_properties — Fields for the selected jumping spider. */
void draw_jumping_spider_properties(EditorState *es, int y)
{
    JumpingSpiderPlacement *p =
        &es->level.jumping_spiders[es->selection.index];

    field_label(es, y, "x", FIELD_ID(ENT_JUMPING_SPIDER, 0));
    if (ui_float_field(&es->ui, FIELD_ID(ENT_JUMPING_SPIDER, 0),
                       FIELD_X, y, FIELD_W, &p->x))
        editor_commit_change(es);
    y += ROW_H;

    field_label(es, y, "vx", FIELD_ID(ENT_JUMPING_SPIDER, 1));
    if (ui_float_field_nonzero(&es->ui, FIELD_ID(ENT_JUMPING_SPIDER, 1),
                               FIELD_X, y, FIELD_W, &p->vx,
                               PATROL_SPEED_LIMIT))
        editor_commit_change(es);
    y += ROW_H;

    field_label(es, y, "patrol_x0", FIELD_ID(ENT_JUMPING_SPIDER, 2));
    if (ui_float_field_limited(&es->ui, FIELD_ID(ENT_JUMPING_SPIDER, 2),
                               FIELD_X, y, FIELD_W, &p->patrol_x0,
                               -FLT_MAX, p->patrol_x1 - SPIDER_FRAME_W))
        editor_commit_change(es);
    y += ROW_H;

    field_label(es, y, "patrol_x1", FIELD_ID(ENT_JUMPING_SPIDER, 3));
    if (ui_float_field_limited(&es->ui, FIELD_ID(ENT_JUMPING_SPIDER, 3),
                               FIELD_X, y, FIELD_W, &p->patrol_x1,
                               p->patrol_x0 + SPIDER_FRAME_W, FLT_MAX))
        editor_commit_change(es);
}

/* draw_bird_properties — Fields for the selected bird. */
void draw_bird_properties(EditorState *es, int y)
{
    BirdPlacement *p = &es->level.birds[es->selection.index];

    field_label(es, y, "x", FIELD_ID(ENT_BIRD, 0));
    if (ui_float_field(&es->ui, FIELD_ID(ENT_BIRD, 0),
                       FIELD_X, y, FIELD_W, &p->x))
        editor_commit_change(es);
    y += ROW_H;

    field_label(es, y, "base_y", FIELD_ID(ENT_BIRD, 1));
    if (ui_float_field(&es->ui, FIELD_ID(ENT_BIRD, 1),
                       FIELD_X, y, FIELD_W, &p->base_y))
        editor_commit_change(es);
    y += ROW_H;

    field_label(es, y, "vx", FIELD_ID(ENT_BIRD, 2));
    if (ui_float_field_nonzero(&es->ui, FIELD_ID(ENT_BIRD, 2),
                               FIELD_X, y, FIELD_W, &p->vx,
                               PATROL_SPEED_LIMIT))
        editor_commit_change(es);
    y += ROW_H;

    field_label(es, y, "patrol_x0", FIELD_ID(ENT_BIRD, 3));
    if (ui_float_field_limited(&es->ui, FIELD_ID(ENT_BIRD, 3),
                               FIELD_X, y, FIELD_W, &p->patrol_x0,
                               -FLT_MAX, p->patrol_x1 - BIRD_FRAME_W))
        editor_commit_change(es);
    y += ROW_H;

    field_label(es, y, "patrol_x1", FIELD_ID(ENT_BIRD, 4));
    if (ui_float_field_limited(&es->ui, FIELD_ID(ENT_BIRD, 4),
                               FIELD_X, y, FIELD_W, &p->patrol_x1,
                               p->patrol_x0 + BIRD_FRAME_W, FLT_MAX))
        editor_commit_change(es);
    y += ROW_H;

    field_label(es, y, "frame_index", FIELD_ID(ENT_BIRD, 5));
    if (ui_int_field(&es->ui, FIELD_ID(ENT_BIRD, 5),
                     FIELD_X, y, FIELD_W, &p->frame_index))
        editor_commit_change(es);
}

/* draw_faster_bird_properties — Fields for the selected faster bird. */
void draw_faster_bird_properties(EditorState *es, int y)
{
    /*
     * Faster birds use the same BirdPlacement struct and the same
     * fields as regular birds — they just live in a separate array.
     */
    BirdPlacement *p = &es->level.faster_birds[es->selection.index];

    field_label(es, y, "x", FIELD_ID(ENT_FASTER_BIRD, 0));
    if (ui_float_field(&es->ui, FIELD_ID(ENT_FASTER_BIRD, 0),
                       FIELD_X, y, FIELD_W, &p->x))
        editor_commit_change(es);
    y += ROW_H;

    field_label(es, y, "base_y", FIELD_ID(ENT_FASTER_BIRD, 1));
    if (ui_float_field(&es->ui, FIELD_ID(ENT_FASTER_BIRD, 1),
                       FIELD_X, y, FIELD_W, &p->base_y))
        editor_commit_change(es);
    y += ROW_H;

    field_label(es, y, "vx", FIELD_ID(ENT_FASTER_BIRD, 2));
    if (ui_float_field_nonzero(&es->ui, FIELD_ID(ENT_FASTER_BIRD, 2),
                               FIELD_X, y, FIELD_W, &p->vx,
                               PATROL_SPEED_LIMIT))
        editor_commit_change(es);
    y += ROW_H;

    field_label(es, y, "patrol_x0", FIELD_ID(ENT_FASTER_BIRD, 3));
    if (ui_float_field_limited(&es->ui, FIELD_ID(ENT_FASTER_BIRD, 3),
                               FIELD_X, y, FIELD_W, &p->patrol_x0,
                               -FLT_MAX, p->patrol_x1 - BIRD_FRAME_W))
        editor_commit_change(es);
    y += ROW_H;

    field_label(es, y, "patrol_x1", FIELD_ID(ENT_FASTER_BIRD, 4));
    if (ui_float_field_limited(&es->ui, FIELD_ID(ENT_FASTER_BIRD, 4),
                               FIELD_X, y, FIELD_W, &p->patrol_x1,
                               p->patrol_x0 + BIRD_FRAME_W, FLT_MAX))
        editor_commit_change(es);
    y += ROW_H;

    field_label(es, y, "frame_index", FIELD_ID(ENT_FASTER_BIRD, 5));
    if (ui_int_field(&es->ui, FIELD_ID(ENT_FASTER_BIRD, 5),
                     FIELD_X, y, FIELD_W, &p->frame_index))
        editor_commit_change(es);
}

/* draw_fish_properties — Fields for the selected fish. */
void draw_fish_properties(EditorState *es, int y)
{
    FishPlacement *p = &es->level.fish[es->selection.index];

    field_label(es, y, "x", FIELD_ID(ENT_FISH, 0));
    if (ui_float_field(&es->ui, FIELD_ID(ENT_FISH, 0),
                       FIELD_X, y, FIELD_W, &p->x))
        editor_commit_change(es);
    y += ROW_H;

    field_label(es, y, "vx", FIELD_ID(ENT_FISH, 1));
    if (ui_float_field_nonzero(&es->ui, FIELD_ID(ENT_FISH, 1),
                               FIELD_X, y, FIELD_W, &p->vx,
                               PATROL_SPEED_LIMIT))
        editor_commit_change(es);
    y += ROW_H;

    field_label(es, y, "patrol_x0", FIELD_ID(ENT_FISH, 2));
    if (ui_float_field_limited(&es->ui, FIELD_ID(ENT_FISH, 2),
                               FIELD_X, y, FIELD_W, &p->patrol_x0,
                               -FLT_MAX, p->patrol_x1 - FISH_FRAME_W))
        editor_commit_change(es);
    y += ROW_H;

    field_label(es, y, "patrol_x1", FIELD_ID(ENT_FISH, 3));
    if (ui_float_field_limited(&es->ui, FIELD_ID(ENT_FISH, 3),
                               FIELD_X, y, FIELD_W, &p->patrol_x1,
                               p->patrol_x0 + FISH_FRAME_W, FLT_MAX))
        editor_commit_change(es);
}

/* draw_faster_fish_properties — Fields for the selected faster fish. */
void draw_faster_fish_properties(EditorState *es, int y)
{
    /*
     * Faster fish use the same FishPlacement struct and the same
     * fields as regular fish — they just live in a separate array.
     */
    FishPlacement *p = &es->level.faster_fish[es->selection.index];

    field_label(es, y, "x", FIELD_ID(ENT_FASTER_FISH, 0));
    if (ui_float_field(&es->ui, FIELD_ID(ENT_FASTER_FISH, 0),
                       FIELD_X, y, FIELD_W, &p->x))
        editor_commit_change(es);
    y += ROW_H;

    field_label(es, y, "vx", FIELD_ID(ENT_FASTER_FISH, 1));
    if (ui_float_field_nonzero(&es->ui, FIELD_ID(ENT_FASTER_FISH, 1),
                               FIELD_X, y, FIELD_W, &p->vx,
                               PATROL_SPEED_LIMIT))
        editor_commit_change(es);
    y += ROW_H;

    field_label(es, y, "patrol_x0", FIELD_ID(ENT_FASTER_FISH, 2));
    if (ui_float_field_limited(&es->ui, FIELD_ID(ENT_FASTER_FISH, 2),
                               FIELD_X, y, FIELD_W, &p->patrol_x0,
                               -FLT_MAX, p->patrol_x1 - FISH_FRAME_W))
        editor_commit_change(es);
    y += ROW_H;

    field_label(es, y, "patrol_x1", FIELD_ID(ENT_FASTER_FISH, 3));
    if (ui_float_field_limited(&es->ui, FIELD_ID(ENT_FASTER_FISH, 3),
                               FIELD_X, y, FIELD_W, &p->patrol_x1,
                               p->patrol_x0 + FISH_FRAME_W, FLT_MAX))
        editor_commit_change(es);
}

/* draw_axe_trap_properties — Fields for the selected axe trap. */
void draw_axe_trap_properties(EditorState *es, int y)
{
    AxeTrapPlacement *p = &es->level.axe_traps[es->selection.index];

    field_label(es, y, "pillar_x", FIELD_ID(ENT_AXE_TRAP, 0));
    if (ui_float_field(&es->ui, FIELD_ID(ENT_AXE_TRAP, 0),
                       FIELD_X, y, FIELD_W, &p->pillar_x))
        editor_commit_change(es);
    y += ROW_H;

    field_label(es, y, "y", FIELD_ID(ENT_AXE_TRAP, 3));
    if (ui_float_field(&es->ui, FIELD_ID(ENT_AXE_TRAP, 3),
                       FIELD_X, y, FIELD_W, &p->y))
        editor_commit_change(es);
    y += ROW_H;

    int mode_sel = (int)p->mode;
    field_label(es, y, "mode", FIELD_ID(ENT_AXE_TRAP, 1));
    if (ui_dropdown(&es->ui, FIELD_ID(ENT_AXE_TRAP, 1),
                    FIELD_X, y, FIELD_W,
                    axe_mode_opts, 2, &mode_sel)) {
        p->mode = (AxeTrapMode)mode_sel;
        editor_commit_change(es);
    }
}

/* draw_circular_saw_properties — Fields for the selected circular saw. */
void draw_circular_saw_properties(EditorState *es, int y)
{
    CircularSawPlacement *p =
        &es->level.circular_saws[es->selection.index];

    field_label(es, y, "x", FIELD_ID(ENT_CIRCULAR_SAW, 0));
    if (ui_float_field(&es->ui, FIELD_ID(ENT_CIRCULAR_SAW, 0),
                       FIELD_X, y, FIELD_W, &p->x))
        editor_commit_change(es);
    y += ROW_H;

    field_label(es, y, "y", FIELD_ID(ENT_CIRCULAR_SAW, 4));
    if (ui_float_field(&es->ui, FIELD_ID(ENT_CIRCULAR_SAW, 4),
                       FIELD_X, y, FIELD_W, &p->y))
        editor_commit_change(es);
    y += ROW_H;

    field_label(es, y, "patrol_x0", FIELD_ID(ENT_CIRCULAR_SAW, 1));
    if (ui_float_field(&es->ui, FIELD_ID(ENT_CIRCULAR_SAW, 1),
                       FIELD_X, y, FIELD_W, &p->patrol_x0))
        editor_commit_change(es);
    y += ROW_H;

    field_label(es, y, "patrol_x1", FIELD_ID(ENT_CIRCULAR_SAW, 2));
    if (ui_float_field(&es->ui, FIELD_ID(ENT_CIRCULAR_SAW, 2),
                       FIELD_X, y, FIELD_W, &p->patrol_x1))
        editor_commit_change(es);
    y += ROW_H;

    field_label(es, y, "direction", FIELD_ID(ENT_CIRCULAR_SAW, 3));
    if (ui_int_field(&es->ui, FIELD_ID(ENT_CIRCULAR_SAW, 3),
                     FIELD_X, y, FIELD_W, &p->direction))
        editor_commit_change(es);
}

/* draw_spike_row_properties — Fields for the selected spike row. */
void draw_spike_row_properties(EditorState *es, int y)
{
    SpikeRowPlacement *p =
        &es->level.spike_rows[es->selection.index];

    field_label(es, y, "x", FIELD_ID(ENT_SPIKE_ROW, 0));
    if (ui_float_field(&es->ui, FIELD_ID(ENT_SPIKE_ROW, 0),
                       FIELD_X, y, FIELD_W, &p->x))
        editor_commit_change(es);
    y += ROW_H;

    field_label(es, y, "count", FIELD_ID(ENT_SPIKE_ROW, 1));
    if (ui_int_field(&es->ui, FIELD_ID(ENT_SPIKE_ROW, 1),
                     FIELD_X, y, FIELD_W, &p->count))
        editor_commit_change(es);
}

/* draw_spike_platform_properties — Fields for the selected spike platform. */
void draw_spike_platform_properties(EditorState *es, int y)
{
    SpikePlatformPlacement *p =
        &es->level.spike_platforms[es->selection.index];

    field_label(es, y, "x", FIELD_ID(ENT_SPIKE_PLATFORM, 0));
    if (ui_float_field(&es->ui, FIELD_ID(ENT_SPIKE_PLATFORM, 0),
                       FIELD_X, y, FIELD_W, &p->x))
        editor_commit_change(es);
    y += ROW_H;

    field_label(es, y, "y", FIELD_ID(ENT_SPIKE_PLATFORM, 1));
    if (ui_float_field(&es->ui, FIELD_ID(ENT_SPIKE_PLATFORM, 1),
                       FIELD_X, y, FIELD_W, &p->y))
        editor_commit_change(es);
    y += ROW_H;

    field_label(es, y, "tile_count", FIELD_ID(ENT_SPIKE_PLATFORM, 2));
    if (ui_int_field(&es->ui, FIELD_ID(ENT_SPIKE_PLATFORM, 2),
                     FIELD_X, y, FIELD_W, &p->tile_count))
        editor_commit_change(es);
}

/* draw_spike_block_properties — Fields for the selected spike block. */
void draw_spike_block_properties(EditorState *es, int y)
{
    SpikeBlockPlacement *p =
        &es->level.spike_blocks[es->selection.index];

    field_label(es, y, "rail_index", FIELD_ID(ENT_SPIKE_BLOCK, 0));
    if (ui_int_field(&es->ui, FIELD_ID(ENT_SPIKE_BLOCK, 0),
                     FIELD_X, y, FIELD_W, &p->rail_index))
        editor_commit_change(es);
    y += ROW_H;

    field_label(es, y, "t_offset", FIELD_ID(ENT_SPIKE_BLOCK, 1));
    if (ui_float_field(&es->ui, FIELD_ID(ENT_SPIKE_BLOCK, 1),
                       FIELD_X, y, FIELD_W, &p->t_offset))
        editor_commit_change(es);
    y += ROW_H;

    field_label(es, y, "speed", FIELD_ID(ENT_SPIKE_BLOCK, 2));
    if (ui_float_field_limited(&es->ui, FIELD_ID(ENT_SPIKE_BLOCK, 2),
                               FIELD_X, y, FIELD_W, &p->speed,
                               RAIL_SPEED_MIN, MAX_RAIL_SPEED))
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

/* draw_bridge_properties — Fields for the selected bridge. */
void draw_bridge_properties(EditorState *es, int y)
{
    BridgePlacement *p = &es->level.bridges[es->selection.index];

    field_label(es, y, "x", FIELD_ID(ENT_BRIDGE, 0));
    if (ui_float_field(&es->ui, FIELD_ID(ENT_BRIDGE, 0),
                       FIELD_X, y, FIELD_W, &p->x))
        editor_commit_change(es);
    y += ROW_H;

    field_label(es, y, "y", FIELD_ID(ENT_BRIDGE, 1));
    if (ui_float_field(&es->ui, FIELD_ID(ENT_BRIDGE, 1),
                       FIELD_X, y, FIELD_W, &p->y))
        editor_commit_change(es);
    y += ROW_H;

    field_label(es, y, "brick_count", FIELD_ID(ENT_BRIDGE, 2));
    if (ui_int_field(&es->ui, FIELD_ID(ENT_BRIDGE, 2),
                     FIELD_X, y, FIELD_W, &p->brick_count))
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

/* draw_vine_properties — Fields for the selected vine. */
void draw_vine_properties(EditorState *es, int y)
{
    VinePlacement *p = &es->level.vines[es->selection.index];

    field_label(es, y, "x", FIELD_ID(ENT_VINE, 0));
    if (ui_float_field(&es->ui, FIELD_ID(ENT_VINE, 0),
                       FIELD_X, y, FIELD_W, &p->x))
        editor_commit_change(es);
    y += ROW_H;

    field_label(es, y, "y", FIELD_ID(ENT_VINE, 1));
    if (ui_float_field(&es->ui, FIELD_ID(ENT_VINE, 1),
                       FIELD_X, y, FIELD_W, &p->y))
        editor_commit_change(es);
    y += ROW_H;

    field_label(es, y, "tile_count", FIELD_ID(ENT_VINE, 2));
    if (ui_int_field(&es->ui, FIELD_ID(ENT_VINE, 2),
                     FIELD_X, y, FIELD_W, &p->tile_count))
        editor_commit_change(es);
    y += ROW_H;

    field_label(es, y, "vine_type", FIELD_ID(ENT_VINE, 3));
    if (ui_dropdown(&es->ui, FIELD_ID(ENT_VINE, 3),
                    FIELD_X, y, FIELD_W,
                    vine_type_opts, 2, &p->vine_type))
        editor_commit_change(es);
}

/* draw_ladder_properties — Fields for the selected ladder. */
void draw_ladder_properties(EditorState *es, int y)
{
    LadderPlacement *p = &es->level.ladders[es->selection.index];

    field_label(es, y, "x", FIELD_ID(ENT_LADDER, 0));
    if (ui_float_field(&es->ui, FIELD_ID(ENT_LADDER, 0),
                       FIELD_X, y, FIELD_W, &p->x))
        editor_commit_change(es);
    y += ROW_H;

    field_label(es, y, "y", FIELD_ID(ENT_LADDER, 1));
    if (ui_float_field(&es->ui, FIELD_ID(ENT_LADDER, 1),
                       FIELD_X, y, FIELD_W, &p->y))
        editor_commit_change(es);
    y += ROW_H;

    field_label(es, y, "tile_count", FIELD_ID(ENT_LADDER, 2));
    if (ui_int_field(&es->ui, FIELD_ID(ENT_LADDER, 2),
                     FIELD_X, y, FIELD_W, &p->tile_count))
        editor_commit_change(es);
}

/* draw_rope_properties — Fields for the selected rope. */
void draw_rope_properties(EditorState *es, int y)
{
    RopePlacement *p = &es->level.ropes[es->selection.index];

    field_label(es, y, "x", FIELD_ID(ENT_ROPE, 0));
    if (ui_float_field(&es->ui, FIELD_ID(ENT_ROPE, 0),
                       FIELD_X, y, FIELD_W, &p->x))
        editor_commit_change(es);
    y += ROW_H;

    field_label(es, y, "y", FIELD_ID(ENT_ROPE, 1));
    if (ui_float_field(&es->ui, FIELD_ID(ENT_ROPE, 1),
                       FIELD_X, y, FIELD_W, &p->y))
        editor_commit_change(es);
    y += ROW_H;

    field_label(es, y, "tile_count", FIELD_ID(ENT_ROPE, 2));
    if (ui_int_field(&es->ui, FIELD_ID(ENT_ROPE, 2),
                     FIELD_X, y, FIELD_W, &p->tile_count))
        editor_commit_change(es);
}

