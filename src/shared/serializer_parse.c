/*
 * serializer_parse.c — Internal TOML parse helpers and v1 schema checks.
 */

#include "serializer_parse.h"

#include <limits.h>  /* INT_MIN, INT_MAX */
#include <float.h>   /* FLT_MAX — check before narrowing a TOML double */
#include <math.h>    /* isfinite */
#include <stdarg.h>  /* va_list */
#include <stdio.h>   /* vsnprintf */
#include <string.h>  /* memchr, memcmp, strlen */

#include "serializer_types.h" /* enum validation */
#include "../game.h"          /* MAX_* array limits */

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

typedef struct {
    const char *name;
    SerializerValueType type;
    const char *const *enum_values;
    size_t enum_count;
    SerializerEnumValidator enum_validator;
    int required;
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
} SerializerRootSpec;

#define INTEGER_FIELD(key) { key, SERIALIZER_VALUE_INTEGER, NULL, 0, NULL, 0 }
#define NUMBER_FIELD(key)  { key, SERIALIZER_VALUE_NUMBER,  NULL, 0, NULL, 0 }
#define STRING_FIELD(key)  { key, SERIALIZER_VALUE_STRING,  NULL, 0, NULL, 0 }
#define REQUIRED_NUMBER_FIELD(key) \
    { key, SERIALIZER_VALUE_NUMBER, NULL, 0, NULL, 1 }
#define ENUM_FIELD(key, values, validator) \
    { key, SERIALIZER_VALUE_ENUM, values, sizeof(values) / sizeof((values)[0]), \
      validator, 0 }

static const char *const RAIL_LAYOUT_VALUES[] = {"RECT", "HORIZ"};
static const char *const AXE_MODE_VALUES[] = {"PENDULUM", "SPIN"};
static const char *const FLOAT_MODE_VALUES[] = {"STATIC", "CRUMBLE", "RAIL"};
static const char *const BOUNCEPAD_VALUES[] = {"GREEN", "WOOD", "RED"};

static const SerializerFieldSpec XY_FIELDS[] = {
    NUMBER_FIELD("x"),
    NUMBER_FIELD("y")
};

static const SerializerFieldSpec CHECKPOINT_FIELDS[] = {
    REQUIRED_NUMBER_FIELD("x"),
    REQUIRED_NUMBER_FIELD("y")
};

static const SerializerFieldSpec RAIL_FIELDS[] = {
    ENUM_FIELD("layout", RAIL_LAYOUT_VALUES, serializer_rail_layout_is_valid),
    INTEGER_FIELD("x"),
    INTEGER_FIELD("y"),
    INTEGER_FIELD("w"),
    INTEGER_FIELD("h"),
    INTEGER_FIELD("end_cap")
};

static const SerializerFieldSpec PLATFORM_FIELDS[] = {
    NUMBER_FIELD("x"),
    INTEGER_FIELD("tile_height"),
    INTEGER_FIELD("tile_width"),
    STRING_FIELD("tile_path")
};

static const SerializerFieldSpec LAST_STAR_FIELDS[] = {
    NUMBER_FIELD("x"),
    NUMBER_FIELD("y"),
    STRING_FIELD("next_phase")
};

static const SerializerFieldSpec SPIDER_FIELDS[] = {
    NUMBER_FIELD("x"),
    NUMBER_FIELD("vx"),
    NUMBER_FIELD("patrol_x0"),
    NUMBER_FIELD("patrol_x1"),
    INTEGER_FIELD("frame_index")
};

static const SerializerFieldSpec JUMPING_SPIDER_FIELDS[] = {
    NUMBER_FIELD("x"),
    NUMBER_FIELD("vx"),
    NUMBER_FIELD("patrol_x0"),
    NUMBER_FIELD("patrol_x1")
};

static const SerializerFieldSpec BIRD_FIELDS[] = {
    NUMBER_FIELD("x"),
    NUMBER_FIELD("base_y"),
    NUMBER_FIELD("vx"),
    NUMBER_FIELD("patrol_x0"),
    NUMBER_FIELD("patrol_x1"),
    INTEGER_FIELD("frame_index")
};

static const SerializerFieldSpec FISH_FIELDS[] = {
    NUMBER_FIELD("x"),
    NUMBER_FIELD("vx"),
    NUMBER_FIELD("patrol_x0"),
    NUMBER_FIELD("patrol_x1")
};

