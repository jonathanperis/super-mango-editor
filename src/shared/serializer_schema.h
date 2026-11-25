/*
 * serializer_schema.h — The level file schema and the serializer's
 *                       internal entry points (not a public API).
 *
 * The level format is described once, as data: one table row per TOML key
 * (serializer_schema.c).  Four files read that table, and this header is
 * what they share:
 *
 *   serializer_schema.c    the table itself, finding a row, --dump-schema
 *   serializer_validate.c  the strict v1 check, the legacy check and its
 *                          warnings, and the line a message is about
 *   serializer_load.c      serializer_load_schema: TOML values into LevelDef
 *   serializer_save.c      serializer_write_entry: LevelDef back to TOML
 */

#pragma once

#include <stddef.h>                         /* size_t */
#include <stdio.h>                          /* FILE */
#include "../../vendor/tomlc17/tomlc17.h"  /* toml_datum_t */
#include "../levels/level.h"                /* LevelDef */

/* ================================================================== */
/* The byte-offset table pattern                                        */
/* ================================================================== */

/*
 * Three places in this project describe many struct members with a table
 * instead of writing one function per member:
 *
 *   - the schema rows below (serializer_schema.c): every key of a level
 *     file and the LevelDef member it is loaded into;
 *   - s_damage_sources (collision/game_collision.c): where each enemy's
 *     count is in GameWorld;
 *   - the PropertyRow tables (editor/properties_entities.c): which member
 *     of a placement each field of the properties panel edits.
 *
 * A row cannot hold "the member x of a SpiderPlacement" as a pointer,
 * because there is no SpiderPlacement yet when the table is written, and
 * there will be many of them.  It holds a number instead:
 *
 *     offsetof(SpiderPlacement, x)     how many bytes into a
 *                                      SpiderPlacement the member x starts
 *
 * offsetof (<stddef.h>) is a compile-time constant, so the table can be a
 * static const array.  To reach the member of one real struct, add that
 * number to the struct's address, counted in bytes:
 *
 *     char *member = (char *)item + row->offset;    item: a SpiderPlacement *
 *
 * (char is one byte, so pointer arithmetic on a char * moves byte by
 * byte.)  member now points at that struct's x.  The value is then copied
 * out with memcpy (the serializer), read through a pointer of the
 * member's type (game_collision.c's world_count), or the address is handed
 * to a widget that writes through it (properties_entities.c's draw_row).
 * Either way the row must also say what type the member is, since the
 * offset alone does not: a float read as an int gives nonsense.  The
 * serializer's MEMBER_IS check makes the compiler verify that.
 *
 * The payoff is that the loader, the writer, the collision pass and the
 * properties panel are each one loop over a table: adding an entity, or a
 * key to one, adds a row, not a function.
 */

/* ================================================================== */
/* The schema rows (serializer_schema.c)                               */
/* ================================================================== */

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
 * row in serializer_schema.c (plus its validation rule, its editor field
 * and the manual).
 *
 * `storage` says what the member is, which decides how the bytes at
 * `offset` are read and written. SERIALIZER_STORE_NONE marks a row that the
 * schema check reads but that is loaded and saved by hand; the only
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
    /* A string that is "" is written as LEVEL_UNTITLED_NAME instead, so a
     * saved level always has a name a person can read. Only `name` uses
     * it. Loading does not do the reverse: a file without a name loads
     * with an empty one, and the campaign then shows its file name. */
    SERIALIZER_WRITE_UNTITLED_IF_EMPTY
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


/* The rows of the table, in order (NULL past the end).  The loader walks
 * them all; the writer and the checks look one up by its key. */
const SerializerRootSpec *serializer_schema_row(size_t index);

/* The row of a level-wide key, or of the field `name` of a table: name_len
 * bytes of name (a TOML key may hold a NUL, which never matches).  NULL
 * when there is none. */
const SerializerRootSpec *serializer_find_root(const char *name, int name_len);
const SerializerFieldSpec *serializer_find_field(const SerializerFieldSpec *fields,
                                                 size_t field_count,
                                                 const char *name, int name_len);

/* 1 when any key of the row's tables is required: such an array is read
 * strictly even in a file without format_version. */
int serializer_root_is_strict(const SerializerRootSpec *root);
/* 1 when text is one of the field's enum names (and its validator, if any,
 * accepts it).  An unknown name loads as the row's default. */
int serializer_enum_name_known(const SerializerFieldSpec *field, const char *text);

/* Does the TOML key (supplied_len bytes) equal expected exactly?  A key
 * with an embedded NUL never does. */
int serializer_key_matches(const char *supplied, int supplied_len,
                           const char *expected);

/* The value of `expected` in a TOML table, by serializer_key_matches;
 * a datum of type TOML_UNKNOWN when the key is missing or table is not a
 * table. */
toml_datum_t serializer_table_get(toml_datum_t table, const char *expected);

/* ================================================================== */
/* Checking a parsed file (serializer_validate.c)                      */
/* ================================================================== */

/*
 * Validate explicit v1 schema before any TOML value is normalized into a
 * LevelDef. Missing format_version selects legacy-compatible loading,
 * which still refuses what it cannot skip: a number that does not fit its
 * field, an array longer than its LevelDef array, a floor gap that is not
 * a number, and a [[checkpoints]] entry without a numeric x and y. Every
 * message starts with the value's full TOML path ("root.platforms[1].
 * tile_height"), so serializer_toml_line can give its line.
 */
