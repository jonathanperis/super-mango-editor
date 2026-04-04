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

#include <SDL.h>        /* SDL_Rect, SDL_RenderSetClipRect       */
#include <stdio.h>      /* snprintf for header label formatting */
#include <string.h>     /* strrchr for filename extraction       */

#include "properties.h"
#include "editor.h"     /* EditorState, EntityType, CANVAS_W, PANEL_W, etc. */
#include "ui.h"         /* ui_panel, ui_label, ui_separator, ui_float_field,
                           ui_int_field, ui_dropdown                         */
#include "../levels/level.h" /* LevelDef, all *Placement structs            */

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

/* Parallax subsection collapse state — accessed by editor.c for layout */
int g_plx_open = 0;

/* ------------------------------------------------------------------ */
/* Human-readable entity type names                                    */
/* ------------------------------------------------------------------ */

/*
 * entity_type_names — display string for each EntityType enum value.
 *
 * Indexed by the EntityType enum (0 .. ENT_COUNT-1).  Used in the panel
 * header to show e.g. "Spider #2" when the user selects the third spider.
 */
static const char *entity_type_names[ENT_COUNT] = {
    [ENT_SEA_GAP]          = "Sea Gap",
    [ENT_RAIL]             = "Rail",
    [ENT_PLATFORM]         = "Platform",
    [ENT_COIN]             = "Coin",
    [ENT_YELLOW_STAR]      = "Yellow Star",
    [ENT_LAST_STAR]        = "Last Star",
    [ENT_SPIDER]           = "Spider",
    [ENT_JUMPING_SPIDER]   = "Jumping Spider",
    [ENT_BIRD]             = "Bird",
    [ENT_FASTER_BIRD]      = "Faster Bird",
    [ENT_FISH]             = "Fish",
    [ENT_FASTER_FISH]      = "Faster Fish",
    [ENT_AXE_TRAP]         = "Axe Trap",
    [ENT_CIRCULAR_SAW]     = "Circular Saw",
    [ENT_SPIKE_ROW]        = "Spike Row",
    [ENT_SPIKE_PLATFORM]   = "Spike Platform",
    [ENT_SPIKE_BLOCK]      = "Spike Block",
    [ENT_BLUE_FLAME]       = "Blue Flame",
    [ENT_FLOAT_PLATFORM]   = "Float Platform",
    [ENT_BRIDGE]           = "Bridge",
    [ENT_BOUNCEPAD_SMALL]  = "Bouncepad (S)",
    [ENT_BOUNCEPAD_MEDIUM] = "Bouncepad (M)",
    [ENT_BOUNCEPAD_HIGH]   = "Bouncepad (H)",
    [ENT_VINE]             = "Vine",
    [ENT_LADDER]           = "Ladder",
    [ENT_ROPE]             = "Rope",
    [ENT_PLAYER_SPAWN]     = "Player Spawn",
};

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

/* ------------------------------------------------------------------ */
/* properties_render                                                   */
/* ------------------------------------------------------------------ */