static const SerializerFieldSpec AXE_FIELDS[] = {
    NUMBER_FIELD("pillar_x"),
    NUMBER_FIELD("y"),
    ENUM_FIELD("mode", AXE_MODE_VALUES, serializer_axe_mode_is_valid)
};

static const SerializerFieldSpec CIRCULAR_SAW_FIELDS[] = {
    NUMBER_FIELD("x"),
    NUMBER_FIELD("y"),
    NUMBER_FIELD("patrol_x0"),
    NUMBER_FIELD("patrol_x1"),
    INTEGER_FIELD("direction")
};

static const SerializerFieldSpec SPIKE_ROW_FIELDS[] = {
    NUMBER_FIELD("x"),
    INTEGER_FIELD("count")
};

static const SerializerFieldSpec SPIKE_PLATFORM_FIELDS[] = {
    NUMBER_FIELD("x"),
    NUMBER_FIELD("y"),
    INTEGER_FIELD("tile_count")
};

static const SerializerFieldSpec SPIKE_BLOCK_FIELDS[] = {
    INTEGER_FIELD("rail_index"),
    NUMBER_FIELD("t_offset"),
    NUMBER_FIELD("speed")
};

static const SerializerFieldSpec X_ONLY_FIELDS[] = {
    NUMBER_FIELD("x")
};

static const SerializerFieldSpec FLOAT_PLATFORM_FIELDS[] = {
    ENUM_FIELD("mode", FLOAT_MODE_VALUES, serializer_float_mode_is_valid),
    NUMBER_FIELD("x"),
    NUMBER_FIELD("y"),
    INTEGER_FIELD("tile_count"),
    INTEGER_FIELD("rail_index"),
    NUMBER_FIELD("t_offset"),
    NUMBER_FIELD("speed")
};

static const SerializerFieldSpec BRIDGE_FIELDS[] = {
    NUMBER_FIELD("x"),
    NUMBER_FIELD("y"),
    INTEGER_FIELD("brick_count")
};

static const SerializerFieldSpec BOUNCEPAD_FIELDS[] = {
    NUMBER_FIELD("x"),
    NUMBER_FIELD("launch_vy"),
    ENUM_FIELD("pad_type", BOUNCEPAD_VALUES,
                serializer_bouncepad_type_is_valid)
};

static const SerializerFieldSpec VINE_FIELDS[] = {
    NUMBER_FIELD("x"),
    NUMBER_FIELD("y"),
    INTEGER_FIELD("tile_count"),
    INTEGER_FIELD("vine_type")
};

static const SerializerFieldSpec CLIMBABLE_FIELDS[] = {
    NUMBER_FIELD("x"),
    NUMBER_FIELD("y"),
    INTEGER_FIELD("tile_count")
};

static const SerializerFieldSpec LAYER_FIELDS[] = {
    STRING_FIELD("path"),
    NUMBER_FIELD("speed")
};

static const SerializerFieldSpec PHYSICS_FIELDS[] = {
    NUMBER_FIELD("walk_max_speed"),
    NUMBER_FIELD("run_max_speed"),
    NUMBER_FIELD("walk_ground_accel"),
    NUMBER_FIELD("run_ground_accel"),
    NUMBER_FIELD("ground_friction"),
    NUMBER_FIELD("ground_counter_accel"),
    NUMBER_FIELD("air_accel_walk"),
    NUMBER_FIELD("air_accel_run"),
    NUMBER_FIELD("air_friction"),
    NUMBER_FIELD("cam_lookahead_vx_factor"),
    NUMBER_FIELD("cam_lookahead_max")
};

#define FIELD_COUNT(fields) (sizeof(fields) / sizeof((fields)[0]))
#define ROOT_VALUE(key, value_kind) \
    { key, SERIALIZER_ROOT_VALUE, value_kind, NULL, 0, 0 }
#define ROOT_TABLE(key, table_fields) \
    { key, SERIALIZER_ROOT_TABLE, SERIALIZER_VALUE_STRING, table_fields, \
      FIELD_COUNT(table_fields), 0 }
#define ROOT_INTEGER_ARRAY(key, max_items) \
    { key, SERIALIZER_ROOT_ARRAY_INTEGER, SERIALIZER_VALUE_INTEGER, NULL, 0, \
      max_items }
