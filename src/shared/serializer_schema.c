/*
 * serializer_schema.c — The level file schema, as one table.
 *
 * ROOT_FIELDS has one row per top-level TOML key; a [table] or [[array]]
 * row points at a field table with one row per key of its elements.  Each
 * row says the key's type and limits and where its value lives in LevelDef.
 * The checks (serializer_validate.c), the loader (serializer_load.c), the
 * writer (serializer_save.c) and `level_check --dump-schema` all read these
 * rows, so adding a key to the format is one line here.  The types are in
 * serializer_schema.h.
 */

#include "serializer_schema.h"

#include <stddef.h>  /* offsetof */
#include <string.h>  /* memchr, memcmp, strlen */

#include "serializer_types.h" /* enum name <-> value conversions */
#include "../levels/level.h"  /* LevelDef and its MAX_* array limits */

/* ------------------------------------------------------------------ */
/* Matching TOML keys                                                   */
/* ------------------------------------------------------------------ */

/*
 * Match schema keys by exact byte length and reject embedded NULs explicitly.
 * tomlc17's public lookup is also length-aware; these helpers additionally
 * enforce the serializer's key-validation contract.
 */
int serializer_key_matches(const char *supplied, int supplied_len,
                            const char *expected)
{
    size_t expected_len;

    if (!supplied || supplied_len < 0 || !expected) return 0;
    expected_len = strlen(expected);
    if ((size_t)supplied_len != expected_len) return 0;
    if (memchr(supplied, '\0', (size_t)supplied_len) != NULL) return 0;
    return memcmp(supplied, expected, expected_len) == 0;
}

toml_datum_t serializer_table_get(toml_datum_t table,
                                         const char *expected)
{
    toml_datum_t missing = {0};

    if (table.type != TOML_TABLE || !expected) return missing;
    for (int i = 0; i < table.u.tab.size; i++) {
        if (serializer_key_matches(table.u.tab.key[i], table.u.tab.len[i],
                             expected)) {
            return table.u.tab.value[i];
        }
    }
    return missing;
}

/* ------------------------------------------------------------------ */
/* The table                                                            */
/* ------------------------------------------------------------------ */


/*
 * A string field names the char array its value is copied into, and its
 * capacity is that array's size: TEXT_AT("path", LevelLayer, path, ...) asks
 * FIELD_SIZE(LevelLayer, path), so a path that would not fit is refused.  FIELD_SIZE
 * asks sizeof about a struct member without needing a variable of that type.
 */
#define FIELD_SIZE(type, member) sizeof(((type *)0)->member)

/*
 * Rows the generic loader and writer handle: the key, then the struct and
 * the member that hold its value.  offsetof(T, member) is how many bytes
 * into a T the member starts; the loader adds it to the address of an
 * element to find where to store the value.
 *
 *   FLOAT_AT(key, T, member)              a float; missing reads as 0
 *   FLOAT_AT_OR(key, T, member, fallback) a float; missing reads as fallback
 *   FLOAT_AT_KEEP(key, T, member)         a float; missing keeps the default
 *   REQUIRED_FLOAT_AT(key, T, member)     a float that must be there: the
 *                                         load fails without a finite number
 *   INT_AT(key, T, member, fallback)      an int; missing reads as fallback
 *   INT_AT_OMIT_ZERO(key, T, member)      an int not written while it is 0
 *   TEXT_AT(key, T, member, omit)         a char array; missing reads as ""
 *   TEXT_AT_OR(key, T, member, omit, text) a char array; missing reads as text
 *   TEXT_AT_KEEP(key, T, member, omit)    a char array; missing keeps the default
 *   ENUM_AT(key, T, member, E, values, validator, from, to, fallback)
 *                                         a C enum of type E, written as its name
 *
 * Every macro also checks, while compiling, that the member has the type
 * the row says (MEMBER_IS below): a FLOAT_AT row on an int member would
 * make the loader write a float's bytes into an int, which the compiler
 * would otherwise accept without a word.
 */

/*
 * MEMBER_IS(T, member, type) — the constant 0, but only when T's member
 * has exactly that type; any other type does not compile.
 *
 * _Generic(e, type: 1) chooses among its associations by the type of e.
 * It has one association and no `default`, so a member of another type
 * matches nothing, and that is a compile error. sizeof turns the choice
 * into a constant that can be added to an offsetof; its operand is never
 * evaluated, so the null pointer is never followed (the same trick as
 * FIELD_SIZE). TEXT_MEMBER checks a char array through the address of its
 * first element, a char *, so the check never depends on how a compiler
 * treats an array inside _Generic.
 */
