#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "collectibles/coin.h"  /* MAX_COINS */
#include "core/app_session.h"
#include "collision/collision_damage.h"  /* game_restart_after_game_over */
#include "core/game_checkpoint.h"
#include "core/game_experiment.h"
#include "core/game_ghost.h"
#include "core/game_overlay.h"
#include "core/game_resume.h"
#include "core/game_profile.h"
#include "shared/platform.h"  /* clock_millis, preference_path_at */
#include "shared/serializer_io.h"
#include "input/game_input.h"
#include "screens/settings_menu.h"
#include "test_paths.h"  /* TEST_OUT scratch directory */
#ifdef _WIN32
#include <direct.h>
#endif
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

#define CHECK(condition) do { if (!(condition)) { fprintf(stderr,"profile test failed: %s:%d: %s\n",__FILE__,__LINE__,#condition); goto fail; } } while (0)

/* Only the session test's audio object redirects this public raylib call.
 * Forward to the real device while checking the exact applied mute value. */
float test_last_music_volume = -1;
static int applied_width, applied_height;
void test_SetWindowSize(int width, int height)
{
    applied_width = width;
    applied_height = height;
    SetWindowSize(width, height);
}
void test_SetMusicVolume(Music music, float volume)
{
    test_last_music_volume = volume;
    SetMusicVolume(music, volume);
}

