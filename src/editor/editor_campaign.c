/*
 * editor_campaign.c — The Campaign view (see editor_campaign.h).
 *
 * The view keeps the manifest's entries as a CampaignCatalog, the same
 * structure the game's start menu loads, so every verdict it shows comes
 * from the game's own rules (campaign_catalog_check in
 * src/levels/campaign_catalog.c).  Next to each entry it remembers what
 * the level file held when it was read (name, next_phase, fingerprint), to
 * know which files a Save must rewrite and that nobody else changed them.
 */

#include "editor_campaign.h"

#include <stdio.h>    /* snprintf */
#include <stdlib.h>   /* realloc, free, calloc */
#include <string.h>   /* memmove, strcmp, strrchr */

#include "editor_files.h"    /* editor_path_for_display */
#include "editor_session.h"  /* editor_set_status, editor_finish_field_edit */
#include "file_dialog.h"     /* file_dialog_open */
#include "../levels/campaign_catalog.h"
#include "../levels/level_path.h"   /* level_resolve_path */
#include "../shared/serializer.h"   /* level_save_toml_checked */
#include "../shared/ui.h"

/* What one entry's level file held when the view read or last wrote it. */
typedef struct {
    char disk_name[LEVEL_NAME_CAPACITY];
    char disk_next[sizeof(((LevelDef *)0)->next_phase)];
    SerializerFileFingerprint fingerprint;
} CampaignDisk;

struct EditorCampaign {
    char manifest_path[EDITOR_PATH_MAX];
    CampaignCatalog catalog;   /* the entries, in the order shown        */
    CampaignDisk *disk;        /* disk[i] belongs to catalog.levels[i]   */
    size_t capacity;           /* room in both arrays                    */
    int manifest_changed;      /* entries added, removed or reordered    */
    int selected;              /* highlighted row, -1 for none           */
    int scroll;                /* first row shown                        */
    int close_armed;           /* Close was refused once for unsaved work */
    char problem[160];         /* campaign-wide problem, "" when none    */
};

/* ------------------------------------------------------------------ */
/* Entries                                                             */
/* ------------------------------------------------------------------ */

/* Make room for `count` entries in both parallel arrays. */
static int reserve(EditorCampaign *c, size_t count)
{
    CampaignLevel *levels;
    CampaignDisk *disk;
    size_t capacity = c->capacity ? c->capacity : 8;

    if (count <= c->capacity) return 0;
    while (capacity < count) capacity *= 2;
    levels = realloc(c->catalog.levels, capacity * sizeof(*levels));
    if (!levels) return -1;
    c->catalog.levels = levels;
    disk = realloc(c->disk, capacity * sizeof(*disk));
    if (!disk) return -1;
    c->disk = disk;
    c->capacity = capacity;
    return 0;
}

/* Note what entry's file holds now: after reading it, or after Save. */
static void remember_disk(const CampaignLevel *entry, CampaignDisk *disk)
{
    char resolved[EDITOR_PATH_MAX];

    memset(disk, 0, sizeof(*disk));
    if (!entry->loaded) return;
    memcpy(disk->disk_name, entry->level.name, sizeof(disk->disk_name));
    memcpy(disk->disk_next, entry->level.next_phase, sizeof(disk->disk_next));
    if (level_resolve_path(entry->path, resolved, sizeof(resolved)) == 0)
        (void)serializer_fingerprint_utf8(resolved, &disk->fingerprint);
}

/* Does entry i's level differ from its file (a renamed or relinked level)? */
static int level_changed(const EditorCampaign *c, size_t i)
{
    const CampaignLevel *entry = &c->catalog.levels[i];
    return entry->loaded &&
           (strcmp(entry->level.name, c->disk[i].disk_name) != 0 ||
            strcmp(entry->level.next_phase, c->disk[i].disk_next) != 0);
}

/* Any edit means a refused Close must be asked again. */
static void edited(EditorCampaign *c)
{
    c->close_armed = 0;
}