#define ROOT_TABLE_ARRAY(key, table_fields, max_items) \
    { key, SERIALIZER_ROOT_ARRAY_TABLE, SERIALIZER_VALUE_STRING, table_fields, \
      FIELD_COUNT(table_fields), max_items }

static const SerializerRootSpec ROOT_FIELDS[] = {
    ROOT_VALUE("format_version", SERIALIZER_VALUE_INTEGER),
    ROOT_VALUE("name", SERIALIZER_VALUE_STRING),
    ROOT_VALUE("description", SERIALIZER_VALUE_STRING),
    ROOT_VALUE("generated_by", SERIALIZER_VALUE_STRING),
    ROOT_VALUE("screen_count", SERIALIZER_VALUE_INTEGER),
    ROOT_INTEGER_ARRAY("floor_gaps", MAX_FLOOR_GAPS),
    ROOT_TABLE_ARRAY("rails", RAIL_FIELDS, MAX_RAILS),
    ROOT_TABLE_ARRAY("platforms", PLATFORM_FIELDS, MAX_PLATFORMS),
    ROOT_TABLE_ARRAY("checkpoints", CHECKPOINT_FIELDS, MAX_CHECKPOINTS),
    ROOT_TABLE_ARRAY("coins", XY_FIELDS, MAX_COINS),
    ROOT_TABLE_ARRAY("star_yellows", XY_FIELDS, MAX_STAR_YELLOWS),
    ROOT_TABLE_ARRAY("star_greens", XY_FIELDS, MAX_STAR_GREENS),
    ROOT_TABLE_ARRAY("star_reds", XY_FIELDS, MAX_STAR_REDS),
    ROOT_TABLE("last_star", LAST_STAR_FIELDS),
    ROOT_TABLE_ARRAY("spiders", SPIDER_FIELDS, MAX_SPIDERS),
    ROOT_TABLE_ARRAY("jumping_spiders", JUMPING_SPIDER_FIELDS,
                     MAX_JUMPING_SPIDERS),
    ROOT_TABLE_ARRAY("birds", BIRD_FIELDS, MAX_BIRDS),
    ROOT_TABLE_ARRAY("faster_birds", BIRD_FIELDS, MAX_FASTER_BIRDS),
    ROOT_TABLE_ARRAY("fish", FISH_FIELDS, MAX_FISH),
    ROOT_TABLE_ARRAY("faster_fish", FISH_FIELDS, MAX_FASTER_FISH),
    ROOT_TABLE_ARRAY("axe_traps", AXE_FIELDS, MAX_AXE_TRAPS),
    ROOT_TABLE_ARRAY("circular_saws", CIRCULAR_SAW_FIELDS, MAX_CIRCULAR_SAWS),
    ROOT_TABLE_ARRAY("spike_rows", SPIKE_ROW_FIELDS, MAX_SPIKE_ROWS),
    ROOT_TABLE_ARRAY("spike_platforms", SPIKE_PLATFORM_FIELDS,
                     MAX_SPIKE_PLATFORMS),
    ROOT_TABLE_ARRAY("spike_blocks", SPIKE_BLOCK_FIELDS, MAX_SPIKE_BLOCKS),
    ROOT_TABLE_ARRAY("blue_flames", X_ONLY_FIELDS, MAX_BLUE_FLAMES),
    ROOT_TABLE_ARRAY("fire_flames", X_ONLY_FIELDS, MAX_FIRE_FLAMES),
    ROOT_TABLE_ARRAY("float_platforms", FLOAT_PLATFORM_FIELDS,
                     MAX_FLOAT_PLATFORMS),
    ROOT_TABLE_ARRAY("bridges", BRIDGE_FIELDS, MAX_BRIDGES),
    ROOT_TABLE_ARRAY("bouncepads_small", BOUNCEPAD_FIELDS,
                     MAX_BOUNCEPADS_SMALL),
    ROOT_TABLE_ARRAY("bouncepads_medium", BOUNCEPAD_FIELDS,
                     MAX_BOUNCEPADS_MEDIUM),
    ROOT_TABLE_ARRAY("bouncepads_high", BOUNCEPAD_FIELDS,
                     MAX_BOUNCEPADS_HIGH),
    ROOT_TABLE_ARRAY("vines", VINE_FIELDS, MAX_VINES),
    ROOT_TABLE_ARRAY("ladders", CLIMBABLE_FIELDS, MAX_LADDERS),
    ROOT_TABLE_ARRAY("ropes", CLIMBABLE_FIELDS, MAX_ROPES),
    ROOT_TABLE_ARRAY("background_layers", LAYER_FIELDS, MAX_BACKGROUND_LAYERS),
    ROOT_TABLE_ARRAY("foreground_layers", LAYER_FIELDS, MAX_BACKGROUND_LAYERS),
    ROOT_TABLE_ARRAY("fog_layers", LAYER_FIELDS, MAX_FOG_TEXTURES),
    ROOT_VALUE("player_start_x", SERIALIZER_VALUE_NUMBER),
    ROOT_VALUE("player_start_y", SERIALIZER_VALUE_NUMBER),
    ROOT_VALUE("music_path", SERIALIZER_VALUE_STRING),
    ROOT_VALUE("music_volume", SERIALIZER_VALUE_INTEGER),
    ROOT_VALUE("floor_tile_path", SERIALIZER_VALUE_STRING),
    ROOT_VALUE("initial_hearts", SERIALIZER_VALUE_INTEGER),
    ROOT_VALUE("initial_lives", SERIALIZER_VALUE_INTEGER),
    ROOT_VALUE("score_per_life", SERIALIZER_VALUE_INTEGER),
    ROOT_VALUE("coin_score", SERIALIZER_VALUE_INTEGER),
    ROOT_TABLE("physics", PHYSICS_FIELDS)
};