static int codec_and_storage(void)
{
    GameProfile *a = calloc(1,sizeof(*a)), *b = calloc(1,sizeof(*b));
    GameProfileData *decoded = calloc(1,sizeof(*decoded));
    char *text = malloc(PROFILE_TEXT_MAX);
    char path[160];
    char lock_path[176];
    snprintf(path,sizeof(path),TEST_OUT "profile-test-%llu.toml",(unsigned long long)clock_millis());
    snprintf(lock_path,sizeof(lock_path),"%s.lock",path);
    CHECK(a && b && decoded && text);
    game_profile_init(a); game_profile_init(b);
    a->data.settings.keys[BIND_JUMP] = 13; /* Existing version-1 wire ID for J. */
    a->data.settings.buttons[BIND_RUN] = PAD_X;
    a->data.settings.dead_zone = 10000;
    /* Keys follow the shared level reference rule (level_ref.h). */
    CHECK(game_profile_key_valid("levels/café.toml"));
    CHECK(!game_profile_key_valid("levels/labs/01_collision.toml"));
    CHECK(!game_profile_key_valid("levels/con.toml") && !game_profile_key_valid("levels/a:b.toml"));
    game_profile_select(a,"levels/café.toml");
    CHECK(game_profile_record(a,"levels/café.toml",100,3,12.5f)==0);
    CHECK(game_profile_record(a,"levels/café.toml",90,2,20.0f)==0);
    CHECK(game_profile_record(a,"levels/café.toml",150,1,10.0f)==0);
    CHECK(game_profile_encode(&a->data,text,PROFILE_TEXT_MAX)==0);
    CHECK(game_profile_decode(decoded,text)==0);
    CHECK(decoded->settings.keys[BIND_JUMP]==13 && decoded->count==1);
    CHECK(input_key_from_binding(13)==KEY_J && !game_settings_has_unavailable_binding(&decoded->settings));
    a->data.settings.keys[BIND_LEFT] = 261; /* Legacy AudioPlay. */
    a->data.settings.buttons[BIND_LEFT] = 16; /* Legacy Paddle1. */
    CHECK(game_profile_encode(&a->data,text,PROFILE_TEXT_MAX)==0 && game_profile_decode(decoded,text)==0);
    CHECK(decoded->settings.keys[BIND_LEFT]==261 && decoded->settings.buttons[BIND_LEFT]==16);
    CHECK(game_settings_has_unavailable_binding(&decoded->settings));
    CHECK(input_key_from_binding(261)==KEY_NULL && input_pad_button(16)==0);
    CHECK(decoded->levels[0].best_score==150 && decoded->levels[0].best_coins==3 && decoded->levels[0].best_time==10);
    const char *bad[] = {"format_version=3\n", "format_version=0\n", "format_version=1\nmuted=2\n", "format_version=1\ndead_zone=30000\n",
        "format_version=1\nkeys=[4,4,26,22,44,225]\n", "format_version=1\nwindow_scale=nan\n",
        "format_version=1\nunknown=1\n", "format_version=1\nlast_level=\"levels/a\\u0000.toml\"\n"};
    for (size_t i=0;i<sizeof(bad)/sizeof(bad[0]);i++) {
        CHECK(game_profile_decode(decoded,bad[i])==-1);
        CHECK(decoded->count==1 && decoded->settings.keys[BIND_JUMP]==13);
    }
    /* A profile saved before the inspector keys were reserved: Jump on F2
     * goes back to its default (Space) and the rest of the profile loads. */
    CHECK(game_profile_decode(decoded,"format_version=1\nkeys=[4,7,26,22,59,225]\n")==0);
    CHECK(decoded->settings.keys[BIND_JUMP]==BINDING_KEY_SPACE && decoded->settings.keys[BIND_LEFT]==BINDING_KEY_A);
    /* Down already holds Space, so every key returns to its default. */
    CHECK(game_profile_decode(decoded,"format_version=1\nkeys=[4,7,26,44,46,225]\n")==0);
    CHECK(decoded->settings.keys[BIND_DOWN]==BINDING_KEY_S && decoded->settings.keys[BIND_JUMP]==BINDING_KEY_SPACE);
    CHECK(game_settings_valid(&decoded->settings));
    CHECK(game_profile_open(a,path)==0 && game_profile_save(a)==0);
    CHECK(game_profile_open(b,path)==0);
    a->data.settings.muted=1;
    CHECK(game_profile_save(a)==0);
    b->data.settings.music_volume=0;
    CHECK(game_profile_save(b)==-1 && b->error); /* stale writer */
    SerializerFileFingerprint before, after;
    CHECK(serializer_fingerprint_utf8(path,&before)==1);
#ifdef _WIN32
    wchar_t *wide = serializer_utf8_to_wide(lock_path);
    CHECK(wide);
    HANDLE lock = CreateFileW(wide, GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    free(wide); CHECK(lock != INVALID_HANDLE_VALUE);
    int busy = game_profile_save(a);
    CloseHandle(lock);
#else
    int lock = open(lock_path,O_RDWR|O_CREAT,0600);
    CHECK(lock >= 0);
    int acquired = flock(lock,LOCK_EX|LOCK_NB);
    int busy = acquired == 0 ? game_profile_save(a) : 0;
    close(lock);
#endif
    CHECK(busy == -1);
    CHECK(serializer_fingerprint_utf8(path,&after)==1 && serializer_fingerprint_equal(&before,&after));
    serializer_test_set_failure(SERIALIZER_TEST_FAILURE_FLUSH);
    CHECK(game_profile_save(a)==-1);
    serializer_test_set_failure(SERIALIZER_TEST_FAILURE_NONE);
    CHECK(serializer_fingerprint_utf8(path,&after)==1 && serializer_fingerprint_equal(&before,&after));
    /* A Windows replace that moved the old profile away but not the new one
     * in: the temp file is the only copy, so it must survive, the status
     * must name it, and this run must stop writing. */
    {
        a->error = 0;
        serializer_test_set_failure(SERIALIZER_TEST_FAILURE_REPLACE_STRANDED);
        CHECK(game_profile_save(a)==-1);
        serializer_test_set_failure(SERIALIZER_TEST_FAILURE_NONE);
        const char *kept = strstr(a->status, "safe in ");
        CHECK(kept && a->error && !a->writable && !a->pending_text);
        kept += strlen("safe in ");
        CHECK(serializer_probe_path_utf8(kept)==SERIALIZER_PATH_EXISTING);
        CHECK(serializer_fingerprint_utf8(path,&after)==1 && serializer_fingerprint_equal(&before,&after));
        CHECK(game_profile_save(a)==-1); /* no second write over the kept copy */
        remove(kept);
        a->writable = 1;
    }
    game_profile_close(b); game_profile_init(b);
    FILE *fp = fopen(path,"wb"); CHECK(fp);
    fputs("not a profile",fp); fclose(fp);
    CHECK(serializer_fingerprint_utf8(path,&before)==1);
    CHECK(game_profile_open(b,path)==-1 && b->error && game_profile_save(b)==-1);
    CHECK(serializer_fingerprint_utf8(path,&after)==1 && serializer_fingerprint_equal(&before,&after));
    game_profile_close(a); game_profile_close(b);
    free(a); free(b); free(decoded); free(text); remove(path); remove(lock_path);
    return 0;
fail:
    serializer_test_set_failure(SERIALIZER_TEST_FAILURE_NONE);
    if (a) game_profile_close(a);
    if (b) game_profile_close(b);
    free(a); free(b); free(decoded); free(text); remove(path); remove(lock_path);
    return 1;
}

/*
 * A profile written before the stricter level-reference rule can hold a key
 * such as "levels/con.toml".  Only those entries (and such a last_level) are
 * dropped; the rest loads, and the next save writes the cleaned profile.
 * Every other kind of damage still rejects the whole file.
 */
static int legacy_level_keys_are_dropped(void)
{
    static const char legacy[] =
        "format_version = 1\nlast_level = \"levels/con.toml\"\n"
        "[[levels]]\npath = \"levels/con.toml\"\nscore = 5\ncoins = 1\ntime = 3\n"
        "[[levels]]\npath = \"levels/kept.toml\"\nscore = 7\ncoins = 2\ntime = 4\n"
        "[[levels]]\npath = \"levels/labs/01_collision.toml\"\nscore = 1\ncoins = 0\ntime = 1\n";
    static const char *const still_rejected[] = {
        /* A bad value next to a now-invalid key: the entry is malformed. */
        "format_version = 1\n[[levels]]\npath = \"levels/con.toml\"\nscore = \"x\"\ncoins = 1\ntime = 3\n",
        /* Missing field. */
        "format_version = 1\n[[levels]]\npath = \"levels/con.toml\"\nscore = 1\ncoins = 1\n",
        /* An embedded NUL is not a legacy key; it is damage. */
        "format_version = 1\n[[levels]]\npath = \"levels/a\\u0000.toml\"\nscore = 1\ncoins = 1\ntime = 1\n",
        /* Duplicates among the kept entries. */
        "format_version = 1\n"
        "[[levels]]\npath = \"levels/a.toml\"\nscore = 1\ncoins = 1\ntime = 1\n"
        "[[levels]]\npath = \"levels/con.toml\"\nscore = 1\ncoins = 1\ntime = 1\n"
        "[[levels]]\npath = \"levels/a.toml\"\nscore = 2\ncoins = 1\ntime = 1\n",
    };
    GameProfile *profile = calloc(1, sizeof(*profile));
    GameProfileData *decoded = calloc(1, sizeof(*decoded));
    char path[160];
    char lock_path[176];
    char *saved = NULL;
    FILE *fp;

    snprintf(path, sizeof(path), TEST_OUT "profile-legacy-%llu.toml", (unsigned long long)clock_millis());
    snprintf(lock_path, sizeof(lock_path), "%s.lock", path);
    CHECK(profile && decoded);
    CHECK(game_profile_decode(decoded, legacy) == 0);
    CHECK(decoded->count == 1 && !strcmp(decoded->levels[0].path, "levels/kept.toml"));
    CHECK(decoded->levels[0].best_score == 7 && decoded->last_level[0] == '\0');
    for (size_t i = 0; i < sizeof(still_rejected) / sizeof(still_rejected[0]); i++)
        CHECK(game_profile_decode(decoded, still_rejected[i]) == -1);

    /* The file loads writable, and the compare-and-swap baseline is the
     * original text, so the next save replaces it with the cleaned data. */
    fp = fopen(path, "wb");
    CHECK(fp);
    fputs(legacy, fp);
    fclose(fp);
    game_profile_init(profile);
    CHECK(game_profile_open(profile, path) == 0 && profile->writable);
    CHECK(profile->data.count == 1);
    CHECK(game_profile_save(profile) == 0);
    game_profile_close(profile);
    game_profile_init(profile);
    CHECK(game_profile_open(profile, path) == 0 && profile->data.count == 1);
    saved = profile->baseline;
    CHECK(saved && !strstr(saved, "con.toml") && !strstr(saved, "labs/"));
    CHECK(strstr(saved, "levels/kept.toml") != NULL);
    game_profile_close(profile);
    free(profile); free(decoded); remove(path); remove(lock_path);
    return 0;
fail:
    if (profile) game_profile_close(profile);
    free(profile); free(decoded); remove(path); remove(lock_path);
    return 1;
}

/*
 * Format version 2 adds the optional [resume] table (the Continue point).
 * It must survive encode/decode bit for bit, a version-1 profile must still
 * load (with no Continue point) and be written back as version 2, and every
 * malformed [resume] must reject the whole file like any other damage.
 */
static int resume_codec_and_migration(void)
{
    static const char version1[] =
        "format_version = 1\nmuted = 1\n"
        "[[levels]]\npath = \"levels/kept.toml\"\nscore = 7\ncoins = 2\ntime = 4\n";
    static const char resume_head[] = "format_version = 2\n[resume]\npath = \"levels/a.toml\"\n";
    /* One valid [resume] body; each bad case below changes one line. */
    static const char *const fields[] = {
        "level_hash = \"00000000deadbeef\"\n", "coins = \"0000000000000005\"\n", "checkpoint = -1\n",
        "legacy_screen = 2\n", "score = 120\n", "level_score_start = 20\n", "score_life_next = 300\n",
        "lives = 2\n", "respawn_x = 812.5\n", "respawn_y = 252\n", "elapsed = 41.25\n"};
    enum { FIELD_COUNT = sizeof(fields) / sizeof(fields[0]) };
    static const struct { int field; const char *line; } bad[] = {
        {0, "level_hash = \"00000000DEADBEEF\"\n"},  /* uppercase */
        {0, "level_hash = \"deadbeef\"\n"},          /* too short */
        {0, "level_hash = 7\n"},                     /* not a string */
        {1, "coins = \"zz00000000000005\"\n"},
        {2, "checkpoint = -2\n"},
        {2, "checkpoint = 99\n"},                    /* MAX_CHECKPOINTS is 99 */
        {4, "score = -1\n"},
        {5, "level_score_start = 121\n"},            /* more than the score */
        {7, "lives = -1\n"},
        {8, "respawn_x = nan\n"},
        {10, "elapsed = -1.0\n"},
        {10, "elapsed = 41.25\nextra = 1\n"},        /* unknown key */
        {10, ""},                                    /* missing field */
    };
    GameProfileData *decoded = calloc(1, sizeof(*decoded));
    GameProfile *profile = calloc(1, sizeof(*profile));
    char *text = malloc(PROFILE_TEXT_MAX), *again = malloc(PROFILE_TEXT_MAX);
    char build[2048];
    CHECK(decoded && profile && text && again);

    /* Version 1 loads with no Continue point and is written as version 2. */
    CHECK(game_profile_decode(decoded, version1) == 0);
    CHECK(!decoded->resume.path[0] && decoded->count == 1 && decoded->settings.muted == 1);
    CHECK(decoded->settings.ghost == 1);  /* the version-2 default: ghost on */
    /* The ghost setting is a version-2 key, and only 0 or 1. */
    CHECK(game_profile_decode(decoded, "format_version = 2\nghost = 0\n") == 0 && decoded->settings.ghost == 0);
    CHECK(game_profile_decode(decoded, "format_version = 1\nghost = 0\n") == -1);
    CHECK(game_profile_decode(decoded, "format_version = 2\nghost = 2\n") == -1);
    CHECK(game_profile_decode(decoded, version1) == 0);
    CHECK(game_profile_encode(decoded, text, PROFILE_TEXT_MAX) == 0);
    CHECK(!strncmp(text, "format_version = 2\n", 19) && !strstr(text, "[resume]"));

    /* A full [resume] round-trips exactly. */
    size_t used = (size_t)snprintf(build, sizeof(build), "%s", resume_head);
    for (int i = 0; i < FIELD_COUNT; i++) used += (size_t)snprintf(build + used, sizeof(build) - used, "%s", fields[i]);
    CHECK(game_profile_decode(decoded, build) == 0);
    const GameResume *r = &decoded->resume;
    CHECK(!strcmp(r->path, "levels/a.toml") && r->level_hash == 0xdeadbeefu && r->coins == 5);
    CHECK(r->checkpoint == -1 && r->legacy_screen == 2 && r->score == 120 && r->level_score_start == 20);
    CHECK(r->score_life_next == 300 && r->lives == 2 && r->respawn_x == 812.5f && r->respawn_y == 252.0f);
    CHECK(r->elapsed == 41.25f);
    decoded->resume.level_hash = 0xfedcba9876543210ull;  /* top bit set: why it is a string */
    decoded->resume.coins = 0x8000000000000001ull;      /* coins 0 and 63 */
    CHECK(game_profile_encode(decoded, text, PROFILE_TEXT_MAX) == 0);
    CHECK(strstr(text, "level_hash = \"fedcba9876543210\"") && strstr(text, "coins = \"8000000000000001\""));
    CHECK(game_profile_decode(decoded, text) == 0);
    CHECK(game_profile_encode(decoded, again, PROFILE_TEXT_MAX) == 0 && !strcmp(text, again));
    CHECK(decoded->resume.level_hash == 0xfedcba9876543210ull && decoded->resume.coins == 0x8000000000000001ull);

    /* The fuzz seed for this table must itself be a valid profile, or the
     * fuzzer would only ever explore the rejection path from it. */
    {
        FILE *seed = fopen("tests/fuzz/corpus/profile/resume.toml", "rb");
        CHECK(seed);
        size_t size = fread(text, 1, PROFILE_TEXT_MAX - 1, seed);
        fclose(seed);
        text[size] = '\0';
        CHECK(game_profile_decode(decoded, text) == 0 && decoded->resume.legacy_screen == 3);
        CHECK(decoded->settings.ghost == 0);
    }

    /* [resume] in a version-1 file is damage: version 1 never had one. */
    build[strlen("format_version = ")] = '1';
    CHECK(game_profile_decode(decoded, build) == -1);

    for (size_t b = 0; b < sizeof(bad) / sizeof(bad[0]); b++) {
        used = (size_t)snprintf(build, sizeof(build), "%s", resume_head);
        for (int i = 0; i < FIELD_COUNT; i++)
            used += (size_t)snprintf(build + used, sizeof(build) - used, "%s",
                                     i == bad[b].field ? bad[b].line : fields[i]);
        if (game_profile_decode(decoded, build) != -1) {
            fprintf(stderr, "profile test: accepted bad [resume] case %zu\n", b);
            goto fail;
        }
    }

    /* Recording: a changed point marks the profile; the same one does not. */
    game_profile_init(profile);
    GameResume point = {.path = "levels/a.toml", .level_hash = 9, .checkpoint = 0, .lives = 3,
                        .respawn_x = 304, .respawn_y = 252};
    CHECK(game_profile_set_resume(profile, &point) == 0 && profile->dirty && profile->revision == 1);
    CHECK(game_profile_set_resume(profile, &point) == 0 && profile->revision == 1);
    CHECK(game_profile_resume(profile, "levels/a.toml") && !game_profile_resume(profile, "levels/b.toml"));
    point.lives = -1;
    CHECK(game_profile_set_resume(profile, &point) == -1 && profile->data.resume.lives == 3);
    game_profile_clear_resume(profile);
    CHECK(!profile->data.resume.path[0] && profile->revision == 2);
    game_profile_clear_resume(profile);
    CHECK(profile->revision == 2);

    free(decoded); free(profile); free(text); free(again);
    return 0;
fail:
    free(decoded); free(profile); free(text); free(again);
    return 1;
}

static int level_key_boundaries(void)
{
    char *prefs = preference_path_at(MANGO_TEST_OUTDIR, "migration-path-test", "Application");
    if (!prefs || strcmp(prefs, TEST_OUT "migration-path-test/Application/")) { free(prefs); return 1; }
#ifdef _WIN32
    _rmdir(prefs);
    _rmdir(TEST_OUT "migration-path-test");
#else
    rmdir(prefs);
    rmdir(TEST_OUT "migration-path-test");
#endif
    free(prefs);
    GameProfile *profile = calloc(1, sizeof(*profile));
    char key[PROFILE_LEVEL_PATH + 1];
    CHECK(profile);
    for (size_t length = PROFILE_LEVEL_PATH - 1; length <= PROFILE_LEVEL_PATH; length++) {
        game_profile_init(profile);
        memset(key, 'a', length);
        memcpy(key, "levels/", 7);
        memcpy(key + length - 5, ".toml", 5);
        key[length] = '\0';
        game_profile_select(profile, key);
        int result = game_profile_record(profile, key, 10, 1, 2.0f);
        if (length < PROFILE_LEVEL_PATH) {
            CHECK(result == 0 && profile->data.count == 1);
            CHECK(!strcmp(profile->data.last_level, key));
            CHECK(!strcmp(profile->data.levels[0].path, key));
        } else {
            CHECK(result == -1 && profile->data.count == 0);
            CHECK(!profile->data.last_level[0] && !profile->revision && !profile->dirty);
        }
    }
    free(profile);
    return 0;
fail:
    free(profile);
    return 1;
}

static int pending_snapshot_bookkeeping(void)
{
    GameProfile *profile = calloc(1, sizeof(*profile));
    CHECK(profile);
    game_profile_init(profile);
    profile->baseline = malloc(16);
    profile->pending_text = malloc(16);
    CHECK(profile->baseline && profile->pending_text);
    strcpy(profile->baseline, "old");
    strcpy(profile->pending_text, "snapshot");
    char *snapshot = profile->pending_text;
    profile->dirty = 1;
    profile->revision = profile->pending_revision = 1;
    CHECK(game_profile_poll(profile) == PROFILE_SAVE_PENDING);
    profile->data.settings.music_volume = 64;
    profile->revision++;
    CHECK(game_profile_finish_save(profile, PROFILE_SAVE_OK) == PROFILE_SAVE_OK);
    CHECK(profile->baseline == snapshot && !profile->pending_text && profile->dirty);
    CHECK(profile->data.settings.music_volume == 64);
    CHECK(game_profile_finish_save(profile, PROFILE_SAVE_OK) == PROFILE_SAVE_ERROR);
    profile->pending_text = malloc(16);
    CHECK(profile->pending_text);
    strcpy(profile->pending_text, "failed");
    CHECK(game_profile_finish_save(profile, PROFILE_SAVE_ERROR) == PROFILE_SAVE_ERROR);
    CHECK(profile->baseline == snapshot && profile->dirty && profile->error);
    profile->pending_text = malloc(16);
    CHECK(profile->pending_text);
    game_profile_close(profile);
    CHECK(!profile->pending_text && !profile->baseline);
    game_profile_close(profile);
    free(profile);
    return 0;
fail:
    if (profile) game_profile_close(profile);
    free(profile);
    return 1;
}

static int settings_and_bindings(void)
{
    GameProfile *profile = calloc(1,sizeof(*profile));
    SettingsMenu *menu = calloc(1,sizeof(*menu));
    CHECK(profile && menu);
    game_profile_init(profile);
    InputEvent event = {.type=INPUT_KEY_DOWN,.key=KEY_F1};
    int handled = settings_menu_event(menu,profile,&event,PAD_BACK);
    if (handled != 1 || !menu->open) fprintf(stderr,"settings input: type=%u key=%d repeat=%u handled=%d open=%d\n",
        event.type,event.key,event.repeat,handled,menu->open);
    CHECK(handled==1 && menu->open);
    menu->page=1; menu->selected=BIND_JUMP;
    event.key=KEY_ENTER;
    CHECK(settings_menu_event(menu,profile,&event,PAD_BACK)==1 && menu->capture==1);
    event.key=KEY_A; event.binding=4;
    settings_menu_event(menu,profile,&event,PAD_BACK);
    CHECK(menu->capture==1 && profile->data.settings.keys[BIND_JUMP]==44);
    event.key=KEY_J; event.binding=13;
    settings_menu_event(menu,profile,&event,PAD_BACK);
    CHECK(menu->capture==0 && profile->data.settings.keys[BIND_JUMP]==13);
    uint8_t keys[512]={0}, buttons[PAD_COUNT]={0};
    keys[13]=1;
    CHECK(game_input_keyboard_mask(keys,&profile->data.settings)&PLAYER_INPUT_JUMP);
    keys[13]=0; keys[44]=1;
    CHECK(!(game_input_keyboard_mask(keys,&profile->data.settings)&PLAYER_INPUT_JUMP));
    menu->selected=PROFILE_ACTION_COUNT+BIND_RUN;
    event.key=KEY_ENTER;
    settings_menu_event(menu,profile,&event,PAD_BACK);
    event.type=INPUT_PAD_DOWN; event.button=PAD_X;
    settings_menu_event(menu,profile,&event,PAD_BACK);
    CHECK(profile->data.settings.buttons[BIND_RUN]==PAD_X);
    buttons[PAD_X]=1;
    profile->data.settings.dead_zone=10000;
    CHECK(game_input_controller_mask(buttons,9000,0,&profile->data.settings)==PLAYER_INPUT_RUN);
    CHECK(game_input_controller_mask(buttons,12000,0,&profile->data.settings)==(PLAYER_INPUT_RUN|PLAYER_INPUT_RIGHT));
    event.button=PAD_BACK;
    settings_menu_event(menu,profile,&event,PAD_BACK);
    CHECK(!menu->open);
    /* The debug inspector's keys are refused in every run, not only while
     * capturing in --debug: a profile is shared by both kinds of run. */
    CHECK(game_settings_key_debug_reserved(BINDING_KEY_F2) && game_settings_key_debug_reserved(BINDING_KEY_F10));
    CHECK(game_settings_key_debug_reserved(BINDING_KEY_MINUS) && game_settings_key_debug_reserved(BINDING_KEY_EQUAL));
    CHECK(!game_settings_key_allowed(BINDING_KEY_F2) && !game_settings_key_allowed(BINDING_KEY_EQUAL));
    CHECK(game_settings_key_allowed(BINDING_KEY_F10 + 1) && game_settings_key_allowed(BINDING_KEY_F10 + 2)); /* F11, F12 */
    settings_menu_open(menu);
    menu->page=1; menu->selected=BIND_JUMP;
    event.type=INPUT_KEY_DOWN; event.key=KEY_ENTER; event.binding=BINDING_KEY_ENTER;
    settings_menu_event(menu,profile,&event,PAD_BACK);
    CHECK(menu->capture==1);
    event.key=KEY_F2; event.binding=BINDING_KEY_F2;
    settings_menu_event(menu,profile,&event,PAD_BACK);
    CHECK(menu->capture==1 && profile->data.settings.keys[BIND_JUMP]==13);
    CHECK(strstr(menu->message,"debug inspector")!=NULL);
    settings_menu_cleanup(menu); free(menu); free(profile);
    return 0;
fail:
    if (menu) settings_menu_cleanup(menu);
    free(menu); free(profile); return 1;
}

static int persistent_session(void)
{
    char path[160];
    char lock_path[176];
    snprintf(path,sizeof(path),TEST_OUT "profile-session-%llu.toml",(unsigned long long)clock_millis());
    snprintf(lock_path,sizeof(lock_path),"%s.lock",path);
    AppSessionConfig config={.level_path="levels/00_sandbox_01.toml",.profile_enabled=1,.profile_path=path};
    puts("profile session: create");
    AppSession *session=session_create(&config);
    CHECK(session);
    InputEvent event = {.type=INPUT_KEY_DOWN,.key=KEY_F1};
    puts("profile session: open settings");
    input_push(&event); session_frame(session);
    CHECK(session->settings.open && session->game->screen.completion.level_elapsed==0);
    session->profile.data.settings.muted=1;
    session->profile.data.settings.high_contrast=1;
    session->profile.data.settings.reduced_motion=1;
    session->profile.data.settings.window_scale=3;
    session->profile.dirty=1; session->profile.revision++;
    event.key=KEY_ESCAPE;
    puts("profile session: close settings");
    input_push(&event); session_frame(session);
    CHECK(!session->settings.open && test_last_music_volume==0);
    CHECK(applied_width==1200 && applied_height==900);
#ifndef MANGO_MEMORY_TESTS
    /* Memory renders frames but has no OS resize implementation. The desktop
     * suite additionally verifies that the real window applied the request.
     * An OS may clamp a window to a smaller monitor (CI VMs often run at
     * 1024x768), so the exact size is only required when it fits. */
    {
        int monitor = GetCurrentMonitor();
        if (GetMonitorWidth(monitor) > 1200 && GetMonitorHeight(monitor) > 900)
            CHECK(GetScreenWidth()==1200 && GetScreenHeight()==900);
        else
            CHECK(GetScreenWidth()>WINDOW_W/2 && GetScreenWidth()<=1200 &&
                  GetScreenHeight()>0 && GetScreenHeight()<=900);
    }
#endif
    session->game->world.score=200;
    game_complete_level(session->game);
    puts("profile session: completion");
    session_frame(session);
    CHECK(game_profile_result(&session->profile,"levels/00_sandbox_01.toml"));
    /* F8 restarts the level for a recording, which reloads the music at the
     * level's own volume; the player's mute must survive that restart. */
    puts("profile session: experiment restart");
    test_last_music_volume = -1;
    CHECK(game_experiment_begin(session->game)==0);
    CHECK(test_last_music_volume==0);
    puts("profile session: destroy");
    session_destroy(&session);
    config.level_path=NULL; config.continue_last=1;
    puts("profile session: continue");
    session=session_create(&config);
    CHECK(session && session->game && session->profile.data.settings.muted==1);
    CHECK(!strcmp(session->game->screen.profile_level_key,"levels/00_sandbox_01.toml"));
    CHECK(session->profile.data.count==1);
    /* A remembered stage that no longer loads used to end the program;
     * --continue now falls back to the level selector. */
    game_profile_select(&session->profile, "levels/zz_removed_stage.toml");
    session_destroy(&session);
    puts("profile session: continue to a removed stage");
    session=session_create(&config);
    CHECK(session && !session->game && session->menu && session->screen==APP_SCREEN_MENU);
    session_destroy(&session);
    config.level_path="levels/00_sandbox_01.toml"; config.smoke_test_frames=1;
    session=session_create(&config);
    CHECK(session && !session->profile.enabled && !session->profile.baseline);
    CHECK(!session->profile.data.settings.muted);
    session_destroy(&session);
    CHECK(serializer_probe_path_utf8(path)==SERIALIZER_PATH_EXISTING);
    remove(path);
    remove(lock_path);
    return 0;
fail:
    session_destroy(&session); remove(path); remove(lock_path); return 1;
}

/*
 * Continue, end to end through the session and a real profile file:
 * reaching a new respawn point and pausing records a Continue point,
 * leaving mid-level keeps it on disk, --continue and the menu's Continue
 * pick the level up there, finishing the level forgets it, and a level
 * whose bytes changed loses it before the menu can offer it.
 */
static int continue_round_trip(void)
{
    char path[160], lock_path[176];
    snprintf(path, sizeof(path), TEST_OUT "profile-continue-%llu.toml", (unsigned long long)clock_millis());
    snprintf(lock_path, sizeof(lock_path), "%s.lock", path);
    const char *level = "levels/00_sandbox_01.toml";
    AppSessionConfig config = {.level_path = level, .profile_enabled = 1, .profile_path = path};
    AppSession *session = session_create(&config);
    CHECK(session && session->game && !session->game->screen.resumed);
    GameState *game = session->game;
    CHECK(!game_profile_resume(&session->profile, level));  /* nothing yet */

    /* Walk into the third screen: the automatic screen checkpoint moves the
     * respawn point. Collect coin 3 on the way and pause there. */
    game->world.player.x = 2.0f * GAME_W + 50.0f;
    game_checkpoint_update(game);
    float respawn_x = game->world.respawn_x;
    CHECK(respawn_x > 80.0f && game->world.legacy_checkpoint_screen == 2);
    game->world.coins[3].active = 0;
    game->world.score = 30;
    game->world.lives = 2;
    game->screen.completion.level_elapsed = 12.5f;
    game_overlay_set_pause_reason(game, GAME_PAUSE_REASON_PLAYER, 1);
    session_frame(session);
    const GameResume *saved = game_profile_resume(&session->profile, level);
    CHECK(saved && saved->respawn_x == respawn_x && saved->legacy_screen == 2);
    CHECK(saved->coins == (1u << 3) && saved->score == 30 && saved->lives == 2);
    CHECK(saved->level_hash == game->world.source_level_hash && saved->elapsed >= 12.5f);

    /* Leave part-way: the point is written to disk with the profile. */
    game->world.score = 40;  /* changed since the pause: Exit records it again */
    game->screen.route = GAME_ROUTE_EXIT;
    session_frame(session);
    CHECK(session->ended);
    session_destroy(&session);

    /* --continue resumes there: respawn, score, lives, coins, camera. */
    config.level_path = NULL;
    config.continue_last = 1;
    session = session_create(&config);
    CHECK(session && session->game && session->game->screen.resumed);
    game = session->game;
    CHECK(game->world.respawn_x == respawn_x && game->world.score == 40 && game->world.lives == 2);
    CHECK(!game->world.coins[3].active && game->world.coins[2].active && game->screen.completion.level_elapsed >= 12.5f);
    CHECK(fabsf(game->world.player.x - (respawn_x + (TILE_SIZE - game->world.player.w) / 2.0f)) < 0.01f);
    CHECK(game->world.camera.x > 0.0f);  /* snapped to the respawn, not panning from 0 */

    /* Finishing the level forgets the point. */
    game_complete_level(game);
    session_frame(session);
    CHECK(!game_profile_resume(&session->profile, level));
    session_destroy(&session);

    /* Level Select part-way also keeps a point, and the menu then offers
     * Continue (C key) beside Play for that level. */
    config.continue_last = 0;
    config.level_path = level;
    session = session_create(&config);
    CHECK(session && session->game);
    session->game->screen.route = GAME_ROUTE_LEVEL_SELECT;
    session_frame(session);
    CHECK(session->menu && session->screen == APP_SCREEN_MENU);
    saved = game_profile_resume(&session->profile, level);
    CHECK(saved && saved->score == 0 && saved->checkpoint == -1);
    GameResume point = *saved;
    point.score = 55;  /* tell this point apart from a fresh start */
    CHECK(game_profile_set_resume(&session->profile, &point) == 0);
    CHECK(!strcmp(session->menu->selected_level_path, level) && start_menu_can_continue(session->menu));
    input_clear();
    session->menu->confirm_release_required = 0;
    InputEvent continue_key = {.type = INPUT_KEY_DOWN, .key = KEY_C};
    input_push(&continue_key);
    session_frame(session);
    CHECK(session->game && session->game->screen.resumed && session->game->world.score == 55);
    session->game->screen.route = GAME_ROUTE_LEVEL_SELECT;
    session_frame(session);
    CHECK(session->menu);
    session_destroy(&session);

    /* A stale hash (the level was edited since) is dropped on menu open. */
    config.level_path = level;
    session = session_create(&config);
    CHECK(session && session->game);
    game_resume_capture(session->game, &point);
    point.level_hash ^= 1;
    CHECK(game_profile_set_resume(&session->profile, &point) == 0);
    session_destroy(&session);  /* native teardown saves the dirty profile */
    config.level_path = NULL;
    session = session_create(&config);
    CHECK(session && session->menu && !game_profile_resume(&session->profile, level));
    CHECK(!start_menu_can_continue(session->menu));
    session_destroy(&session);
    remove(path);
    remove(lock_path);
    return 0;
fail:
    session_destroy(&session);
    remove(path);
    remove(lock_path);
    return 1;
}

/*
 * Ghost text is untrusted like the profile: a strict decoder, a stable
 * encoding, and file names derived from the profile's own path.
 */
static int ghost_codec_and_paths(void)
{
    static const char good[] =
        "format_version = 1\nlevel = \"levels/a.toml\"\nlevel_hash = \"00000000000000ff\"\n"
        "time = 0.05\nsteps = 3\nframes = [\"0450011c000451011c00\", \"0452011c22\"]\n";
    /* Each bad case is the good text with one fault. */
    static const char *const bad[] = {
        "format_version = 2\nlevel = \"levels/a.toml\"\nlevel_hash = \"00000000000000ff\"\n"
        "time = 0.05\nsteps = 3\nframes = [\"0450011c000451011c00\", \"0452011c22\"]\n",
        "format_version = 1\nlevel = \"levels/labs/a.toml\"\nlevel_hash = \"00000000000000ff\"\n"
        "time = 0.05\nsteps = 3\nframes = [\"0450011c000451011c00\", \"0452011c22\"]\n",
        "format_version = 1\nlevel = \"levels/a.toml\"\nlevel_hash = \"00000000000000FF\"\n"
        "time = 0.05\nsteps = 3\nframes = [\"0450011c000451011c00\", \"0452011c22\"]\n",
        "format_version = 1\nlevel = \"levels/a.toml\"\nlevel_hash = \"00000000000000ff\"\n"
        "time = 0.05\nsteps = 4\nframes = [\"0450011c000451011c00\", \"0452011c22\"]\n",   /* too few */
        "format_version = 1\nlevel = \"levels/a.toml\"\nlevel_hash = \"00000000000000ff\"\n"
        "time = 0.05\nsteps = 2\nframes = [\"0450011c000451011c00\", \"0452011c22\"]\n",   /* too many */
        "format_version = 1\nlevel = \"levels/a.toml\"\nlevel_hash = \"00000000000000ff\"\n"
        "time = 0.05\nsteps = 3\nframes = [\"0450011c000451011c00\", \"0452011c40\"]\n",   /* cell 64 */
        "format_version = 1\nlevel = \"levels/a.toml\"\nlevel_hash = \"00000000000000ff\"\n"
        "time = 0.05\nsteps = 3\nframes = [\"0450011c000451011c0\", \"0452011c220\"]\n",   /* split sample */
        "format_version = 1\nlevel = \"levels/a.toml\"\nlevel_hash = \"00000000000000ff\"\n"
        "time = 0.05\nsteps = 3\nframes = [\"0450011c000451011c00\", \"0452011g22\"]\n",   /* bad digit */
        "format_version = 1\nlevel = \"levels/a.toml\"\nlevel_hash = \"00000000000000ff\"\n"
        "time = 9999\nsteps = 3\nframes = [\"0450011c000451011c00\", \"0452011c22\"]\n",   /* time > cap */
        "format_version = 1\nlevel = \"levels/a.toml\"\nlevel_hash = \"00000000000000ff\"\n"
        "time = 0.05\nsteps = 0\nframes = []\n",
        "format_version = 1\nlevel = \"levels/a.toml\"\nlevel_hash = \"00000000000000ff\"\n"
        "time = 0.05\nsteps = 3\nframes = [\"0450011c000451011c00\", \"0452011c22\"]\nextra = 1\n",
        "format_version = 1\nlevel = \"levels/a.toml\"\nlevel_hash = \"00000000000000ff\"\n"
        "time = 0.05\nframes = [\"0450011c000451011c00\", \"0452011c22\"]\n",              /* no steps */
    };
    GameGhostTrack track = {0};
    char *text = malloc(GHOST_TEXT_MAX), *again = malloc(GHOST_TEXT_MAX);
    char path[64];
    CHECK(text && again);
    CHECK(game_ghost_decode(&track, good) == 0);
    CHECK(track.count == 3 && track.level_hash == 0xff && track.time == 0.05f);
    CHECK(track.samples[0].x == 0x0450 && track.samples[0].y == 0x011c && track.samples[0].cell == 0);
    CHECK(track.samples[2].x == 0x0452 && track.samples[2].cell == 0x22);  /* facing left, cell 2 */
    CHECK(game_ghost_encode(&track, text, GHOST_TEXT_MAX) == 0);
    game_ghost_track_free(&track);
    CHECK(game_ghost_decode(&track, text) == 0 && game_ghost_encode(&track, again, GHOST_TEXT_MAX) == 0);
    CHECK(!strcmp(text, again));
    CHECK(game_ghost_encode(&track, text, 40) == -1);  /* does not fit */
    game_ghost_track_free(&track);
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        if (game_ghost_decode(&track, bad[i]) != -1 || track.samples) {
            fprintf(stderr, "profile test: accepted bad ghost case %zu\n", i);
            goto fail;
        }
    }
    /* The ghost fuzz seed must decode, or fuzzing would only explore the
     * rejection path from it. */
    {
        FILE *seed = fopen("tests/fuzz/corpus/profile/ghost.toml", "rb");
        CHECK(seed);
        size_t size = fread(text, 1, GHOST_TEXT_MAX - 1, seed);
        fclose(seed);
        text[size] = '\0';
        CHECK(game_ghost_decode(&track, text) == 0 && track.count == 5);
        CHECK(track.samples[3].cell == 0x24 && track.samples[4].cell == 0x2d);
        game_ghost_track_free(&track);
    }
    /* Oversized text is refused before parsing. */
    memset(text, ' ', GHOST_TEXT_MAX - 1);
    text[GHOST_TEXT_MAX - 1] = '\0';
    CHECK(game_ghost_decode(&track, text) == -1);

    CHECK(game_ghost_file_path("/p/profile.toml", "levels/01_x.toml", path, sizeof(path)) == 0);
    CHECK(!strcmp(path, "/p/profile-ghost-01_x.toml"));
    CHECK(game_ghost_file_path("/p/mine", "levels/a.toml", path, sizeof(path)) == 0);
    CHECK(!strcmp(path, "/p/mine-ghost-a.toml"));
    CHECK(game_ghost_file_path("/p/profile.toml", "levels/a.toml", path, 8) == -1);
    CHECK(game_ghost_file_path("", "levels/a.toml", path, sizeof(path)) == -1);
    CHECK(game_ghost_file_path("/p/profile.toml", "levels/labs/a.toml", path, sizeof(path)) == -1);
    free(text); free(again);
    return 0;
fail:
    game_ghost_track_free(&track);
    free(text); free(again);
    return 1;
}