#define MEMBER_IS(T, member, type) (0 * sizeof(_Generic(((T *)0)->member, type: 1)))
#define TEXT_MEMBER(T, member) (0 * sizeof(_Generic(&(((T *)0)->member)[0], char *: 1)))

#define FLOAT_AT(key, T, member) \
    { key, SERIALIZER_VALUE_NUMBER, NULL, 0, NULL, 0, 0, \
      SERIALIZER_STORE_FLOAT, offsetof(T, member) + MEMBER_IS(T, member, float), 0.0, NULL, NULL, \
      NULL, SERIALIZER_WRITE_ALWAYS, 0 }
#define FLOAT_AT_OR(key, T, member, fallback_value) \
    { key, SERIALIZER_VALUE_NUMBER, NULL, 0, NULL, 0, 0, \
      SERIALIZER_STORE_FLOAT, offsetof(T, member) + MEMBER_IS(T, member, float), \
      (double)(fallback_value), NULL, NULL, NULL, SERIALIZER_WRITE_ALWAYS, 0 }
#define FLOAT_AT_KEEP(key, T, member) \
    { key, SERIALIZER_VALUE_NUMBER, NULL, 0, NULL, 0, 0, \
      SERIALIZER_STORE_FLOAT, offsetof(T, member) + MEMBER_IS(T, member, float), 0.0, NULL, NULL, \
      NULL, SERIALIZER_WRITE_ALWAYS, 1 }
#define REQUIRED_FLOAT_AT(key, T, member) \
    { key, SERIALIZER_VALUE_NUMBER, NULL, 0, NULL, 1, 0, \
      SERIALIZER_STORE_FLOAT, offsetof(T, member) + MEMBER_IS(T, member, float), 0.0, NULL, NULL, \
      NULL, SERIALIZER_WRITE_ALWAYS, 0 }
#define INT_AT(key, T, member, fallback_value) \
    { key, SERIALIZER_VALUE_INTEGER, NULL, 0, NULL, 0, 0, \
      SERIALIZER_STORE_INT, offsetof(T, member) + MEMBER_IS(T, member, int), \
      (double)(fallback_value), NULL, NULL, NULL, SERIALIZER_WRITE_ALWAYS, 0 }
#define INT_AT_OMIT_ZERO(key, T, member) \
    { key, SERIALIZER_VALUE_INTEGER, NULL, 0, NULL, 0, 0, \
      SERIALIZER_STORE_INT, offsetof(T, member) + MEMBER_IS(T, member, int), 0.0, NULL, NULL, \
      NULL, SERIALIZER_OMIT_ZERO, 0 }
#define TEXT_AT(key, T, member, omit_rule) \
    { key, SERIALIZER_VALUE_STRING, NULL, 0, NULL, 0, FIELD_SIZE(T, member), \
      SERIALIZER_STORE_STRING, offsetof(T, member) + TEXT_MEMBER(T, member), 0.0, NULL, NULL, \
      NULL, omit_rule, 0 }
#define TEXT_AT_OR(key, T, member, omit_rule, fallback_text) \
    { key, SERIALIZER_VALUE_STRING, NULL, 0, NULL, 0, FIELD_SIZE(T, member), \
      SERIALIZER_STORE_STRING, offsetof(T, member) + TEXT_MEMBER(T, member), 0.0, fallback_text, \
      NULL, NULL, omit_rule, 0 }
#define TEXT_AT_KEEP(key, T, member, omit_rule) \
    { key, SERIALIZER_VALUE_STRING, NULL, 0, NULL, 0, FIELD_SIZE(T, member), \
      SERIALIZER_STORE_STRING, offsetof(T, member) + TEXT_MEMBER(T, member), 0.0, NULL, NULL, \
      NULL, omit_rule, 1 }
#define ENUM_AT(key, T, member, E, values, validator, from_name, to_name, fallback_name) \
    { key, SERIALIZER_VALUE_ENUM, values, sizeof(values) / sizeof((values)[0]), validator, 0, 0, \
      SERIALIZER_STORE_ENUM, offsetof(T, member) + MEMBER_IS(T, member, E), 0.0, fallback_name, \
      from_name, to_name, SERIALIZER_WRITE_ALWAYS, 0 }

static const char *const RAIL_LAYOUT_VALUES[] = {"RECT", "HORIZ"};
static const char *const AXE_MODE_VALUES[] = {"PENDULUM", "SPIN"};
static const char *const FLOAT_MODE_VALUES[] = {"STATIC", "CRUMBLE", "RAIL"};
static const char *const BOUNCEPAD_VALUES[] = {"GREEN", "WOOD", "RED"};

