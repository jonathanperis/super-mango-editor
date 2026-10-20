/*
 * asset_choices.h — The asset files the editor's dropdowns offer.
 *
 * One table per dropdown (layer images, floor and platform tiles, level
 * music), so the property panels and the tests read the same lists.
 * editor_validation_test checks that each list names every file in its
 * asset folder, so a new image cannot be left out again.
 */
#pragma once

typedef enum {
    ASSET_CHOICES_BACKGROUNDS,    /* parallax background layers   */
    ASSET_CHOICES_FOREGROUNDS,    /* strips drawn over the floor  */
    ASSET_CHOICES_FOG,            /* drifting fog overlays        */
    ASSET_CHOICES_FLOOR_TILES,    /* the level floor's tileset    */
    ASSET_CHOICES_PLATFORM_TILES, /* a platform's own tile image  */
    ASSET_CHOICES_MUSIC,          /* the level's background sound */
    ASSET_CHOICES_COUNT
} AssetChoicesKind;

/* names[i] is shown in the dropdown for paths[i]; an empty path ("") is
 * the "(none)" or "(default)" choice. */
typedef struct {
    const char **names;
    const char **paths;
    int          count;
} AssetChoices;

const AssetChoices *editor_asset_choices(AssetChoicesKind kind);

/* The file name the dropdown shows for a path no choice names (one typed
 * into the TOML by hand): its last path component, or "---" when empty. */
const char *editor_asset_unlisted_name(const char *path);
