/*
 * asset_cache_test.c — Which files are read, and when they are freed.
 *
 * Two cases, both run by session-test (which opens the window and the audio
 * device before each case and links the counting builds of the loaders):
 *
 *   asset_cache_contract_test   the cache alone: holders, idle entries,
 *                               trim, a full table, a missing file.
 *   session_asset_reuse_test    a whole session: Replay, Retry, Level
 *                               Select -> Play and Next Level read from
 *                               disk only what the session does not hold.
 *
 * "Loads" below always means calls that really read a file, counted inside
 * texture_load, sound_load, music_load and font_load themselves, so a load
 * that went around the cache would be counted too.
 */
#include <stdio.h>
#include <string.h>

#include "test_paths.h"

#include "collision/collision_damage.h"  /* game_restart_after_game_over */
#include "core/app_session.h"
#include "core/game_resources.h"         /* DEFAULT_FLOOR_TILE_PATH */
#include "game.h"
#include "levels/level.h"
#include "levels/level_session.h"
#include "shared/asset_cache.h"
#include "shared/platform.h"             /* str_copy */

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

static int cache_idle_count(const AssetCache *cache)
{
    int idle = 0;
    for (int i = 0; i < cache->count; i++) idle += cache->entries[i].holders == 0;
    return idle;
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

/* ------------------------------------------------------------------ */
/* A whole session                                                     */
/* ------------------------------------------------------------------ */

#define MAX_LEVEL_FILES (MAX_BACKGROUND_LAYERS + MAX_FOG_TEXTURES + MAX_PLATFORMS + 8)

typedef struct {
    char paths[MAX_LEVEL_FILES][ASSET_CACHE_PATH_SIZE];
    int  count;
} FileSet;

static int file_set_has(const FileSet *set, const char *path)
{
    for (int i = 0; i < set->count; i++)
        if (strcmp(set->paths[i], path) == 0) return 1;
    return 0;
}

static void file_set_add(FileSet *set, const char *path)
{
    if (!path[0] || file_set_has(set, path) || set->count >= MAX_LEVEL_FILES) return;
    str_copy(set->paths[set->count++], path, ASSET_CACHE_PATH_SIZE);
}

/*
 * level_textures — Every image a game on this level holds in the cache,
 * worked out from the LevelDef alone (not from the cache under test): its
 * own layers, strip, floor and platform tiles, plus the three every game
 * holds.
 */
static void level_textures(const LevelDef *def, FileSet *set)
{
    set->count = 0;
    file_set_add(set, "assets/sprites/foregrounds/water.png");
    file_set_add(set, "assets/sprites/player/player.png");
    file_set_add(set, "assets/sprites/screens/hud_coins.png");
    for (int i = 0; i < def->background_layer_count; i++) file_set_add(set, def->background_layers[i].path);
    for (int i = 0; i < def->fog_layer_count; i++) file_set_add(set, def->fog_layers[i].path);
    if (def->foreground_layer_count > 0)
        file_set_add(set, def->foreground_layers[def->foreground_layer_count - 1].path);
    if (strcmp(def->floor_tile_path, DEFAULT_FLOOR_TILE_PATH) != 0) file_set_add(set, def->floor_tile_path);
    for (int i = 0; i < def->platform_count; i++) file_set_add(set, def->platforms[i].tile_path);
}

static int count_missing_from(const FileSet *set, const FileSet *other)
{
    int missing = 0;
    for (int i = 0; i < set->count; i++) missing += !file_set_has(other, set->paths[i]);
    return missing;
}

static const LevelDef *active_level(const AppSession *session)
{
    return (const LevelDef *)session->game->world.runtime.current_level;
}

/*
 * After a level has opened, the cache holds exactly that level's files:
 * each one held, none idle, nothing left over from the level before.
 */
static int expect_cache_holds_level(const char *step, const AppSession *session)
{
    static FileSet files;
    const LevelDef *def = active_level(session);
    const AssetCache *cache = &session->cache;
    int textures = 0, failed = 0;

    level_textures(def, &files);
    for (int i = 0; i < cache->count; i++) textures += cache->entries[i].kind == ASSET_TEXTURE;
    for (int i = 0; i < files.count; i++) {
        const CachedAsset *entry = cache_entry(cache, files.paths[i]);
        if (!entry || entry->holders < 1) {
            fprintf(stderr, "asset_cache_test: %s: %s is not held\n", step, files.paths[i]);
            failed = 1;
        }
    }
    if (expect_int(step, textures, files.count) || expect_int(step, cache_idle_count(cache), 0)) {
        fprintf(stderr, "asset_cache_test: %s: cache holds other files than the level's\n", step);
        failed = 1;
    }
    return failed;
}

/* Finish the level with Next Level chosen, as the completion screen does. */
static int go_to_next_level(AppSession *session)
{
    GameState *game = session->game;
    char next[sizeof(game->screen.completion.next_phase)];
    /* Copy the path: the level change frees the LevelDef it comes from. */
    str_copy(next, active_level(session)->next_phase, sizeof(next));
    str_copy(game->screen.completion.next_phase, next, sizeof(game->screen.completion.next_phase));
    game->screen.route = GAME_ROUTE_NEXT_LEVEL;
    session_frame(session);
    return !session->game || strcmp(session->game->world.level_path, next) != 0;
}

static int replay(AppSession *session)
{
    int opened = session->game_open_count;
    session->game->screen.route = GAME_ROUTE_REPLAY;
    session_frame(session);
    return !session->game || session->game_open_count != opened + 1;
}

int session_asset_reuse_test(void)
{
    const char *sandbox = "levels/00_sandbox_01.toml";
    static FileSet before, after;
    Counts live = live_now(), mark = loads_now();
    int failed = 0;

    AppSessionConfig config = {.level_path = sandbox};
    AppSession *session = session_create(&config);
    if (!session || !session->game) {
        session_destroy(&session);
        return 1;
    }

    /* The first game really reads its files; without this the zeros below
     * would prove nothing. The shared sprites and sounds, then the level's
     * own images, its music and the font. */
    level_textures(active_level(session), &before);
    Counts opened = loads_since(&mark);
    failed |= expect_int("level has its own files", before.count > 3, 1);
    int shared_textures = (int)(sizeof(TextureResources) / sizeof(Texture2D *));
    int shared_sounds = (int)(sizeof(AudioResources) / sizeof(SoundEffect *));
    failed |= expect_counts("first open reads every file once", opened,
                            (Counts){shared_textures + before.count, shared_sounds, 1, 1});
    failed |= expect_int("game borrows the session's cache", session->game->cache == &session->cache, 1);
    failed |= expect_cache_holds_level("first open", session);

    /* Replay: a new GameState on the same level. */
    Texture2D *sheet = session->game->world.player.texture;
    MusicTrack *track = session->game->world.music;
    if (replay(session)) goto broken;
    failed |= expect_no_loads("Replay reads nothing", &mark);
    failed |= expect_int("Replay reuses the player sheet", session->game->world.player.texture == sheet, 1);
    failed |= expect_int("Replay reuses the music track", session->game->world.music == track, 1);
    failed |= expect_int("Replay restarts the music",
                         track && IsMusicStreamPlaying(track->stream), 1);
    failed |= expect_cache_holds_level("Replay", session);

    /* Retry after game over: the same GameState starts the level again. */
    session->game->screen.game_over = 1;
    game_restart_after_game_over(session->game);
    failed |= expect_no_loads("Retry reads nothing", &mark);

    /* Level Select, then Play the same level. The menu loads its own logo,
     * sound and font; the game that follows loads nothing. */
    session->game->screen.route = GAME_ROUTE_LEVEL_SELECT;
    session_frame(session);
    if (!session->menu) goto broken;
    failed |= expect_int("the closed game's files wait, idle", cache_idle_count(&session->cache),
                         session->cache.count);
    failed |= expect_int("an idle track is silent", IsMusicStreamPlaying(track->stream), 0);
    (void)loads_since(&mark);
    str_copy(session->menu->selected_level_path, sandbox, sizeof(session->menu->selected_level_path));
    session->menu->route = MENU_ROUTE_PLAY;
    session_frame(session);
    if (!session->game) goto broken;
    failed |= expect_no_loads("going back into the level reads nothing", &mark);
    failed |= expect_cache_holds_level("back in the level", session);

    /* Next Level to a level with other scenery: only its new files are
     * read, and the old level's files are unloaded once it has opened. */
    int live_textures = texture_test_live();
    if (go_to_next_level(session)) goto broken;
    level_textures(active_level(session), &after);
    int added = count_missing_from(&after, &before), dropped = count_missing_from(&before, &after);
    failed |= expect_int("the next level adds files", added > 0, 1);
    failed |= expect_int("the next level drops files", dropped > 0, 1);
    failed |= expect_counts("Next Level reads only what it adds", loads_since(&mark),
                            (Counts){added, 0, 1, 0});
    failed |= expect_int("the old level's files are unloaded", texture_test_live(),
                         live_textures + added - dropped);
    failed |= expect_int("one music track is loaded", music_test_live(), live.music + 1);
    failed |= expect_cache_holds_level("Next Level", session);

    if (replay(session)) goto broken;
    failed |= expect_no_loads("Replay of the next level reads nothing", &mark);

    /* Next Level to a level with the same scenery and music: nothing. */
    before = after;
    if (go_to_next_level(session)) goto broken;
    level_textures(active_level(session), &after);
    failed |= expect_int("this level shares every file", count_missing_from(&after, &before), 0);
    failed |= expect_no_loads("Next Level with the same files reads nothing", &mark);
    failed |= expect_cache_holds_level("second Next Level", session);

    /* Level Select, then Play a different level: the same rule through a
     * new GameState. Only the files the chosen level adds are read. */
    before = after;
    session->game->screen.route = GAME_ROUTE_LEVEL_SELECT;
    session_frame(session);
    if (!session->menu) goto broken;
    (void)loads_since(&mark);
    str_copy(session->menu->selected_level_path, sandbox, sizeof(session->menu->selected_level_path));
    session->menu->route = MENU_ROUTE_PLAY;
    session_frame(session);
    if (!session->game) goto broken;
    level_textures(active_level(session), &after);
    added = count_missing_from(&after, &before);
    failed |= expect_int("the chosen level adds files", added > 0, 1);
    failed |= expect_counts("Play of another level reads only what it adds", loads_since(&mark),
                            (Counts){added, 0, 1, 0});
    failed |= expect_cache_holds_level("Play of another level", session);

    /* The end of the session unloads everything, each thing once. */
    session_destroy(&session);
    failed |= expect_counts("session end leaves nothing loaded", live_now(), live);
    return failed;

broken:
    fprintf(stderr, "asset_cache_test: a session step did not open its screen\n");
    session_destroy(&session);
    return 1;
}

/*
 * A GameState without a session has no cache: it loads its files itself
 * and frees them itself, through a level change as well.
 */
int game_without_session_frees_its_files_test(void)
{
    static GameState gs;
    Counts live = live_now();
    int failed = 0;

    memset(&gs, 0, sizeof(gs));
    str_copy(gs.world.level_path, "levels/00_sandbox_01.toml", sizeof(gs.world.level_path));
    if (game_init(&gs) != 0) return 1;
    failed |= expect_int("no session, no cache", gs.cache == NULL, 1);
    failed |= expect_int("it owns its shared sprites", gs.owns_assets, 1);
    failed |= expect_int("next level loads", game_load_next_phase(&gs), 0);
    game_cleanup(&gs);
    failed |= expect_counts("the game freed what it loaded", live_now(), live);
    return failed;
}
