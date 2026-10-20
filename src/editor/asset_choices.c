/*
 * asset_choices.c — The asset files the editor's dropdowns offer.
 */
#include "asset_choices.h"

#include <string.h> /* strrchr */

#define BG "assets/sprites/backgrounds/"
#define FG "assets/sprites/foregrounds/"
#define LV "assets/sprites/levels/"
#define SND "assets/sounds/levels/"

/* The original twelve first, then the volcanic and smoke sets. */
static const char *bg_names[] = {
    "sky_blue.png", "sky_blue_lightened.png", "castle_pillars.png", "forest_leafs.png",
    "clouds_bg.png", "glacial_mountains.png", "glacial_mountains_lightened.png",
    "clouds_mg_3.png", "clouds_mg_2.png", "clouds_lonely.png", "clouds_mg_1.png",
    "clouds_mg_1_lightened.png", "sky_fire.png", "sky_fire_lightened.png",
    "volcanic_mountains.png", "volcanic_mountains_lightened.png", "smoke_bg.png",
    "smoke_lonely.png", "smoke_mg_3.png", "smoke_mg_2.png", "smoke_mg_1.png",
    "smoke_mg_1_lightened.png",
};
static const char *bg_paths[] = {
    BG "sky_blue.png", BG "sky_blue_lightened.png", BG "castle_pillars.png",
    BG "forest_leafs.png", BG "clouds_bg.png", BG "glacial_mountains.png",
    BG "glacial_mountains_lightened.png", BG "clouds_mg_3.png", BG "clouds_mg_2.png",
    BG "clouds_lonely.png", BG "clouds_mg_1.png", BG "clouds_mg_1_lightened.png",
    BG "sky_fire.png", BG "sky_fire_lightened.png", BG "volcanic_mountains.png",
    BG "volcanic_mountains_lightened.png", BG "smoke_bg.png", BG "smoke_lonely.png",
    BG "smoke_mg_3.png", BG "smoke_mg_2.png", BG "smoke_mg_1.png",
    BG "smoke_mg_1_lightened.png",
};

static const char *fg_names[] = {
    "fog_1.png", "fog_2.png", "water.png", "clouds.png", "lava.png",
};
static const char *fg_paths[] = {
    FG "fog_1.png", FG "fog_2.png", FG "water.png", FG "clouds.png", FG "lava.png",
};

static const char *fog_names[] = {
    "fog_1.png", "fog_2.png", "fog_fire_1.png", "fog_fire_2.png", "smoke.png",
};
static const char *fog_paths[] = {
    FG "fog_1.png", FG "fog_2.png", FG "fog_fire_1.png", FG "fog_fire_2.png", FG "smoke.png",
};

static const char *floor_names[] = {
    "grass_tileset.png", "brick_tileset.png", "cloud_tileset.png",
    "grass_rock_tileset.png", "leaf_tileset.png", "stone_tileset.png",
};
static const char *floor_paths[] = {
    LV "grass_tileset.png", LV "brick_tileset.png", LV "cloud_tileset.png",
    LV "grass_rock_tileset.png", LV "leaf_tileset.png", LV "stone_tileset.png",
};

static const char *platform_names[] = {
    "(default)", "stone_platform.png", "leaf_platform.png", "brick_platform.png",
    "grass_platform.png", "grass_rock_platform.png",
};
static const char *platform_paths[] = {
    "", LV "stone_platform.png", LV "leaf_platform.png", LV "brick_platform.png",
    LV "grass_platform.png", LV "grass_rock_platform.png",
};

static const char *music_names[] = { "(none)", "water.qoa", "lava.qoa", "winds.qoa" };
static const char *music_paths[] = { "", SND "water.qoa", SND "lava.qoa", SND "winds.qoa" };

#define CHOICES(names, paths) { names, paths, (int)(sizeof(paths) / sizeof(paths[0])) }
_Static_assert(sizeof(bg_names) == sizeof(bg_paths), "one name per background");
_Static_assert(sizeof(fg_names) == sizeof(fg_paths), "one name per foreground");
_Static_assert(sizeof(fog_names) == sizeof(fog_paths), "one name per fog image");
_Static_assert(sizeof(floor_names) == sizeof(floor_paths), "one name per floor tileset");
_Static_assert(sizeof(platform_names) == sizeof(platform_paths), "one name per platform tile");
_Static_assert(sizeof(music_names) == sizeof(music_paths), "one name per sound");

static const AssetChoices choices[ASSET_CHOICES_COUNT] = {
    [ASSET_CHOICES_BACKGROUNDS]    = CHOICES(bg_names, bg_paths),
    [ASSET_CHOICES_FOREGROUNDS]    = CHOICES(fg_names, fg_paths),
    [ASSET_CHOICES_FOG]            = CHOICES(fog_names, fog_paths),
    [ASSET_CHOICES_FLOOR_TILES]    = CHOICES(floor_names, floor_paths),
    [ASSET_CHOICES_PLATFORM_TILES] = CHOICES(platform_names, platform_paths),
    [ASSET_CHOICES_MUSIC]          = CHOICES(music_names, music_paths),
};

const AssetChoices *editor_asset_choices(AssetChoicesKind kind)
{
    if ((int)kind < 0 || kind >= ASSET_CHOICES_COUNT) return NULL;
    return &choices[kind];
}

const char *editor_asset_unlisted_name(const char *path)
{
    if (!path || path[0] == '\0') return "---";
    const char *slash = strrchr(path, '/');
    const char *back = strrchr(path, '\\');
    if (back && (!slash || back > slash)) slash = back;
    return slash && slash[1] != '\0' ? slash + 1 : path;
}
