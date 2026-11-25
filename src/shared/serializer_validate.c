/*
 * serializer_validate.c — Checking a parsed level file against the schema.
 *
 * A file that states format_version = 1 is checked strictly (validate_root_v1):
 * every key must be in the schema table, with the right type and inside its
 * limits.  A file without it is read leniently, but what the loader cannot
 * skip is still refused (validate_legacy), and what it does skip is reported
 * (serializer_legacy_warnings).  serializer_toml_line finds the line a
 * message is about.  The table itself is in serializer_schema.c.
 */

#include "serializer_schema.h"

#include <ctype.h>   /* isalnum */
#include <float.h>   /* FLT_MAX — check before narrowing a TOML double */
#include <limits.h>  /* INT_MIN, INT_MAX */
#include <math.h>    /* isfinite */
#include <stdarg.h>  /* va_list */
#include <stdio.h>   /* vsnprintf */
#include <stdlib.h>  /* strtol */
#include <string.h>  /* memchr, strcmp, strlen */

#include "serializer_emit.h"  /* serializer_message_text */
#include "printf_format.h"    /* PRINTF_FORMAT */
#include "utf8.h"             /* utf8_valid */

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

/*
 * show_key — A key from the file, ready to quote in a message.
 *
 * Keys are untrusted: a quoted TOML key may hold any byte, an ESC that
 * starts a terminal escape sequence included, and these messages reach
 * stderr and the editor's status bar. serializer_message_text writes every
 * such byte as \xHH. A key longer than the buffer is cut; the message
 * only has to say which key it is.
 */