/*
 * The enum converters in serializer_types.c take and return their own enum
 * type. The table stores one kind of function pointer for all of them, so
 * each gets a pair of small adapters that speak int. An enum member is
 * read and written through an int as well; the asserts say that this is
 * sound on the compilers the project builds with.
 */
_Static_assert(sizeof(RailLayout) == sizeof(int) && sizeof(AxeTrapMode) == sizeof(int) &&
               sizeof(FloatPlatformMode) == sizeof(int) && sizeof(BouncepadType) == sizeof(int),
               "enum members are stored through an int");

static int rail_layout_from_name(const char *name) { return (int)serializer_rail_layout_from_str(name); }
static const char *rail_layout_to_name(int value) { return serializer_rail_layout_to_str((RailLayout)value); }
static int axe_mode_from_name(const char *name) { return (int)serializer_axe_mode_from_str(name); }
static const char *axe_mode_to_name(int value) { return serializer_axe_mode_to_str((AxeTrapMode)value); }
static int float_mode_from_name(const char *name) { return (int)serializer_float_mode_from_str(name); }
static const char *float_mode_to_name(int value) { return serializer_float_mode_to_str((FloatPlatformMode)value); }
static int bouncepad_type_from_name(const char *name) { return (int)serializer_bouncepad_type_from_str(name); }
static const char *bouncepad_type_to_name(int value) { return serializer_bouncepad_type_to_str((BouncepadType)value); }

/*
 * The field tables. The order of the rows is the order the writer emits
 * the keys in, so it is part of the file format: moving a row changes
 * every saved level.
 *
 * A table is written against one struct and may serve several arrays
 * whose elements have the same layout (coins and the three stars are all
 * "float x, y"). The _Static_asserts beside such a table prove that, so a
 * member added to one of those structs cannot silently shift another's.
 */
