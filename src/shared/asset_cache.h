/*
 * asset_cache.h — Files loaded once and shared by counting who holds them.
 *
 * A level names its own images and music: background layers, fog, the water
 * strip, a floor tileset, platform tiles, a music track. The player sheet,
 * the HUD coin icon and the HUD font are needed by every game. A Replay
 * closes one GameState and opens another on the same level, so loading these
 * with the game and freeing them with it read the same files again each time.
 * The start menu's logo, confirm sound and font are in the same position:
 * every return to Level Select opens a new menu.
 *
 * The cache keeps one entry per file path. Two words describe an entry:
 *
 *   holders  how many callers acquired it and have not released it yet.
 *            This is a reference count. The cache owns the texture; a
 *            holder owns one reference, not the texture itself.
 *   idle     an entry with holders == 0. It stays loaded, because the next
 *            game very likely wants it (a Replay wants all of them).
 *
 * Idle entries are unloaded by asset_cache_trim. The game calls it right
 * after a level has finished loading: by then the new level holds everything
 * it needs, so whatever is still idle belonged only to the previous level.
 * The cache therefore never keeps more than the current level's files, plus
 * the previous level's for the short time between closing one and opening
 * the next. It does not grow with the number of levels visited.
 *
 * Compare this with GameAssets (game_assets.h), the sprites every level
 * shares. Those are simply borrowed: AppSession owns them for the whole run
 * and a GameState copies the pointers, with no counting, because their
 * lifetime never depends on who is using them. Counting is needed here
 * because these files should go away once no level uses them.
 *
 * Rules for callers:
 *   - Every successful acquire is paired with exactly one release of the
 *     pointer it returned. Clear your copy of the pointer after releasing.
 *   - Never call texture_unload, sound_unload, music_unload or font_unload
 *     on something you acquired: that would free it under the other holders.
 *   - Acquiring returns NULL when the file cannot be loaded. Nothing is
 *     remembered about the failure, so the next acquire tries the file
 *     again. Releasing NULL does nothing.
 *   - cache may be NULL: acquire then loads a private copy and release
 *     unloads it. A GameState started without a session works this way.
 */
#pragma once

#include "audio.h"     /* MusicTrack, SoundEffect */
#include "graphics.h"  /* Texture2D */
#include "text.h"      /* TextFont */

/* Same capacity as every asset path in a LevelDef (char[64]). */
#define ASSET_CACHE_PATH_SIZE 64

/*
 * How many entries the cache can hold. One game holds at most 51 (8
 * background layers, 4 fog layers, 32 platform tiles, a floor tileset, two
 * water strips, the player sheet, the HUD icon, the music and the font;
 * level_resources.c checks this at compile time), and the start menu's logo
 * and confirm sound make 53. The rest is room for the previous level's idle
 * entries. When the table is full, acquire unloads the idle entries to make
 * room.
 */
#define ASSET_CACHE_MAX 64

typedef enum {
    ASSET_TEXTURE,
    ASSET_SOUND,
    ASSET_MUSIC,
    ASSET_FONT
} AssetKind;

typedef struct {
    AssetKind kind;
    char      path[ASSET_CACHE_PATH_SIZE];  /* the key; "" for the font  */
    void     *asset;    /* Texture2D *, SoundEffect *, MusicTrack * or
                           TextFont *, by kind; never NULL in a used entry */
    int       holders;  /* acquires not yet released; 0 = idle           */
} CachedAsset;

/*
 * AssetCache — the table. A zeroed AssetCache is a valid empty cache, so its
 * owner needs no init call: AppSession holds one by value and calloc zeroes it.
 */
typedef struct AssetCache {
    CachedAsset entries[ASSET_CACHE_MAX];
    int         count;  /* used entries, always the first `count` slots */
} AssetCache;

/* The texture for path: the cached one, or a newly loaded one. */
Texture2D *asset_cache_texture_acquire(AssetCache *cache, const char *path);
void asset_cache_texture_release(AssetCache *cache, Texture2D *texture);

/* The sound effect for path. Releasing does not stop a sound that is
 * playing (unlike music, it is short and does not loop); a holder that
 * wants silence calls sound_stop first. */
SoundEffect *asset_cache_sound_acquire(AssetCache *cache, const char *path);
void asset_cache_sound_release(AssetCache *cache, SoundEffect *sound);

/* The music stream for path. A track stops playing when its last holder
 * releases it; it stays loaded, rewound to its start, until the next trim. */
MusicTrack *asset_cache_music_acquire(AssetCache *cache, const char *path);
void asset_cache_music_release(AssetCache *cache, MusicTrack *music);

/* The text font handle (text.h). There is one font, so it has no path. */
TextFont *asset_cache_font_acquire(AssetCache *cache);
void asset_cache_font_release(AssetCache *cache, TextFont *font);

/* Unload every idle entry. Entries somebody holds are untouched. */
void asset_cache_trim(AssetCache *cache);

/*
 * Unload everything and empty the cache, when its owner shuts down. By then
 * every holder must have released; an entry still held is reported on
 * stderr and unloaded anyway (the GPU and the audio device are about to
 * close). Returns how many entries were still held: 0 when all is well.
 */
int asset_cache_clear(AssetCache *cache);