/* Pretend the player ran `steps` fixed steps standing at the start, then
 * finished in `seconds`; the session records the result and the ghost. */
static void finish_run(AppSession *session, int steps, float seconds)
{
    GameState *game = session->game;
    for (int i = 0; i < steps; i++) game_ghost_step(game);
    game->screen.completion.level_elapsed = seconds;
    game_complete_level(game);
    session_frame(session);
}

/*
 * The session keeps the fastest finished run as the level's ghost: the
 * first run is saved, a slower one leaves it alone, a faster one replaces
 * it, and the next game loads it to race. A ghost recorded on another
 * version of the level, or a damaged file, is ignored and then replaced.
 */
static int ghost_session_keeps_the_fastest_run(void)
{
    char path[160], lock_path[176], ghost_path[200];
    const char *level = "levels/00_sandbox_01.toml";
    snprintf(path, sizeof(path), TEST_OUT "profile-ghost-%llu.toml", (unsigned long long)clock_millis());
    snprintf(lock_path, sizeof(lock_path), "%s.lock", path);
    CHECK(game_ghost_file_path(path, level, ghost_path, sizeof(ghost_path)) == 0);
    AppSessionConfig config = {.level_path = level, .profile_enabled = 1, .profile_path = path};
    AppSession *session = session_create(&config);
    SerializerFileFingerprint before, after;
    CHECK(session && session->game && session->game->screen.ghost && !session->game->screen.ghost->best.count);

    finish_run(session, 30, 0.5f);
    CHECK(serializer_probe_path_utf8(ghost_path) == SERIALIZER_PATH_EXISTING);
    session->game->screen.route = GAME_ROUTE_REPLAY;
    session_frame(session);
    CHECK(session->game->screen.ghost->best.count == 30 && session->game->screen.ghost->best.time == 0.5f);

    /* Slower: the stored ghost stays as it was. */
    CHECK(serializer_fingerprint_utf8(ghost_path, &before) == 1);
    finish_run(session, 60, 1.0f);
    CHECK(serializer_fingerprint_utf8(ghost_path, &after) == 1 && serializer_fingerprint_equal(&before, &after));

    /* Faster: it replaces the ghost. */
    session->game->screen.route = GAME_ROUTE_REPLAY;
    session_frame(session);
    finish_run(session, 15, 0.25f);
    session->game->screen.route = GAME_ROUTE_REPLAY;
    session_frame(session);
    CHECK(session->game->screen.ghost->best.count == 15 && session->game->screen.ghost->best.time == 0.25f);

    /* Recorded on another version of the level: not loaded, and the next
     * finished run (however slow) takes its place. */
    {
        GameGhostTrack stale = session->game->screen.ghost->best;
        stale.level_hash ^= 1;
        CHECK(game_ghost_save(&session->profile, &stale) == 0);
    }
    session->game->screen.route = GAME_ROUTE_REPLAY;
    session_frame(session);
    CHECK(session->game->screen.ghost->best.count == 0);
    finish_run(session, 90, 1.5f);
    session->game->screen.route = GAME_ROUTE_REPLAY;
    session_frame(session);
    CHECK(session->game->screen.ghost->best.count == 90);

    /* A damaged file is ignored the same way. */
    FILE *fp = fopen(ghost_path, "wb");
    CHECK(fp);
    fputs("not a ghost", fp);
    fclose(fp);
    session->game->screen.route = GAME_ROUTE_REPLAY;
    session_frame(session);
    CHECK(session->game->screen.ghost->best.count == 0);

    /* Runs without a personal profile have no ghost at all. */
    session_destroy(&session);
    config.profile_enabled = 0;
    session = session_create(&config);
    CHECK(session && session->game && !session->game->screen.ghost);
    session_destroy(&session);
    remove(ghost_path); remove(path); remove(lock_path);
    return 0;
fail:
    session_destroy(&session);
    remove(ghost_path); remove(path); remove(lock_path);
    return 1;
}

