/*
 * serializer_parse.c — Internal TOML parse helpers and v1 schema checks.
 */

#include "serializer_parse.h"

#include <ctype.h>   /* isalnum */
#include <limits.h>  /* INT_MIN, INT_MAX */
#include <float.h>   /* FLT_MAX — check before narrowing a TOML double */
#include <math.h>    /* isfinite */
#include <stdarg.h>  /* va_list */
#include <stdio.h>   /* vsnprintf */
#include <stdlib.h>  /* strtol */
#include <string.h>  /* memchr, memcmp, strlen, strncpy */

#include "serializer_emit.h"  /* fmt_float, write_toml_key_string */
#include "serializer_types.h" /* enum validation */
#include "printf_format.h"    /* PRINTF_FORMAT */
#include "utf8.h"             /* utf8_valid */
#include "../levels/level.h" /* LevelDef and its MAX_* array limits */

/* ------------------------------------------------------------------ */
/* Permissive legacy field readers                                      */
/* ------------------------------------------------------------------ */

/*
 * Match schema keys by exact byte length and reject embedded NULs explicitly.
 * tomlc17's public lookup is also length-aware; these helpers additionally
 * enforce the serializer's key-validation contract.
 */
static int toml_key_matches(const char *supplied, int supplied_len,
                            const char *expected)
{
    size_t expected_len;

    if (!supplied || supplied_len < 0 || !expected) return 0;
    expected_len = strlen(expected);
    if ((size_t)supplied_len != expected_len) return 0;
    if (memchr(supplied, '\0', (size_t)supplied_len) != NULL) return 0;
    return memcmp(supplied, expected, expected_len) == 0;
}

static toml_datum_t toml_table_get_exact(toml_datum_t table,
                                         const char *expected)
{
    toml_datum_t missing = {0};

    if (table.type != TOML_TABLE || !expected) return missing;
    for (int i = 0; i < table.u.tab.size; i++) {
        if (toml_key_matches(table.u.tab.key[i], table.u.tab.len[i],
                             expected)) {
            return table.u.tab.value[i];
        }
    }
    return missing;
}

/*
 * get_float — Read a numeric field from a TOML table, returning float.
 *
 * TOML distinguishes integers (no decimal point) from floats (with decimal).
 * A field written as "79" comes back as TOML_INT64, while "79.0" comes as
 * TOML_FP64.  Legacy loading accepts both and converts to float.
 */
float get_float(toml_datum_t tab, const char *key, float fallback)
{
    toml_datum_t d = toml_table_get_exact(tab, key);
    if (d.type == TOML_FP64)  return (float)d.u.fp64;
    if (d.type == TOML_INT64) return (float)d.u.int64;
    return fallback;
}

/*
 * get_int — Read an integer field from a TOML table.
 *
 * Legacy loading accepts both TOML_INT64 and TOML_FP64 for compatibility with
 * old hand-written files.  Explicit v1 files are checked before this helper
 * runs, so v1 fractional values never reach this truncating conversion.
 */
int get_int(toml_datum_t tab, const char *key, int fallback)
{
    toml_datum_t d = toml_table_get_exact(tab, key);
    if (d.type == TOML_INT64 && d.u.int64 >= INT_MIN && d.u.int64 <= INT_MAX)
        return (int)d.u.int64;
    if (d.type == TOML_FP64 && isfinite(d.u.fp64) &&
        trunc(d.u.fp64) >= INT_MIN && trunc(d.u.fp64) <= INT_MAX)
        return (int)d.u.fp64;
    return fallback;
}

/*
 * get_str — Read a TOML string field from a table.
 *
 * The returned pointer belongs to the parsed TOML tree and remains valid until
 * toml_free.  Legacy loading uses the fallback for missing/non-string values.
 */
const char *get_str(toml_datum_t tab, const char *key, const char *fallback)
{
    toml_datum_t d = toml_table_get_exact(tab, key);
    if (d.type == TOML_STRING) return d.u.str.ptr;
    return fallback;
}

void copy_str(char *dst, size_t dst_size, const char *src)
{
    if (!dst || dst_size == 0) return;

    strncpy(dst, src ? src : "", dst_size - 1);
    dst[dst_size - 1] = '\0';
}

/* ------------------------------------------------------------------ */
/* Explicit v1 schema                                                   */
/* ------------------------------------------------------------------ */

typedef enum {
    SERIALIZER_VALUE_INTEGER,
    SERIALIZER_VALUE_NUMBER,
    SERIALIZER_VALUE_STRING,
    SERIALIZER_VALUE_ENUM
} SerializerValueType;

typedef int (*SerializerEnumValidator)(const char *value);

/*
 * SerializerFieldSpec — everything the serializer knows about one key of a
 * [[table]]: its name and type (what the schema check needs), and where
 * its value lives in the C struct, what it defaults to and how it is
 * written (what the loader and the writer need).
 *
 * One row therefore serves four readers:
 *   - the v1 schema check (name, type, enum_values, required, capacity),
 *   - the loader, serializer_load_schema (offset, storage, fallback),
 *   - the writer, serializer_write_entry (offset, storage, omit),
 *   - `level_check --dump-schema`, which prints the table so a test can
 *     compare tools/validate_levels.py with it.
 * Before, each of those was written out by hand per field, in four files,
 * and only the round-trip fuzzer noticed when two of them disagreed.
 *
 * Adding a key to an entity is now: the struct member in level.h and one
 * row here (plus its validation rule, its editor field and the manual).
 *
 * `storage` says what the member is, which decides how the bytes at
 * `offset` are read and written. SERIALIZER_STORE_NONE marks a row that is
 * only checked here and is loaded and saved by hand elsewhere; the only
 * one left is format_version, which is a constant and not a level value.
 *
 * The same rows describe the level-wide keys, [physics] and [last_star].
 * Their values live in LevelDef itself, so their offsets are measured from
 * the start of a LevelDef instead of from the start of an array element.
 */