#undef ROOT_TABLE_ARRAY
#undef ROOT_INTEGER_ARRAY
#undef ROOT_TABLE
#undef ROOT_VALUE
#undef FIELD_COUNT
#undef ENUM_FIELD
#undef STRING_FIELD
#undef NUMBER_FIELD
#undef INTEGER_FIELD

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

static int validate_scalar(toml_datum_t value, SerializerValueType expected,
                           const char *path, const char *const *enum_values,
                           size_t enum_count,
                           SerializerEnumValidator enum_validator,
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
        const char *key = strrchr(path, '.');
        key = key ? key + 1 : path;
        size_t capacity = sizeof(((LevelDef *)0)->music_path);
        if (!strcmp(key, "name")) capacity = sizeof(((LevelDef *)0)->name);
        if (!strcmp(key, "description")) capacity = sizeof(((LevelDef *)0)->description);
        if (!strcmp(key, "generated_by")) capacity = sizeof(((LevelDef *)0)->generated_by);
        if (!strcmp(key, "next_phase")) capacity = sizeof(((LevelDef *)0)->next_phase);
        if (value.u.str.len < 0 || (size_t)value.u.str.len >= capacity) {
            return schema_error(error, error_size, "%s exceeds its %zu-byte storage", path, capacity - 1);
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
                            field->enum_validator,
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
        char item_path[256];
        toml_datum_t item = value.u.arr.elem[i];

        if (field->type == SERIALIZER_ROOT_ARRAY_INTEGER) {
            snprintf(item_path, sizeof(item_path), "%s[%d]", path, i);
            if (validate_scalar(item, SERIALIZER_VALUE_INTEGER, item_path,
                                NULL, 0, NULL, error, error_size) != 0) {
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
                                    NULL, error, error_size) != 0) {
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
                                 const char *path, char *error, size_t error_size)
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
        return validate_scalar(value, type, path, NULL, 0, NULL, error, error_size);
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
                                           field->name, error, error_size) != 0) return -1;
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
            if (validate_legacy_value(value, field->value_type, field->name, error, error_size) != 0)
                return -1;
        } else if (field->type == SERIALIZER_ROOT_TABLE) {
            if (validate_legacy_fields(value, field, error, error_size) != 0) return -1;
        } else if (value.type == TOML_ARRAY) {
            for (int j = 0; j < value.u.arr.size; j++) {
                if (field->type == SERIALIZER_ROOT_ARRAY_INTEGER) {
                    if (validate_legacy_value(value.u.arr.elem[j], SERIALIZER_VALUE_INTEGER,
                                               field->name, error, error_size) != 0) return -1;
                } else if (validate_legacy_fields(value.u.arr.elem[j], field, error, error_size) != 0)
                    return -1;
            }
        }
    }
    return 0;
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