/*
 * A run started with --start-x / --start-checkpoint (the editor's
 * "Playtest from here") skipped part of the level. It used to record its
 * short time as the best time and its recording as the ghost, and to save
 * its respawn point as the Continue point, replacing the real one with a
 * point --continue then refused. Now it writes none of the three, and it
 * leaves the real Continue point alone even when it finishes the level.
 */
static int start_point_runs_leave_the_profile_alone(void)
{
    char path[160], lock_path[176], ghost_path[200];
    const char *level = "levels/00_sandbox_01.toml";
    snprintf(path, sizeof(path), TEST_OUT "profile-start-point-%llu.toml", (unsigned long long)clock_millis());
    snprintf(lock_path, sizeof(lock_path), "%s.lock", path);
    CHECK(game_ghost_file_path(path, level, ghost_path, sizeof(ghost_path)) == 0);
    AppSessionConfig config = {.level_path = level, .profile_enabled = 1, .profile_path = path};

    /* A normal run leaves a Continue point in the third screen, score 30. */
    AppSession *session = session_create(&config);
    CHECK(session && session->game);
    session->game->world.player.x = 2.0f * GAME_W + 50.0f;
    game_checkpoint_update(session->game);
    session->game->world.score = 30;
    session->game->screen.route = GAME_ROUTE_EXIT;
    session_frame(session);
    session_destroy(&session);

    /* A playtest from x 300 moves its respawn point, pauses and finishes. */
    config.start.kind = LEVEL_START_AT_X;
    config.start.x = 300.0f;
    session = session_create(&config);
    CHECK(session && session->game);
    GameState *game = session->game;
    game->world.player.x = 3.0f * GAME_W + 50.0f;
    game_checkpoint_update(game);
    game->world.score = 99;
    game_overlay_set_pause_reason(game, GAME_PAUSE_REASON_PLAYER, 1);
    session_frame(session);
    const GameResume *saved = game_profile_resume(&session->profile, level);
    CHECK(saved && saved->score == 30 && saved->legacy_screen == 2);
    game_overlay_set_pause_reason(game, GAME_PAUSE_REASON_PLAYER, 0);
    finish_run(session, 30, 0.5f);
    CHECK(game->screen.profile_completion_recorded);
    CHECK(!game_profile_result(&session->profile, level));
    CHECK(serializer_probe_path_utf8(ghost_path) == SERIALIZER_PATH_MISSING);
    saved = game_profile_resume(&session->profile, level);
    CHECK(saved && saved->score == 30);
    session_destroy(&session);

    /* The real Continue point still works. */
    config.start.kind = LEVEL_START_DEFAULT;
    config.level_path = NULL;
    config.continue_last = 1;
    session = session_create(&config);
    CHECK(session && session->game && session->game->screen.resumed && session->game->world.score == 30);
    session_destroy(&session);
    remove(ghost_path); remove(path); remove(lock_path);
    return 0;
fail:
    session_destroy(&session);
    remove(ghost_path); remove(path); remove(lock_path);
    return 1;
}

