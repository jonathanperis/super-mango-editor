/*
 * start_menu.c — A small screen with input, drawing and explicit ownership.
 *
 * The menu borrows the session's catalog/profile/settings. It owns its logical
 * render target, font, logo and confirmation sound. Choosing Play sets a route;
 * AppSession performs the transition after this frame rather than nesting loops.
 */
#include "start_menu.h"
#include "../shared/platform.h"  /* str_copy */
#include "settings_menu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MENU_GAME_W 400
#define MENU_GAME_H 300
/* The logo keeps its 4:3 shape at three quarters of its old size, which
 * leaves room for the level list between it and the Play button. */
#define LOGO_DISPLAY_W 96
#define LOGO_DISPLAY_H 72
#define LOGO_Y 6
#define BTN_W 120
#define BTN_H 28
#define BTN_X ((MENU_GAME_W-BTN_W)/2)
#define BTN_Y 170
/* With a Continue point, Continue and Play sit side by side, 8 px apart. */
#define BTN_PAIR_X ((MENU_GAME_W-2*BTN_W-8)/2)
/* Level list: START_MENU_LIST_ROWS rows of LIST_ROW_H pixels from LIST_Y. */
#define LIST_X 20
#define LIST_W 360
#define LIST_Y 84
#define LIST_ROW_H 15

static int point_in_rect(int x, int y, int rx, int ry, int w, int h)
{
    return x >= rx && x < rx+w && y >= ry && y < ry+h;
}

static void select_level(StartMenu *menu, int index)
{
    /* Wrap at the catalog ends; the manifest, not a directory scan, owns the
     * level order. Copy the path because later transitions outlive this menu. */
    if (!menu->catalog || !menu->catalog->count) return;
    if (index < 0) index = (int)menu->catalog->count-1;
    if ((size_t)index >= menu->catalog->count) index = 0;
    menu->selected_level = index;
    str_copy(menu->selected_level_path, menu->catalog->levels[index].path, sizeof(menu->selected_level_path));
}

void start_menu_get_input_state(const StartMenu *menu, GameInputPhysicalState *state)
{
    game_input_read_physical(menu ? menu->controller : 0, state);
}

static int confirm_held(const StartMenu *menu)
{
    GameInputPhysicalState state;
    start_menu_get_input_state(menu, &state);
    return ((state.keyboard_mask | state.controller_mask) & GAME_INPUT_CONFIRM) != 0;
}

static const CampaignLevel *selected_entry(const StartMenu *menu)
{
    return &menu->catalog->levels[menu->selected_level];
}

void start_menu_format_time(float seconds, char *out, size_t size)
{
    if (!out || !size) return;
    if (!(seconds >= 0.0f)) { snprintf(out, size, "--"); return; }  /* also NaN */
    /* Round once, to whole hundredths, and split that integer. Rounding the
     * seconds part on its own could print 59.996 as "0:60.00". The clamp
     * keeps the multiplication inside long (a best time is at most 1e9 s). */
    if (seconds > 1e7f) seconds = 1e7f;
    long hundredths = (long)(seconds * 100.0f + 0.5f);
    long minutes = hundredths / 6000;
    long whole_seconds = (hundredths / 100) % 60;
    snprintf(out, size, "%ld:%02ld.%02ld", minutes, whole_seconds, hundredths % 100);
}

void start_menu_level_row(const StartMenu *menu, size_t index, StartMenuLevelRow *row)
{
    const CampaignLevel *entry = &menu->catalog->levels[index];
    /* The profile records a result only when a level is finished, so a
     * stored result is the "cleared" flag. A broken entry shows no result:
     * it cannot be played, so an old best would only confuse. */
    const GameProgress *best = entry->available ? game_profile_result(menu->profile, entry->path) : NULL;

    str_copy(row->name, entry->display_name, sizeof(row->name));
    row->available = entry->available;
    row->cleared = best != NULL;
    if (best) {
        start_menu_format_time(best->best_time, row->time, sizeof(row->time));
        snprintf(row->coins, sizeof(row->coins), "%d/%d", best->best_coins, entry->level.coin_count);
    } else {
        snprintf(row->time, sizeof(row->time), "--");
        snprintf(row->coins, sizeof(row->coins), "--");
    }
}

/* First catalog index shown, keeping the selection inside the visible rows. */
static size_t list_first_row(const StartMenu *menu)
{
    size_t count = menu->catalog->count;
    size_t selected = (size_t)menu->selected_level;
    if (count <= START_MENU_LIST_ROWS || selected < START_MENU_LIST_ROWS / 2) return 0;
    size_t first = selected - START_MENU_LIST_ROWS / 2;
    if (first + START_MENU_LIST_ROWS > count) first = count - START_MENU_LIST_ROWS;
    return first;
}