typedef enum {
    SERIALIZER_STORE_NONE,    /* not handled by the generic loader/writer */
    SERIALIZER_STORE_FLOAT,   /* float member                             */
    SERIALIZER_STORE_INT,     /* int member                               */
    SERIALIZER_STORE_STRING,  /* char array of string_capacity bytes      */
    SERIALIZER_STORE_ENUM     /* a C enum member, written as its name     */
} SerializerStorage;

/* When the writer leaves a key out (the loader then gives the same value
 * back as its fallback, so nothing is lost). */
typedef enum {
    SERIALIZER_WRITE_ALWAYS,
    SERIALIZER_OMIT_EMPTY,    /* a string that is ""  */
    SERIALIZER_OMIT_ZERO,     /* an integer that is 0 */
    /* A string that is "" is written as its fallback instead: a level with
     * no name is saved as "Untitled", the name it would load with. */
    SERIALIZER_WRITE_FALLBACK_IF_EMPTY
} SerializerOmit;

typedef struct {
    const char *name;
    SerializerValueType type;
    const char *const *enum_values;
    size_t enum_count;
    SerializerEnumValidator enum_validator;
    int required;
    size_t string_capacity; /* STRING fields: bytes of the char array the value
                             * is copied into, including the final NUL */
    /* ---- where the value lives, for the generic loader and writer ---- */
    SerializerStorage storage;
    size_t offset;              /* offsetof(<element struct>, <member>)       */
    double fallback;            /* FLOAT/INT: used when the key is missing    */
    const char *text_fallback;  /* ENUM and STRING: the text used when the key
                                 * is missing (NULL reads as "")               */
    int (*enum_from_name)(const char *name);   /* ENUM: name -> value          */
    const char *(*enum_to_name)(int value);    /* ENUM: value -> name          */
    SerializerOmit omit;
    /* 1 = a missing key leaves the member as it is, so the value that
     * level_def_init_defaults gave it stays (the last star's position). */
    int keep_missing;
} SerializerFieldSpec;

typedef enum {
    SERIALIZER_ROOT_VALUE,
    SERIALIZER_ROOT_TABLE,
    SERIALIZER_ROOT_ARRAY_INTEGER,
    SERIALIZER_ROOT_ARRAY_TABLE
} SerializerRootType;

typedef struct {
    const char *name;
    SerializerRootType type;
    SerializerValueType value_type;
    const SerializerFieldSpec *fields;
    size_t field_count;
    int max_count;
    size_t string_capacity; /* string values: same as in SerializerFieldSpec */
    /* ---- an array of tables the generic loader and writer handle ------ */
    size_t items_offset;    /* offsetof(LevelDef, <array>)                  */
    size_t count_offset;    /* offsetof(LevelDef, <count>)                  */
    size_t item_size;       /* sizeof one element; 0 = not such an array    */
    /* ---- one [table]: pad its keys to this width so the "=" line up ---- */
    int key_width;
} SerializerRootSpec;

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
 *   ENUM_AT(key, T, member, values, validator, from, to, fallback)
 *                                         a C enum, written as its name
 */
#define FLOAT_AT(key, T, member) \
    { key, SERIALIZER_VALUE_NUMBER, NULL, 0, NULL, 0, 0, \
      SERIALIZER_STORE_FLOAT, offsetof(T, member), 0.0, NULL, NULL, NULL, SERIALIZER_WRITE_ALWAYS, 0 }
#define FLOAT_AT_OR(key, T, member, fallback_value) \
    { key, SERIALIZER_VALUE_NUMBER, NULL, 0, NULL, 0, 0, \
      SERIALIZER_STORE_FLOAT, offsetof(T, member), (double)(fallback_value), NULL, NULL, NULL, \
      SERIALIZER_WRITE_ALWAYS, 0 }
#define FLOAT_AT_KEEP(key, T, member) \
    { key, SERIALIZER_VALUE_NUMBER, NULL, 0, NULL, 0, 0, \
      SERIALIZER_STORE_FLOAT, offsetof(T, member), 0.0, NULL, NULL, NULL, SERIALIZER_WRITE_ALWAYS, 1 }
#define REQUIRED_FLOAT_AT(key, T, member) \
    { key, SERIALIZER_VALUE_NUMBER, NULL, 0, NULL, 1, 0, \
      SERIALIZER_STORE_FLOAT, offsetof(T, member), 0.0, NULL, NULL, NULL, SERIALIZER_WRITE_ALWAYS, 0 }
#define INT_AT(key, T, member, fallback_value) \
    { key, SERIALIZER_VALUE_INTEGER, NULL, 0, NULL, 0, 0, \
      SERIALIZER_STORE_INT, offsetof(T, member), (double)(fallback_value), NULL, NULL, NULL, \
      SERIALIZER_WRITE_ALWAYS, 0 }
#define INT_AT_OMIT_ZERO(key, T, member) \
    { key, SERIALIZER_VALUE_INTEGER, NULL, 0, NULL, 0, 0, \
      SERIALIZER_STORE_INT, offsetof(T, member), 0.0, NULL, NULL, NULL, SERIALIZER_OMIT_ZERO, 0 }
#define TEXT_AT(key, T, member, omit_rule) \
    { key, SERIALIZER_VALUE_STRING, NULL, 0, NULL, 0, FIELD_SIZE(T, member), \
      SERIALIZER_STORE_STRING, offsetof(T, member), 0.0, NULL, NULL, NULL, omit_rule, 0 }
#define TEXT_AT_OR(key, T, member, omit_rule, fallback_text) \
    { key, SERIALIZER_VALUE_STRING, NULL, 0, NULL, 0, FIELD_SIZE(T, member), \
      SERIALIZER_STORE_STRING, offsetof(T, member), 0.0, fallback_text, NULL, NULL, omit_rule, 0 }
