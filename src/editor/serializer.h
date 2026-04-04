/*
 * serializer.h — JSON serialization for level definitions.
 *
 * Provides bidirectional conversion between LevelDef structs and cJSON
 * trees, plus convenience functions for reading/writing JSON files.
 *
 * The editor uses these to save levels as human-readable .json files and
 * load them back into the engine's LevelDef format at runtime.
 *
 * Depends on:
 *   - cJSON (vendor/cJSON) for JSON tree building and parsing.
 *   - level.h for the LevelDef struct and all placement types.
 */
#pragma once

#include "../../vendor/cJSON/cJSON.h" /* cJSON tree type */
#include "../levels/level.h"          /* LevelDef and all placement structs */

/* ------------------------------------------------------------------ */
/* In-memory conversion                                                */
/* ------------------------------------------------------------------ */

/*
 * level_to_json — Build a cJSON tree from a LevelDef.
 *
 * Serializes every field of the LevelDef (name, sea_gaps, all 25 entity
 * arrays, and the single last_star) into a cJSON object.  Enum fields
 * are stored as human-readable strings ("RECT", "SPIN", etc.) so the
 * JSON is easy to read and edit by hand.
 *
 * Returns a newly allocated cJSON object on success, or NULL on failure.
 * The caller owns the returned tree and must free it with cJSON_Delete().
 */
cJSON *level_to_json(const LevelDef *def);

/*
 * level_from_json — Populate a LevelDef from a cJSON tree.
 *
 * Reads every known key from the JSON object and fills the corresponding
 * LevelDef fields.  Missing arrays are treated as empty (count = 0).
 * Array sizes are validated against the MAX_* constants; if any array
 * exceeds its limit, the function returns -1 immediately.
 *
 * Returns 0 on success, -1 on error (oversized array or bad data).
 */
int level_from_json(const cJSON *json, LevelDef *def);

/* ------------------------------------------------------------------ */
/* File I/O                                                            */
/* ------------------------------------------------------------------ */

/*
 * level_save_json — Serialize a LevelDef to a pretty-printed JSON file.
 *
 * Calls level_to_json internally, renders the tree with cJSON_Print
 * (indented, human-readable), writes the string to `path`, then frees
 * all temporary memory.
 *
 * Returns 0 on success, -1 on error (serialization or file I/O failure).
 */
int level_save_json(const LevelDef *def, const char *path);

/*
 * level_load_json — Read a JSON file and deserialize it into a LevelDef.
 *
 * Reads the entire file into memory, parses it with cJSON_Parse, then
 * calls level_from_json to populate `def`.  All temporary memory (file
 * buffer and cJSON tree) is freed before returning.
 *
 * Returns 0 on success, -1 on error (file not found, parse error, or
 * level_from_json validation failure).
 */
int level_load_json(const char *path, LevelDef *def);