/* Settle a half-typed name before the list changes under its field. */
static int finish_edit(EditorState *es)
{
    return editor_finish_field_edit(es);
}

void editor_campaign_check(EditorState *es)
{
    EditorCampaign *c = es ? es->campaign : NULL;
    if (!c) return;
    (void)campaign_catalog_check(&c->catalog, c->problem, sizeof(c->problem));
}

const CampaignCatalog *editor_campaign_entries(const EditorState *es)
{
    return es && es->campaign ? &es->campaign->catalog : NULL;
}

const char *editor_campaign_problem(const EditorState *es)
{
    return es && es->campaign ? es->campaign->problem : NULL;
}

int editor_campaign_unsaved(const EditorState *es)
{
    const EditorCampaign *c = es ? es->campaign : NULL;
    if (!c) return 0;
    if (c->manifest_changed) return 1;
    for (size_t i = 0; i < c->catalog.count; i++)
        if (level_changed(c, i)) return 1;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Opening and closing                                                 */
/* ------------------------------------------------------------------ */

int editor_campaign_open(EditorState *es, const char *manifest_path)
{
    EditorCampaign *c;

    if (!es) return -1;
    if (es->campaign) return 0;
    if (!finish_edit(es)) return -1;
    if (!manifest_path) manifest_path = CAMPAIGN_MANIFEST_PATH;
    if (strlen(manifest_path) >= sizeof(c->manifest_path)) {
        editor_set_status(es, "Campaign: path too long");
        return -1;
    }

    c = calloc(1, sizeof(*c));
    if (!c) {
        editor_set_status(es, "Campaign: out of memory");
        return -1;
    }
    memcpy(c->manifest_path, manifest_path, strlen(manifest_path) + 1);
    /* The parse half of the game's load: a campaign the start menu would
     * refuse (no playable level) still opens here, to be repaired. */
    if (campaign_catalog_read(manifest_path, &c->catalog) != 0) {
        free(c);
        editor_set_status(es, "Campaign: cannot read %s (see the terminal for why)",
                          manifest_path);
        return -1;
    }
    c->capacity = c->catalog.count;
    c->disk = calloc(c->capacity ? c->capacity : 1, sizeof(*c->disk));
    if (!c->disk) {
        campaign_catalog_cleanup(&c->catalog);
        free(c);
        editor_set_status(es, "Campaign: out of memory");
        return -1;
    }
    for (size_t i = 0; i < c->catalog.count; i++)
        remember_disk(&c->catalog.levels[i], &c->disk[i]);
    c->selected = c->catalog.count > 0 ? 0 : -1;
    es->campaign = c;
    editor_campaign_check(es);
    editor_set_status(es, "Campaign: %zu levels in %s (Esc returns to the level)",
                      c->catalog.count, manifest_path);
    return 0;
}

void editor_campaign_free(EditorState *es)
{
    if (!es || !es->campaign) return;
    campaign_catalog_cleanup(&es->campaign->catalog);
    free(es->campaign->disk);
    free(es->campaign);
    es->campaign = NULL;
}

int editor_campaign_close(EditorState *es, int force)
{
    EditorCampaign *c = es ? es->campaign : NULL;

    if (!c) return 1;
    if (!force && !c->close_armed && editor_campaign_unsaved(es)) {
        c->close_armed = 1;
        editor_set_status(es, "Unsaved campaign changes: Save, or Close again to discard them");
        return 0;
    }
    /* A name being typed belongs to the view going away. */
    if (es->ui.active_id >= CAMPAIGN_NAME_FIELD_ID &&
        es->ui.active_id < CAMPAIGN_NAME_FIELD_ID + 1000)
        ui_cancel_active_edit(&es->ui);
    editor_campaign_free(es);
    editor_set_status(es, "Campaign closed");
    return 1;
}

/* ------------------------------------------------------------------ */
/* Editing the list                                                    */
/* ------------------------------------------------------------------ */

int editor_campaign_move(EditorState *es, int index, int delta)
{
    EditorCampaign *c = es ? es->campaign : NULL;
    int other = index + delta;
    CampaignLevel entry;
    CampaignDisk disk;

    if (!c || index < 0 || (size_t)index >= c->catalog.count) return -1;
    if (other < 0 || (size_t)other >= c->catalog.count) return -1;
    if (!finish_edit(es)) return -1;
    entry = c->catalog.levels[index];
    c->catalog.levels[index] = c->catalog.levels[other];
    c->catalog.levels[other] = entry;
    disk = c->disk[index];
    c->disk[index] = c->disk[other];
    c->disk[other] = disk;
    c->selected = other;
    c->manifest_changed = 1;
    edited(c);
    editor_campaign_check(es);
    return 0;
}

int editor_campaign_remove(EditorState *es, int index)
{
    EditorCampaign *c = es ? es->campaign : NULL;
    size_t after;

    if (!c || index < 0 || (size_t)index >= c->catalog.count) return -1;
    if (!finish_edit(es)) return -1;
    editor_set_status(es, "Removed %s from the campaign (the file stays)",
                      c->catalog.levels[index].path);
    after = c->catalog.count - (size_t)index - 1;
    memmove(&c->catalog.levels[index], &c->catalog.levels[index + 1],
            after * sizeof(c->catalog.levels[0]));
    memmove(&c->disk[index], &c->disk[index + 1], after * sizeof(c->disk[0]));
    c->catalog.count--;
    if (c->selected >= (int)c->catalog.count) c->selected = (int)c->catalog.count - 1;
    c->manifest_changed = 1;
    edited(c);
    editor_campaign_check(es);
    return 0;
}

int editor_campaign_add(EditorState *es, const char *path)
{
    EditorCampaign *c = es ? es->campaign : NULL;
    char candidate[CAMPAIGN_LEVEL_PATH_SIZE];
    const char *name;
    CampaignLevel *entry;

    if (!c || !path || !path[0]) return -1;
    if (!finish_edit(es)) return -1;

    /* A manifest names levels as levels/<name>.toml relative to the game
     * folder.  A picked file is an absolute path: it qualifies when that
     * relative name leads to the very same file. */
    {
        const char *slash = strrchr(path, '/');
        const char *backslash = strrchr(path, '\\');   /* Windows pickers */
        if (backslash && (!slash || backslash > slash)) slash = backslash;
        name = slash ? slash + 1 : path;
    }
    if (snprintf(candidate, sizeof(candidate), "levels/%s", name) >= (int)sizeof(candidate) ||
        !campaign_entry_path_valid(candidate)) {
        editor_set_status(es, "Not added: a campaign lists levels/<name>.toml files only");
        return -1;
    }
    if (!serializer_file_exists_utf8(candidate) ||
        (strcmp(path, candidate) != 0 && !serializer_same_file_utf8(path, candidate))) {
        editor_set_status(es, "Not added: %s is not a file directly in levels/", name);
        return -1;
    }
    for (size_t i = 0; i < c->catalog.count; i++) {
        if (strcmp(c->catalog.levels[i].path, candidate) == 0) {
            editor_set_status(es, "Not added: %s is already in the campaign", candidate);
            return -1;
        }
    }
    if (reserve(c, c->catalog.count + 1) != 0) {
        editor_set_status(es, "Not added: out of memory");
        return -1;
    }

    entry = &c->catalog.levels[c->catalog.count];
    memset(entry, 0, sizeof(*entry));
    memcpy(entry->path, candidate, strlen(candidate) + 1);
    campaign_entry_load(entry);
    remember_disk(entry, &c->disk[c->catalog.count]);
    c->catalog.count++;
    c->selected = (int)c->catalog.count - 1;
    c->manifest_changed = 1;
    edited(c);
    editor_campaign_check(es);
    editor_set_status(es, "Added %s%s", candidate,
                      entry->loaded ? "" : " (it does not load; it stays unavailable)");
    return 0;
}

int editor_campaign_link_in_order(EditorState *es)
{
    EditorCampaign *c = es ? es->campaign : NULL;
    int changed = 0;

    if (!c || !finish_edit(es)) return 0;
    for (size_t i = 0; i < c->catalog.count; i++) {
        CampaignLevel *entry = &c->catalog.levels[i];
        const char *next = i + 1 < c->catalog.count ? c->catalog.levels[i + 1].path : "";
        if (!entry->loaded || strcmp(entry->level.next_phase, next) == 0) continue;
        /* Paths are at most CAMPAIGN_LEVEL_PATH_SIZE - 1 bytes, which
         * next_phase (256 bytes) always holds. */
        snprintf(entry->level.next_phase, sizeof(entry->level.next_phase), "%s", next);
        changed++;
    }
    edited(c);
    editor_campaign_check(es);
    if (changed > 0)
        editor_set_status(es, "Linked %d level%s in campaign order (Save writes them)",
                          changed, changed == 1 ? "" : "s");
    else
        editor_set_status(es, "Every next_phase already follows the campaign order");
    return changed;
}

/* ------------------------------------------------------------------ */
/* Saving                                                              */
/* ------------------------------------------------------------------ */

int editor_campaign_save(EditorState *es)
{
    EditorCampaign *c = es ? es->campaign : NULL;
    char reload[EDITOR_PATH_MAX] = "";
    int written = 0;
    int unloaded = 0;

    if (!c || !finish_edit(es)) return -1;

    /* 1. The game's rules first: a campaign-wide problem, or an entry this
     *    view can fix (its order or name), blocks the save.  A level file
     *    that does not load is the file's problem; the menu shows it
     *    disabled, so it may stay listed. */
    editor_campaign_check(es);
    if (c->problem[0]) {
        editor_set_status(es, "Campaign not saved: %s", c->problem);
        return -1;
    }
    for (size_t i = 0; i < c->catalog.count; i++) {
        const CampaignLevel *entry = &c->catalog.levels[i];
        if (!entry->loaded) { unloaded++; continue; }
        if (!entry->available) {
            editor_set_status(es, "Campaign not saved: %s: %s (Link in order fixes next_phase)",
                              entry->path, entry->problem);
            return -1;
        }
    }

    /* 2. Before writing anything: every file to rewrite must still hold
     *    what the view read (else one could be saved and the next refused,
     *    leaving half a chain), and the level open in the editor must not
     *    be rewritten under unsaved edits; when it is clean, it is reloaded
     *    after the save. */
    for (size_t i = 0; i < c->catalog.count; i++) {
        char resolved[EDITOR_PATH_MAX];
        SerializerFileFingerprint now;
        if (!level_changed(c, i)) continue;
        if (level_resolve_path(c->catalog.levels[i].path, resolved, sizeof(resolved)) != 0 ||
            serializer_fingerprint_utf8(resolved, &now) != 1 ||
            !serializer_fingerprint_equal(&now, &c->disk[i].fingerprint)) {
            editor_set_status(es, "Campaign not saved: %s changed on disk; close and reopen "
                              "the Campaign view", c->catalog.levels[i].path);
            return -1;
        }
        if (es->file_path[0] == '\0' || !serializer_same_file_utf8(es->file_path, resolved))
            continue;
        if (es->modified) {
            editor_set_status(es, "Campaign not saved: save or discard your changes to %s first",
                              c->catalog.levels[i].path);
            return -1;
        }
        memcpy(reload, es->file_path, sizeof(reload));
    }

    /* 3. Each changed level file, through the checked save: if someone
     *    else changed the file since the view read it, it is left alone. */
    for (size_t i = 0; i < c->catalog.count; i++) {
        CampaignLevel *entry = &c->catalog.levels[i];
        char resolved[EDITOR_PATH_MAX];
        int result;
        if (!level_changed(c, i)) continue;
        if (level_resolve_path(entry->path, resolved, sizeof(resolved)) != 0) {
            editor_set_status(es, "Campaign not saved: cannot find %s", entry->path);
            return -1;
        }
        result = level_save_toml_checked(&entry->level, resolved, SERIALIZER_SAVE_REPLACE,
                                         &c->disk[i].fingerprint);
        if (result == -2) {
            editor_set_status(es, "Campaign not saved: %s changed on disk; close and reopen "
                              "the Campaign view", entry->path);
            return -1;
        }
        if (result != 0) {
            editor_set_status(es, "Campaign not saved: writing %s failed", entry->path);
            return -1;
        }
        remember_disk(entry, &c->disk[i]);
        written++;
    }

    /* 4. The manifest, when its list changed. */
    if (c->manifest_changed) {
        if (campaign_manifest_save(c->manifest_path, &c->catalog) != 0) {
            editor_set_status(es, "Campaign not saved: writing %s failed", c->manifest_path);
            return -1;
        }
        c->manifest_changed = 0;
    }
    c->close_armed = 0;

    /* 5. Show the level open in the editor as it now is on disk. */
    if (reload[0]) (void)editor_load_level(es, reload);
    editor_set_status(es, "Campaign saved: %zu levels, %d level file%s updated%s",
                      c->catalog.count, written, written == 1 ? "" : "s",
                      unloaded ? "; some listed files do not load" : "");
    return 0;
}

/* ------------------------------------------------------------------ */
/* The view                                                            */
/* ------------------------------------------------------------------ */

#define CAMPAIGN_RED   ((Color){0xFF, 0x70, 0x70, 0xFF})
#define CAMPAIGN_GREEN ((Color){0x70, 0xE0, 0x80, 0xFF})

/* How many rows fit between the header and the buttons. */
static int visible_rows(void)
{
    return (CAMPAIGN_BUTTONS_Y - 8 - CAMPAIGN_ROWS_Y) / CAMPAIGN_ROW_H;
}

void editor_campaign_wheel(EditorState *es, float wheel)
{
    EditorCampaign *c = es ? es->campaign : NULL;
    int max_scroll;

    if (!c) return;
    c->scroll -= (int)wheel;
    max_scroll = (int)c->catalog.count - visible_rows();
    if (c->scroll > max_scroll) c->scroll = max_scroll;
    if (c->scroll < 0) c->scroll = 0;
}

/* One entry row: number, file, the level's name (editable), its verdict. */
static void draw_row(EditorState *es, EditorCampaign *c, int i, int y)
{
    CampaignLevel *entry = &c->catalog.levels[i];
    int right = CANVAS_W - CAMPAIGN_VIEW_X;
    int in_row = es->ui.mouse_x >= CAMPAIGN_VIEW_X - 4 && es->ui.mouse_x < right &&
                 es->ui.mouse_y >= y - 2 && es->ui.mouse_y < y - 2 + CAMPAIGN_ROW_H;
    int in_name = in_row && es->ui.mouse_x >= CAMPAIGN_NAME_X &&
                  es->ui.mouse_x < CAMPAIGN_NAME_X + CAMPAIGN_NAME_W;
    char text[CAMPAIGN_LEVEL_PATH_SIZE + 16];

    /* A click anywhere on the row selects it; outside the name field the
     * click is used up here. */
    if (in_row && es->ui.mouse_clicked) {
        c->selected = i;
        if (!in_name) es->ui.mouse_clicked = 0;
    }
    if (i == c->selected)
        DrawRectangle(CAMPAIGN_VIEW_X - 4, y - 2, right - CAMPAIGN_VIEW_X + 4,
                      CAMPAIGN_ROW_H, UI_BTN_HOT);

    snprintf(text, sizeof(text), "%d. %s", i + 1, entry->path);
    ui_label(&es->ui, CAMPAIGN_VIEW_X, y + 2, text);
    if (entry->loaded) {
        if (ui_text_field(&es->ui, CAMPAIGN_NAME_FIELD_ID + i, CAMPAIGN_NAME_X, y,
                          CAMPAIGN_NAME_W, entry->level.name,
                          (int)sizeof(entry->level.name))) {
            edited(c);
            editor_campaign_check(es);
        }
    } else {
        ui_label_color(&es->ui, CAMPAIGN_NAME_X, y + 2, "(cannot load)", UI_TEXT_DIM);
    }
    ui_label_color(&es->ui, CAMPAIGN_NAME_X + CAMPAIGN_NAME_W + 12, y + 2,
                   entry->available ? "ok" : entry->problem,
                   entry->available ? CAMPAIGN_GREEN : CAMPAIGN_RED);
}

/* The button row along the bottom of the view. */
static void draw_buttons(EditorState *es, EditorCampaign *c)
{
    static const char *labels[] = {
        "Up", "Down", "Remove", "Add level...", "Link in order", "Save", "Close"
    };
    int y = CAMPAIGN_BUTTONS_Y;

    for (int b = 0; b < 7; b++) {
        int x = CAMPAIGN_VIEW_X + b * CAMPAIGN_BUTTON_STEP;
        if (!ui_button(&es->ui, x, y, CAMPAIGN_BUTTON_W, 24, labels[b])) continue;
        switch (b) {
        case 0: (void)editor_campaign_move(es, c->selected, -1); break;
        case 1: (void)editor_campaign_move(es, c->selected, +1); break;
        case 2: (void)editor_campaign_remove(es, c->selected); break;
        case 3: {
            char picked[EDITOR_PATH_MAX];
            if (finish_edit(es) &&
                file_dialog_open(picked, (int)sizeof(picked)) == FILE_DIALOG_SELECTED)
                (void)editor_campaign_add(es, picked);
            break;
        }
        case 4: (void)editor_campaign_link_in_order(es); break;
        case 5: (void)editor_campaign_save(es); break;
        case 6: (void)editor_campaign_close(es, 0); break;
        }
        /* Close frees the view; nothing below may touch it. */
        return;
    }
}

void editor_campaign_render(EditorState *es)
{
    EditorCampaign *c = es ? es->campaign : NULL;
    char title[EDITOR_PATH_MAX + 64];
    int rows;

    if (!c) return;
    /* The verdicts follow every edit, typed names included. */
    editor_campaign_check(es);
    ui_panel(&es->ui, 0, TOOLBAR_H, CANVAS_W, CANVAS_H);

    snprintf(title, sizeof(title), "Campaign: %s (%zu levels)%s", c->manifest_path,
             c->catalog.count, editor_campaign_unsaved(es) ? " *" : "");
    ui_label(&es->ui, CAMPAIGN_VIEW_X, TOOLBAR_H + 10, title);
    if (c->problem[0])
        ui_label_color(&es->ui, CAMPAIGN_VIEW_X, TOOLBAR_H + 30, c->problem, CAMPAIGN_RED);
    else
        ui_label_color(&es->ui, CAMPAIGN_VIEW_X, TOOLBAR_H + 30,
                       "The start menu lists these in order; each level's next_phase "
                       "must name the next one.", UI_TEXT_DIM);
    ui_label_color(&es->ui, CAMPAIGN_VIEW_X, TOOLBAR_H + 50, "Level file", UI_TEXT_DIM);
    ui_label_color(&es->ui, CAMPAIGN_NAME_X, TOOLBAR_H + 50, "Name (menu label)", UI_TEXT_DIM);
    ui_label_color(&es->ui, CAMPAIGN_NAME_X + CAMPAIGN_NAME_W + 12, TOOLBAR_H + 50,
                   "Game's verdict", UI_TEXT_DIM);

    rows = visible_rows();
    if (c->scroll > (int)c->catalog.count - rows) c->scroll = (int)c->catalog.count - rows;
    if (c->scroll < 0) c->scroll = 0;
    for (int i = c->scroll; i < (int)c->catalog.count && i < c->scroll + rows; i++)
        draw_row(es, c, i, CAMPAIGN_ROWS_Y + (i - c->scroll) * CAMPAIGN_ROW_H);

    draw_buttons(es, c);
}