#define SAME_LAYOUT(A, B, member) \
    _Static_assert(sizeof(A) == sizeof(B) && offsetof(A, member) == offsetof(B, member), \
                   #A " and " #B " must keep the same layout: they share a field table")

static const SerializerFieldSpec XY_FIELDS[] = {
    FLOAT_AT("x", CoinPlacement, x),
    FLOAT_AT("y", CoinPlacement, y)
};
SAME_LAYOUT(CoinPlacement, StarYellowPlacement, x);
SAME_LAYOUT(CoinPlacement, StarYellowPlacement, y);
SAME_LAYOUT(CoinPlacement, StarGreenPlacement, x);
SAME_LAYOUT(CoinPlacement, StarGreenPlacement, y);
SAME_LAYOUT(CoinPlacement, StarRedPlacement, x);
SAME_LAYOUT(CoinPlacement, StarRedPlacement, y);

/* Checkpoints are strict even in a file without format_version: both keys
 * are required, so a checkpoint without a finite x and y fails the load
 * instead of quietly becoming a Continue point at 0,0. */
static const SerializerFieldSpec CHECKPOINT_FIELDS[] = {
    REQUIRED_FLOAT_AT("x", CheckpointPlacement, x),
    REQUIRED_FLOAT_AT("y", CheckpointPlacement, y)
};

static const SerializerFieldSpec RAIL_FIELDS[] = {
    ENUM_AT("layout", RailPlacement, layout, RailLayout, RAIL_LAYOUT_VALUES, serializer_rail_layout_is_valid,
            rail_layout_from_name, rail_layout_to_name, "RECT"),
    INT_AT("x", RailPlacement, x, 0),
    INT_AT("y", RailPlacement, y, 0),
    INT_AT("w", RailPlacement, w, 0),
    INT_AT("h", RailPlacement, h, 0),
    INT_AT("end_cap", RailPlacement, end_cap, 0)
};

static const SerializerFieldSpec PLATFORM_FIELDS[] = {
    FLOAT_AT("x", PlatformPlacement, x),
    INT_AT("tile_height", PlatformPlacement, tile_height, 1),
    INT_AT("tile_width", PlatformPlacement, tile_width, 1),
    TEXT_AT("tile_path", PlatformPlacement, tile_path, SERIALIZER_OMIT_EMPTY)
};

/* [last_star] is one table, not an array, so its rows are measured from
 * the start of LevelDef. A missing x or y keeps the position
 * level_def_init_defaults gave the star. */
static const SerializerFieldSpec LAST_STAR_FIELDS[] = {
    FLOAT_AT_KEEP("x", LevelDef, last_star.x),
    FLOAT_AT_KEEP("y", LevelDef, last_star.y),
    /* last_star.next_phase is stored in LevelDef.next_phase */
    TEXT_AT_KEEP("next_phase", LevelDef, next_phase, SERIALIZER_OMIT_EMPTY)
};

static const SerializerFieldSpec SPIDER_FIELDS[] = {
    FLOAT_AT("x", SpiderPlacement, x),
    FLOAT_AT("vx", SpiderPlacement, vx),
    FLOAT_AT("patrol_x0", SpiderPlacement, patrol_x0),
    FLOAT_AT("patrol_x1", SpiderPlacement, patrol_x1),
    INT_AT("frame_index", SpiderPlacement, frame_index, 0)
};

static const SerializerFieldSpec JUMPING_SPIDER_FIELDS[] = {
    FLOAT_AT("x", JumpingSpiderPlacement, x),
    FLOAT_AT("vx", JumpingSpiderPlacement, vx),
    FLOAT_AT("patrol_x0", JumpingSpiderPlacement, patrol_x0),
    FLOAT_AT("patrol_x1", JumpingSpiderPlacement, patrol_x1)
};

/* birds and faster_birds are both BirdPlacement arrays. */
static const SerializerFieldSpec BIRD_FIELDS[] = {
    FLOAT_AT("x", BirdPlacement, x),
    FLOAT_AT("base_y", BirdPlacement, base_y),
    FLOAT_AT("vx", BirdPlacement, vx),
    FLOAT_AT("patrol_x0", BirdPlacement, patrol_x0),
    FLOAT_AT("patrol_x1", BirdPlacement, patrol_x1),
    INT_AT("frame_index", BirdPlacement, frame_index, 0)
};

/* fish and faster_fish are both FishPlacement arrays. */
static const SerializerFieldSpec FISH_FIELDS[] = {
    FLOAT_AT("x", FishPlacement, x),
    FLOAT_AT("vx", FishPlacement, vx),
    FLOAT_AT("patrol_x0", FishPlacement, patrol_x0),
    FLOAT_AT("patrol_x1", FishPlacement, patrol_x1)
};

static const SerializerFieldSpec AXE_FIELDS[] = {
    FLOAT_AT("pillar_x", AxeTrapPlacement, pillar_x),
    FLOAT_AT("y", AxeTrapPlacement, y),
    ENUM_AT("mode", AxeTrapPlacement, mode, AxeTrapMode, AXE_MODE_VALUES, serializer_axe_mode_is_valid,
            axe_mode_from_name, axe_mode_to_name, "PENDULUM")
};

static const SerializerFieldSpec CIRCULAR_SAW_FIELDS[] = {
    FLOAT_AT("x", CircularSawPlacement, x),
    FLOAT_AT("y", CircularSawPlacement, y),
    FLOAT_AT("patrol_x0", CircularSawPlacement, patrol_x0),
    FLOAT_AT("patrol_x1", CircularSawPlacement, patrol_x1),
    INT_AT("direction", CircularSawPlacement, direction, 1)
};

static const SerializerFieldSpec SPIKE_ROW_FIELDS[] = {
    FLOAT_AT("x", SpikeRowPlacement, x),
    INT_AT("count", SpikeRowPlacement, count, 1)
};

static const SerializerFieldSpec SPIKE_PLATFORM_FIELDS[] = {
    FLOAT_AT("x", SpikePlatformPlacement, x),
    FLOAT_AT("y", SpikePlatformPlacement, y),
    INT_AT("tile_count", SpikePlatformPlacement, tile_count, 1)
};

static const SerializerFieldSpec SPIKE_BLOCK_FIELDS[] = {
    INT_AT("rail_index", SpikeBlockPlacement, rail_index, 0),
    FLOAT_AT("t_offset", SpikeBlockPlacement, t_offset),
    FLOAT_AT("speed", SpikeBlockPlacement, speed)
};

static const SerializerFieldSpec X_ONLY_FIELDS[] = {
    FLOAT_AT("x", BlueFlamePlacement, x)
};
SAME_LAYOUT(BlueFlamePlacement, FireFlamePlacement, x);

static const SerializerFieldSpec FLOAT_PLATFORM_FIELDS[] = {
    ENUM_AT("mode", FloatPlatformPlacement, mode, FloatPlatformMode, FLOAT_MODE_VALUES, serializer_float_mode_is_valid,
            float_mode_from_name, float_mode_to_name, "STATIC"),
    FLOAT_AT("x", FloatPlatformPlacement, x),
    FLOAT_AT("y", FloatPlatformPlacement, y),
    INT_AT("tile_count", FloatPlatformPlacement, tile_count, 1),
    INT_AT("rail_index", FloatPlatformPlacement, rail_index, 0),
    FLOAT_AT("t_offset", FloatPlatformPlacement, t_offset),
    FLOAT_AT("speed", FloatPlatformPlacement, speed)
};

static const SerializerFieldSpec BRIDGE_FIELDS[] = {
    FLOAT_AT("x", BridgePlacement, x),
    FLOAT_AT("y", BridgePlacement, y),
    INT_AT("brick_count", BridgePlacement, brick_count, 1)
};

/*
 * The three bouncepad arrays hold the same struct and differ in one thing:
 * the pad_type a pad gets when a file without format_version leaves the
 * key out. That used to be three hand-written loaders, each with its own
 * string; here it is the one argument that differs.
 */
#define BOUNCEPAD_FIELDS_DEFAULTING_TO(pad_type_name) { \
    FLOAT_AT("x", BouncepadPlacement, x), \
    FLOAT_AT("launch_vy", BouncepadPlacement, launch_vy), \
    ENUM_AT("pad_type", BouncepadPlacement, pad_type, BouncepadType, BOUNCEPAD_VALUES, \
            serializer_bouncepad_type_is_valid, bouncepad_type_from_name, \
            bouncepad_type_to_name, pad_type_name) }