/*
 * The ghost races from step 0 of the level. A run that picks the level up
 * part-way (Continue, or a --start-x playtest) used to show it too, from
 * the level start, where it had nothing to do with the player. It is now
 * hidden for those runs and shown again for the next whole one.
 */
static int ghost_hidden_on_partial_runs(void)
{
    char path[160], lock_path[176], ghost_path[200];
    const char *level = "levels/00_sandbox_01.toml";
    snprintf(path, sizeof(path), TEST_OUT "profile-ghost-hidden-%llu.toml", (unsigned long long)clock_millis());
    snprintf(lock_path, sizeof(lock_path), "%s.lock", path);
    CHECK(game_ghost_file_path(path, level, ghost_path, sizeof(ghost_path)) == 0);
    AppSessionConfig config = {.level_path = level, .profile_enabled = 1, .profile_path = path};
    AppSession *session = session_create(&config);
    CHECK(session && session->game);
    finish_run(session, 30, 0.5f);
    session->game->screen.route = GAME_ROUTE_REPLAY;
    session_frame(session);
    CHECK(session->game->screen.ghost->best.count == 30);
    CHECK(game_ghost_current(session->game) != NULL);  /* a whole run races it */

    /* Leave a Continue point in the third screen. */
    session->game->world.player.x = 2.0f * GAME_W + 50.0f;
    game_checkpoint_update(session->game);
    session->game->screen.route = GAME_ROUTE_EXIT;
    session_frame(session);
    session_destroy(&session);

    config.level_path = NULL;
    config.continue_last = 1;
    session = session_create(&config);
    CHECK(session && session->game && session->game->screen.resumed);
    CHECK(session->game->screen.ghost && session->game->screen.ghost->best.count == 30);
    game_ghost_step(session->game);
    CHECK(game_ghost_current(session->game) == NULL);
    session_destroy(&session);

    config.level_path = level;
    config.continue_last = 0;
    config.start.kind = LEVEL_START_AT_X;
    config.start.x = 300.0f;
    session = session_create(&config);
    CHECK(session && session->game && session->game->screen.ghost);
    game_ghost_step(session->game);
    CHECK(game_ghost_current(session->game) == NULL);
    /* Retry after Game Over is a whole attempt again: the ghost returns. */
    session->game->screen.game_over = 1;
    game_restart_after_game_over(session->game);
    game_ghost_step(session->game);
    CHECK(game_ghost_current(session->game) != NULL);
    session_destroy(&session);
    remove(ghost_path); remove(path); remove(lock_path);
    return 0;
fail:
    session_destroy(&session);
    remove(ghost_path); remove(path); remove(lock_path);
    return 1;
}