/* Shorten text (whole UTF-8 characters at a time, ending in "..") until it
 * is at most max_width pixels wide, so a long name cannot run into the
 * time column. */
static void fit_text(TextFont *font, char *text, size_t capacity, int max_width)
{
    char original[CAMPAIGN_DISPLAY_NAME_SIZE];
    int width = 0;
    str_copy(original, text, sizeof(original));
    size_t keep = strlen(original);
    while (keep > 0 && font_measure(font, text, &width, NULL) == 0 && width > max_width) {
        /* Keep one whole character fewer: step back over its UTF-8
         * continuation bytes (binary 10xxxxxx) to its first byte. */
        do { keep--; } while (keep > 0 && ((unsigned char)original[keep] & 0xc0) == 0x80);
        snprintf(text, capacity, "%.*s..", (int)keep, original);
    }
}

/*
 * draw_level_list — One row per campaign entry: cleared mark, name, best
 * time and best coins. The selected row is highlighted; a broken entry is
 * drawn in dark grey, like the disabled Play button.
 */
static void draw_level_list(StartMenu *menu)
{
    Color text = {200,200,200,255}, dim = {80,80,80,255}, gold = {240,200,90,255};
    size_t first = list_first_row(menu);
    for (size_t i = first; i < menu->catalog->count && i < first + START_MENU_LIST_ROWS; i++) {
        StartMenuLevelRow row;
        start_menu_level_row(menu, i, &row);
        int y = LIST_Y + (int)(i - first) * LIST_ROW_H;
        if ((int)i == menu->selected_level)
            DrawRectangle(LIST_X, y, LIST_W, LIST_ROW_H, (Color){40,70,105,255});
        int text_y = y + (LIST_ROW_H - TEXT_FONT_SIZE) / 2;
        Color color = row.available ? text : dim;
        /* A star for a cleared level; the default font has no check mark. */
        font_draw(menu->font, row.cleared ? "*" : "-", LIST_X + 6, text_y, row.cleared ? gold : color);
        fit_text(menu->font, row.name, sizeof(row.name), 200);
        font_draw(menu->font, row.name, LIST_X + 18, text_y, color);
        font_draw(menu->font, row.time, LIST_X + 230, text_y, color);
        font_draw(menu->font, row.coins, LIST_X + 300, text_y, color);
    }
}

int start_menu_can_continue(const StartMenu *menu)
{
    /* AppSession drops a Continue point whose level file has changed before
     * it opens this menu, so a stored point for this path is usable. */
    return menu && selected_entry(menu)->available &&
           game_profile_resume(menu->profile, menu->selected_level_path) != NULL;
}

/* Play is centred alone, or on the right of Continue when there is one. */
static IntRect play_button(const StartMenu *menu)
{
    if (start_menu_can_continue(menu)) return (IntRect){BTN_PAIR_X + BTN_W + 8, BTN_Y, BTN_W, BTN_H};
    return (IntRect){BTN_X, BTN_Y, BTN_W, BTN_H};
}

static IntRect continue_button(void)
{
    return (IntRect){BTN_PAIR_X, BTN_Y, BTN_W, BTN_H};
}

/* route is MENU_ROUTE_PLAY (from the start) or MENU_ROUTE_CONTINUE. */
static void start_level(StartMenu *menu, MenuRoute route)
{
    /* A confirm carried from a prior screen must be released before this
     * menu can act on another press. Sound playback accepts a missing slot.
     * An unavailable level is listed so the player can see it is broken,
     * but it cannot start; the menu already shows why. */
    if (menu->route != MENU_ROUTE_NONE) return;
    if (!selected_entry(menu)->available) return;
    if (route == MENU_ROUTE_CONTINUE && !start_menu_can_continue(menu)) return;
    if (menu->confirm_release_required && confirm_held(menu)) return;
    menu->confirm_release_required = 0;
    sound_play(menu->snd_confirm, 128);
    menu->route = route;
}

static void draw_button(StartMenu *menu, IntRect r, const char *label, int enabled)
{
    Vector2 mouse = input_mouse();
    int hovering = point_in_rect((int)mouse.x, (int)mouse.y, r.x, r.y, r.w, r.h);
    /* A disabled button is darker and never highlights on hover. */
    Color color = !enabled ? (Color){45,45,45,255} :
                  hovering ? (Color){74,144,217,255} : (Color){77,77,77,255};
    DrawRectangle(r.x, r.y, r.w, r.h, color);
    DrawRectangleLines(r.x, r.y, r.w, r.h, (Color){224, 224, 224, 255});
    font_draw_centered(menu->font, label, r.x + r.w/2, r.y + (r.h-TEXT_FONT_SIZE)/2,
                       enabled ? WHITE : (Color){110,110,110,255});
}