void properties_render(EditorState *es, int start_y, int available_h)
{
    if (es->selection.index < 0)
        return;

    /*
     * Use caller-supplied position instead of the old fixed PROP_Y / PROP_H
     * constants.  The layout orchestrator in editor.c computes where this
     * section starts and how tall it is based on the sections above it.
     */
    int prop_x = PROP_X;
    int prop_y = start_y;
    int prop_h = available_h;

    /* Panel background */
    ui_panel(&es->ui, prop_x, prop_y, PROP_W, prop_h);

    /* ---- Fixed title bar (same style as palette header) ------------- */
    {
        SDL_Color title_bg = UI_TITLE_BG;
        SDL_SetRenderDrawColor(es->ui.renderer,
                               title_bg.r, title_bg.g, title_bg.b, title_bg.a);
        SDL_Rect title_rect = { prop_x, prop_y, PROP_W, ROW_H + 4 };
        SDL_RenderFillRect(es->ui.renderer, &title_rect);
    }

    /* ---- Collapsible header ----------------------------------------- */
    char header[64];
    if (es->selection.type == ENT_PLAYER_SPAWN ||
        es->selection.type == ENT_LAST_STAR) {
        snprintf(header, sizeof(header), "%s %s",
                 es->panel_open ? "v" : ">",
                 entity_type_names[es->selection.type]);
    } else {
        snprintf(header, sizeof(header), "%s %s #%d",
                 es->panel_open ? "v" : ">",
                 entity_type_names[es->selection.type],
                 es->selection.index);
    }

    int content_x = prop_x + 8;

    int hdr_hovered = (es->ui.mouse_x >= prop_x &&
                       es->ui.mouse_x < prop_x + PROP_W &&
                       es->ui.mouse_y >= prop_y &&
                       es->ui.mouse_y < prop_y + ROW_H + 4);
    if (hdr_hovered && es->ui.mouse_clicked)
        es->panel_open = !es->panel_open;

    ui_label_color(&es->ui, content_x, prop_y + 4, header,
                   hdr_hovered ? UI_TEXT : UI_ACCENT);

    if (!es->panel_open) return;

    /* Clip content below the title bar */
    SDL_Rect prop_clip = { prop_x, prop_y + ROW_H + 4, PROP_W, prop_h - ROW_H - 4 };
    SDL_RenderSetClipRect(es->ui.renderer, &prop_clip);

    int y = prop_y + ROW_H + 8;

    /* ---- Per-type field rendering ----------------------------------- */

    /*
     * Each case accesses the selected placement struct by pointer so that
     * the ui_*_field widgets can read and write the value in place.
     * When any widget returns 1 (value changed), we set es->modified = 1
     * so the editor knows the level has unsaved changes.
     */
    switch (es->selection.type) {

    /* ================================================================ */
    /* World geometry                                                    */
    /* ================================================================ */

    case ENT_SEA_GAP: {
        /*
         * sea_gaps is an int array — each element is a single x coordinate.
         * We take a pointer to the array element so ui_int_field can modify it.
         */
        int *p = &es->level.sea_gaps[es->selection.index];
        ui_label(&es->ui, CONTENT_X, y, "x:");
        if (ui_int_field(&es->ui, FIELD_ID(ENT_SEA_GAP, 0),
                         FIELD_X, y, FIELD_W, p))
            es->modified = 1;
        break;
    }

    case ENT_RAIL: {
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
            es->modified = 1;
        }
        y += ROW_H;

        ui_label(&es->ui, CONTENT_X, y, "x:");
        if (ui_int_field(&es->ui, FIELD_ID(ENT_RAIL, 1),
                         FIELD_X, y, FIELD_W, &p->x))
            es->modified = 1;
        y += ROW_H;

        ui_label(&es->ui, CONTENT_X, y, "y:");
        if (ui_int_field(&es->ui, FIELD_ID(ENT_RAIL, 2),
                         FIELD_X, y, FIELD_W, &p->y))
            es->modified = 1;
        y += ROW_H;

        ui_label(&es->ui, CONTENT_X, y, "w:");
        if (ui_int_field(&es->ui, FIELD_ID(ENT_RAIL, 3),
                         FIELD_X, y, FIELD_W, &p->w))
            es->modified = 1;
        y += ROW_H;

        ui_label(&es->ui, CONTENT_X, y, "h:");
        if (ui_int_field(&es->ui, FIELD_ID(ENT_RAIL, 4),
                         FIELD_X, y, FIELD_W, &p->h))
            es->modified = 1;
        y += ROW_H;

        ui_label(&es->ui, CONTENT_X, y, "end_cap:");
        if (ui_int_field(&es->ui, FIELD_ID(ENT_RAIL, 5),
                         FIELD_X, y, FIELD_W, &p->end_cap))
            es->modified = 1;
        break;
    }

    case ENT_PLATFORM: {
        PlatformPlacement *p = &es->level.platforms[es->selection.index];

        ui_label(&es->ui, CONTENT_X, y, "x:");
        if (ui_float_field(&es->ui, FIELD_ID(ENT_PLATFORM, 0),
                           FIELD_X, y, FIELD_W, &p->x))
            es->modified = 1;
        y += ROW_H;

        ui_label(&es->ui, CONTENT_X, y, "tile_height:");
        if (ui_int_field(&es->ui, FIELD_ID(ENT_PLATFORM, 1),
                         FIELD_X, y, FIELD_W, &p->tile_height))
            es->modified = 1;
        y += ROW_H;

        ui_label(&es->ui, CONTENT_X, y, "tile_width:");
        if (ui_int_field(&es->ui, FIELD_ID(ENT_PLATFORM, 2),
                         FIELD_X, y, FIELD_W, &p->tile_width))
            es->modified = 1;
        break;
    }

    /* ================================================================ */
    /* Collectibles                                                      */
    /* ================================================================ */

    case ENT_COIN: {
        CoinPlacement *p = &es->level.coins[es->selection.index];

        ui_label(&es->ui, CONTENT_X, y, "x:");
        if (ui_float_field(&es->ui, FIELD_ID(ENT_COIN, 0),
                           FIELD_X, y, FIELD_W, &p->x))
            es->modified = 1;
        y += ROW_H;

        ui_label(&es->ui, CONTENT_X, y, "y:");
        if (ui_float_field(&es->ui, FIELD_ID(ENT_COIN, 1),
                           FIELD_X, y, FIELD_W, &p->y))
            es->modified = 1;
        break;
    }

    case ENT_YELLOW_STAR: {
        YellowStarPlacement *p =
            &es->level.yellow_stars[es->selection.index];

        ui_label(&es->ui, CONTENT_X, y, "x:");
        if (ui_float_field(&es->ui, FIELD_ID(ENT_YELLOW_STAR, 0),
                           FIELD_X, y, FIELD_W, &p->x))
            es->modified = 1;
        y += ROW_H;

        ui_label(&es->ui, CONTENT_X, y, "y:");
        if (ui_float_field(&es->ui, FIELD_ID(ENT_YELLOW_STAR, 1),
                           FIELD_X, y, FIELD_W, &p->y))
            es->modified = 1;
        break;
    }

    case ENT_LAST_STAR: {
        /*
         * last_star is a single struct in LevelDef, not an array.
         * The selection index is always 0 for this type.
         */
        LastStarPlacement *p = &es->level.last_star;

        ui_label(&es->ui, CONTENT_X, y, "x:");
        if (ui_float_field(&es->ui, FIELD_ID(ENT_LAST_STAR, 0),
                           FIELD_X, y, FIELD_W, &p->x))
            es->modified = 1;
        y += ROW_H;

        ui_label(&es->ui, CONTENT_X, y, "y:");
        if (ui_float_field(&es->ui, FIELD_ID(ENT_LAST_STAR, 1),
                           FIELD_X, y, FIELD_W, &p->y))
            es->modified = 1;
        break;
    }

    /* ================================================================ */
    /* Player                                                            */
    /* ================================================================ */

    case ENT_PLAYER_SPAWN: {
        /*
         * player_start_x / player_start_y are scalar fields in LevelDef,
         * not a struct like LastStarPlacement.  The selection index is
         * always 0 because there is exactly one player spawn per level.
         */
        ui_label(&es->ui, CONTENT_X, y, "x:");
        if (ui_float_field(&es->ui, FIELD_ID(ENT_PLAYER_SPAWN, 0),
                           FIELD_X, y, FIELD_W,
                           &es->level.player_start_x))
            es->modified = 1;
        y += ROW_H;

        ui_label(&es->ui, CONTENT_X, y, "y:");
        if (ui_float_field(&es->ui, FIELD_ID(ENT_PLAYER_SPAWN, 1),
                           FIELD_X, y, FIELD_W,
                           &es->level.player_start_y))
            es->modified = 1;
        break;
    }

    /* ================================================================ */
    /* Enemies                                                           */
    /* ================================================================ */

    case ENT_SPIDER: {
        SpiderPlacement *p = &es->level.spiders[es->selection.index];

        ui_label(&es->ui, CONTENT_X, y, "x:");
        if (ui_float_field(&es->ui, FIELD_ID(ENT_SPIDER, 0),
                           FIELD_X, y, FIELD_W, &p->x))
            es->modified = 1;
        y += ROW_H;

        ui_label(&es->ui, CONTENT_X, y, "vx:");
        if (ui_float_field(&es->ui, FIELD_ID(ENT_SPIDER, 1),
                           FIELD_X, y, FIELD_W, &p->vx))
            es->modified = 1;
        y += ROW_H;

        ui_label(&es->ui, CONTENT_X, y, "patrol_x0:");
        if (ui_float_field(&es->ui, FIELD_ID(ENT_SPIDER, 2),
                           FIELD_X, y, FIELD_W, &p->patrol_x0))
            es->modified = 1;
        y += ROW_H;

        ui_label(&es->ui, CONTENT_X, y, "patrol_x1:");
        if (ui_float_field(&es->ui, FIELD_ID(ENT_SPIDER, 3),
                           FIELD_X, y, FIELD_W, &p->patrol_x1))
            es->modified = 1;
        y += ROW_H;

        ui_label(&es->ui, CONTENT_X, y, "frame_index:");
        if (ui_int_field(&es->ui, FIELD_ID(ENT_SPIDER, 4),
                         FIELD_X, y, FIELD_W, &p->frame_index))
            es->modified = 1;
        break;
    }

    case ENT_JUMPING_SPIDER: {
        JumpingSpiderPlacement *p =
            &es->level.jumping_spiders[es->selection.index];

        ui_label(&es->ui, CONTENT_X, y, "x:");
        if (ui_float_field(&es->ui, FIELD_ID(ENT_JUMPING_SPIDER, 0),
                           FIELD_X, y, FIELD_W, &p->x))
            es->modified = 1;
        y += ROW_H;

        ui_label(&es->ui, CONTENT_X, y, "vx:");
        if (ui_float_field(&es->ui, FIELD_ID(ENT_JUMPING_SPIDER, 1),
                           FIELD_X, y, FIELD_W, &p->vx))
            es->modified = 1;
        y += ROW_H;

        ui_label(&es->ui, CONTENT_X, y, "patrol_x0:");
        if (ui_float_field(&es->ui, FIELD_ID(ENT_JUMPING_SPIDER, 2),
                           FIELD_X, y, FIELD_W, &p->patrol_x0))
            es->modified = 1;
        y += ROW_H;

        ui_label(&es->ui, CONTENT_X, y, "patrol_x1:");
        if (ui_float_field(&es->ui, FIELD_ID(ENT_JUMPING_SPIDER, 3),
                           FIELD_X, y, FIELD_W, &p->patrol_x1))
            es->modified = 1;
        break;
    }

    case ENT_BIRD: {
        BirdPlacement *p = &es->level.birds[es->selection.index];

        ui_label(&es->ui, CONTENT_X, y, "x:");
        if (ui_float_field(&es->ui, FIELD_ID(ENT_BIRD, 0),
                           FIELD_X, y, FIELD_W, &p->x))
            es->modified = 1;
        y += ROW_H;

        ui_label(&es->ui, CONTENT_X, y, "base_y:");
        if (ui_float_field(&es->ui, FIELD_ID(ENT_BIRD, 1),
                           FIELD_X, y, FIELD_W, &p->base_y))
            es->modified = 1;
        y += ROW_H;

        ui_label(&es->ui, CONTENT_X, y, "vx:");
        if (ui_float_field(&es->ui, FIELD_ID(ENT_BIRD, 2),
                           FIELD_X, y, FIELD_W, &p->vx))
            es->modified = 1;
        y += ROW_H;

        ui_label(&es->ui, CONTENT_X, y, "patrol_x0:");
        if (ui_float_field(&es->ui, FIELD_ID(ENT_BIRD, 3),
                           FIELD_X, y, FIELD_W, &p->patrol_x0))
            es->modified = 1;
        y += ROW_H;

        ui_label(&es->ui, CONTENT_X, y, "patrol_x1:");
        if (ui_float_field(&es->ui, FIELD_ID(ENT_BIRD, 4),
                           FIELD_X, y, FIELD_W, &p->patrol_x1))
            es->modified = 1;
        y += ROW_H;

        ui_label(&es->ui, CONTENT_X, y, "frame_index:");
        if (ui_int_field(&es->ui, FIELD_ID(ENT_BIRD, 5),
                         FIELD_X, y, FIELD_W, &p->frame_index))
            es->modified = 1;
        break;
    }

    case ENT_FASTER_BIRD: {
        /*
         * Faster birds use the same BirdPlacement struct and the same
         * fields as regular birds — they just live in a separate array.
         */
        BirdPlacement *p = &es->level.faster_birds[es->selection.index];

        ui_label(&es->ui, CONTENT_X, y, "x:");
        if (ui_float_field(&es->ui, FIELD_ID(ENT_FASTER_BIRD, 0),
                           FIELD_X, y, FIELD_W, &p->x))
            es->modified = 1;
        y += ROW_H;

        ui_label(&es->ui, CONTENT_X, y, "base_y:");
        if (ui_float_field(&es->ui, FIELD_ID(ENT_FASTER_BIRD, 1),
                           FIELD_X, y, FIELD_W, &p->base_y))
            es->modified = 1;
        y += ROW_H;

        ui_label(&es->ui, CONTENT_X, y, "vx:");
        if (ui_float_field(&es->ui, FIELD_ID(ENT_FASTER_BIRD, 2),
                           FIELD_X, y, FIELD_W, &p->vx))
            es->modified = 1;
        y += ROW_H;

        ui_label(&es->ui, CONTENT_X, y, "patrol_x0:");
        if (ui_float_field(&es->ui, FIELD_ID(ENT_FASTER_BIRD, 3),
                           FIELD_X, y, FIELD_W, &p->patrol_x0))
            es->modified = 1;
        y += ROW_H;

        ui_label(&es->ui, CONTENT_X, y, "patrol_x1:");
        if (ui_float_field(&es->ui, FIELD_ID(ENT_FASTER_BIRD, 4),
                           FIELD_X, y, FIELD_W, &p->patrol_x1))
            es->modified = 1;
        y += ROW_H;

        ui_label(&es->ui, CONTENT_X, y, "frame_index:");
        if (ui_int_field(&es->ui, FIELD_ID(ENT_FASTER_BIRD, 5),
                         FIELD_X, y, FIELD_W, &p->frame_index))
            es->modified = 1;
        break;
    }

    case ENT_FISH: {
        FishPlacement *p = &es->level.fish[es->selection.index];

        ui_label(&es->ui, CONTENT_X, y, "x:");
        if (ui_float_field(&es->ui, FIELD_ID(ENT_FISH, 0),
                           FIELD_X, y, FIELD_W, &p->x))
            es->modified = 1;
        y += ROW_H;

        ui_label(&es->ui, CONTENT_X, y, "vx:");
        if (ui_float_field(&es->ui, FIELD_ID(ENT_FISH, 1),
                           FIELD_X, y, FIELD_W, &p->vx))
            es->modified = 1;
        y += ROW_H;

        ui_label(&es->ui, CONTENT_X, y, "patrol_x0:");
        if (ui_float_field(&es->ui, FIELD_ID(ENT_FISH, 2),
                           FIELD_X, y, FIELD_W, &p->patrol_x0))
            es->modified = 1;
        y += ROW_H;

        ui_label(&es->ui, CONTENT_X, y, "patrol_x1:");
        if (ui_float_field(&es->ui, FIELD_ID(ENT_FISH, 3),
                           FIELD_X, y, FIELD_W, &p->patrol_x1))
            es->modified = 1;
        break;
    }

    case ENT_FASTER_FISH: {
        /*
         * Faster fish use the same FishPlacement struct and the same
         * fields as regular fish — they just live in a separate array.
         */
        FishPlacement *p = &es->level.faster_fish[es->selection.index];

        ui_label(&es->ui, CONTENT_X, y, "x:");
        if (ui_float_field(&es->ui, FIELD_ID(ENT_FASTER_FISH, 0),
                           FIELD_X, y, FIELD_W, &p->x))
            es->modified = 1;
        y += ROW_H;

        ui_label(&es->ui, CONTENT_X, y, "vx:");
        if (ui_float_field(&es->ui, FIELD_ID(ENT_FASTER_FISH, 1),
                           FIELD_X, y, FIELD_W, &p->vx))
            es->modified = 1;
        y += ROW_H;

        ui_label(&es->ui, CONTENT_X, y, "patrol_x0:");
        if (ui_float_field(&es->ui, FIELD_ID(ENT_FASTER_FISH, 2),
                           FIELD_X, y, FIELD_W, &p->patrol_x0))
            es->modified = 1;
        y += ROW_H;

        ui_label(&es->ui, CONTENT_X, y, "patrol_x1:");
        if (ui_float_field(&es->ui, FIELD_ID(ENT_FASTER_FISH, 3),
                           FIELD_X, y, FIELD_W, &p->patrol_x1))
            es->modified = 1;
        break;
    }

    /* ================================================================ */
    /* Hazards                                                           */
    /* ================================================================ */

    case ENT_AXE_TRAP: {
        AxeTrapPlacement *p = &es->level.axe_traps[es->selection.index];

        ui_label(&es->ui, CONTENT_X, y, "pillar_x:");
        if (ui_float_field(&es->ui, FIELD_ID(ENT_AXE_TRAP, 0),
                           FIELD_X, y, FIELD_W, &p->pillar_x))
            es->modified = 1;
        y += ROW_H;

        ui_label(&es->ui, CONTENT_X, y, "y:");
        if (ui_float_field(&es->ui, FIELD_ID(ENT_AXE_TRAP, 3),
                           FIELD_X, y, FIELD_W, &p->y))
            es->modified = 1;
        y += ROW_H;

        int mode_sel = (int)p->mode;
        ui_label(&es->ui, CONTENT_X, y, "mode:");
        if (ui_dropdown(&es->ui, FIELD_ID(ENT_AXE_TRAP, 1),
                        FIELD_X, y, FIELD_W,
                        axe_mode_opts, 2, &mode_sel)) {
            p->mode = (AxeTrapMode)mode_sel;
            es->modified = 1;
        }
        break;
    }

    case ENT_CIRCULAR_SAW: {
        CircularSawPlacement *p =
            &es->level.circular_saws[es->selection.index];

        ui_label(&es->ui, CONTENT_X, y, "x:");
        if (ui_float_field(&es->ui, FIELD_ID(ENT_CIRCULAR_SAW, 0),
                           FIELD_X, y, FIELD_W, &p->x))
            es->modified = 1;
        y += ROW_H;

        ui_label(&es->ui, CONTENT_X, y, "y:");
        if (ui_float_field(&es->ui, FIELD_ID(ENT_CIRCULAR_SAW, 4),
                           FIELD_X, y, FIELD_W, &p->y))
            es->modified = 1;
        y += ROW_H;

        ui_label(&es->ui, CONTENT_X, y, "patrol_x0:");
        if (ui_float_field(&es->ui, FIELD_ID(ENT_CIRCULAR_SAW, 1),
                           FIELD_X, y, FIELD_W, &p->patrol_x0))
            es->modified = 1;
        y += ROW_H;

        ui_label(&es->ui, CONTENT_X, y, "patrol_x1:");
        if (ui_float_field(&es->ui, FIELD_ID(ENT_CIRCULAR_SAW, 2),
                           FIELD_X, y, FIELD_W, &p->patrol_x1))
            es->modified = 1;
        y += ROW_H;

        ui_label(&es->ui, CONTENT_X, y, "direction:");
        if (ui_int_field(&es->ui, FIELD_ID(ENT_CIRCULAR_SAW, 3),
                         FIELD_X, y, FIELD_W, &p->direction))
            es->modified = 1;
        break;
    }

    case ENT_SPIKE_ROW: {
        SpikeRowPlacement *p =
            &es->level.spike_rows[es->selection.index];

        ui_label(&es->ui, CONTENT_X, y, "x:");
        if (ui_float_field(&es->ui, FIELD_ID(ENT_SPIKE_ROW, 0),
                           FIELD_X, y, FIELD_W, &p->x))
            es->modified = 1;
        y += ROW_H;

        ui_label(&es->ui, CONTENT_X, y, "count:");
        if (ui_int_field(&es->ui, FIELD_ID(ENT_SPIKE_ROW, 1),
                         FIELD_X, y, FIELD_W, &p->count))
            es->modified = 1;
        break;
    }

    case ENT_SPIKE_PLATFORM: {
        SpikePlatformPlacement *p =
            &es->level.spike_platforms[es->selection.index];

        ui_label(&es->ui, CONTENT_X, y, "x:");
        if (ui_float_field(&es->ui, FIELD_ID(ENT_SPIKE_PLATFORM, 0),
                           FIELD_X, y, FIELD_W, &p->x))
            es->modified = 1;
        y += ROW_H;

        ui_label(&es->ui, CONTENT_X, y, "y:");
        if (ui_float_field(&es->ui, FIELD_ID(ENT_SPIKE_PLATFORM, 1),
                           FIELD_X, y, FIELD_W, &p->y))
            es->modified = 1;
        y += ROW_H;

        ui_label(&es->ui, CONTENT_X, y, "tile_count:");
        if (ui_int_field(&es->ui, FIELD_ID(ENT_SPIKE_PLATFORM, 2),
                         FIELD_X, y, FIELD_W, &p->tile_count))
            es->modified = 1;
        break;
    }

    case ENT_SPIKE_BLOCK: {
        SpikeBlockPlacement *p =
            &es->level.spike_blocks[es->selection.index];

        ui_label(&es->ui, CONTENT_X, y, "rail_index:");
        if (ui_int_field(&es->ui, FIELD_ID(ENT_SPIKE_BLOCK, 0),
                         FIELD_X, y, FIELD_W, &p->rail_index))
            es->modified = 1;
        y += ROW_H;

        ui_label(&es->ui, CONTENT_X, y, "t_offset:");
        if (ui_float_field(&es->ui, FIELD_ID(ENT_SPIKE_BLOCK, 1),
                           FIELD_X, y, FIELD_W, &p->t_offset))
            es->modified = 1;
        y += ROW_H;

        ui_label(&es->ui, CONTENT_X, y, "speed:");
        if (ui_float_field(&es->ui, FIELD_ID(ENT_SPIKE_BLOCK, 2),
                           FIELD_X, y, FIELD_W, &p->speed))
            es->modified = 1;
        break;
    }

    case ENT_BLUE_FLAME: {
        BlueFlamePlacement *p =
            &es->level.blue_flames[es->selection.index];

        ui_label(&es->ui, CONTENT_X, y, "x:");
        if (ui_float_field(&es->ui, FIELD_ID(ENT_BLUE_FLAME, 0),
                           FIELD_X, y, FIELD_W, &p->x))
            es->modified = 1;
        break;
    }

    /* ================================================================ */
    /* Surfaces                                                          */
    /* ================================================================ */

    case ENT_FLOAT_PLATFORM: {
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
            p->mode = (FloatPlatformMode)mode_sel;
            es->modified = 1;
        }
        y += ROW_H;

        ui_label(&es->ui, CONTENT_X, y, "x:");
        if (ui_float_field(&es->ui, FIELD_ID(ENT_FLOAT_PLATFORM, 1),
                           FIELD_X, y, FIELD_W, &p->x))
            es->modified = 1;
        y += ROW_H;

        ui_label(&es->ui, CONTENT_X, y, "y:");
        if (ui_float_field(&es->ui, FIELD_ID(ENT_FLOAT_PLATFORM, 2),
                           FIELD_X, y, FIELD_W, &p->y))
            es->modified = 1;
        y += ROW_H;

        ui_label(&es->ui, CONTENT_X, y, "tile_count:");
        if (ui_int_field(&es->ui, FIELD_ID(ENT_FLOAT_PLATFORM, 3),
                         FIELD_X, y, FIELD_W, &p->tile_count))
            es->modified = 1;
        y += ROW_H;

        ui_label(&es->ui, CONTENT_X, y, "rail_index:");
        if (ui_int_field(&es->ui, FIELD_ID(ENT_FLOAT_PLATFORM, 4),
                         FIELD_X, y, FIELD_W, &p->rail_index))
            es->modified = 1;
        y += ROW_H;

        ui_label(&es->ui, CONTENT_X, y, "t_offset:");
        if (ui_float_field(&es->ui, FIELD_ID(ENT_FLOAT_PLATFORM, 5),
                           FIELD_X, y, FIELD_W, &p->t_offset))
            es->modified = 1;
        y += ROW_H;

        ui_label(&es->ui, CONTENT_X, y, "speed:");
        if (ui_float_field(&es->ui, FIELD_ID(ENT_FLOAT_PLATFORM, 6),
                           FIELD_X, y, FIELD_W, &p->speed))
            es->modified = 1;
        break;
    }

    case ENT_BRIDGE: {
        BridgePlacement *p = &es->level.bridges[es->selection.index];

        ui_label(&es->ui, CONTENT_X, y, "x:");
        if (ui_float_field(&es->ui, FIELD_ID(ENT_BRIDGE, 0),
                           FIELD_X, y, FIELD_W, &p->x))
            es->modified = 1;
        y += ROW_H;

        ui_label(&es->ui, CONTENT_X, y, "y:");
        if (ui_float_field(&es->ui, FIELD_ID(ENT_BRIDGE, 1),
                           FIELD_X, y, FIELD_W, &p->y))
            es->modified = 1;
        y += ROW_H;

        ui_label(&es->ui, CONTENT_X, y, "brick_count:");
        if (ui_int_field(&es->ui, FIELD_ID(ENT_BRIDGE, 2),
                         FIELD_X, y, FIELD_W, &p->brick_count))
            es->modified = 1;
        break;
    }

    case ENT_BOUNCEPAD_SMALL: {
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
            es->modified = 1;
        y += ROW_H;

        ui_label(&es->ui, CONTENT_X, y, "launch_vy:");
        if (ui_float_field(&es->ui, FIELD_ID(ENT_BOUNCEPAD_SMALL, 1),
                           FIELD_X, y, FIELD_W, &p->launch_vy))
            es->modified = 1;
        break;
    }

    case ENT_BOUNCEPAD_MEDIUM: {
        BouncepadPlacement *p =
            &es->level.bouncepads_medium[es->selection.index];

        ui_label(&es->ui, CONTENT_X, y, "type: Medium (Wood)");
        y += ROW_H;

        ui_label(&es->ui, CONTENT_X, y, "x:");
        if (ui_float_field(&es->ui, FIELD_ID(ENT_BOUNCEPAD_MEDIUM, 0),
                           FIELD_X, y, FIELD_W, &p->x))
            es->modified = 1;
        y += ROW_H;

        ui_label(&es->ui, CONTENT_X, y, "launch_vy:");
        if (ui_float_field(&es->ui, FIELD_ID(ENT_BOUNCEPAD_MEDIUM, 1),
                           FIELD_X, y, FIELD_W, &p->launch_vy))
            es->modified = 1;
        break;
    }

    case ENT_BOUNCEPAD_HIGH: {
        BouncepadPlacement *p =
            &es->level.bouncepads_high[es->selection.index];

        ui_label(&es->ui, CONTENT_X, y, "type: High (Red)");
        y += ROW_H;

        ui_label(&es->ui, CONTENT_X, y, "x:");
        if (ui_float_field(&es->ui, FIELD_ID(ENT_BOUNCEPAD_HIGH, 0),
                           FIELD_X, y, FIELD_W, &p->x))
            es->modified = 1;
        y += ROW_H;

        ui_label(&es->ui, CONTENT_X, y, "launch_vy:");
        if (ui_float_field(&es->ui, FIELD_ID(ENT_BOUNCEPAD_HIGH, 1),
                           FIELD_X, y, FIELD_W, &p->launch_vy))
            es->modified = 1;
        break;
    }

    /* ================================================================ */
    /* Decorations & climbables                                          */
    /* ================================================================ */

    case ENT_VINE: {
        VinePlacement *p = &es->level.vines[es->selection.index];

        ui_label(&es->ui, CONTENT_X, y, "x:");
        if (ui_float_field(&es->ui, FIELD_ID(ENT_VINE, 0),
                           FIELD_X, y, FIELD_W, &p->x))
            es->modified = 1;
        y += ROW_H;

        ui_label(&es->ui, CONTENT_X, y, "y:");
        if (ui_float_field(&es->ui, FIELD_ID(ENT_VINE, 1),
                           FIELD_X, y, FIELD_W, &p->y))
            es->modified = 1;
        y += ROW_H;

        ui_label(&es->ui, CONTENT_X, y, "tile_count:");
        if (ui_int_field(&es->ui, FIELD_ID(ENT_VINE, 2),
                         FIELD_X, y, FIELD_W, &p->tile_count))
            es->modified = 1;
        break;
    }

    case ENT_LADDER: {
        LadderPlacement *p = &es->level.ladders[es->selection.index];

        ui_label(&es->ui, CONTENT_X, y, "x:");
        if (ui_float_field(&es->ui, FIELD_ID(ENT_LADDER, 0),
                           FIELD_X, y, FIELD_W, &p->x))
            es->modified = 1;
        y += ROW_H;

        ui_label(&es->ui, CONTENT_X, y, "y:");
        if (ui_float_field(&es->ui, FIELD_ID(ENT_LADDER, 1),
                           FIELD_X, y, FIELD_W, &p->y))
            es->modified = 1;
        y += ROW_H;

        ui_label(&es->ui, CONTENT_X, y, "tile_count:");
        if (ui_int_field(&es->ui, FIELD_ID(ENT_LADDER, 2),
                         FIELD_X, y, FIELD_W, &p->tile_count))
            es->modified = 1;
        break;
    }

    case ENT_ROPE: {
        RopePlacement *p = &es->level.ropes[es->selection.index];

        ui_label(&es->ui, CONTENT_X, y, "x:");
        if (ui_float_field(&es->ui, FIELD_ID(ENT_ROPE, 0),
                           FIELD_X, y, FIELD_W, &p->x))
            es->modified = 1;
        y += ROW_H;

        ui_label(&es->ui, CONTENT_X, y, "y:");
        if (ui_float_field(&es->ui, FIELD_ID(ENT_ROPE, 1),
                           FIELD_X, y, FIELD_W, &p->y))
            es->modified = 1;
        y += ROW_H;

        ui_label(&es->ui, CONTENT_X, y, "tile_count:");
        if (ui_int_field(&es->ui, FIELD_ID(ENT_ROPE, 2),
                         FIELD_X, y, FIELD_W, &p->tile_count))
            es->modified = 1;
        break;
    }

    /* ---- Fallthrough for ENT_COUNT (not a real type) ---------------- */
    case ENT_COUNT:
        break;
    }

    SDL_RenderSetClipRect(es->ui.renderer, NULL);
}