/* Collect raylib warnings so a test can see what the session logged. */
static char last_warning[256];
static void capture_warning(int level, const char *text, va_list args)
{
    if (level == LOG_WARNING) vsnprintf(last_warning, sizeof(last_warning), text, args);
}

/*
 * The coin cap is MAX_COINS, the most coins a level can hold, not a second
 * literal 64. And when a finished level's result cannot be recorded (here:
 * the profile already holds PROFILE_LEVEL_COUNT other levels) the session
 * used to drop it silently; it now logs a warning naming the level.
 */
static int result_cap_and_record_failure(void)
{
    GameProfile profile;
    AppSession *session = NULL;
    game_profile_init(&profile);
    CHECK(game_profile_record(&profile, "levels/coins.toml", 1, MAX_COINS, 1.0f) == 0);
    CHECK(game_profile_record(&profile, "levels/coins.toml", 1, MAX_COINS + 1, 1.0f) == -1);
    CHECK(game_profile_result(&profile, "levels/coins.toml")->best_coins == MAX_COINS);
    game_profile_close(&profile);

    AppSessionConfig config = {.level_path = "levels/00_sandbox_01.toml"};
    session = session_create(&config);
    CHECK(session && session->game);
    for (int i = 0; i < PROFILE_LEVEL_COUNT; i++) {
        char key[64];
        snprintf(key, sizeof(key), "levels/filler_%03d.toml", i);
        CHECK(game_profile_record(&session->profile, key, 1, 1, 1.0f) == 0);
    }
    last_warning[0] = '\0';
    SetTraceLogCallback(capture_warning);
    game_complete_level(session->game);
    session_frame(session);
    SetTraceLogCallback(NULL);
    CHECK(strstr(last_warning, "levels/00_sandbox_01.toml") != NULL);
    CHECK(strstr(last_warning, "not recorded") != NULL);
    session_destroy(&session);
    return 0;
fail:
    SetTraceLogCallback(NULL);
    session_destroy(&session);
    return 1;
}

int game_profile_contract_test(void)
{
    puts("profile: codec/storage");
    if (codec_and_storage()) return 1;
    if (legacy_level_keys_are_dropped()) return 1;
    puts("profile: resume codec/migration");
    if (resume_codec_and_migration()) return 1;
    if (level_key_boundaries()) return 1;
    if (pending_snapshot_bookkeeping()) return 1;
    if (result_cap_and_record_failure()) return 1;
    puts("profile: settings/bindings");
    if (settings_and_bindings()) return 1;
    puts("profile: session integration");
    if (persistent_session()) return 1;
    puts("profile: continue round trip");
    if (continue_round_trip()) return 1;
    puts("profile: ghost codec and session");
    if (ghost_codec_and_paths()) return 1;
    if (ghost_session_keeps_the_fastest_run()) return 1;
    puts("profile: start-point runs");
    if (start_point_runs_leave_the_profile_alone()) return 1;
    if (ghost_hidden_on_partial_runs()) return 1;
    puts("game_profile_contract_test: ok");
    return 0;
}
