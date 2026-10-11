/*
 * asset_cache_test.c — Which files are read, and when they are freed.
 *
 * asset_cache_contract_test checks the cache alone: holders, idle entries,
 * trim, a full table, a missing file. session-test runs it (that binary
 * opens the window and the audio device before each case and links the
 * counting builds of the loaders).
 *
 * "Loads" below always means calls that really read a file, counted inside
 * texture_load, sound_load, music_load and font_load themselves.
 */
#include <stdio.h>
#include <string.h>

#include "test_paths.h"

#include "shared/asset_cache.h"

#define COIN_PNG  "assets/sprites/collectibles/coin.png"
#define STAR_PNG  "assets/sprites/collectibles/star_yellow.png"
#define MUSIC_WAV "assets/sounds/levels/water.wav"

typedef struct {
    int textures, sounds, music, fonts;
} Counts;

static Counts loads_now(void)
{
    return (Counts){texture_test_loads(), sound_test_loads(), music_test_loads(), font_test_loads()};
}

static Counts live_now(void)
{
    return (Counts){texture_test_live(), sound_test_live(), music_test_live(), font_test_live()};
}

static int expect_int(const char *name, int actual, int expected)
{
    if (actual == expected) return 0;
    fprintf(stderr, "asset_cache_test: %s got %d expected %d\n", name, actual, expected);
    return 1;
}

static int expect_counts(const char *name, Counts got, Counts want)
{
    if (got.textures == want.textures && got.sounds == want.sounds &&
        got.music == want.music && got.fonts == want.fonts) return 0;
    fprintf(stderr, "asset_cache_test: %s got %d textures, %d sounds, %d music, %d fonts; "
            "expected %d, %d, %d, %d\n", name, got.textures, got.sounds, got.music, got.fonts,
            want.textures, want.sounds, want.music, want.fonts);
    return 1;
}

/* Files loaded since *mark; then move the mark to now. */
static Counts loads_since(Counts *mark)
{
    Counts now = loads_now();
    Counts delta = {now.textures - mark->textures, now.sounds - mark->sounds,
                    now.music - mark->music, now.fonts - mark->fonts};
    *mark = now;
    return delta;
}

static int expect_no_loads(const char *name, Counts *mark)
{
    return expect_counts(name, loads_since(mark), (Counts){0, 0, 0, 0});
}

static const CachedAsset *cache_entry(const AssetCache *cache, const char *path)
{
    for (int i = 0; i < cache->count; i++)
        if (strcmp(cache->entries[i].path, path) == 0) return &cache->entries[i];
    return NULL;
}

/* ------------------------------------------------------------------ */
/* The cache alone                                                     */
/* ------------------------------------------------------------------ */

static int holders_are_counted_and_idle_entries_stay_loaded(void)
{
    AssetCache cache = {0};
    Counts live = live_now(), mark = loads_now();
    int failed = 0;

    Texture2D *first = asset_cache_texture_acquire(&cache, COIN_PNG);
    Texture2D *second = asset_cache_texture_acquire(&cache, COIN_PNG);
    failed |= expect_int("acquire loads the file", first != NULL, 1);
    failed |= expect_int("second acquire returns the same texture", second == first, 1);
    failed |= expect_counts("two acquires, one load", loads_since(&mark), (Counts){1, 0, 0, 0});
    failed |= expect_int("one entry", cache.count, 1);
    failed |= expect_int("two holders", cache.entries[0].holders, 2);

    asset_cache_texture_release(&cache, first);
    failed |= expect_int("one holder left", cache.entries[0].holders, 1);
    asset_cache_texture_release(&cache, second);
    failed |= expect_int("idle, not removed", cache.count, 1);
    failed |= expect_int("idle has no holders", cache.entries[0].holders, 0);
    failed |= expect_int("idle texture is still loaded", texture_test_live(), live.textures + 1);

    /* Releasing once too often, or something the cache never handed out,
     * is reported and changes nothing: no double unload. */
    Texture2D foreign = {0};
    asset_cache_texture_release(&cache, first);
    asset_cache_texture_release(&cache, &foreign);
    asset_cache_texture_release(&cache, NULL);
    failed |= expect_int("over-release keeps the count at zero", cache.entries[0].holders, 0);
    failed |= expect_int("over-release unloads nothing", texture_test_live(), live.textures + 1);

    /* The next holder gets the idle texture back without a load. */
    Texture2D *again = asset_cache_texture_acquire(&cache, COIN_PNG);
    failed |= expect_int("idle entry is reused", again == first, 1);
    failed |= expect_no_loads("reuse reads no file", &mark);

    /* Trim leaves a held entry alone and unloads an idle one. */
    Texture2D *star = asset_cache_texture_acquire(&cache, STAR_PNG);
    asset_cache_texture_release(&cache, again);
    asset_cache_trim(&cache);
    failed |= expect_int("trim keeps the held entry", cache.count, 1);
    failed |= expect_int("the held entry moved down", strcmp(cache.entries[0].path, STAR_PNG), 0);
    failed |= expect_int("its texture is untouched", cache.entries[0].asset == star, 1);
    failed |= expect_int("trim unloaded the idle one", texture_test_live(), live.textures + 1);
    asset_cache_texture_release(&cache, star);

    failed |= expect_int("nothing held at shutdown", asset_cache_clear(&cache), 0);
    failed |= expect_int("clear empties the table", cache.count, 0);
    failed |= expect_counts("everything unloaded once", live_now(), live);
    return failed;
}

