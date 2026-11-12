/*
 * serializer_parse.h — Internal TOML parse helpers.
 */

#pragma once

#include <stddef.h>                         /* size_t */
#include <stdio.h>                          /* FILE */
#include "../../vendor/tomlc17/tomlc17.h"  /* toml_datum_t */
#include "../levels/level.h"                /* LevelDef */

/* Read a TOML numeric field as float, accepting int or float scalars. */
float get_float(toml_datum_t tab, const char *key, float fallback);

/* Read a TOML integer field, accepting int or float scalars. */
int get_int(toml_datum_t tab, const char *key, int fallback);

/* Read a TOML string field, returning fallback for missing/non-string values. */
const char *get_str(toml_datum_t tab, const char *key, const char *fallback);

/*
 * Copy src (NULL reads as "") into the dst_size-byte array dst.  A longer
 * src is cut so dst always ends in a NUL; the schema check has already
 * refused strings that would not fit, so the cut is only a safety net.
 */
void copy_str(char *dst, size_t dst_size, const char *src);

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

/*
 * serializer_load_schema — Read every value of a level into def.
 *
 * One loop over the schema table in serializer_parse.c.  Each row says
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
 *                                  (serializer_parse.c, ROOT_FIELDS)
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
