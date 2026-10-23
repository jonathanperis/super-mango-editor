/*
 * properties_internal.h — Helpers shared by the property panel files.
 *
 * Not the public editor API (that is properties.h). The layout macros and
 * the field helpers are what every draw_<type>_properties function and the
 * Level Config groups use, so they live once, here.
 */
#pragma once

#include "editor.h"
#include "../player/player.h"   /* JUMP_VY */
#include "../game_constants.h"  /* MAX_PATROL_SPEED */

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
/* An empty next-phase path: the game ends after this level. */
#define NEXT_PHASE_HINT "(none: the last level)"

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
 *   enemy vx    — a patrol speed: not 0 (the enemy would never move) and at
 *                 most MAX_PATROL_SPEED px/s either way (game_constants.h
 *                 explains why), the sign being its first direction.  The
 *                 field clamps a bigger value and refuses a typed 0, and the
 *                 status bar says which happened (show_field_note).
 */
/* The limit is an int (see MAX_PATROL_SPEED). Casting that int, rather than
 * the macro's division, keeps the value and stays out of a float division.
 * Defined in properties_entities.c, the only file that reads the limit. */
extern const int patrol_speed_limit_px;
#define PATROL_SPEED_LIMIT    ((float)patrol_speed_limit_px)

void show_field_note(EditorState *es);
int option_index(const char *value, const char *const *paths, int count);
void field_label(EditorState *es, int y, const char *key, int id);
void draw_section_title(EditorState *es, int x, int y, const char *title, int *open);

void draw_rail_properties(EditorState *es, int y);
void draw_floor_gap_properties(EditorState *es, int y);
void draw_checkpoint_properties(EditorState *es, int y);
void draw_platform_properties(EditorState *es, int y);
void draw_coin_properties(EditorState *es, int y);
void draw_star_yellow_properties(EditorState *es, int y);
void draw_star_green_properties(EditorState *es, int y);
void draw_star_red_properties(EditorState *es, int y);
void draw_last_star_properties(EditorState *es, int y);
void draw_player_spawn_properties(EditorState *es, int y);
void draw_spider_properties(EditorState *es, int y);
void draw_jumping_spider_properties(EditorState *es, int y);
void draw_bird_properties(EditorState *es, int y);
void draw_faster_bird_properties(EditorState *es, int y);
void draw_fish_properties(EditorState *es, int y);
void draw_faster_fish_properties(EditorState *es, int y);
void draw_axe_trap_properties(EditorState *es, int y);
void draw_circular_saw_properties(EditorState *es, int y);
void draw_spike_row_properties(EditorState *es, int y);
void draw_spike_platform_properties(EditorState *es, int y);
void draw_spike_block_properties(EditorState *es, int y);
void draw_blue_flame_properties(EditorState *es, int y);
void draw_fire_flame_properties(EditorState *es, int y);
void draw_float_platform_properties(EditorState *es, int y);
void draw_bridge_properties(EditorState *es, int y);
void draw_bouncepad_small_properties(EditorState *es, int y);
void draw_bouncepad_medium_properties(EditorState *es, int y);
void draw_bouncepad_high_properties(EditorState *es, int y);
void draw_vine_properties(EditorState *es, int y);
void draw_ladder_properties(EditorState *es, int y);
void draw_rope_properties(EditorState *es, int y);
