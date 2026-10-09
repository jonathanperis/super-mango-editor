/*
 * editor_validation.h — Level editor validation report helpers.
 */
#pragma once

#include "../levels/level.h"
#include "../levels/level_validate.h" /* LevelIssueLocation */

/* The panel lists this many messages; the counts include any beyond it. */
#define EDITOR_VALIDATION_MAX_MESSAGES 16
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

int editor_validate_level(const LevelDef *def, EditorValidationReport *report);
const char *editor_validation_summary(const EditorValidationReport *report);
/* How many errors and warnings the report counted but had no room to list. */
int editor_validation_hidden_count(const EditorValidationReport *report);