int start_menu_init(StartMenu *menu)
{
    menu->frame_target = LoadRenderTexture(MENU_GAME_W, MENU_GAME_H);
    if (!IsRenderTextureValid(menu->frame_target)) return -1;
    SetTextureFilter(menu->frame_target.texture, TEXTURE_FILTER_POINT);
    menu->font = font_load();
    if (!menu->font) return -1;
    menu->logo_tex = texture_load("assets/sprites/screens/start_menu_logo.png");
    menu->snd_confirm = sound_load("assets/sounds/screens/confirm_ui.wav");
    /* Start on the first playable entry; a loaded catalog always has one. */
    int first = campaign_first_available(menu->catalog);
    select_level(menu, first >= 0 ? first : 0);
    return 0;
}

StartMenu *start_menu_create(const CampaignCatalog *catalog)
{
    if (!catalog || !catalog->levels || !catalog->count || !IsWindowReady()) return NULL;
    StartMenu *menu = calloc(1, sizeof(*menu));
    if (!menu) return NULL;
    menu->catalog = catalog;
    if (start_menu_init(menu)) {
        start_menu_close(&menu);
        return NULL;
    }
    return menu;
}

void start_menu_set_error(StartMenu *menu, const char *message)
{
    if (!menu) return;
    str_copy(menu->error_message, message ? message : "", sizeof(menu->error_message));
    menu->confirm_release_required = 1;
}

void start_menu_refresh_controller(StartMenu *menu)
{
    if (menu) menu->controller = input_first_gamepad();
}