static const SerializerFieldSpec BOUNCEPAD_SMALL_FIELDS[]  = BOUNCEPAD_FIELDS_DEFAULTING_TO("GREEN");
static const SerializerFieldSpec BOUNCEPAD_MEDIUM_FIELDS[] = BOUNCEPAD_FIELDS_DEFAULTING_TO("WOOD");
static const SerializerFieldSpec BOUNCEPAD_HIGH_FIELDS[]   = BOUNCEPAD_FIELDS_DEFAULTING_TO("RED");

static const SerializerFieldSpec VINE_FIELDS[] = {
    FLOAT_AT("x", VinePlacement, x),
    FLOAT_AT("y", VinePlacement, y),
    INT_AT("tile_count", VinePlacement, tile_count, 1),
    /* 0 (green) is the default and is not written. */
    INT_AT_OMIT_ZERO("vine_type", VinePlacement, vine_type)
};

static const SerializerFieldSpec CLIMBABLE_FIELDS[] = {
    FLOAT_AT("x", LadderPlacement, x),
    FLOAT_AT("y", LadderPlacement, y),
    INT_AT("tile_count", LadderPlacement, tile_count, 1)
};
SAME_LAYOUT(LadderPlacement, RopePlacement, x);
SAME_LAYOUT(LadderPlacement, RopePlacement, y);
SAME_LAYOUT(LadderPlacement, RopePlacement, tile_count);

/* background_layers, foreground_layers and fog_layers are all LevelLayer. */
static const SerializerFieldSpec LAYER_FIELDS[] = {
    TEXT_AT("path", LevelLayer, path, SERIALIZER_WRITE_ALWAYS),
    FLOAT_AT("speed", LevelLayer, speed)
};

/*
 * [physics] is one table of LevelDef.physics. LEVEL_PHYSICS_ENGINE_DEFAULT
 * (-1) means "use the engine's own value" (level_physics.c), so that is
 * what a missing key reads as, the same value level_def_init_defaults sets.
 */
#define PHYSICS_AT(key, member) \
    FLOAT_AT_OR(key, LevelDef, physics.member, LEVEL_PHYSICS_ENGINE_DEFAULT)
static const SerializerFieldSpec PHYSICS_FIELDS[] = {
    PHYSICS_AT("walk_max_speed", walk_max_speed),
    PHYSICS_AT("run_max_speed", run_max_speed),
    PHYSICS_AT("walk_ground_accel", walk_ground_accel),
    PHYSICS_AT("run_ground_accel", run_ground_accel),
    PHYSICS_AT("ground_friction", ground_friction),
    PHYSICS_AT("ground_counter_accel", ground_counter_accel),
    PHYSICS_AT("air_accel_walk", air_accel_walk),
    PHYSICS_AT("air_accel_run", air_accel_run),
    PHYSICS_AT("air_friction", air_friction),
    PHYSICS_AT("cam_lookahead_vx_factor", cam_lookahead_vx_factor),
    PHYSICS_AT("cam_lookahead_max", cam_lookahead_max)
};
#undef PHYSICS_AT