int serializer_validate_schema(toml_datum_t top, int *strict,
                               char *error, size_t error_size);

/*
 * serializer_legacy_warnings — Say what a file without format_version loses.
 *
 * Such a file is read leniently: a key the loader does not know, or a value
 * of a type it cannot use, is skipped and the field keeps its default.
 * That is what lets old files load, but it also hides typing mistakes: a
 * hand-written file that forgets the version line and spells a table
 * [[plataforms]], or writes x = "12", loads with those values silently
 * gone, and the editor's next save (which writes format_version = 1)
 * makes the loss permanent.
 *
 * warn is called once per skipped key or value, and once per enum value
 * the enum does not have (pad_type = "WOD": the row's default is used),
 * with a message that starts with its TOML path (so serializer_toml_line
 * can find its line).  A key or value quoted in a message has its control
 * bytes escaped (serializer_message_text).  What the loader refuses
 * instead of skipping (see serializer_validate_schema) is not a warning.
 * At most SERIALIZER_LEGACY_WARNING_MAX are reported.  Returns how many
 * there were; 0 for a file that states its format_version, which is
 * checked strictly instead and has nothing to warn about.
 */
#define SERIALIZER_LEGACY_WARNING_MAX 16
typedef void (*SerializerWarningFn)(void *context, const char *message);
int serializer_legacy_warnings(toml_datum_t top, SerializerWarningFn warn,
                               void *context);

/*
 * serializer_toml_line — The source line of the value a message is about.
 *
 * Schema and validator messages start with a TOML path ("root.coins[2].x
 * has type string", "spiders[0].vx is 0.00 ..."); this follows that path
 * through the parsed tree and returns the line tomlc17 recorded for the
 * deepest part it finds (the element's [[coins]] header when the key
 * itself is missing).  Returns 0 when the message has no path or the
 * path is not in the tree.
 */
int serializer_toml_line(toml_datum_t top, const char *message);

/* ================================================================== */
/* Loading and writing (serializer_load.c, serializer_save.c)          */
/* ================================================================== */

/*
 * serializer_load_schema — Read every value of a level into def.
 *
 * One loop over the schema table in serializer_schema.c.  Each row says
 * what kind of key it is and where its value lives in LevelDef:
 *
 *   - a level-wide key (name, music_volume ...) is read into its member, a
 *     missing key taking the row's fallback;
 *   - a single [table] ([physics], [last_star]) is read key by key; a
 *     missing table keeps what level_def_init_defaults set;
 *   - floor_gaps, a plain array of integers;
 *   - an [[array]] of tables is copied element by element, each key into
 *     the struct member its field row names.  A missing array stays empty,
 *     and an item that is not a table is skipped (not counted).
 *
 * An enum value the enum does not have reads as the row's default.
 *
 * Returns 0, or -1 (after printing why) when an array holds more items than
 * its LevelDef array can, a floor gap is not a number, or a required key
 * ([[checkpoints]] x and y) has no finite number.  serializer_validate_schema
 * has refused all of these already, with the value's path and line; this
 * is the safety net for a caller that skipped it.  def may be half filled
 * after a failure: level_load_toml_text reads into a scratch copy and
 * throws it away, so the caller's level is never touched.
 *
 * format_version is the one key this does not read (serializer_load_header).
 */
int serializer_load_schema(toml_datum_t top, LevelDef *def);

/*
 * serializer_write_entry — Write one row of the schema table as TOML.
 *
 * name is the row's key: "music_volume" writes one `key = value` line,
 * "physics" a whole [physics] table, "floor_gaps" the integer array,
 * "spiders" every [[spiders]] table.  The keys of a table are written in
 * the order of its field table, which is therefore part of the file
 * format.  Nothing is written for a name that is not in the table, or for
 * format_version.  The caller decides the order of the entries in the file
 * by the order of its calls.
 */
void serializer_write_entry(FILE *fp, const LevelDef *def, const char *name);

/*
 * The rows of the schema table and the order a save writes them in, so a
 * test can check that every row but format_version (written by hand, as
 * the first line) is saved exactly once.
 *
 *   serializer_schema_row_name(i)  the key of row i, in table order
 *                                  (serializer_schema.c, ROOT_FIELDS)
 *   serializer_save_order_name(i)  the i-th entry a save writes
 *                                  (serializer_save.c: s_key_order,
 *                                  [physics], then s_table_order)
 *
 * Each returns NULL for an index at or past its count.
 */
size_t serializer_schema_row_count(void);
const char *serializer_schema_row_name(size_t index);
size_t serializer_save_order_count(void);
const char *serializer_save_order_name(size_t index);

/*
 * serializer_dump_schema — Print the schema table as text, one line per
 * key, for tools/level_check.c --dump-schema:
 *
 *     value   <key> <type> [capacity=N]
 *     integers <key> max=N
 *     table   <key>
 *     tables  <key> max=N
 *     field   <owner> <key> <type> [required] [capacity=N] [values=A,B]
 *
 * tests/schema_parity_test.py compares tools/validate_levels.py with it,
 * so the Python copy of these facts cannot drift from the C one.
 */
void serializer_dump_schema(FILE *fp);
