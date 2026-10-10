/*
 * editor_validation.c — Pure validation checks used by the level editor.
 *
 * Every rule the game applies comes from level_validate_runtime_each, so
 * the editor and the game can never disagree about what is valid.  The
 * editor adds only what the game cannot know without the files at hand:
 * that each asset or next_phase file exists, plus two warnings.
 */

#include "editor_validation.h"

#include "../effects/fog.h"         /* MAX_FOG_TEXTURES */
#include "../effects/parallax.h"    /* MAX_BACKGROUND_LAYERS */
#include "../levels/level_loader.h"
#include "../levels/level_validate.h" /* LevelIssueLocation */
#include "../shared/serializer_io.h"
#include "../surfaces/platform.h"   /* MAX_PLATFORMS */

#include <stdio.h>
#include <string.h>

/* Every path field a level has: music, floor tile, next_phase, one tile
 * per platform and one per layer. */
#define PATH_FIELDS_MAX (3 + MAX_PLATFORMS + 2 * MAX_BACKGROUND_LAYERS + MAX_FOG_TEXTURES)

/*
 * One validation pass: the report being filled, and the path fields the
 * game's rules already rejected (unsafe, wrong folder or file type).  Such
 * a path already has its error; checking whether it exists would report
 * the same field twice, and would probe a path that may point anywhere.
 */
typedef struct {
    EditorValidationReport *report;
    char rejected[PATH_FIELDS_MAX][48];
    int rejected_count;
} ValidationPass;

static void report_add_at(EditorValidationReport *report, int is_error,
                          const LevelIssueLocation *where,
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
    if (where) {
        report->locations[idx] = *where;
    } else {
        /* Messages begin with the field they are about; read it back. */
        (void)level_issue_location_parse(report->messages[idx],
                                         &report->locations[idx]);
    }
    report->message_count++;
}

/* A message whose location is read from its leading field name. */
static void report_add(EditorValidationReport *report, int is_error,
                       const char *fmt, const char *field, const char *path)
{
    report_add_at(report, is_error, NULL, fmt, field, path);
}

/* A message pointing at one place that its wording does not start with. */
static LevelIssueLocation location(const char *path)
{
    LevelIssueLocation where;
    memset(&where, 0, sizeof(where));
    where.index = -1;
    snprintf(where.path, sizeof(where.path), "%s", path);
    return where;
}

/* 1 when a field name names a path: "music_path", "platforms[2].tile_path",
 * "fog_layers[0].path" or "next_phase". */
static int is_path_field(const char *field, size_t length)
{
    return (length >= 4 && memcmp(field + length - 4, "path", 4) == 0) ||
           (length == 10 && memcmp(field, "next_phase", 10) == 0);
}

/* level_validate_runtime_each hands every runtime error to this, with its
 * location already read from the message.  Runtime messages begin with the
 * field they are about, which is how a rejected path field is noted. */
static void add_runtime_issue(void *context, const char *message,
                              const LevelIssueLocation *where)
{
    ValidationPass *pass = context;
    size_t length = strcspn(message, " ");

    report_add_at(pass->report, 1, where, "%s", message, NULL);
    if (is_path_field(message, length) && pass->rejected_count < PATH_FIELDS_MAX &&
        length < sizeof(pass->rejected[0])) {
        memcpy(pass->rejected[pass->rejected_count], message, length);
        pass->rejected[pass->rejected_count][length] = '\0';
        pass->rejected_count++;
    }
}

/* The editor's own check of a path the game accepted: does the file exist? */
static void check_path(ValidationPass *pass, const char *field, const char *path)
{
    if (!path || path[0] == '\0') return;
    for (int i = 0; i < pass->rejected_count; i++)
        if (strcmp(pass->rejected[i], field) == 0) return;
    if (!serializer_file_exists_utf8(path))
        report_add(pass->report, 1, "%s missing: %s", field, path);
}

int editor_validate_level(const LevelDef *def, EditorValidationReport *report)
{
    char field[64];
    /* Static: about 2.6 KB of field names; the editor is single-threaded. */
    static ValidationPass pass;

    if (!report) return -1;
    memset(report, 0, sizeof(*report));

    if (!def) {
        report_add(report, 1, "%s", "LevelDef is NULL", NULL);
        return -1;
    }

    /* Every runtime rule the game applies, each failure its own message
     * (the game itself stops at the first one).  Nothing below repeats
     * one: screen_count 0, say, is the game's default of 4 screens, and
     * fine here too. */
    pass.report = report;
    pass.rejected_count = 0;
    (void)level_validate_runtime_each(def, add_runtime_issue, &pass);

    check_path(&pass, "music_path", def->music_path);
    check_path(&pass, "floor_tile_path", def->floor_tile_path);
    check_path(&pass, "next_phase", def->next_phase);

    for (int i = 0; i < def->platform_count && i < MAX_PLATFORMS; i++) {
        snprintf(field, sizeof(field), "platforms[%d].tile_path", i);
        check_path(&pass, field, def->platforms[i].tile_path);
    }

    for (int i = 0; i < def->background_layer_count && i < MAX_BACKGROUND_LAYERS; i++) {
        snprintf(field, sizeof(field), "background_layers[%d].path", i);
        check_path(&pass, field, def->background_layers[i].path);
    }
    for (int i = 0; i < def->foreground_layer_count && i < MAX_BACKGROUND_LAYERS; i++) {
        snprintf(field, sizeof(field), "foreground_layers[%d].path", i);
        check_path(&pass, field, def->foreground_layers[i].path);
    }
    for (int i = 0; i < def->fog_layer_count && i < MAX_FOG_TEXTURES; i++) {
        snprintf(field, sizeof(field), "fog_layers[%d].path", i);
        check_path(&pass, field, def->fog_layers[i].path);
    }

    if (def->name[0] == '\0') {
        LevelIssueLocation where = location("name");
        report_add_at(report, 0, &where, "%s", "level name is empty", NULL);
    }

    if (def->last_star.x == 0.0f && def->last_star.y == 0.0f) {
        report_add(report, 0, "%s", "last_star remains at origin", NULL);
    }

    return report->error_count == 0 ? 0 : -1;
}

void editor_load_report_add(void *context, const char *message, int line)
{
    EditorLoadReport *report = context;

    if (!report || !message) return;
    report->total++;
    if (report->count >= EDITOR_LOAD_PROBLEM_MAX) return;
    if (line > 0)
        snprintf(report->messages[report->count], EDITOR_VALIDATION_MESSAGE_LEN,
                 "line %d: %s", line, message);
    else
        snprintf(report->messages[report->count], EDITOR_VALIDATION_MESSAGE_LEN,
                 "%s", message);
    report->count++;
}

int editor_validation_hidden_count(const EditorValidationReport *report)
{
    int hidden;

    if (!report) return 0;
    hidden = report->error_count + report->warning_count - report->message_count;
    return hidden > 0 ? hidden : 0;
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
