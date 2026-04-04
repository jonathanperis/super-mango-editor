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

#include "../levels/level.h" /* LevelDef and all placement structs */

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
 * On POSIX, the file is created with 0644 permissions (owner-writable only).
 *
 * Returns 0 on success, -1 on error (serialization or file I/O failure).
 */
int level_save_toml(const LevelDef *def, const char *path);

/*
 * level_load_toml — Read a TOML file and deserialize it into a LevelDef.
 *
 * Uses toml_parse_file_ex to parse the file, then walks the resulting
 * table tree to populate `def`.  Missing arrays are treated as empty
 * (count = 0).  Array sizes are validated against the MAX_* constants;
 * if any array exceeds its limit, the function returns -1 immediately.
 *
 * Returns 0 on success, -1 on error (file not found, parse error, or
 * array size validation failure).
 */
int level_load_toml(const char *path, LevelDef *def);
