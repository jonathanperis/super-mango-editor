/*
 * editor_validation.h — Level editor validation report helpers.
 */
#pragma once

#include <stdint.h>

#include "../levels/level.h"
#include "../levels/level_validate.h" /* LevelIssueLocation */

/* The report keeps this many messages, more than any level a person writes
 * by hand produces; the counts include any beyond it.  The Level Config
 * panel shows EDITOR_VALIDATION_VISIBLE_ROWS of them at a time and the wheel
 * scrolls through the rest. */
#define EDITOR_VALIDATION_MAX_MESSAGES 256
#define EDITOR_VALIDATION_VISIBLE_ROWS 8
/* Room for "<field> unsafe: " plus a full 255-byte level path, so a long
 * asset path is reported whole instead of being cut off mid-name. */
#define EDITOR_VALIDATION_MESSAGE_LEN  384

/*
 * locations[i] says where messages[i] points (an entity or a Level Config
 * field, in TOML terms); an empty path means "nowhere in particular".
 * Clicking a message in the editor goes there.
 */
typedef struct {
    int error_count;
    int warning_count;
    int message_count;
    char messages[EDITOR_VALIDATION_MAX_MESSAGES][EDITOR_VALIDATION_MESSAGE_LEN];
    LevelIssueLocation locations[EDITOR_VALIDATION_MAX_MESSAGES];
} EditorValidationReport;

/*
 * EditorLoadReport — why the last Open failed (count == 0: it did not, or
 * the designer dismissed the list).  A file that will not load never
 * becomes the document, so its problems cannot be validation messages;
 * the Level Config panel lists them above those instead.  Each message
 * starts with "line N: " when the problem is on one line of the file.
 */
#define EDITOR_LOAD_PROBLEM_MAX 8
typedef struct {
    char file[64];   /* the file name, for the list's heading */
    int  count;      /* problems listed                       */
    int  total;      /* problems found (may exceed the list)  */
    char messages[EDITOR_LOAD_PROBLEM_MAX][EDITOR_VALIDATION_MESSAGE_LEN];
} EditorLoadReport;

/* Add one problem; the signature is level_load_toml_explained's
 * LevelLoadProblemFn, with the report as context. */
void editor_load_report_add(void *report, const char *message, int line);

int editor_validate_level(const LevelDef *def, EditorValidationReport *report);
/* editor_validate_level, except that whether an asset file exists is asked
 * again only when the last answer for that path is probe_max_age_ms old or
 * older.  The editor's frame loop validates every frame of a drag (each
 * frame a new document) and passes its one-second recheck interval, so a
 * drag no longer opens every asset file 30 times a second.  Save and
 * playtest use editor_validate_level, which always asks. */
int editor_validate_level_reusing(const LevelDef *def, EditorValidationReport *report,
                                  uint32_t probe_max_age_ms);
const char *editor_validation_summary(const EditorValidationReport *report);
/* How many errors and warnings the report counted but had no room to list. */
int editor_validation_hidden_count(const EditorValidationReport *report);