#define FIELD_COUNT(fields) (sizeof(fields) / sizeof((fields)[0]))
#define NOT_AN_ARRAY 0, 0, 0   /* items_offset, count_offset, item_size */
/* A key that is only checked here: format_version, read and written by
 * hand because it is a constant of the format and not a value of a level. */
#define ROOT_VALUE(key, value_kind) \
    { key, SERIALIZER_ROOT_VALUE, value_kind, NULL, 0, 0, 0, NOT_AN_ARRAY, 0 }
/*
 * A level-wide key: one value in LevelDef itself. The row carries a field
 * table of exactly one row, written right here as a compound literal
 * (an unnamed array with static storage), so a root key is loaded and
 * written by the same load_field and write_field as a key of a spider.
 *
 *   ROOT_FLOAT(key, member)                    a float; missing reads as 0
 *   ROOT_INT(key, member, fallback)            an int
 *   ROOT_TEXT(key, member, omit, fallback)     a char array
 */
#define ROOT_ONE(key, value_kind, capacity, field_row) \
    { key, SERIALIZER_ROOT_VALUE, value_kind, (const SerializerFieldSpec[]){ field_row }, 1, \
      0, capacity, NOT_AN_ARRAY, 0 }
#define ROOT_FLOAT(key, member) \
    ROOT_ONE(key, SERIALIZER_VALUE_NUMBER, 0, FLOAT_AT(key, LevelDef, member))
#define ROOT_INT(key, member, fallback_value) \
    ROOT_ONE(key, SERIALIZER_VALUE_INTEGER, 0, INT_AT(key, LevelDef, member, fallback_value))
#define ROOT_TEXT(key, member, omit_rule, fallback_text) \
    ROOT_ONE(key, SERIALIZER_VALUE_STRING, FIELD_SIZE(LevelDef, member), \
             TEXT_AT_OR(key, LevelDef, member, omit_rule, fallback_text))
/* One [table] whose keys live in LevelDef. width pads the keys so the
 * "=" signs line up in the saved file (0 = no padding). */
#define ROOT_TABLE(key, table_fields, width) \
    { key, SERIALIZER_ROOT_TABLE, SERIALIZER_VALUE_STRING, table_fields, \
      FIELD_COUNT(table_fields), 0, 0, NOT_AN_ARRAY, width }
/* A plain array of integers: the LevelDef array and its count member. */
#define ROOT_INTEGERS(key, array_member, count_member) \
    { key, SERIALIZER_ROOT_ARRAY_INTEGER, SERIALIZER_VALUE_INTEGER, NULL, 0, \
      (int)(sizeof(((LevelDef *)0)->array_member) / sizeof(((LevelDef *)0)->array_member[0])), 0, \
      offsetof(LevelDef, array_member), offsetof(LevelDef, count_member), \
      sizeof(((LevelDef *)0)->array_member[0]), 0 }
/*
 * An array of tables the generic loader and writer handle: the key, its
 * field table, and the LevelDef array and count members that hold it. The
 * capacity is the array's own length (sizeof array / sizeof element), so
 * it cannot disagree with level.h.
 */
#define ROOT_TABLES(key, table_fields, array_member, count_member) \
    { key, SERIALIZER_ROOT_ARRAY_TABLE, SERIALIZER_VALUE_STRING, table_fields, \
      FIELD_COUNT(table_fields), \
      (int)(sizeof(((LevelDef *)0)->array_member) / sizeof(((LevelDef *)0)->array_member[0])), 0, \
      offsetof(LevelDef, array_member), offsetof(LevelDef, count_member), \
      sizeof(((LevelDef *)0)->array_member[0]), 0 }

/* floor_gaps is read and written as ints. */
_Static_assert(sizeof(((LevelDef *)0)->floor_gaps[0]) == sizeof(int),
               "ROOT_INTEGERS stores each element through an int");