static int music_and_font_are_shared_too(void)
{
    AssetCache cache = {0};
    Counts live = live_now(), mark = loads_now();
    int failed = 0;

    MusicTrack *music = asset_cache_music_acquire(&cache, MUSIC_WAV);
    TextFont *font = asset_cache_font_acquire(&cache);
    TextFont *font_again = asset_cache_font_acquire(&cache);
    if (!music || !font) {
        fprintf(stderr, "asset_cache_test: music or font did not load\n");
        asset_cache_clear(&cache);
        return 1;
    }
    failed |= expect_int("one font handle for both holders", font_again == font, 1);
    failed |= expect_counts("one music file, one font", loads_since(&mark), (Counts){0, 0, 1, 1});

    /* Releasing the last holder silences the track but keeps it loaded. */
    music_play(music);
    failed |= expect_int("track plays", IsMusicStreamPlaying(music->stream), 1);
    asset_cache_music_release(&cache, music);
    failed |= expect_int("idle track is stopped", IsMusicStreamPlaying(music->stream), 0);
    failed |= expect_int("idle track is still loaded", music_test_live(), live.music + 1);
    failed |= expect_int("same track for the next holder",
                         asset_cache_music_acquire(&cache, MUSIC_WAV) == music, 1);
    failed |= expect_no_loads("reacquiring music and font reads nothing", &mark);

    /* A texture and a music file may not be confused by their kind. */
    failed |= expect_int("a track is not a texture",
                         asset_cache_texture_acquire(&cache, MUSIC_WAV) == NULL, 1);

    asset_cache_music_release(&cache, music);
    asset_cache_font_release(&cache, font);
    asset_cache_font_release(&cache, font_again);
    asset_cache_trim(&cache);
    failed |= expect_int("trim empties an all-idle cache", cache.count, 0);
    failed |= expect_counts("music and font unloaded once", live_now(), live);
    return failed;
}

/*
 * A file that cannot be loaded is not remembered: the next acquire reads
 * the disk again, so putting the file back fixes the next game.
 */
static int missing_file_is_tried_again(void)
{
    const char *path = TEST_OUT "asset-cache-retry.png";
    AssetCache cache = {0};
    Counts live = live_now();
    int failed = 0;

    if (strlen(path) >= ASSET_CACHE_PATH_SIZE) return 0;  /* an unusually long OUTDIR */
    remove(path);
    failed |= expect_int("missing file gives NULL", asset_cache_texture_acquire(&cache, path) == NULL, 1);
    failed |= expect_int("missing file adds no entry", cache.count, 0);

    int size = 0;
    unsigned char *bytes = LoadFileData(COIN_PNG, &size);
    if (!bytes || !SaveFileData(path, bytes, size)) {
        fprintf(stderr, "asset_cache_test: cannot write %s\n", path);
        UnloadFileData(bytes);
        return 1;
    }
    UnloadFileData(bytes);
    Texture2D *texture = asset_cache_texture_acquire(&cache, path);
    failed |= expect_int("the file is read once it exists", texture != NULL, 1);
    asset_cache_texture_release(&cache, texture);
    remove(path);

    /* A path longer than the key is refused instead of being cut short. */
    char too_long[ASSET_CACHE_PATH_SIZE + 8];
    memset(too_long, 'a', sizeof(too_long) - 1);
    too_long[sizeof(too_long) - 1] = '\0';
    failed |= expect_int("too long a path gives NULL", asset_cache_texture_acquire(&cache, too_long) == NULL, 1);
    failed |= expect_int("NULL path gives NULL", asset_cache_texture_acquire(&cache, NULL) == NULL, 1);

    asset_cache_clear(&cache);
    failed |= expect_counts("nothing left behind", live_now(), live);
    return failed;
}