#define SHOWN_KEY_SIZE 96
static const char *show_key(char *out, size_t out_size, const char *key, int key_len)
{
    return serializer_message_text(out, out_size, key, key_len > 0 ? (size_t)key_len : 0);
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
        field = serializer_find_field(fields, field_count, key, key_len);
        if (!field) {
            char shown[SHOWN_KEY_SIZE];
            return schema_error(error, error_size, "%s has unknown key '%s'", path,
                                show_key(shown, sizeof(shown), key, key_len));
        }
        snprintf(child_path, sizeof(child_path), "%s.%s", path, field->name);
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
                if (serializer_key_matches(table.u.tab.key[j], table.u.tab.len[j],
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
        field = serializer_find_root(key, key_len);
        if (!field) {
            char shown[SHOWN_KEY_SIZE];
            return schema_error(error, error_size, "root has unknown key '%s'",
                                show_key(shown, sizeof(shown), key, key_len));
        }
        snprintf(path, sizeof(path), "root.%s", field->name);

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

/*
 * A file without format_version is read leniently: a value of a type the
 * loader cannot use is skipped (and serializer_legacy_warnings says so).
 * What it cannot skip is checked here, before anything is loaded, so the
 * error names the value by its full TOML path ("root.platforms[1].
 * tile_height") and serializer_toml_line can find its line:
 *   - a number the loader would read but that does not fit (an integer
 *     outside C int range, an infinite float, a string too long);
 *   - an array with more items than its LevelDef array holds;
 *   - a floor gap that is not a number;
 *   - [[checkpoints]], whose x and y are required (see CHECKPOINT_FIELDS).
 * serializer_load_schema checks the same things again, as a safety net.
 */
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

/* The keys of one table: path names the table ("root.physics",
 * "root.coins[3]"). strict: the table belongs to an array whose keys are
 * required, so it must be a table and have them. */
static int validate_legacy_fields(toml_datum_t table, const SerializerRootSpec *root,
                                  const char *path, int strict,
                                  char *error, size_t error_size)
{
    if (table.type != TOML_TABLE) {
        if (!strict) return 0;   /* skipped; the legacy warnings say so */
        return schema_error(error, error_size, "%s has type %s, expected table",
                            path, toml_type_name(table.type));
    }
    for (int i = 0; i < table.u.tab.size; i++) {
        const SerializerFieldSpec *field = serializer_find_field(root->fields, root->field_count,
                                                       table.u.tab.key[i], table.u.tab.len[i]);
        /* The table's path (up to 271 bytes) + "." + the key. */
        char child_path[320];

        if (!field) continue;
        snprintf(child_path, sizeof(child_path), "%s.%s", path, field->name);
        if (field->required) {
            /* A required key is read strictly, whatever the version. */
            if (validate_scalar(table.u.tab.value[i], field->type, child_path, NULL, 0,
                                NULL, field->string_capacity, error, error_size) != 0)
                return -1;
        } else if (validate_legacy_value(table.u.tab.value[i], field->type,
                                         field->string_capacity, child_path,
                                         error, error_size) != 0) {
            return -1;
        }
    }
    for (size_t f = 0; f < root->field_count; f++) {
        if (root->fields[f].required &&
            serializer_table_get(table, root->fields[f].name).type == TOML_UNKNOWN)
            return schema_error(error, error_size, "%s missing required key '%s'",
                                path, root->fields[f].name);
    }
    return 0;
}

/* An array: path is "root.<key>". */
static int validate_legacy_array(toml_datum_t value, const SerializerRootSpec *root,
                                 const char *path, char *error, size_t error_size)
{
    const int strict = serializer_root_is_strict(root);

    if (value.type != TOML_ARRAY) {
        if (!strict) return 0;   /* skipped; the legacy warnings say so */
        return schema_error(error, error_size, "%s has type %s, expected array",
                            path, toml_type_name(value.type));
    }
    if (value.u.arr.size > root->max_count) {
        return schema_error(error, error_size, "%s has %d items (max %d)", path,
                            value.u.arr.size, root->max_count);
    }
    for (int j = 0; j < value.u.arr.size; j++) {
        /* Parent path (up to 255 chars) + "[" + up to 10 digits + "]". */
        char item_path[256 + 16];
        toml_datum_t item = value.u.arr.elem[j];

        snprintf(item_path, sizeof(item_path), "%s[%d]", path, j);
        if (root->type == SERIALIZER_ROOT_ARRAY_INTEGER) {
            /* A floor gap that is not a number has no value to skip to. */
            if (item.type != TOML_INT64 && item.type != TOML_FP64)
                return schema_error(error, error_size, "%s has type %s, expected integer",
                                    item_path, toml_type_name(item.type));
            if (validate_legacy_value(item, SERIALIZER_VALUE_INTEGER, 0, item_path,
                                      error, error_size) != 0)
                return -1;
        } else if (validate_legacy_fields(item, root, item_path, strict,
                                          error, error_size) != 0) {
            return -1;
        }
    }
    return 0;
}

static int validate_legacy(toml_datum_t top, char *error, size_t error_size)
{
    for (int i = 0; i < top.u.tab.size; i++) {
        const SerializerRootSpec *field = serializer_find_root(top.u.tab.key[i], top.u.tab.len[i]);
        toml_datum_t value = top.u.tab.value[i];
        char path[256];

        if (!field) continue;
        snprintf(path, sizeof(path), "root.%s", field->name);
        if (field->type == SERIALIZER_ROOT_VALUE) {
            if (validate_legacy_value(value, field->value_type, field->string_capacity,
                                      path, error, error_size) != 0)
                return -1;
        } else if (field->type == SERIALIZER_ROOT_TABLE) {
            if (validate_legacy_fields(value, field, path, 0, error, error_size) != 0)
                return -1;
        } else if (validate_legacy_array(value, field, path, error, error_size) != 0) {
            return -1;
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

/*
 * The keys of one table. Only what the loader really skips is reported:
 * what validate_legacy refuses (a [[checkpoints]] entry that is not a
 * table, or whose x is not a number) fails the load instead, and saying
 * "it is ignored" about it would be untrue.
 */
static void legacy_warn_fields(LegacyWarnings *warnings, toml_datum_t table,
                               const SerializerRootSpec *root, const char *path,
                               int strict)
{
    if (table.type != TOML_TABLE) {
        if (!strict)
            legacy_warn(warnings, "%s has type %s, expected table; it is ignored",
                        path, toml_type_name(table.type));
        return;
    }
    for (int i = 0; i < table.u.tab.size; i++) {
        const char *key = table.u.tab.key[i];
        toml_datum_t value = table.u.tab.value[i];
        const SerializerFieldSpec *field =
            serializer_find_field(root->fields, root->field_count, key, table.u.tab.len[i]);

        if (!field) {
            char shown[SHOWN_KEY_SIZE];
            legacy_warn(warnings, "%s has unknown key '%s'; it is ignored", path,
                        show_key(shown, sizeof(shown), key, table.u.tab.len[i]));
        } else if (field->required) {
            continue;   /* checked strictly by validate_legacy */
        } else if (!legacy_value_is_read(value, field->type)) {
            legacy_warn(warnings, "%s.%s has type %s and is ignored", path, field->name,
                        toml_type_name(value.type));
        } else if (field->type == SERIALIZER_VALUE_ENUM &&
                   !serializer_enum_name_known(field, value.u.str.ptr)) {
            /* load_field gives such a value the row's default. */
            char shown[SHOWN_KEY_SIZE];
            legacy_warn(warnings, "%s.%s has unknown value '%s'; %s is used instead",
                        path, field->name,
                        show_key(shown, sizeof(shown), value.u.str.ptr, value.u.str.len),
                        field->text_fallback);
        }
    }
}

int serializer_legacy_warnings(toml_datum_t top, SerializerWarningFn warn,
                               void *context)
{
    LegacyWarnings warnings = { warn, context, 0 };

    if (top.type != TOML_TABLE) return 0;
    /* A stated version is checked strictly: unknown keys are errors there. */
    if (serializer_table_get(top, "format_version").type != TOML_UNKNOWN) return 0;

    for (int i = 0; i < top.u.tab.size; i++) {
        const char *key = top.u.tab.key[i];
        const SerializerRootSpec *field = serializer_find_root(key, top.u.tab.len[i]);
        toml_datum_t value = top.u.tab.value[i];
        char path[256];

        if (!field) {
            char shown[SHOWN_KEY_SIZE];
            legacy_warn(&warnings, "root has unknown key '%s'; it is ignored",
                        show_key(shown, sizeof(shown), key, top.u.tab.len[i]));
            continue;
        }
        snprintf(path, sizeof(path), "root.%s", field->name);
        if (field->type == SERIALIZER_ROOT_VALUE) {
            if (!legacy_value_is_read(value, field->value_type))
                legacy_warn(&warnings, "%s has type %s and is ignored", path,
                            toml_type_name(value.type));
        } else if (field->type == SERIALIZER_ROOT_TABLE) {
            legacy_warn_fields(&warnings, value, field, path, 0);
        } else if (value.type != TOML_ARRAY) {
            /* A strict array that is not an array fails the load instead. */
            if (!serializer_root_is_strict(field))
                legacy_warn(&warnings, "%s has type %s, expected array; it is ignored",
                            path, toml_type_name(value.type));
        } else if (field->type == SERIALIZER_ROOT_ARRAY_TABLE) {
            /* (A floor gap that is not a number fails the load, so a plain
             * array of integers has nothing to warn about.) */
            for (int j = 0; j < value.u.arr.size; j++) {
                /* Parent path (up to 255 chars) + "[" + up to 10 digits + "]". */
                char item_path[256 + 16];
                snprintf(item_path, sizeof(item_path), "%s[%d]", path, j);
                legacy_warn_fields(&warnings, value.u.arr.elem[j], field, item_path,
                                   serializer_root_is_strict(field));
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

    version = serializer_table_get(top, "format_version");
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