int start_menu_frame(StartMenu *menu)
{
    if (!menu || !menu->catalog || !menu->catalog->count) return 0;
    if (menu->route != MENU_ROUTE_NONE && !menu->route_waiting_render) return 0;
    start_menu_refresh_controller(menu);
    /* The settings panel gets first refusal. A consumed command must not
     * also change the level selector or start a game underneath that panel. */
    InputEvent event;
    while (input_poll(&event)) {
        if (event.type == INPUT_QUIT) {
            if (menu->route == MENU_ROUTE_NONE) menu->route = MENU_ROUTE_EXIT;
            continue;
        }
        if (menu->route != MENU_ROUTE_NONE) continue;
        if (settings_menu_event(menu->settings_menu, menu->profile, &event, PAD_Y)) continue;
        if (event.type == INPUT_MOUSE_DOWN && event.button == MOUSE_BUTTON_LEFT) {
            int list_h = START_MENU_LIST_ROWS * LIST_ROW_H;
            IntRect play = play_button(menu), resume = continue_button();
            if (menu->settings_menu && point_in_rect(event.x,event.y,125,270,150,24))
                settings_menu_open(menu->settings_menu);
            else if (point_in_rect(event.x, event.y, play.x, play.y, play.w, play.h))
                start_level(menu, MENU_ROUTE_PLAY);
            else if (start_menu_can_continue(menu) &&
                     point_in_rect(event.x, event.y, resume.x, resume.y, resume.w, resume.h))
                start_level(menu, MENU_ROUTE_CONTINUE);
            else if (point_in_rect(event.x, event.y, LIST_X, LIST_Y, LIST_W, list_h)) {
                /* A click on a row selects that level; Play still starts it. */
                size_t row = list_first_row(menu) + (size_t)((event.y - LIST_Y) / LIST_ROW_H);
                if (row < menu->catalog->count) select_level(menu, (int)row);
            }
        } else if ((event.type == INPUT_KEY_DOWN || event.type == INPUT_PAD_DOWN) && !event.repeat) {
            int key = event.type == INPUT_KEY_DOWN ? event.key : KEY_NULL;
            int button = event.type == INPUT_PAD_DOWN ? event.button : -1;
            if (key == KEY_ESCAPE || button == PAD_B || button == PAD_BACK) menu->route = MENU_ROUTE_EXIT;
            else if (key == KEY_LEFT || key == KEY_A || key == KEY_UP || button == PAD_LEFT || button == PAD_UP)
                select_level(menu, menu->selected_level-1);
            else if (key == KEY_RIGHT || key == KEY_D || key == KEY_DOWN || button == PAD_RIGHT || button == PAD_DOWN)
                select_level(menu, menu->selected_level+1);
            else if (key == KEY_C || button == PAD_X)
                start_level(menu, MENU_ROUTE_CONTINUE);
            else if (game_input_event_confirms(&event))
                start_level(menu, MENU_ROUTE_PLAY);
        }
    }
    if (menu->route != MENU_ROUTE_NONE && !menu->route_waiting_render) return 0;
    if (menu->confirm_release_required && !confirm_held(menu)) menu->confirm_release_required = 0;
    /* Draw in logical 400x300 coordinates. Window scaling happens only when
     * display_present copies the completed target to the OS framebuffer. */
    BeginDrawing();
    BeginTextureMode(menu->frame_target);
    ClearBackground(BLACK);
    IntRect logo = {(MENU_GAME_W - LOGO_DISPLAY_W) / 2, LOGO_Y, LOGO_DISPLAY_W, LOGO_DISPLAY_H};
    sprite_draw(menu->logo_tex, NULL, &logo, 0, SPRITE_NORMAL, WHITE);
    draw_level_list(menu);
    const CampaignLevel *entry = selected_entry(menu);
    int can_continue = start_menu_can_continue(menu);
    Color red = {220,120,120,255};
    if (can_continue) draw_button(menu, continue_button(), "Continue", 1);
    draw_button(menu, play_button(menu), "Play", entry->available);
    char text[160];
    Color grey = {120,120,120,255};
    /* Under the Play button: the selected level's full result, or why it
     * cannot be played. The list above already shows every level's best. */
    StartMenuLevelRow row;
    start_menu_level_row(menu, (size_t)menu->selected_level, &row);
    const GameProgress *best = row.cleared ? game_profile_result(menu->profile, entry->path) : NULL;
    if (best) snprintf(text, sizeof(text), "Best: %s  %s coins  %d pts", row.time, row.coins, best->best_score);
    else snprintf(text, sizeof(text), "%s: not cleared yet", entry->display_name);
    font_draw_centered(menu->font, text, MENU_GAME_W/2, 214, entry->available ? grey : (Color){80,80,80,255});
    if (menu->error_message[0])
        font_draw_centered(menu->font, menu->error_message, MENU_GAME_W/2, 232, red);
    else if (!entry->available) {
        snprintf(text, sizeof(text), "Unavailable: %s", entry->problem);
        font_draw_centered(menu->font, text, MENU_GAME_W/2, 232, red);
    } else if (can_continue) {
        /* Say where Continue picks up, so it is not a surprise. */
        const GameResume *resume = game_profile_resume(menu->profile, menu->selected_level_path);
        char at[24];
        if (resume->checkpoint >= 0) snprintf(at, sizeof(at), "checkpoint %d", resume->checkpoint + 1);
        else snprintf(at, sizeof(at), "%s", resume->legacy_screen > 0 ? "last screen reached" : "the start");
        snprintf(text, sizeof(text), "Continue from %s: %d pts, %d lives", at, resume->score, resume->lives);
        font_draw_centered(menu->font, text, MENU_GAME_W/2, 232, (Color){150,200,150,255});
    }
    font_draw_centered(menu->font, can_continue ? "Arrows: level  Enter/A: play  C/X: continue  Esc: exit"
                                                : "Arrows/D-pad: level  Enter/A: play  Esc: exit",
                       MENU_GAME_W/2, 250, grey);
    if (menu->settings_menu) {
        DrawRectangle(125,270,150,24,(Color){50,65,85,255});
        font_draw_centered(menu->font,"Settings (F1 / Y)",MENU_GAME_W/2,270+(24-TEXT_FONT_SIZE)/2,WHITE);
    }
    settings_menu_render(menu->settings_menu,menu->profile,menu->font);
    display_present(menu->frame_target);
    return 1;
}

void start_menu_cleanup(StartMenu *menu)
{
    if (!menu) return;
    /* Free owned screen resources while the shared context is still alive.
     * Clearing slots prevents another cleanup from reusing released handles. */
    sound_unload(menu->snd_confirm);
    menu->snd_confirm = NULL;
    texture_unload(menu->logo_tex);
    menu->logo_tex = NULL;
    font_unload(menu->font);
    menu->font = NULL;
    if (IsRenderTextureValid(menu->frame_target))
        UnloadRenderTexture(menu->frame_target);
    menu->frame_target = (RenderTexture2D){0};
    menu->controller = 0;
}

void start_menu_close(StartMenu **menu)
{
    if (!menu || !*menu) return;
    start_menu_cleanup(*menu);
    free(*menu);
    *menu = NULL;
}