/* See docs/wiki/level-design.md for these keys as a level author meets them. */
static const SerializerRootSpec ROOT_FIELDS[] = {
    ROOT_VALUE("format_version", SERIALIZER_VALUE_INTEGER),
    ROOT_TEXT("name", name, SERIALIZER_WRITE_UNTITLED_IF_EMPTY, ""),
    ROOT_TEXT("description", description, SERIALIZER_OMIT_EMPTY, ""),
    ROOT_TEXT("generated_by", generated_by, SERIALIZER_OMIT_EMPTY, ""),
    ROOT_INT("screen_count", screen_count, LEVEL_DEFAULT_SCREEN_COUNT),
    ROOT_INTEGERS("floor_gaps", floor_gaps, floor_gap_count),
    ROOT_TABLES("rails", RAIL_FIELDS, rails, rail_count),
    ROOT_TABLES("platforms", PLATFORM_FIELDS, platforms, platform_count),
    ROOT_TABLES("checkpoints", CHECKPOINT_FIELDS, checkpoints, checkpoint_count),
    ROOT_TABLES("coins", XY_FIELDS, coins, coin_count),
    ROOT_TABLES("star_yellows", XY_FIELDS, star_yellows, star_yellow_count),
    ROOT_TABLES("star_greens", XY_FIELDS, star_greens, star_green_count),
    ROOT_TABLES("star_reds", XY_FIELDS, star_reds, star_red_count),
    ROOT_TABLE("last_star", LAST_STAR_FIELDS, 0),
    ROOT_TABLES("spiders", SPIDER_FIELDS, spiders, spider_count),
    ROOT_TABLES("jumping_spiders", JUMPING_SPIDER_FIELDS, jumping_spiders, jumping_spider_count),
    ROOT_TABLES("birds", BIRD_FIELDS, birds, bird_count),
    ROOT_TABLES("faster_birds", BIRD_FIELDS, faster_birds, faster_bird_count),
    ROOT_TABLES("fish", FISH_FIELDS, fish, fish_count),
    ROOT_TABLES("faster_fish", FISH_FIELDS, faster_fish, faster_fish_count),
    ROOT_TABLES("axe_traps", AXE_FIELDS, axe_traps, axe_trap_count),
    ROOT_TABLES("circular_saws", CIRCULAR_SAW_FIELDS, circular_saws, circular_saw_count),
    ROOT_TABLES("spike_rows", SPIKE_ROW_FIELDS, spike_rows, spike_row_count),
    ROOT_TABLES("spike_platforms", SPIKE_PLATFORM_FIELDS, spike_platforms, spike_platform_count),
    ROOT_TABLES("spike_blocks", SPIKE_BLOCK_FIELDS, spike_blocks, spike_block_count),
    ROOT_TABLES("blue_flames", X_ONLY_FIELDS, blue_flames, blue_flame_count),
    ROOT_TABLES("fire_flames", X_ONLY_FIELDS, fire_flames, fire_flame_count),
    ROOT_TABLES("float_platforms", FLOAT_PLATFORM_FIELDS, float_platforms, float_platform_count),
    ROOT_TABLES("bridges", BRIDGE_FIELDS, bridges, bridge_count),
    ROOT_TABLES("bouncepads_small", BOUNCEPAD_SMALL_FIELDS, bouncepads_small, bouncepad_small_count),
    ROOT_TABLES("bouncepads_medium", BOUNCEPAD_MEDIUM_FIELDS, bouncepads_medium, bouncepad_medium_count),
    ROOT_TABLES("bouncepads_high", BOUNCEPAD_HIGH_FIELDS, bouncepads_high, bouncepad_high_count),
    ROOT_TABLES("vines", VINE_FIELDS, vines, vine_count),
    ROOT_TABLES("ladders", CLIMBABLE_FIELDS, ladders, ladder_count),
    ROOT_TABLES("ropes", CLIMBABLE_FIELDS, ropes, rope_count),
    ROOT_TABLES("background_layers", LAYER_FIELDS, background_layers, background_layer_count),
    ROOT_TABLES("foreground_layers", LAYER_FIELDS, foreground_layers, foreground_layer_count),
    ROOT_TABLES("fog_layers", LAYER_FIELDS, fog_layers, fog_layer_count),
    ROOT_FLOAT("player_start_x", player_start_x),
    ROOT_FLOAT("player_start_y", player_start_y),
    ROOT_TEXT("music_path", music_path, SERIALIZER_WRITE_ALWAYS, ""),
    ROOT_INT("music_volume", music_volume, 0),
    ROOT_TEXT("floor_tile_path", floor_tile_path, SERIALIZER_WRITE_ALWAYS, ""),
    ROOT_INT("initial_hearts", initial_hearts, 0),
    ROOT_INT("initial_lives", initial_lives, 0),
    ROOT_INT("score_per_life", score_per_life, 0),
    ROOT_INT("coin_score", coin_score, 0),
    /* 20 is the longest key but one; the file has always been aligned so. */
    ROOT_TABLE("physics", PHYSICS_FIELDS, 20)
};
#define ROOT_FIELD_COUNT FIELD_COUNT(ROOT_FIELDS)

