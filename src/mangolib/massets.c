/*
 * massets.c — A table of loaded files with a holder count per entry.
 *
 * See massets.h for the idea and the rules. How to read this file:
 *   1. asset_load / asset_unload: the only two places that know how each
 *      kind of asset is loaded and freed.
 *   2. cache_acquire / cache_release: the reference counting, the same for
 *      every kind.
 *   3. The public functions: thin, typed wrappers around those two.
 */

#include "massets.h"

#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Loading and freeing one asset                                       */
/* ------------------------------------------------------------------ */

static void *asset_load(AssetKind kind, const char *path)
{
    switch (kind) {
    case ASSET_TEXTURE: return texture_load(path);
    case ASSET_SOUND:   return sound_load(path);
    case ASSET_MUSIC:   return music_load(path);
    case ASSET_FONT:    return font_load();
    }
    return NULL;
}

static void asset_unload(AssetKind kind, void *asset)
{
    switch (kind) {
    case ASSET_TEXTURE: texture_unload(asset); break;
    case ASSET_SOUND:   sound_unload(asset);   break;  /* stops its voices   */
    case ASSET_MUSIC:   music_unload(asset);   break;  /* stops it if playing */
    case ASSET_FONT:    font_unload(asset);    break;
    }
}

/* ------------------------------------------------------------------ */
/* The reference counting                                              */
/* ------------------------------------------------------------------ */

static CachedAsset *cache_find_path(AssetCache *cache, AssetKind kind, const char *path)
{
    for (int i = 0; i < cache->count; i++) {
        CachedAsset *entry = &cache->entries[i];
        if (entry->kind == kind && strcmp(entry->path, path) == 0) return entry;
    }
    return NULL;
}

static CachedAsset *cache_find_asset(AssetCache *cache, AssetKind kind, const void *asset)
{
    for (int i = 0; i < cache->count; i++) {
        CachedAsset *entry = &cache->entries[i];
        if (entry->kind == kind && entry->asset == asset) return entry;
    }
    return NULL;
}

/*
 * cache_acquire — Return the asset for (kind, path) and count one more holder.
 *
 * A hit costs a string compare per entry and reads no file. A miss loads the
 * file and adds an entry with one holder. A file that fails to load adds no
 * entry, so the caller's next attempt reads the disk again: putting a
 * missing file back fixes the next game without a restart.
 */
static void *cache_acquire(AssetCache *cache, AssetKind kind, const char *path)
{
    if (!path) return NULL;
    if (!cache) return asset_load(kind, path);  /* a private copy */

    CachedAsset *entry = cache_find_path(cache, kind, path);
    if (entry) {
        entry->holders++;
        return entry->asset;
    }

    /* The path is the key, so it must fit. Every LevelDef path does. */
    if (strlen(path) >= ASSET_CACHE_PATH_SIZE) {
        fprintf(stderr, "Asset cache: path too long to cache: %s\n", path);
        return NULL;
    }
    /* Full: drop what nobody holds before giving up. */
    if (cache->count >= ASSET_CACHE_MAX) asset_cache_trim(cache);
    if (cache->count >= ASSET_CACHE_MAX) {
        fprintf(stderr, "Asset cache: full (%d files held), cannot load %s\n", ASSET_CACHE_MAX, path);
        return NULL;
    }

    void *asset = asset_load(kind, path);
    if (!asset) return NULL;

    entry = &cache->entries[cache->count++];
    entry->kind = kind;
    memcpy(entry->path, path, strlen(path) + 1);
    entry->asset = asset;
    entry->holders = 1;
    return asset;
}

/*
 * cache_release — Count one holder fewer.
 *
 * The asset is not unloaded here even when the count reaches zero: it turns
 * idle and waits for asset_cache_trim (see massets.h for why). Returns
 * the entry when this release made it idle, NULL otherwise.
 */
static CachedAsset *cache_release(AssetCache *cache, AssetKind kind, void *asset)
{
    if (!asset) return NULL;
    if (!cache) {
        asset_unload(kind, asset);  /* the private copy acquire loaded */
        return NULL;
    }

    CachedAsset *entry = cache_find_asset(cache, kind, asset);
    /*
     * Both of these are bugs in the caller: releasing something this cache
     * never handed out, or releasing more often than acquiring. Say so and
     * change nothing. Unloading here could free an asset under a holder
     * that is still entitled to it.
     */
    if (!entry) {
        fprintf(stderr, "Asset cache: release of an asset it does not own\n");
        return NULL;
    }
    if (entry->holders <= 0) {
        fprintf(stderr, "Asset cache: %s released more often than acquired\n", entry->path);
        return NULL;
    }
    entry->holders--;
    return entry->holders == 0 ? entry : NULL;
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

Texture2D *asset_cache_texture_acquire(AssetCache *cache, const char *path)
{
    return cache_acquire(cache, ASSET_TEXTURE, path);
}

void asset_cache_texture_release(AssetCache *cache, Texture2D *texture)
{
    cache_release(cache, ASSET_TEXTURE, texture);
}

SoundEffect *asset_cache_sound_acquire(AssetCache *cache, const char *path)
{
    return cache_acquire(cache, ASSET_SOUND, path);
}

void asset_cache_sound_release(AssetCache *cache, SoundEffect *sound)
{
    cache_release(cache, ASSET_SOUND, sound);
}

MusicTrack *asset_cache_music_acquire(AssetCache *cache, const char *path)
{
    return cache_acquire(cache, ASSET_MUSIC, path);
}

void asset_cache_music_release(AssetCache *cache, MusicTrack *music)
{
    /*
     * An idle track must not keep playing: the level that started it is
     * gone. Unloading used to silence it; now the track stays loaded, so
     * stop it here. Stopping also rewinds, so the next holder's music_play
     * starts from the beginning, exactly as a newly loaded track would.
     */
    if (cache_release(cache, ASSET_MUSIC, music)) music_stop_track(music);
}

TextFont *asset_cache_font_acquire(AssetCache *cache)
{
    return cache_acquire(cache, ASSET_FONT, "");
}

void asset_cache_font_release(AssetCache *cache, TextFont *font)
{
    cache_release(cache, ASSET_FONT, font);
}

void asset_cache_trim(AssetCache *cache)
{
    if (!cache) return;
    /* Unload the idle entries and move the held ones down over the gaps.
     * Holders keep the asset pointer, not the entry's address, so moving an
     * entry inside the table does not disturb them. */
    int kept = 0;
    for (int i = 0; i < cache->count; i++) {
        CachedAsset *entry = &cache->entries[i];
        if (entry->holders > 0) cache->entries[kept++] = *entry;
        else asset_unload(entry->kind, entry->asset);
    }
    for (int i = kept; i < cache->count; i++) cache->entries[i] = (CachedAsset){0};
    cache->count = kept;
}

int asset_cache_clear(AssetCache *cache)
{
    if (!cache) return 0;
    int still_held = 0;
    /* Newest first: the reverse of the order the entries were loaded in. */
    for (int i = cache->count - 1; i >= 0; i--) {
        CachedAsset *entry = &cache->entries[i];
        if (entry->holders > 0) {
            fprintf(stderr, "Asset cache: %s still has %d holder(s) at shutdown\n",
                    entry->path[0] ? entry->path : "the font", entry->holders);
            still_held++;
        }
        asset_unload(entry->kind, entry->asset);
    }
    *cache = (AssetCache){0};
    return still_held;
}
