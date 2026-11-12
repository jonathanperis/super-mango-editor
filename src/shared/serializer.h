/*
 * serializer.h — TOML serialization for level definitions.
 *
 * Provides save/load functions that convert between LevelDef structs and
 * TOML files on disk.  Uses the tomlc17 library (vendor/tomlc17) for
 * parsing, and plain fprintf for emitting.
 *
 * The editor uses these to save levels as human-readable .toml files and
 * load them back into the engine's LevelDef format at runtime.
 *
 * Depends on:
 *   - tomlc17 (vendor/tomlc17) for TOML parsing.
 *   - level.h for the LevelDef struct and all placement types.
 */
#pragma once

#include <stddef.h> /* size_t */
#include <stdio.h>  /* FILE, for SerializerEmitFn */

#include "../levels/level.h" /* LevelDef and all placement structs */
#include "serializer_io.h"   /* SerializerFileFingerprint */

typedef enum {
    SERIALIZER_SAVE_REPLACE = 0,
    SERIALIZER_SAVE_CREATE_ONLY = 1
} SerializerSavePolicy;

/* Prints a file's whole contents into fp (see serializer_write_temp). */
typedef void (*SerializerEmitFn)(FILE *fp, const void *context);

/*
 * serializer_write_temp — The first half of a save, for a caller that
 * saves several files together and wants every one written before any is
 * replaced (the editor's Campaign view).  Writes emit's text
 * to a new sibling temporary file of `path` (permissions as a level save),
 * flushed and closed, and puts its name in temp_path.  Returns 0, or -1
 * with nothing left on disk.  Then either serializer_install_temp(temp_path,
 * path, 0) or serializer_remove_temp(temp_path).
 */
int serializer_write_temp(const char *path, SerializerEmitFn emit,
                          const void *context, char *temp_path,
                          size_t temp_path_size);

/* ------------------------------------------------------------------ */
/* File I/O                                                            */
/* ------------------------------------------------------------------ */

/*
 * level_save_toml — Serialize a LevelDef to a human-readable TOML file.
 *
 * Writes every field of the LevelDef (name, floor_gaps, all entity arrays,
 * parallax/foreground layers, and level-wide configuration) to the file
 * at `path`.  Enum fields are stored as human-readable strings ("RECT",
 * "SPIN", etc.) so the TOML is easy to read and edit by hand.
 *
 * On POSIX, a new file gets 0666 minus the process umask (usually 0644),
 * as any saved document would; replacing a regular file keeps its
 * permission bits, and its owner and group where the OS allows it. A
 * sibling temporary file is installed atomically.
 *
 * Returns 0 on success, -1 on error (serialization or file I/O failure).
 */
int level_save_toml(const LevelDef *def, const char *path);

/* Like level_save_toml, but a new file is private to its owner (0600):
 * for the editor's playtest copies, which nobody else should read. */
int level_save_toml_private(const LevelDef *def, const char *path);

/* Save with explicit create-only or replacement semantics. */
int level_save_toml_with_policy(const LevelDef *def, const char *path,
                                SerializerSavePolicy policy);

/* Save with a final source fingerprint recheck before replacement.
 * Returns -2 (nothing written) when the file no longer matches expected. */
int level_save_toml_checked(const LevelDef *def, const char *path,
                            SerializerSavePolicy policy,
                            const SerializerFileFingerprint *expected);

/*
 * Every save above returns SERIALIZER_REPLACE_TEMP_KEPT (-3) when the
 * operating system moved the old file away but could not put the new one in
 * its place (this can happen on Windows).  The level is then safe in a
 * temporary file next to the destination; this returns that file's path
 * until the next save starts ("" when the last save did not end this way).
 */
const char *level_save_kept_temp_path(void);

/* serializer_write_temp for a level: refuses (-1) a level the game would
 * refuse, like every save above, and otherwise writes the text
 * level_save_toml would write into a temporary sibling of path. */
int level_write_temp(const LevelDef *def, const char *path,
                     char *temp_path, size_t temp_path_size);

/* Save a recovery copy with its known normal destination embedded as metadata.
 * Recovery copies are private to their owner (0600) on POSIX. */
int level_save_toml_recovery(const LevelDef *def, const char *path,
                             const char *original_path);

/* Read recovery destination metadata; empty output means destination unknown. */
int level_read_recovery_path(const char *path, char *buf, size_t buf_size);

/*
 * level_load_toml — Read a TOML file and deserialize it into a LevelDef.
 *
 * Reads the file (level_load_toml_file), parses it with toml_parse, then
 * walks the resulting table tree to populate `def`.
 * Missing arrays are treated as empty (count = 0).  The result must pass
 * level_validate_runtime; `def` is only written when the whole load
 * succeeds.
 *
 * Returns 0 on success, -1 on error (file not found, parse error, schema
 * error, or a level that fails validation).
 */
int level_load_toml(const char *path, LevelDef *def);

/*
 * LevelLoadProblemFn — hears one reason a level could not be loaded.
 * line is the 1-based line in the file the problem is on, or 0 when it is
 * not tied to one line (the file cannot be opened, say).  message is only
 * valid during the call.
 */
typedef void (*LevelLoadProblemFn)(void *context, const char *message, int line);

/*
 * level_load_toml_explained — level_load_toml, and when it fails, say why.
 * report (may be NULL) is called for: a file that cannot be opened, a TOML
 * syntax error (one, with its line), a schema error (the first, with the
 * line of the value), or — for a file that is valid TOML but breaks the
 * runtime rules — every runtime error, each with its line.  The editor
 * lists them when a level will not open.
 *
 * A level that does load can still have something to say: a file without
 * format_version is read leniently, and each key or value that was skipped
 * is reported with a message starting "warning: " (see
 * serializer_legacy_warnings).  A caller that only cares about failures
 * can ignore every message of a load that returned 0.
 */
int level_load_toml_explained(const char *path, LevelDef *def,
                              LevelLoadProblemFn report, void *context);

/*
 * LEVEL_TEXT_MAX — the largest level file the loader reads, in bytes
 * (the file plus one NUL).  The shipped levels are about 11 KB and a level
 * with every array full is well under 100 KB, so 1 MiB refuses nothing a
 * person would write while keeping a crafted or mistaken file (a level
 * path pointed at a video, say) from being read into memory whole.
 */
#define LEVEL_TEXT_MAX ((size_t)1024 * 1024)

/*
 * level_load_toml_file — Read a level file once, then hash and parse those
 * same bytes.
 *
 * This is the entry point for a level on disk: level_load_toml and
 * level_load_toml_explained are this call with fewer arguments.  The file
 * is read into memory (at most LEVEL_TEXT_MAX bytes), and when fingerprint
 * is not NULL it receives the size and hash of exactly the bytes that were
 * parsed.  Hashing the file in a separate read could describe different
 * bytes if another program rewrote it in between; one read cannot.  The
 * fingerprint is filled in whenever the file could be read, even if the
 * level is then rejected.  report is as for level_load_toml_explained.
 */
int level_load_toml_file(const char *path, LevelDef *def,
                         LevelLoadProblemFn report, void *context,
                         SerializerFileFingerprint *fingerprint);

/*
 * level_load_toml_text — Parse a level from bytes already in memory.
 *
 * text need not be NUL-terminated; size is its length.  label stands in
 * for the file name in messages.  Everything after the read is the same as
 * for a file, which makes this the entry point for callers that have no
 * file: the fuzz harnesses, and tests.
 */
int level_load_toml_text(const char *text, size_t size, const char *label,
                         LevelDef *def, LevelLoadProblemFn report, void *context);
