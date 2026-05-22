/*
 * editor_validation.c — Pure validation checks used by the level editor.
 */

#include "editor_validation.h"

#include "../levels/level_loader.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static int path_exists(const char *path)
{
    FILE *fp;

    if (!path || path[0] == '\0') return 1;

    fp = fopen(path, "rb");
    if (!fp) return 0;
    fclose(fp);
    return 1;
}

static int has_parent_segment(const char *value)
{
    const char *p = value;
    while (*p) {
        const char *start = p;
        const char *end;
        while (*p && *p != '/') p++;
        end = p;
        if ((end - start) == 2 && start[0] == '.' && start[1] == '.') {
            return 1;
        }
        if (*p == '/') p++;
    }
    return 0;
}

static int has_control_char(const char *value)
{
    const unsigned char *p = (const unsigned char *)value;
    while (*p) {
        if (iscntrl(*p)) return 1;
        p++;
    }
    return 0;
}

static int is_safe_repo_path_shape(const char *path)
{
    if (!path || path[0] == '\0') return 1;
    if (path[0] == '/' || strchr(path, '\\') != NULL ||
        (isalpha((unsigned char)path[0]) && path[1] == ':')) {
        return 0;
    }
    if (has_parent_segment(path) || has_control_char(path)) return 0;
    return 1;
}

static void report_add(EditorValidationReport *report, int is_error,
                       const char *fmt, const char *field, const char *path)
{
    int idx;

    if (!report) return;
    if (is_error) report->error_count++;
    else report->warning_count++;

    idx = report->message_count;
    if (idx >= EDITOR_VALIDATION_MAX_MESSAGES) return;

    if (path) {
        snprintf(report->messages[idx], EDITOR_VALIDATION_MESSAGE_LEN,
                 fmt, field, path);
    } else {
        snprintf(report->messages[idx], EDITOR_VALIDATION_MESSAGE_LEN,
                 "%s", field);
    }
    report->message_count++;
}

static void check_path(EditorValidationReport *report, const char *field,
                       const char *path)
{
    if (!path || path[0] == '\0') return;
    if (!is_safe_repo_path_shape(path)) {
        report_add(report, 1, "%s unsafe: %s", field, path);
        return;
    }
    if (!path_exists(path)) {
        report_add(report, 1, "%s missing: %s", field, path);
    }
}

int editor_validate_level(const LevelDef *def, EditorValidationReport *report)
{
    char err[128];

    if (!report) return -1;
    memset(report, 0, sizeof(*report));

    if (!def) {
        report_add(report, 1, "%s", "LevelDef is NULL", NULL);
        return -1;
    }

    if (level_validate_runtime(def, err, sizeof(err)) != 0) {
        report_add(report, 1, "%s", err, NULL);
    }

    if (def->screen_count <= 0) {
        report_add(report, 1, "%s", "screen_count must be > 0", NULL);
    }

    check_path(report, "music_path", def->music_path);
    check_path(report, "floor_tile_path", def->floor_tile_path);
    check_path(report, "next_phase", def->next_phase);

    for (int i = 0; i < def->background_layer_count && i < MAX_BACKGROUND_LAYERS; i++) {
        check_path(report, "background_layers[].path", def->background_layers[i].path);
    }
    for (int i = 0; i < def->foreground_layer_count && i < MAX_BACKGROUND_LAYERS; i++) {
        check_path(report, "foreground_layers[].path", def->foreground_layers[i].path);
    }
    for (int i = 0; i < def->fog_layer_count && i < MAX_FOG_TEXTURES; i++) {
        check_path(report, "fog_layers[].path", def->fog_layers[i].path);
    }

    if (def->name[0] == '\0') {
        report_add(report, 0, "%s", "level name is empty", NULL);
    }

    if (def->last_star.x == 0.0f && def->last_star.y == 0.0f) {
        report_add(report, 0, "%s", "last_star remains at origin", NULL);
    }

    return report->error_count == 0 ? 0 : -1;
}

const char *editor_validation_summary(const EditorValidationReport *report)
{
    static char summary[64];

    if (!report) return "Validation unavailable";
    if (report->error_count == 0 && report->warning_count == 0) {
        return "Validation: OK";
    }

    snprintf(summary, sizeof(summary), "Validation: %d error(s), %d warning(s)",
             report->error_count, report->warning_count);
    return summary;
}
