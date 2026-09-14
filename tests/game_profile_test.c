#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>
#include "core/app_session.h"
#include "core/game_profile.h"
#include "editor/serializer_io.h"
#include "input/game_input.h"
#include "screens/settings_menu.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

#define CHECK(condition) do { if (!(condition)) { fprintf(stderr,"profile test failed: %s:%d: %s\n",__FILE__,__LINE__,#condition); goto fail; } } while (0)

static int codec_and_storage(void)
{
    GameProfile *a = calloc(1,sizeof(*a)), *b = calloc(1,sizeof(*b));
    GameProfileData *decoded = calloc(1,sizeof(*decoded));
    char *text = malloc(PROFILE_TEXT_MAX);
    char path[160];
    char lock_path[176];
    snprintf(path,sizeof(path),"out/profile-test-%llu.toml",(unsigned long long)SDL_GetPerformanceCounter());
    snprintf(lock_path,sizeof(lock_path),"%s.lock",path);
    CHECK(a && b && decoded && text);
    game_profile_init(a); game_profile_init(b);
    a->data.settings.keys[BIND_JUMP] = SDL_SCANCODE_J;
    a->data.settings.buttons[BIND_RUN] = SDL_CONTROLLER_BUTTON_X;
    a->data.settings.dead_zone = 10000;
    game_profile_select(a,"levels/café.toml");
    CHECK(game_profile_record(a,"levels/café.toml",100,3,12.5f)==0);
    CHECK(game_profile_record(a,"levels/café.toml",90,2,20.0f)==0);
    CHECK(game_profile_record(a,"levels/café.toml",150,1,10.0f)==0);
    CHECK(game_profile_encode(&a->data,text,PROFILE_TEXT_MAX)==0);
    CHECK(game_profile_decode(decoded,text)==0);
    CHECK(decoded->settings.keys[BIND_JUMP]==SDL_SCANCODE_J && decoded->count==1);
    CHECK(decoded->levels[0].best_score==150 && decoded->levels[0].best_coins==3 && decoded->levels[0].best_time==10);
    const char *bad[] = {"format_version=2\n", "format_version=1\nmuted=2\n", "format_version=1\ndead_zone=30000\n",
        "format_version=1\nkeys=[4,4,26,22,44,225]\n", "format_version=1\nwindow_scale=nan\n",
        "format_version=1\nunknown=1\n", "format_version=1\nlast_level=\"levels/a\\u0000.toml\"\n"};
    for (size_t i=0;i<sizeof(bad)/sizeof(bad[0]);i++) {
        CHECK(game_profile_decode(decoded,bad[i])==-1);
        CHECK(decoded->count==1 && decoded->settings.keys[BIND_JUMP]==SDL_SCANCODE_J);
    }
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
    SDL_Event event; SDL_zero(event); event.type=SDL_KEYDOWN; event.key.keysym.sym=SDLK_F1;
    int handled = settings_menu_event(menu,profile,&event,SDL_CONTROLLER_BUTTON_BACK);
    if (handled != 1 || !menu->open) fprintf(stderr,"settings input: type=%u key=%d repeat=%u handled=%d open=%d\n",
        event.type,event.key.keysym.sym,event.key.repeat,handled,menu->open);
    CHECK(handled==1 && menu->open);
    menu->page=1; menu->selected=BIND_JUMP;
    event.key.keysym.sym=SDLK_RETURN;
    CHECK(settings_menu_event(menu,profile,&event,SDL_CONTROLLER_BUTTON_BACK)==1 && menu->capture==1);
    event.key.keysym.sym=SDLK_a; event.key.keysym.scancode=SDL_SCANCODE_A;
    settings_menu_event(menu,profile,&event,SDL_CONTROLLER_BUTTON_BACK);
    CHECK(menu->capture==1 && profile->data.settings.keys[BIND_JUMP]==SDL_SCANCODE_SPACE);
    event.key.keysym.sym=SDLK_j; event.key.keysym.scancode=SDL_SCANCODE_J;
    settings_menu_event(menu,profile,&event,SDL_CONTROLLER_BUTTON_BACK);
    CHECK(menu->capture==0 && profile->data.settings.keys[BIND_JUMP]==SDL_SCANCODE_J);
    Uint8 keys[SDL_NUM_SCANCODES]={0}, buttons[SDL_CONTROLLER_BUTTON_MAX]={0};
    keys[SDL_SCANCODE_J]=1;
    CHECK(game_input_keyboard_mask(keys,&profile->data.settings)&PLAYER_INPUT_JUMP);
    keys[SDL_SCANCODE_J]=0; keys[SDL_SCANCODE_SPACE]=1;
    CHECK(!(game_input_keyboard_mask(keys,&profile->data.settings)&PLAYER_INPUT_JUMP));
    menu->selected=PROFILE_ACTION_COUNT+BIND_RUN;
    event.key.keysym.sym=SDLK_RETURN;
    settings_menu_event(menu,profile,&event,SDL_CONTROLLER_BUTTON_BACK);
    event.type=SDL_CONTROLLERBUTTONDOWN; event.cbutton.button=SDL_CONTROLLER_BUTTON_X;
    settings_menu_event(menu,profile,&event,SDL_CONTROLLER_BUTTON_BACK);
    CHECK(profile->data.settings.buttons[BIND_RUN]==SDL_CONTROLLER_BUTTON_X);
    buttons[SDL_CONTROLLER_BUTTON_X]=1;
    profile->data.settings.dead_zone=10000;
    CHECK(game_input_controller_mask(buttons,9000,0,&profile->data.settings)==PLAYER_INPUT_RUN);
    CHECK(game_input_controller_mask(buttons,12000,0,&profile->data.settings)==(PLAYER_INPUT_RUN|PLAYER_INPUT_RIGHT));
    event.cbutton.button=SDL_CONTROLLER_BUTTON_BACK;
    settings_menu_event(menu,profile,&event,SDL_CONTROLLER_BUTTON_BACK);
    CHECK(!menu->open);
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
    snprintf(path,sizeof(path),"out/profile-session-%llu.toml",(unsigned long long)SDL_GetPerformanceCounter());
    snprintf(lock_path,sizeof(lock_path),"%s.lock",path);
    AppSessionConfig config={.level_path="levels/00_onboarding_01.toml",.profile_enabled=1,.profile_path=path};
    puts("profile session: create");
    AppSession *session=session_create(&config);
    CHECK(session);
    SDL_Event event; SDL_zero(event); event.type=SDL_KEYDOWN; event.key.keysym.sym=SDLK_F1;
    puts("profile session: open settings");
    SDL_PushEvent(&event); session_frame(session);
    CHECK(session->settings.open && session->game->completion.level_elapsed==0);
    session->profile.data.settings.muted=1;
    session->profile.data.settings.high_contrast=1;
    session->profile.data.settings.reduced_motion=1;
    session->profile.data.settings.window_scale=3;
    session->profile.dirty=1; session->profile.revision++;
    event.key.keysym.sym=SDLK_ESCAPE;
    puts("profile session: close settings");
    SDL_PushEvent(&event); session_frame(session);
    CHECK(!session->settings.open && Mix_VolumeMusic(-1)==0);
    int width,height; SDL_GetWindowSize(session->game->window,&width,&height);
    CHECK(width==1200 && height==900);
    session->game->score=200;
    game_complete_level(session->game);
    puts("profile session: completion");
    session_frame(session);
    CHECK(game_profile_result(&session->profile,"levels/00_onboarding_01.toml"));
    puts("profile session: destroy");
    session_destroy(&session);
    CHECK(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_EVENTS)==0);
    CHECK((IMG_Init(IMG_INIT_PNG)&IMG_INIT_PNG) && TTF_Init()==0 && Mix_OpenAudio(44100,MIX_DEFAULT_FORMAT,2,2048)==0);
    config.level_path=NULL; config.continue_last=1;
    puts("profile session: continue");
    session=session_create(&config);
    CHECK(session && session->game && session->profile.data.settings.muted==1);
    CHECK(!strcmp(session->game->profile_level_key,"levels/00_onboarding_01.toml"));
    CHECK(session->profile.data.count==1);
    session_destroy(&session);
    CHECK(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_EVENTS)==0);
    CHECK((IMG_Init(IMG_INIT_PNG)&IMG_INIT_PNG) && TTF_Init()==0 && Mix_OpenAudio(44100,MIX_DEFAULT_FORMAT,2,2048)==0);
    config.level_path="levels/00_onboarding_01.toml"; config.smoke_test_frames=1;
    session=session_create(&config);
    CHECK(session && !session->profile.enabled && !session->profile.baseline);
    CHECK(!session->profile.data.settings.muted);
    session_destroy(&session);
    CHECK(serializer_probe_path_utf8(path)==SERIALIZER_PATH_EXISTING);
    remove(path);
    remove(lock_path);
    CHECK(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_EVENTS)==0);
    CHECK((IMG_Init(IMG_INIT_PNG)&IMG_INIT_PNG) && TTF_Init()==0 && Mix_OpenAudio(44100,MIX_DEFAULT_FORMAT,2,2048)==0);
    return 0;
fail:
    session_destroy(&session); remove(path); remove(lock_path); return 1;
}

int game_profile_contract_test(void)
{
    puts("profile: codec/storage");
    if (codec_and_storage()) return 1;
    if (pending_snapshot_bookkeeping()) return 1;
    puts("profile: settings/bindings");
    if (settings_and_bindings()) return 1;
    puts("profile: session integration");
    if (persistent_session()) return 1;
    puts("game_profile_contract_test: ok");
    return 0;
}
