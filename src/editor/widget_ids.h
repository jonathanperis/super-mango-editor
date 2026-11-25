/*
 * widget_ids.h — Every fixed widget ID the editor draws with, in one place.
 *
 * The immediate-mode widgets (shared/ui.h) know a field only by an int ID.
 * The text field being typed into, the open dropdown, Tab order and "jump
 * to this field" from a validation message all compare IDs, so two
 * widgets drawn with the same ID would share that state.  Each ID is
 * therefore named here, once, and the ranges do not overlap:
 *
 *   1 .. ENT_COUNT*100-1    an entity's property fields: FIELD_ID(type,
 *                           field) = type * 100 + field + 1
 *                           (properties_internal.h)
 *   7000 + row              the Campaign view's level-name fields
 *   8888                    the toolbar's zoom dropdown
 *   9000 and up             level-wide widgets: the Level Config panel and
 *                           the Entities search.  editor_finish_field_edit
 *                           (editor_session.c) tells them from a field of
 *                           the selected entity by `id >= WIDGET_LEVEL_FIRST`.
 *
 * A layer list draws one row per layer: row i's widget is the row's
 * _FIRST ID + i, so each list keeps 100 IDs for itself.
 */
#pragma once

typedef enum {
    WIDGET_CAMPAIGN_NAME_FIRST    = 7000,  /* + row index                 */
    WIDGET_ZOOM_DROPDOWN          = 8888,

    /* ---- Level-wide widgets (9000 and up) ----------------------------- */
    WIDGET_LEVEL_FIRST            = 9000,

    /* Level Config: the level's own fields (properties_config.c). */
    WIDGET_LEVEL_NAME             = 9000,
    WIDGET_LEVEL_DESCRIPTION      = 9001,
    WIDGET_LEVEL_AUTHOR           = 9002,
    WIDGET_MUSIC_VOLUME           = 9003,
    WIDGET_INITIAL_HEARTS         = 9006,
    WIDGET_INITIAL_LIVES          = 9007,
    WIDGET_SCORE_PER_LIFE         = 9008,
    WIDGET_MUSIC_PATH             = 9009,
    WIDGET_FLOOR_TILE             = 9010,
    WIDGET_SCREEN_COUNT           = 9011,
    WIDGET_COIN_SCORE             = 9012,

    /* Level Config: the [physics] overrides. */
    WIDGET_PHYS_WALK_MAX_SPEED    = 9020,
    WIDGET_PHYS_RUN_MAX_SPEED     = 9021,
    WIDGET_PHYS_WALK_GROUND_ACCEL = 9022,
    WIDGET_PHYS_RUN_GROUND_ACCEL  = 9023,
    WIDGET_PHYS_GROUND_FRICTION   = 9024,
    WIDGET_PHYS_GROUND_COUNTER    = 9025,
    WIDGET_PHYS_AIR_ACCEL_WALK    = 9026,
    WIDGET_PHYS_AIR_ACCEL_RUN     = 9027,
    WIDGET_PHYS_AIR_FRICTION      = 9028,
    WIDGET_PHYS_CAM_VX_FACTOR     = 9029,
    WIDGET_PHYS_CAM_MAX           = 9030,

    WIDGET_NEXT_PHASE             = 9031,

    /* Level Config: the layer lists, + row index. */
    WIDGET_BACKGROUND_SPEED_FIRST = 9100,
    WIDGET_BACKGROUND_PATH_FIRST  = 9200,
    WIDGET_FOREGROUND_PATH_FIRST  = 9300,
    WIDGET_FOREGROUND_SPEED_FIRST = 9400,
    WIDGET_FOG_PATH_FIRST         = 9600,
    WIDGET_FOG_SPEED_FIRST        = 9700,

    /* The Entities list's search field (editor_entity_list.c). */
    WIDGET_ENTITY_LIST_SEARCH     = 9900
} EditorWidgetId;