#undef ROOT_INTEGERS
#undef ROOT_TABLE
#undef ROOT_TEXT
#undef ROOT_INT
#undef ROOT_FLOAT
#undef ROOT_ONE
#undef ROOT_VALUE
#undef FIELD_SIZE
#undef TEXT_MEMBER
#undef MEMBER_IS

/* ------------------------------------------------------------------ */
/* Finding a row                                                        */
/* ------------------------------------------------------------------ */

size_t serializer_schema_row_count(void)
{
    return ROOT_FIELD_COUNT;
}

const char *serializer_schema_row_name(size_t index)
{
    return index < serializer_schema_row_count() ? ROOT_FIELDS[index].name : NULL;
}

const SerializerRootSpec *serializer_schema_row(size_t index)
{
    return index < ROOT_FIELD_COUNT ? &ROOT_FIELDS[index] : NULL;
}

const SerializerFieldSpec *serializer_find_field(const SerializerFieldSpec *fields,
                                             size_t field_count,
                                             const char *name, int name_len)
{
    for (size_t i = 0; i < field_count; i++) {
        if (serializer_key_matches(name, name_len, fields[i].name)) {
            return &fields[i];
        }
    }
    return NULL;
}

const SerializerRootSpec *serializer_find_root(const char *name, int name_len)
{
    for (size_t i = 0; i < ROOT_FIELD_COUNT; i++) {
        if (serializer_key_matches(name, name_len, ROOT_FIELDS[i].name)) {
            return &ROOT_FIELDS[i];
        }
    }
    return NULL;
}

/* 1 when any key of the row's tables is required: such an array is read
 * strictly even in a file without format_version. */
int serializer_root_is_strict(const SerializerRootSpec *root)
{
    for (size_t f = 0; f < root->field_count; f++)
        if (root->fields[f].required) return 1;
    return 0;
}

/* 1 when text is one of the names an ENUM row accepts ("RECT", "SPIN"). */
int serializer_enum_name_known(const SerializerFieldSpec *field, const char *text)
{
    if (!text) return 0;
    for (size_t i = 0; i < field->enum_count; i++) {
        if (strcmp(text, field->enum_values[i]) == 0)
            return !field->enum_validator || field->enum_validator(text);
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Printing the schema                                                 */
/* ------------------------------------------------------------------ */

static const char *value_type_name(SerializerValueType type)
{
    switch (type) {
    case SERIALIZER_VALUE_INTEGER: return "integer";
    case SERIALIZER_VALUE_NUMBER:  return "number";
    case SERIALIZER_VALUE_STRING:  return "string";
    case SERIALIZER_VALUE_ENUM:    return "enum";
    }
    return "unknown";
}

/* One "field" line: the key, its type, then what else a checker needs. */
static void dump_field(FILE *fp, const char *owner, const SerializerFieldSpec *field)
{
    fprintf(fp, "field %s %s %s", owner, field->name, value_type_name(field->type));
    if (field->required) fprintf(fp, " required");
    if (field->type == SERIALIZER_VALUE_STRING)
        fprintf(fp, " capacity=%zu", field->string_capacity);
    for (size_t i = 0; i < field->enum_count; i++)
        fprintf(fp, "%s%s", i == 0 ? " values=" : ",", field->enum_values[i]);
    fprintf(fp, "\n");
}

void serializer_dump_schema(FILE *fp)
{
    if (!fp) return;
    for (size_t r = 0; r < ROOT_FIELD_COUNT; r++) {
        const SerializerRootSpec *root = &ROOT_FIELDS[r];

        switch (root->type) {
        case SERIALIZER_ROOT_VALUE:
            fprintf(fp, "value %s %s", root->name, value_type_name(root->value_type));
            if (root->value_type == SERIALIZER_VALUE_STRING)
                fprintf(fp, " capacity=%zu", root->string_capacity);
            fprintf(fp, "\n");
            break;
        case SERIALIZER_ROOT_ARRAY_INTEGER:
            fprintf(fp, "integers %s max=%d\n", root->name, root->max_count);
            break;
        case SERIALIZER_ROOT_TABLE:
            fprintf(fp, "table %s\n", root->name);
            for (size_t f = 0; f < root->field_count; f++)
                dump_field(fp, root->name, &root->fields[f]);
            break;
        case SERIALIZER_ROOT_ARRAY_TABLE:
            fprintf(fp, "tables %s max=%d\n", root->name, root->max_count);
            for (size_t f = 0; f < root->field_count; f++)
                dump_field(fp, root->name, &root->fields[f]);
            break;
        }
    }
}