#define TEXT_AT_KEEP(key, T, member, omit_rule) \
    { key, SERIALIZER_VALUE_STRING, NULL, 0, NULL, 0, FIELD_SIZE(T, member), \
      SERIALIZER_STORE_STRING, offsetof(T, member), 0.0, NULL, NULL, NULL, omit_rule, 1 }
#define ENUM_AT(key, T, member, values, validator, from_name, to_name, fallback_name) \
    { key, SERIALIZER_VALUE_ENUM, values, sizeof(values) / sizeof((values)[0]), validator, 0, 0, \
      SERIALIZER_STORE_ENUM, offsetof(T, member), 0.0, fallback_name, from_name, to_name, \
      SERIALIZER_WRITE_ALWAYS, 0 }

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
    ENUM_AT("layout", RailPlacement, layout, RAIL_LAYOUT_VALUES, serializer_rail_layout_is_valid,
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
    ENUM_AT("mode", AxeTrapPlacement, mode, AXE_MODE_VALUES, serializer_axe_mode_is_valid,
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
    ENUM_AT("mode", FloatPlatformPlacement, mode, FLOAT_MODE_VALUES, serializer_float_mode_is_valid,
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
    ENUM_AT("pad_type", BouncepadPlacement, pad_type, BOUNCEPAD_VALUES, \
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
 * [physics] is one table of LevelDef.physics. -1.0 means "use the engine's
 * own value" (level_physics.c), so that is what a missing key reads as.
 */
#define PHYSICS_AT(key, member) FLOAT_AT_OR(key, LevelDef, physics.member, -1.0)
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

static const SerializerRootSpec ROOT_FIELDS[] = {
    ROOT_VALUE("format_version", SERIALIZER_VALUE_INTEGER),
    ROOT_TEXT("name", name, SERIALIZER_WRITE_FALLBACK_IF_EMPTY, "Untitled"),
    ROOT_TEXT("description", description, SERIALIZER_OMIT_EMPTY, ""),
    ROOT_TEXT("generated_by", generated_by, SERIALIZER_OMIT_EMPTY, ""),
    ROOT_INT("screen_count", screen_count, 4),
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

#undef ROOT_INTEGERS
#undef ROOT_TABLE
#undef ROOT_TEXT
#undef ROOT_INT
#undef ROOT_FLOAT
#undef ROOT_ONE
#undef ROOT_VALUE
#undef FIELD_COUNT
#undef FIELD_SIZE

PRINTF_FORMAT(3, 4)
static int schema_error(char *error, size_t error_size, const char *format, ...)
{
    va_list args;

    if (error && error_size > 0) {
        va_start(args, format);
        vsnprintf(error, error_size, format, args);
        va_end(args);
    }
    return -1;
}

static const char *toml_type_name(toml_type_t type)
{
    switch (type) {
        case TOML_STRING:  return "string";
        case TOML_INT64:   return "integer";
        case TOML_FP64:    return "float";
        case TOML_BOOLEAN: return "boolean";
        case TOML_DATE:    return "date";
        case TOML_TIME:    return "time";
        case TOML_DATETIME: return "datetime";
        case TOML_DATETIMETZ: return "datetime-with-zone";
        case TOML_ARRAY:   return "array";
        case TOML_TABLE:   return "table";
        default:           return "unknown";
    }
}

static const SerializerFieldSpec *find_field(const SerializerFieldSpec *fields,
                                             size_t field_count,
                                             const char *name, int name_len)
{
    for (size_t i = 0; i < field_count; i++) {
        if (toml_key_matches(name, name_len, fields[i].name)) {
            return &fields[i];
        }
    }
    return NULL;
}

static const SerializerRootSpec *find_root_field(const char *name, int name_len)
{
    for (size_t i = 0; i < sizeof(ROOT_FIELDS) / sizeof(ROOT_FIELDS[0]); i++) {
        if (toml_key_matches(name, name_len, ROOT_FIELDS[i].name)) {
            return &ROOT_FIELDS[i];
        }
    }
    return NULL;
}

static int toml_key_has_embedded_nul(const char *key, int key_len)
{
    if (!key || key_len < 0) return 1;
    return memchr(key, '\0', (size_t)key_len) != NULL;
}

static int toml_string_has_embedded_nul(toml_datum_t value)
{
    if (value.type != TOML_STRING || !value.u.str.ptr || value.u.str.len < 0) {
        return 0;
    }
    return memchr(value.u.str.ptr, '\0', (size_t)value.u.str.len) != NULL;
}

static int toml_tree_has_embedded_nul(toml_datum_t value)
{
    if (value.type == TOML_STRING) {
        return toml_string_has_embedded_nul(value);
    }
    if (value.type == TOML_ARRAY) {
        for (int i = 0; i < value.u.arr.size; i++) {
            if (toml_tree_has_embedded_nul(value.u.arr.elem[i])) return 1;
        }
        return 0;
    }
    if (value.type == TOML_TABLE) {
        for (int i = 0; i < value.u.tab.size; i++) {
            if (toml_key_has_embedded_nul(value.u.tab.key[i],
                                          value.u.tab.len[i]) ||
                toml_tree_has_embedded_nul(value.u.tab.value[i]))
                return 1;
        }
    }
    return 0;
}

static int toml_string_matches(toml_datum_t value, const char *expected)
{
    size_t expected_len;

    if (value.type != TOML_STRING || !value.u.str.ptr ||
        value.u.str.len < 0 || !expected) {
        return 0;
    }
    expected_len = strlen(expected);
    if ((size_t)value.u.str.len != expected_len) return 0;
    if (toml_string_has_embedded_nul(value)) return 0;
    return memcmp(value.u.str.ptr, expected, expected_len) == 0;
}

/* string_capacity is only read for SERIALIZER_VALUE_STRING (see
 * TEXT_AT); every other type passes 0. */
static int validate_scalar(toml_datum_t value, SerializerValueType expected,
                           const char *path, const char *const *enum_values,
                           size_t enum_count,
                           SerializerEnumValidator enum_validator,
                           size_t string_capacity,
                           char *error, size_t error_size)
{
    if (expected == SERIALIZER_VALUE_INTEGER) {
        if (value.type != TOML_INT64) {
            return schema_error(error, error_size,
                                "%s has type %s, expected integer",
                                path, toml_type_name(value.type));
        }
        if (value.u.int64 < (int64_t)INT_MIN ||
            value.u.int64 > (int64_t)INT_MAX) {
            return schema_error(error, error_size,
                                "%s integer is outside C int range",
                                path);
        }
        return 0;
    }

    if (expected == SERIALIZER_VALUE_NUMBER) {
        if (value.type == TOML_INT64) {
            /* Every int64 value lies within the finite float range. */
            return 0;
        }
        if (value.type == TOML_FP64) {
            if (!isfinite(value.u.fp64) || fabs(value.u.fp64) > FLT_MAX) {
                return schema_error(error, error_size,
                                    "%s must be a finite number", path);
            }
            return 0;
        }
        return schema_error(error, error_size,
                            "%s has type %s, expected finite number",
                            path, toml_type_name(value.type));
    }

    if (expected == SERIALIZER_VALUE_STRING) {
        if (value.type != TOML_STRING) {
            return schema_error(error, error_size,
                                "%s has type %s, expected string",
                                path, toml_type_name(value.type));
        }
        if (toml_string_has_embedded_nul(value)) {
            return schema_error(error, error_size,
                                "%s contains an embedded NUL", path);
        }
        /* tomlc17 passes raw string bytes through unchecked.  Saving them
         * back would write a file strict TOML readers (Python's tomllib in
         * tools/validate_levels.py) refuse, so refuse it here first. */
        if (value.u.str.len < 0 ||
            !utf8_valid(value.u.str.ptr, (size_t)value.u.str.len)) {
            return schema_error(error, error_size,
                                "%s is not valid UTF-8", path);
        }
        /* The value and its final NUL must fit in the field's array. */
        if ((size_t)value.u.str.len >= string_capacity) {
            return schema_error(error, error_size, "%s exceeds its %zu-byte storage",
                                path, string_capacity - 1);
        }
        return 0;
    }

    if (value.type != TOML_STRING) {
        return schema_error(error, error_size,
                            "%s has type %s, expected enum string",
                            path, toml_type_name(value.type));
    }
    if (toml_string_has_embedded_nul(value)) {
        return schema_error(error, error_size,
                            "%s contains an embedded NUL", path);
    }
    for (size_t i = 0; i < enum_count; i++) {
        /* The exact length/NUL proof happens before any validator call. */
        if (toml_string_matches(value, enum_values[i])) {
            if (!enum_validator || enum_validator(value.u.str.ptr)) return 0;
        }
    }
    return schema_error(error, error_size,
                        "%s has unknown enum value", path);
}

static int validate_table_fields(toml_datum_t table, const char *path,
                                 const SerializerFieldSpec *fields,
                                 size_t field_count, char *error,
                                 size_t error_size)
{
    if (table.type != TOML_TABLE) {
        return schema_error(error, error_size,
                            "%s has type %s, expected table",
                            path, toml_type_name(table.type));
    }

    for (int i = 0; i < table.u.tab.size; i++) {
        const char *key = table.u.tab.key[i];
        int key_len = table.u.tab.len[i];
        const SerializerFieldSpec *field;
        char child_path[256];

        if (toml_key_has_embedded_nul(key, key_len)) {
            return schema_error(error, error_size,
                                "%s has an embedded NUL in a key", path);
        }
        field = find_field(fields, field_count, key, key_len);
        snprintf(child_path, sizeof(child_path), "%s.%s", path, key);
        if (!field) {
            return schema_error(error, error_size,
                                "%s has unknown key '%s'", path, key);
        }
        if (validate_scalar(table.u.tab.value[i], field->type, child_path,
                            field->enum_values, field->enum_count,
                            field->enum_validator, field->string_capacity,
                            error, error_size) != 0) {
            return -1;
        }
    }
    for (size_t i = 0; i < field_count; i++) {
        if (fields[i].required) {
            int present = 0;
            for (int j = 0; j < table.u.tab.size; j++) {
                if (toml_key_matches(table.u.tab.key[j], table.u.tab.len[j],
                                     fields[i].name)) {
                    present = 1;
                    break;
                }
            }
            if (!present) {
                return schema_error(error, error_size,
                                    "%s missing required key '%s'",
                                    path, fields[i].name);
            }
        }
    }
    return 0;
}

static int validate_root_array(const SerializerRootSpec *field,
                               toml_datum_t value, const char *path,
                               char *error, size_t error_size)
{
    if (value.type != TOML_ARRAY) {
        return schema_error(error, error_size,
                            "%s has type %s, expected array",
                            path, toml_type_name(value.type));
    }
    if (value.u.arr.size > field->max_count) {
        return schema_error(error, error_size,
                            "%s has %d items (max %d)", path,
                            value.u.arr.size, field->max_count);
    }

    for (int i = 0; i < value.u.arr.size; i++) {
        /* Parent path (up to 255 chars) + "[" + up to 10 digits + "]". */
        char item_path[256 + 16];
        toml_datum_t item = value.u.arr.elem[i];

        if (field->type == SERIALIZER_ROOT_ARRAY_INTEGER) {
            snprintf(item_path, sizeof(item_path), "%s[%d]", path, i);
            if (validate_scalar(item, SERIALIZER_VALUE_INTEGER, item_path,
                                NULL, 0, NULL, 0, error, error_size) != 0) {
                return -1;
            }
        } else {
            snprintf(item_path, sizeof(item_path), "%s[%d]", path, i);
            if (item.type != TOML_TABLE) {
                return schema_error(error, error_size,
                                    "%s has type %s, expected table",
                                    item_path, toml_type_name(item.type));
            }
            if (validate_table_fields(item, item_path, field->fields,
                                      field->field_count, error,
                                      error_size) != 0) {
                return -1;
            }
        }
    }
    return 0;
}

static int validate_root_v1(toml_datum_t top, char *error, size_t error_size)
{
    for (int i = 0; i < top.u.tab.size; i++) {
        const char *key = top.u.tab.key[i];
        int key_len = top.u.tab.len[i];
        const SerializerRootSpec *field;
        toml_datum_t value = top.u.tab.value[i];
        char path[256];

        if (toml_key_has_embedded_nul(key, key_len)) {
            return schema_error(error, error_size,
                                "root has an embedded NUL in a key");
        }
        field = find_root_field(key, key_len);
        snprintf(path, sizeof(path), "root.%s", key);
        if (!field) {
            return schema_error(error, error_size,
                                "root has unknown key '%s'", key);
        }

        switch (field->type) {
            case SERIALIZER_ROOT_VALUE:
                if (validate_scalar(value, field->value_type, path, NULL, 0,
                                    NULL, field->string_capacity,
                                    error, error_size) != 0) {
                    return -1;
                }
                break;
            case SERIALIZER_ROOT_TABLE:
                if (validate_table_fields(value, path, field->fields,
                                          field->field_count, error,
                                          error_size) != 0) {
                    return -1;
                }
                break;
            case SERIALIZER_ROOT_ARRAY_INTEGER:
            case SERIALIZER_ROOT_ARRAY_TABLE:
                if (validate_root_array(field, value, path, error,
                                        error_size) != 0) {
                    return -1;
                }
                break;
        }
    }
    return 0;
}

/* Legacy files may use fractional integers and omit types/fields. Validate
 * every value we actually consume before the legacy readers convert it. */
static int validate_legacy_value(toml_datum_t value, SerializerValueType type,
                                 size_t string_capacity, const char *path,
                                 char *error, size_t error_size)
{
    if (type == SERIALIZER_VALUE_INTEGER && value.type == TOML_FP64) {
        if (!isfinite(value.u.fp64) || trunc(value.u.fp64) < INT_MIN ||
            trunc(value.u.fp64) > INT_MAX)
            return schema_error(error, error_size, "%s is outside C int range", path);
        return 0;
    }
    if ((type == SERIALIZER_VALUE_INTEGER && value.type == TOML_INT64) ||
        (type == SERIALIZER_VALUE_NUMBER &&
         (value.type == TOML_INT64 || value.type == TOML_FP64)) ||
        (type == SERIALIZER_VALUE_STRING && value.type == TOML_STRING))
        return validate_scalar(value, type, path, NULL, 0, NULL, string_capacity,
                               error, error_size);
    return 0;
}

static int validate_legacy_fields(toml_datum_t table, const SerializerRootSpec *root,
                                  char *error, size_t error_size)
{
    if (table.type != TOML_TABLE) return 0;
    for (int i = 0; i < table.u.tab.size; i++) {
        const SerializerFieldSpec *field = find_field(root->fields, root->field_count,
                                                       table.u.tab.key[i], table.u.tab.len[i]);
        if (field && validate_legacy_value(table.u.tab.value[i], field->type,
                                           field->string_capacity, field->name,
                                           error, error_size) != 0) return -1;
    }
    return 0;
}

static int validate_legacy(toml_datum_t top, char *error, size_t error_size)
{
    for (int i = 0; i < top.u.tab.size; i++) {
        const SerializerRootSpec *field = find_root_field(top.u.tab.key[i], top.u.tab.len[i]);
        toml_datum_t value = top.u.tab.value[i];
        if (!field) continue;
        if (field->type == SERIALIZER_ROOT_VALUE) {
            if (validate_legacy_value(value, field->value_type, field->string_capacity,
                                      field->name, error, error_size) != 0)
                return -1;
        } else if (field->type == SERIALIZER_ROOT_TABLE) {
            if (validate_legacy_fields(value, field, error, error_size) != 0) return -1;
        } else if (value.type == TOML_ARRAY) {
            for (int j = 0; j < value.u.arr.size; j++) {
                if (field->type == SERIALIZER_ROOT_ARRAY_INTEGER) {
                    if (validate_legacy_value(value.u.arr.elem[j], SERIALIZER_VALUE_INTEGER,
                                              0, field->name, error, error_size) != 0)
                        return -1;
                } else if (validate_legacy_fields(value.u.arr.elem[j], field, error, error_size) != 0)
                    return -1;
            }
        }
    }
    return 0;
}

/* Where the legacy warnings go, and how many were found. */
typedef struct {
    SerializerWarningFn warn;
    void *context;
    int count;
} LegacyWarnings;

PRINTF_FORMAT(2, 3)
static void legacy_warn(LegacyWarnings *warnings, const char *format, ...)
{
    char message[256];
    va_list args;

    if (warnings->count++ >= SERIALIZER_LEGACY_WARNING_MAX || !warnings->warn) return;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    warnings->warn(warnings->context, message);
}

/* The legacy readers take an integer from an integer or a float, a number
 * from either, and a string from a string. Anything else is skipped. */
static int legacy_value_is_read(toml_datum_t value, SerializerValueType type)
{
    /* An enum is written as a string ("RECT", "SPIN"). */
    if (type == SERIALIZER_VALUE_STRING || type == SERIALIZER_VALUE_ENUM)
        return value.type == TOML_STRING;
    return value.type == TOML_INT64 || value.type == TOML_FP64;
}

static void legacy_warn_fields(LegacyWarnings *warnings, toml_datum_t table,
                               const SerializerRootSpec *root, const char *path)
{
    if (table.type != TOML_TABLE) {
        legacy_warn(warnings, "%s has type %s, expected table; it is ignored",
                    path, toml_type_name(table.type));
        return;
    }
    for (int i = 0; i < table.u.tab.size; i++) {
        const char *key = table.u.tab.key[i];
        const SerializerFieldSpec *field =
            find_field(root->fields, root->field_count, key, table.u.tab.len[i]);
        if (!field)
            legacy_warn(warnings, "%s has unknown key '%s'; it is ignored", path, key);
        else if (!legacy_value_is_read(table.u.tab.value[i], field->type))
            legacy_warn(warnings, "%s.%s has type %s and is ignored", path, key,
                        toml_type_name(table.u.tab.value[i].type));
    }
}

int serializer_legacy_warnings(toml_datum_t top, SerializerWarningFn warn,
                               void *context)
{
    LegacyWarnings warnings = { warn, context, 0 };

    if (top.type != TOML_TABLE) return 0;
    /* A stated version is checked strictly: unknown keys are errors there. */
    if (toml_table_get_exact(top, "format_version").type != TOML_UNKNOWN) return 0;

    for (int i = 0; i < top.u.tab.size; i++) {
        const char *key = top.u.tab.key[i];
        const SerializerRootSpec *field = find_root_field(key, top.u.tab.len[i]);
        toml_datum_t value = top.u.tab.value[i];
        char path[256];

        if (!field) {
            legacy_warn(&warnings, "root has unknown key '%s'; it is ignored", key);
            continue;
        }
        snprintf(path, sizeof(path), "root.%s", key);
        if (field->type == SERIALIZER_ROOT_VALUE) {
            if (!legacy_value_is_read(value, field->value_type))
                legacy_warn(&warnings, "%s has type %s and is ignored", path,
                            toml_type_name(value.type));
        } else if (field->type == SERIALIZER_ROOT_TABLE) {
            legacy_warn_fields(&warnings, value, field, path);
        } else if (value.type != TOML_ARRAY) {
            legacy_warn(&warnings, "%s has type %s, expected array; it is ignored",
                        path, toml_type_name(value.type));
        } else {
            for (int j = 0; j < value.u.arr.size; j++) {
                /* Parent path (up to 255 chars) + "[" + up to 10 digits + "]". */
                char item_path[256 + 16];
                snprintf(item_path, sizeof(item_path), "%s[%d]", path, j);
                if (field->type == SERIALIZER_ROOT_ARRAY_INTEGER) {
                    if (!legacy_value_is_read(value.u.arr.elem[j], SERIALIZER_VALUE_INTEGER))
                        legacy_warn(&warnings, "%s has type %s and is ignored", item_path,
                                    toml_type_name(value.u.arr.elem[j].type));
                } else {
                    legacy_warn_fields(&warnings, value.u.arr.elem[j], field, item_path);
                }
            }
        }
    }
    return warnings.count;
}

int serializer_toml_line(toml_datum_t top, const char *message)
{
    const char *p = message;
    toml_datum_t node = top;
    int line = 0;

    if (!message) return 0;
    /* Schema messages name the document root "root."; validator ones don't. */
    if (strncmp(p, "root.", 5) == 0) p += 5;

    /* Walk "name", then any "[index]" and ".name" parts, up to the first
     * character that cannot be part of a path (the space before the words). */
    while (*p && *p != ' ') {
        char key[64];
        size_t n = 0;

        while ((isalnum((unsigned char)*p) || *p == '_') && n + 1 < sizeof(key))
            key[n++] = *p++;
        key[n] = '\0';
        if (n == 0 || node.type != TOML_TABLE) break;
        node = toml_get(node, key);
        if (node.type == TOML_UNKNOWN) break;
        if (node.lineno > 0) line = node.lineno;

        while (*p == '[') {
            char *end;
            long index = strtol(p + 1, &end, 10);
            if (*end != ']' || node.type != TOML_ARRAY ||
                index < 0 || index >= node.u.arr.size)
                return line;
            node = node.u.arr.elem[index];
            if (node.lineno > 0) line = node.lineno;
            p = end + 1;
        }
        if (*p != '.') break;
        p++;
    }
    return line;
}

int serializer_validate_schema(toml_datum_t top, int *strict,
                               char *error, size_t error_size)
{
    toml_datum_t version;

    if (strict) *strict = 0;
    if (error && error_size > 0) error[0] = '\0';

    if (top.type != TOML_TABLE) {
        return schema_error(error, error_size,
                            "root has type %s, expected table",
                            toml_type_name(top.type));
    }

    /* Reject NUL-bearing keys and strings before the legacy fallback. */
    if (toml_tree_has_embedded_nul(top)) {
        return schema_error(error, error_size,
                            "document contains an embedded NUL in a key or string");
    }

    /* Check root keys before the legacy escape hatch.  Otherwise a quoted
     * "format_version\\u0000" key can disappear from an exact lookup and
     * make an attempted v1 document look like legacy input. */
    for (int i = 0; i < top.u.tab.size; i++) {
        if (toml_key_has_embedded_nul(top.u.tab.key[i], top.u.tab.len[i])) {
            return schema_error(error, error_size,
                                "root has an embedded NUL in a key");
        }
    }

    version = toml_table_get_exact(top, "format_version");
    if (version.type == TOML_UNKNOWN) {
        /* Missing version is the only legacy-compatible escape hatch. */
        return validate_legacy(top, error, error_size);
    }
    if (version.type != TOML_INT64) {
        return schema_error(error, error_size,
                            "root.format_version has type %s, expected integer 1",
                            toml_type_name(version.type));
    }
    if (version.u.int64 != 1) {
        return schema_error(error, error_size,
                            "root.format_version is %lld (expected 1)",
                            (long long)version.u.int64);
    }

    if (strict) *strict = 1;
    return validate_root_v1(top, error, error_size);
}

/* ------------------------------------------------------------------ */
/* One loader and one writer for every row of the schema table         */
/* ------------------------------------------------------------------ */

/*
 * ROOT_FIELDS says where every value of a level lives in LevelDef and what
 * each key is. The two functions below walk those rows, so there is no
 * per-entity and no per-key loading or saving code: what a spider's keys
 * are, what they default to and in which order they are written is stated
 * once, in SPIDER_FIELDS, and the same is true of music_volume or
 * [physics].
 *
 * Both work on "the struct at this address", a char pointer, and reach a
 * member by adding the row's offset. For an [[array]] that address is one
 * element (moved in steps of item_size); for a level-wide key or a single
 * [table] it is the LevelDef itself. memcpy moves the value in or out: it
 * needs no cast to the member's type and does not care how the struct is
 * aligned.
 */

/* Find a root row by its key (NULL when there is none). */
static const SerializerRootSpec *root_by_name(const char *name)
{
    for (size_t i = 0; i < sizeof(ROOT_FIELDS) / sizeof(ROOT_FIELDS[0]); i++)
        if (strcmp(ROOT_FIELDS[i].name, name) == 0) return &ROOT_FIELDS[i];
    return NULL;
}

/*
 * A required number: an integer or a finite float that still fits a float
 * after narrowing. Returns 0 with the value in *out, -1 otherwise. The
 * range is checked on the double first, because converting a double that
 * is too large for a float is undefined behaviour in C.
 */
static int required_float(toml_datum_t table, const char *key, float *out)
{
    toml_datum_t value = toml_table_get_exact(table, key);

    if (value.type == TOML_INT64) {
        *out = (float)value.u.int64;
        return 0;
    }
    if (value.type != TOML_FP64 || !isfinite(value.u.fp64) ||
        fabs(value.u.fp64) > FLT_MAX) return -1;
    *out = (float)value.u.fp64;
    return 0;
}

/*
 * Read one key of `table` into the struct at `item`, as its row says. A
 * missing key, or a value of a type the legacy readers do not take, gives
 * the row's fallback (get_float, get_int and get_str decide that), or
 * leaves the member alone when the row says keep_missing. Returns 0, or -1
 * when a required key has no usable value.
 */
static int load_field(const SerializerFieldSpec *field, toml_datum_t table, char *item)
{
    char *member = item + field->offset;

    switch (field->storage) {
    case SERIALIZER_STORE_FLOAT: {
        float value;
        if (field->required) {
            if (required_float(table, field->name, &value) != 0) return -1;
        } else {
            float fallback = (float)field->fallback;
            if (field->keep_missing) memcpy(&fallback, member, sizeof(fallback));
            value = get_float(table, field->name, fallback);
        }
        memcpy(member, &value, sizeof(value));
        break;
    }
    case SERIALIZER_STORE_INT: {
        int value = get_int(table, field->name, (int)field->fallback);
        memcpy(member, &value, sizeof(value));
        break;
    }
    case SERIALIZER_STORE_STRING: {
        const char *text = get_str(table, field->name, NULL);
        if (!text && field->keep_missing) break;
        if (!text) text = field->text_fallback ? field->text_fallback : "";
        copy_str(member, field->string_capacity, text);
        break;
    }
    case SERIALIZER_STORE_ENUM: {
        int value = field->enum_from_name(get_str(table, field->name, field->text_fallback));
        memcpy(member, &value, sizeof(value));
        break;
    }
    case SERIALIZER_STORE_NONE:
        break;   /* loaded by hand elsewhere */
    }
    return 0;
}

/* 1 when any key of the row's tables is required: such an array is read
 * strictly even in a file without format_version. */
static int root_is_strict(const SerializerRootSpec *root)
{
    for (size_t f = 0; f < root->field_count; f++)
        if (root->fields[f].required) return 1;
    return 0;
}

/* An [[array]] of tables: copy it element by element. */
static int load_table_array(const SerializerRootSpec *root, toml_datum_t value, LevelDef *def)
{
    const int strict = root_is_strict(root);
    char *item;
    int count;

    /* A value that is not an array leaves the array empty, which is how an
     * optional section works; a strict array refuses it instead. */
    if (value.type != TOML_ARRAY) {
        if (!strict) return 0;
        fprintf(stderr, "serializer: %s must be an array of at most %d tables\n",
                root->name, root->max_count);
        return -1;
    }
    count = value.u.arr.size;
    if (count < 0 || count > root->max_count) {
        fprintf(stderr, "serializer: %s array has %d items (max %d)\n",
                root->name, count, root->max_count);
        return -1;
    }
    memcpy((char *)def + root->count_offset, &count, sizeof(count));
    item = (char *)def + root->items_offset;
    for (int i = 0; i < count; i++, item += root->item_size) {
        toml_datum_t table = value.u.arr.elem[i];

        if (strict && table.type != TOML_TABLE) {
            fprintf(stderr, "serializer: %s[%d] must be a table\n", root->name, i);
            return -1;
        }
        for (size_t f = 0; f < root->field_count; f++) {
            if (load_field(&root->fields[f], table, item) != 0) {
                fprintf(stderr, "serializer: %s[%d] requires a finite number for %s\n",
                        root->name, i, root->fields[f].name);
                return -1;
            }
        }
    }
    return 0;
}

/* A plain array of integers (floor_gaps). A float is cut to its whole
 * part, as the legacy reader always did; anything else is an error. */
static int load_integer_array(const SerializerRootSpec *root, toml_datum_t value, LevelDef *def)
{
    char *item;
    int count;

    if (value.type != TOML_ARRAY) return 0;
    count = value.u.arr.size;
    if (count < 0 || count > root->max_count) {
        fprintf(stderr, "serializer: %s array has %d items (max %d)\n",
                root->name, count, root->max_count);
        return -1;
    }
    memcpy((char *)def + root->count_offset, &count, sizeof(count));
    item = (char *)def + root->items_offset;
    for (int i = 0; i < count; i++, item += root->item_size) {
        toml_datum_t element = value.u.arr.elem[i];
        int number;

        if (element.type == TOML_INT64 &&
            element.u.int64 >= INT_MIN && element.u.int64 <= INT_MAX) {
            number = (int)element.u.int64;
        } else if (element.type == TOML_FP64 && isfinite(element.u.fp64) &&
                   trunc(element.u.fp64) >= INT_MIN && trunc(element.u.fp64) <= INT_MAX) {
            number = (int)element.u.fp64;
        } else {
            fprintf(stderr, "serializer: %s[%d] must be a number that fits an int\n",
                    root->name, i);
            return -1;
        }
        memcpy(item, &number, sizeof(number));
    }
    return 0;
}

int serializer_load_schema(toml_datum_t top, LevelDef *def)
{
    if (!def) return -1;

    for (size_t r = 0; r < sizeof(ROOT_FIELDS) / sizeof(ROOT_FIELDS[0]); r++) {
        const SerializerRootSpec *root = &ROOT_FIELDS[r];
        toml_datum_t value = toml_table_get_exact(top, root->name);

        switch (root->type) {
        case SERIALIZER_ROOT_VALUE:
            /* A level-wide key is a table of one row whose "table" is the
             * root and whose "struct" is the LevelDef. */
            if (root->fields) (void)load_field(&root->fields[0], top, (char *)def);
            break;
        case SERIALIZER_ROOT_TABLE:
            /* A missing [table] keeps what level_def_init_defaults set. */
            if (value.type != TOML_TABLE) break;
            for (size_t f = 0; f < root->field_count; f++)
                (void)load_field(&root->fields[f], value, (char *)def);
            break;
        case SERIALIZER_ROOT_ARRAY_INTEGER:
            if (load_integer_array(root, value, def) != 0) return -1;
            break;
        case SERIALIZER_ROOT_ARRAY_TABLE:
            /* A missing strict array is still fine: it is optional. */
            if (value.type == TOML_UNKNOWN) break;
            if (load_table_array(root, value, def) != 0) return -1;
            break;
        }
    }
    return 0;
}

/* Write one key of the struct at `item`, as its row says. key_width pads
 * the key with spaces (0 = none). */
static void write_field(FILE *fp, const SerializerFieldSpec *field, const char *item,
                        int key_width)
{
    const char *member = item + field->offset;

    switch (field->storage) {
    case SERIALIZER_STORE_FLOAT: {
        float value;
        memcpy(&value, member, sizeof(value));
        /* "%-*s": left-align the key in a column key_width wide. */
        fprintf(fp, "%-*s = %s\n", key_width, field->name, fmt_float(value));
        break;
    }
    case SERIALIZER_STORE_INT: {
        int value;
        memcpy(&value, member, sizeof(value));
        if (field->omit == SERIALIZER_OMIT_ZERO && value == 0) break;
        fprintf(fp, "%-*s = %d\n", key_width, field->name, value);
        break;
    }
    case SERIALIZER_STORE_STRING:
        if (member[0] == '\0' && field->omit == SERIALIZER_OMIT_EMPTY) break;
        if (member[0] == '\0' && field->omit == SERIALIZER_WRITE_FALLBACK_IF_EMPTY)
            member = field->text_fallback;
        write_toml_key_string(fp, field->name, member);
        break;
    case SERIALIZER_STORE_ENUM: {
        int value;
        memcpy(&value, member, sizeof(value));
        write_toml_key_string(fp, field->name, field->enum_to_name(value));
        break;
    }
    case SERIALIZER_STORE_NONE:
        break;   /* written by hand elsewhere */
    }
}

void serializer_write_entry(FILE *fp, const LevelDef *def, const char *name)
{
    const SerializerRootSpec *root = root_by_name(name);
    const char *item;
    int count = 0;

    if (!fp || !def || !root) return;
    if (root->item_size != 0) {
        memcpy(&count, (const char *)def + root->count_offset, sizeof(count));
        if (count > root->max_count) count = root->max_count;
    }
    item = (const char *)def + root->items_offset;

    switch (root->type) {
    case SERIALIZER_ROOT_VALUE:
        if (root->fields) write_field(fp, &root->fields[0], (const char *)def, 0);
        break;
    case SERIALIZER_ROOT_TABLE:
        fprintf(fp, "[%s]\n", root->name);
        for (size_t f = 0; f < root->field_count; f++)
            write_field(fp, &root->fields[f], (const char *)def, root->key_width);
        fprintf(fp, "\n");
        break;
    case SERIALIZER_ROOT_ARRAY_INTEGER:
        /* An empty array is left out; a missing one loads as empty. */
        if (count <= 0) break;
        fprintf(fp, "%s = [", root->name);
        for (int i = 0; i < count; i++, item += root->item_size) {
            int number;
            memcpy(&number, item, sizeof(number));
            fprintf(fp, "%s%d", i > 0 ? ", " : "", number);
        }
        fprintf(fp, "]\n");
        break;
    case SERIALIZER_ROOT_ARRAY_TABLE:
        for (int i = 0; i < count; i++, item += root->item_size) {
            fprintf(fp, "[[%s]]\n", root->name);
            for (size_t f = 0; f < root->field_count; f++)
                write_field(fp, &root->fields[f], item, 0);
            fprintf(fp, "\n");
        }
        break;
    }
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
    for (size_t r = 0; r < sizeof(ROOT_FIELDS) / sizeof(ROOT_FIELDS[0]); r++) {
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
