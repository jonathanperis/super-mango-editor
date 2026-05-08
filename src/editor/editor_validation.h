/*
 * editor_validation.h — Level editor validation report helpers.
 */
#pragma once

#include "../levels/level.h"

#define EDITOR_VALIDATION_MAX_MESSAGES 8
#define EDITOR_VALIDATION_MESSAGE_LEN  128

typedef struct {
    int error_count;
    int warning_count;
    int message_count;
    char messages[EDITOR_VALIDATION_MAX_MESSAGES][EDITOR_VALIDATION_MESSAGE_LEN];
} EditorValidationReport;

int editor_validate_level(const LevelDef *def, EditorValidationReport *report);
const char *editor_validation_summary(const EditorValidationReport *report);