/* The i-th of many spellings of one file: "./" repeated in three places. */
static void spelled_path(int i, char *out, size_t size)
{
    static const char *dots[] = {"", "./", "././", "./././"};
    snprintf(out, size, "assets/%ssprites/%scollectibles/%scoin.png",
             dots[i % 4], dots[(i / 4) % 4], dots[(i / 16) % 4]);
}

static int full_table_makes_room_from_idle_entries(void)
{
    static AssetCache cache;
    static Texture2D *held[ASSET_CACHE_MAX];
    char path[ASSET_CACHE_PATH_SIZE];
    Counts live = live_now();
    int failed = 0;

    cache = (AssetCache){0};
    _Static_assert(ASSET_CACHE_MAX <= 64, "spelled_path makes 64 different paths");
    for (int i = 0; i < ASSET_CACHE_MAX; i++) {
        spelled_path(i, path, sizeof(path));
        held[i] = asset_cache_texture_acquire(&cache, path);
        if (!held[i]) {
            fprintf(stderr, "asset_cache_test: could not load %s\n", path);
            asset_cache_clear(&cache);
            return 1;
        }
    }
    failed |= expect_int("table is full", cache.count, ASSET_CACHE_MAX);

    /* Everything is held: one more file is refused, and nothing leaks. */
    failed |= expect_int("full table refuses another file",
                         asset_cache_texture_acquire(&cache, STAR_PNG) == NULL, 1);
    failed |= expect_int("refusal loaded nothing", texture_test_live(), live.textures + ASSET_CACHE_MAX);

    /* One idle entry is enough: acquire trims it away and takes its place. */
    asset_cache_texture_release(&cache, held[7]);
    held[7] = NULL;
    Texture2D *star = asset_cache_texture_acquire(&cache, STAR_PNG);
    failed |= expect_int("an idle entry made room", star != NULL, 1);
    failed |= expect_int("table is full again", cache.count, ASSET_CACHE_MAX);
    spelled_path(7, path, sizeof(path));
    failed |= expect_int("the idle entry is gone", cache_entry(&cache, path) == NULL, 1);
    spelled_path(8, path, sizeof(path));
    failed |= expect_int("a held entry kept its texture",
                         cache_entry(&cache, path) && cache_entry(&cache, path)->asset == held[8], 1);

    /* Shutting down with holders is reported, and still unloads each once.
     * Two stay held here: held[8] and the star. */
    for (int i = 0; i < ASSET_CACHE_MAX; i++)
        if (i != 8) asset_cache_texture_release(&cache, held[i]);
    failed |= expect_int("clear reports the entries still held", asset_cache_clear(&cache), 2);
    failed |= expect_counts("clear unloaded all of them", live_now(), live);
    return failed;
}

/* Without a cache, acquire and release are a plain load and unload. */
static int null_cache_loads_private_copies(void)
{
    Counts live = live_now(), mark = loads_now();
    int failed = 0;

    Texture2D *one = asset_cache_texture_acquire(NULL, COIN_PNG);
    Texture2D *two = asset_cache_texture_acquire(NULL, COIN_PNG);
    TextFont *font = asset_cache_font_acquire(NULL);
    MusicTrack *music = asset_cache_music_acquire(NULL, MUSIC_WAV);
    failed |= expect_int("private copies are separate", one && two && one != two, 1);
    failed |= expect_counts("each acquire loads", loads_since(&mark), (Counts){2, 0, 1, 1});
    asset_cache_texture_release(NULL, one);
    asset_cache_texture_release(NULL, two);
    asset_cache_font_release(NULL, font);
    asset_cache_music_release(NULL, music);
    asset_cache_trim(NULL);
    failed |= expect_int("clearing no cache is harmless", asset_cache_clear(NULL), 0);
    failed |= expect_counts("each release unloads", live_now(), live);
    return failed;
}

int asset_cache_contract_test(void)
{
    return holders_are_counted_and_idle_entries_stay_loaded() |
           music_and_font_are_shared_too() |
           missing_file_is_tried_again() |
           full_table_makes_room_from_idle_entries() |
           null_cache_loads_private_copies();
}
