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
    [ENT_FLOAT_PLATFORM]   = "Float Platform",
    [ENT_BRIDGE]           = "Bridge",
    [ENT_BOUNCEPAD_SMALL]  = "Bouncepad (S)",
    [ENT_BOUNCEPAD_MEDIUM] = "Bouncepad (M)",
    [ENT_BOUNCEPAD_HIGH]   = "Bouncepad (H)",
    [ENT_VINE]             = "Vine",
    [ENT_LADDER]           = "Ladder",
    [ENT_ROPE]             = "Rope",
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

void properties_render(EditorState *es)
{
    /* If nothing is selected, draw nothing — the palette fills the column. */
    if (es->selection.index < 0)
        return;

    /* ---- Panel background ------------------------------------------- */

    /*
     * ui_panel — draw a dark filled rectangle as the visual container.
     * This covers the bottom-right area so fields render on a clean
     * background rather than on top of whatever the canvas drew.
     */
    ui_panel(&es->ui, PROP_X, PROP_Y, PROP_W, PROP_H);

    /* ---- Header: entity type name + array index --------------------- */

    /*
     * Build a label like "Spider #2" — the human-readable type name from
     * the lookup table plus the zero-based array index.
     */
    char header[64];
    snprintf(header, sizeof(header), "%s #%d",
             entity_type_names[es->selection.type],
             es->selection.index);
    ui_label(&es->ui, CONTENT_X, PROP_Y + 4, header);

    /*
     * ui_separator — thin horizontal line dividing the header from the
     * editable fields.  Drawn at header baseline + ROW_H.
     */
    ui_separator(&es->ui, PROP_X + 4, PROP_Y + ROW_H + 2, PROP_W - 8);

    /*
     * y — cursor tracking the current vertical drawing position.
     * Advances by ROW_H after each field so rows stack top-to-bottom.
     */
    int y = PROP_Y + ROW_H + 8;

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

        /*
         * mode — dropdown that selects between Pendulum and Spin.
         * Cast the AxeTrapMode enum to int for the dropdown widget,
         * then cast back when the selection changes.
         */
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
void level_config_render(EditorState *es) {
    int x = PROP_X;
    int y = PROP_Y;

    /* Panel background */
    ui_panel(&es->ui, x, y, PROP_W, PROP_H);
    y += 4;

    /* Header */
    ui_label_color(&es->ui, x + 8, y, "LEVEL CONFIG", UI_ACCENT);
    y += 22;
    ui_separator(&es->ui, x + 4, y, PROP_W - 8);
    y += 8;

    /* ---- Player spawn ---- */
    ui_label(&es->ui, x + 8, y, "Player Spawn");
    y += 18;
    ui_label(&es->ui, x + 8, y, "x:");
    if (ui_float_field(&es->ui, 9001, x + 30, y, 100, &es->level.player_start_x))
        es->modified = 1;
    ui_label(&es->ui, x + 140, y, "y:");
    if (ui_float_field(&es->ui, 9002, x + 160, y, 100, &es->level.player_start_y))
        es->modified = 1;
    y += 24;

    /* ---- Music ---- */
    ui_separator(&es->ui, x + 4, y, PROP_W - 8);
    y += 6;
    ui_label(&es->ui, x + 8, y, "Music");
    y += 18;
    ui_label(&es->ui, x + 8, y, "path:");
    ui_label(&es->ui, x + 50, y, es->level.music_path[0] ? es->level.music_path : "(none)");
    y += 18;
    ui_label(&es->ui, x + 8, y, "vol:");
    if (ui_int_field(&es->ui, 9003, x + 50, y, 80, &es->level.music_volume))
        es->modified = 1;
    y += 24;

    /* ---- Fog ---- */
    ui_separator(&es->ui, x + 4, y, PROP_W - 8);
    y += 6;
    ui_label(&es->ui, x + 8, y, "Fog");
    y += 18;
    {
        static const char *fog_opts[] = { "Off", "On" };
        if (ui_dropdown(&es->ui, 9004, x + 8, y, 100, fog_opts, 2, &es->level.fog_enabled))
            es->modified = 1;
    }
    y += 24;

    /* ---- Parallax layers ---- */
    ui_separator(&es->ui, x + 4, y, PROP_W - 8);
    y += 6;
    {
        char plx_header[48];
        snprintf(plx_header, sizeof(plx_header), "Parallax (%d layers)",
                 es->level.parallax_layer_count);
        ui_label(&es->ui, x + 8, y, plx_header);
    }
    y += 18;

    /* Show each parallax layer's path (read-only) and speed (editable) */
    for (int i = 0; i < es->level.parallax_layer_count && i < PARALLAX_MAX_LAYERS; i++) {
        if (y + 20 > PROP_Y + PROP_H - 4) break;  /* clip if out of panel */

        char label[16];
        snprintf(label, sizeof(label), "%d:", i);
        ui_label(&es->ui, x + 8, y, label);

        /* Show filename portion of path */
        const char *path = es->level.parallax_layers[i].path;
        const char *slash = strrchr(path, '/');
        const char *display = slash ? slash + 1 : path;
        ui_label(&es->ui, x + 28, y, display[0] ? display : "(empty)");

        /* Speed field */
        ui_label(&es->ui, x + 260, y, "spd:");
        if (ui_float_field(&es->ui, 9100 + i, x + 295, y, 70, &es->level.parallax_layers[i].speed))
            es->modified = 1;
        y += 18;
    }

    /* Button to add a layer */
    if (es->level.parallax_layer_count < PARALLAX_MAX_LAYERS) {
        if (y + 22 < PROP_Y + PROP_H - 4) {
            if (ui_button(&es->ui, x + 8, y, 100, 20, "+ Add Layer")) {
                int idx = es->level.parallax_layer_count;
                strncpy(es->level.parallax_layers[idx].path, "assets/", 63);
                es->level.parallax_layers[idx].speed = 0.1f;
                es->level.parallax_layer_count++;
                es->modified = 1;
            }
        }
    }
}