/* ================================================================== */
/* level_config_render — Level-wide configuration panel                 */
/* ================================================================== */

/*
 * level_config_render — Show editable level-wide settings.
 *
 * Renders in the same bottom-right panel location as the entity properties
 * but is shown when NO entity is selected.  This gives the designer access
 * to parallax background layers, player spawn position, music, and fog
 * without needing to select a specific entity.
 *
 * Widget IDs use the 9000+ range to avoid collisions with entity fields.
 */
void level_config_render(EditorState *es, int start_y, int available_h) {
    /*
     * Use caller-supplied position instead of the old fixed PROP_Y / PROP_H
     * constants.  The layout orchestrator in editor.c computes where this
     * section starts and how tall it is based on the sections around it.
     */
    int x = PROP_X;
    int y = start_y;
    int cfg_h = available_h;

    /* Panel background */
    ui_panel(&es->ui, x, y, PROP_W, cfg_h);

    /* ---- Fixed title bar (same style as palette header) ------------- */
    {
        SDL_Color title_bg = UI_TITLE_BG;
        SDL_SetRenderDrawColor(es->ui.renderer,
                               title_bg.r, title_bg.g, title_bg.b, title_bg.a);
        SDL_Rect title_rect = { x, y, PROP_W, ROW_H + 4 };
        SDL_RenderFillRect(es->ui.renderer, &title_rect);
    }

    /* ---- Collapsible header ---- */
    char cfg_header[32];
    snprintf(cfg_header, sizeof(cfg_header), "%s Level Config",
             es->config_open ? "v" : ">");

    int hdr_hovered = (es->ui.mouse_x >= x &&
                       es->ui.mouse_x < x + PROP_W &&
                       es->ui.mouse_y >= y &&
                       es->ui.mouse_y < y + ROW_H + 4);
    if (hdr_hovered && es->ui.mouse_clicked)
        es->config_open = !es->config_open;

    ui_label_color(&es->ui, x + 8, y + 4, cfg_header,
                   hdr_hovered ? UI_TEXT : UI_ACCENT);

    if (!es->config_open) return;

    y += ROW_H + 8;

    /* ---- Level name ---- */
    ui_label(&es->ui, x + 8, y, "Name:");
    if (ui_text_field(&es->ui, 9000, x + 55, y, 310, es->level.name,
                      (int)sizeof(es->level.name)))
        es->modified = 1;
    y += 24;

    /* ---- World Width (screen count) ---- */
    ui_label(&es->ui, x + 8, y, "Screens:");
    {
        static const char *screen_opts[] = { "1", "2", "3", "4", "5", "6", "8", "10" };
        static const int screen_vals[] = { 1, 2, 3, 4, 5, 6, 8, 10 };
        static const int screen_opt_count = 8;

        int sel = 3; /* default 4 */
        for (int si = 0; si < screen_opt_count; si++) {
            if (es->level.screen_count == screen_vals[si]) { sel = si; break; }
        }
        if (ui_dropdown(&es->ui, 9011, x + 80, y, 50,
                         screen_opts, screen_opt_count, &sel)) {
            es->level.screen_count = screen_vals[sel];
            es->modified = 1;
        }
    }
    {
        char width_text[32];
        snprintf(width_text, sizeof(width_text), "= %dpx",
                 es->level.screen_count * GAME_W);
        ui_label_color(&es->ui, x + 140, y, width_text, UI_TEXT_DIM);
    }
    y += 24;

    /* ---- Background Music ---- */
    ui_separator(&es->ui, x + 4, y, PROP_W - 8);
    y += 6;
    ui_label(&es->ui, x + 8, y, "Background Music:");
    {
        static const char *music_names[] = { "(none)", "game_music.ogg" };
        static const char *music_paths[] = { "", "assets/sounds/levels/game_music.ogg" };
        static const int music_count = 2;

        int sel = 0;
        for (int i = 0; i < music_count; i++) {
            if (strcmp(es->level.music_path, music_paths[i]) == 0) {
                sel = i;
                break;
            }
        }
        if (ui_dropdown(&es->ui, 9009, x + 160, y, 210,
                         music_names, music_count, &sel)) {
            strncpy(es->level.music_path, music_paths[sel],
                    sizeof(es->level.music_path) - 1);
            es->level.music_path[sizeof(es->level.music_path) - 1] = '\0';
            es->modified = 1;
        }
    }
    y += 22;
    ui_label(&es->ui, x + 8, y, "vol:");
    if (ui_int_field(&es->ui, 9003, x + 50, y, 80, &es->level.music_volume))
        es->modified = 1;
    y += 24;

    /* ---- Floor tile ---- */
    ui_separator(&es->ui, x + 4, y, PROP_W - 8);
    y += 6;
    ui_label(&es->ui, x + 8, y, "Floor Tile:");
    {
        /* Display names (shown in dropdown) */
        static const char *floor_tile_names[] = { "grass_tileset.png" };
        /* Full paths (written to LevelDef) */
        static const char *floor_tile_paths[] = {
            "assets/sprites/levels/grass_tileset.png"
        };
        static const int floor_tile_count = 1;

        /* Find which option matches the current path (default to 0) */
        int sel = 0;
        for (int i = 0; i < floor_tile_count; i++) {
            if (strcmp(es->level.floor_tile_path, floor_tile_paths[i]) == 0) {
                sel = i;
                break;
            }
        }
        if (ui_dropdown(&es->ui, 9010, x + 100, y, 270,
                         floor_tile_names, floor_tile_count, &sel)) {
            strncpy(es->level.floor_tile_path, floor_tile_paths[sel],
                    sizeof(es->level.floor_tile_path) - 1);
            es->level.floor_tile_path[sizeof(es->level.floor_tile_path) - 1] = '\0';
            es->modified = 1;
        }
    }
    y += 24;

    /* ---- Fog & Water ---- */
    ui_separator(&es->ui, x + 4, y, PROP_W - 8);
    y += 6;
    {
        static const char *toggle_opts[] = { "Off", "On" };
        ui_label(&es->ui, x + 8, y, "Fog:");
        if (ui_dropdown(&es->ui, 9004, x + 50, y, 80, toggle_opts, 2, &es->level.fog_enabled))
            es->modified = 1;
        ui_label(&es->ui, x + 150, y, "Water:");
        if (ui_dropdown(&es->ui, 9005, x + 205, y, 80, toggle_opts, 2, &es->level.water_enabled))
            es->modified = 1;
        /* TODO: water type dropdown when water_enabled == 1 */
    }
    y += 24;

    /* ---- Game rules ---- */
    ui_separator(&es->ui, x + 4, y, PROP_W - 8);
    y += 6;
    ui_label(&es->ui, x + 8, y, "Game Rules");
    y += 18;
    ui_label(&es->ui, x + 8, y, "hearts:");
    if (ui_int_field(&es->ui, 9006, x + 70, y, 60, &es->level.initial_hearts))
        es->modified = 1;
    ui_label(&es->ui, x + 150, y, "lives:");
    if (ui_int_field(&es->ui, 9007, x + 205, y, 60, &es->level.initial_lives))
        es->modified = 1;
    y += 22;
    ui_label(&es->ui, x + 8, y, "pts/life:");
    if (ui_int_field(&es->ui, 9008, x + 80, y, 80, &es->level.score_per_life))
        es->modified = 1;
    y += 24;

    /* ---- Parallax layers (collapsible subsection) ---- */
    ui_separator(&es->ui, x + 4, y, PROP_W - 8);
    y += 6;
    {
        char plx_header[48];
        snprintf(plx_header, sizeof(plx_header), "%s Parallax (%d layers)",
                 g_plx_open ? "v" : ">",
                 es->level.parallax_layer_count);

        int plx_hovered = (es->ui.mouse_x >= x &&
                           es->ui.mouse_x < x + PROP_W &&
                           es->ui.mouse_y >= y &&
                           es->ui.mouse_y < y + 18);
        if (plx_hovered && es->ui.mouse_clicked)
            g_plx_open = !g_plx_open;

        ui_label_color(&es->ui, x + 8, y, plx_header,
                       plx_hovered ? UI_TEXT : UI_TEXT_DIM);
        y += 18;

        if (!g_plx_open) goto plx_done;
    }

    /* Parallax asset dropdown options */
    static const char *plx_names[] = {
        "sky_blue.png",
        "clouds_bg.png",
        "glacial_mountains.png",
        "clouds_mg_3.png",
        "clouds_mg_2.png",
        "clouds_lonely.png",
        "clouds_mg_1.png"
    };
    static const char *plx_paths[] = {
        "assets/sprites/backgrounds/sky_blue.png",
        "assets/sprites/backgrounds/clouds_bg.png",
        "assets/sprites/backgrounds/glacial_mountains.png",
        "assets/sprites/backgrounds/clouds_mg_3.png",
        "assets/sprites/backgrounds/clouds_mg_2.png",
        "assets/sprites/backgrounds/clouds_lonely.png",
        "assets/sprites/backgrounds/clouds_mg_1.png"
    };
    static const int plx_opt_count = 7;

    for (int i = 0; i < es->level.parallax_layer_count && i < PARALLAX_MAX_LAYERS; i++) {
        char label[16];
        snprintf(label, sizeof(label), "%d:", i);
        ui_label(&es->ui, x + 8, y, label);

        /* Dropdown for parallax asset */
        int sel = 0;
        for (int j = 0; j < plx_opt_count; j++) {
            if (strcmp(es->level.parallax_layers[i].path, plx_paths[j]) == 0) {
                sel = j;
                break;
            }
        }
        if (ui_dropdown(&es->ui, 9200 + i, x + 28, y, 200,
                         plx_names, plx_opt_count, &sel)) {
            strncpy(es->level.parallax_layers[i].path, plx_paths[sel],
                    sizeof(es->level.parallax_layers[i].path) - 1);
            es->level.parallax_layers[i].path[sizeof(es->level.parallax_layers[i].path) - 1] = '\0';
            es->modified = 1;
        }

        /* Speed field */
        ui_label(&es->ui, x + 236, y, "spd:");
        if (ui_float_field(&es->ui, 9100 + i, x + 265, y, 60, &es->level.parallax_layers[i].speed))
            es->modified = 1;
        y += 20;
    }

    /* Buttons to add / remove layers */
    if (es->level.parallax_layer_count < PARALLAX_MAX_LAYERS) {
        if (ui_button(&es->ui, x + 8, y, 80, 20, "+ Add")) {
            int idx = es->level.parallax_layer_count;
            strncpy(es->level.parallax_layers[idx].path, "assets/sprites/backgrounds/sky_blue.png", 63);
            es->level.parallax_layers[idx].speed = 0.1f;
            es->level.parallax_layer_count++;
            es->modified = 1;
        }
    }
    if (es->level.parallax_layer_count > 0) {
        if (ui_button(&es->ui, x + 96, y, 100, 20, "- Remove Last")) {
            es->level.parallax_layer_count--;
            es->modified = 1;
        }
    }

plx_done:
    (void)0;  /* label requires a statement */
}
