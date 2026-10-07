#include "game_inspector.h"
#include "../shared/platform.h"  /* str_copy */
#include "game_experiment.h"
#include "game_overlay.h"
#include "game_timing.h"
#include "../levels/level_physics.h"
#include "../screens/settings_menu.h"
#include "../player/player_internal.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

typedef struct { const char *name; size_t offset; } PhysicsField;
#define FIELD(name) {#name, offsetof(Player, name)}
static const PhysicsField fields[] = {
    FIELD(walk_max_speed), FIELD(run_max_speed), FIELD(walk_ground_accel),
    FIELD(run_ground_accel), FIELD(ground_friction), FIELD(ground_counter_accel),
    FIELD(air_accel_walk), FIELD(air_accel_run), FIELD(air_friction)
};
#undef FIELD
_Static_assert(sizeof(fields) / sizeof(fields[0]) == INSPECTOR_PHYSICS_COUNT, "physics tape layout");

const char *game_inspector_physics_name(int index) { return fields[index].name; }

void game_inspector_physics(Player *player, float *values, int apply)
{
    for (int i = 0; i < INSPECTOR_PHYSICS_COUNT; i++) {
        float *field = (float *)((char *)player + fields[i].offset);
        if (apply) *field = values[i];
        else values[i] = *field;
    }
}

void game_inspector_reset_physics(GameState *gs)
{
    level_apply_player_physics(&gs->player, gs->runtime.current_level);
}

/* Keep the tuning line on screen for a few seconds after F6, F7, - or +,
 * so the value being changed is visible without a permanent panel row. */
#define TUNING_VISIBLE_MS 3000
static void show_tuning(GameState *gs)
{
    gs->inspector.tuning_visible_until = clock_millis() + TUNING_VISIBLE_MS;
}

int game_inspector_event(GameState *gs, const InputEvent *event)
{
    if (!gs->debug_mode || event->type != INPUT_KEY_DOWN || event->repeat ||
        (gs->settings_menu && gs->settings_menu->open)) return 0;
    int key = event->key;
    /* Export and the key help remain available on completion/game-over. */
    if (key == KEY_F9) { game_experiment_export(gs); return 1; }
    if (key == KEY_F5) { gs->inspector.show_keys = !gs->inspector.show_keys; return 1; }
    if (game_overlay_blocks_update(gs) || gs->route != GAME_ROUTE_NONE) return 0;
    switch (key) {
    case KEY_F2:
        gs->inspector.frozen = !gs->inspector.frozen;
        gs->inspector.step_requested = 0;
        return 1;
    case KEY_F3:
        gs->inspector.frozen = 1;
        gs->inspector.step_requested = 1;
        return 1;
    case KEY_F4:
        if (gs->experiment && gs->experiment->replaying) {
            debug_log(&gs->debug, "Replay runs at normal speed; F2/F3 freeze and step it");
            return 1;
        }
        gs->inspector.slow_mode = (gs->inspector.slow_mode + 1) % 3;
        return 1;
    case KEY_F6:
        gs->inspector.physics_field = (gs->inspector.physics_field + 1) % INSPECTOR_PHYSICS_COUNT;
        show_tuning(gs);
        return 1;
    case KEY_F10:
        gs->inspector.entity_index = (gs->inspector.entity_index + 1) %
            (1 + gs->fish_count + gs->float_platform_count + gs->circular_saw_count);
        return 1;
    case KEY_F7:
        if (!gs->experiment || !gs->experiment->replaying) game_inspector_reset_physics(gs);
        show_tuning(gs);
        return 1;
    case KEY_F8:
        if (game_experiment_begin(gs)) debug_log(&gs->debug, "Cannot start experiment");
        return 1;
    case KEY_MINUS:
    case KEY_EQUAL: {
        if (gs->experiment && gs->experiment->replaying) return 1;
        float *value = (float *)((char *)&gs->player + fields[gs->inspector.physics_field].offset);
        float next = *value + (key == KEY_EQUAL ? 25.0f : -25.0f);
        if (next >= 0 && next <= MAX_LEVEL_MOTION) *value = next;
        show_tuning(gs);
        return 1;
    }
    default: return 0;
    }
}

int game_inspector_steps(GameState *gs, float frame_seconds)
{
    /* Overlays, settings and routes own the frame. Forget the time that
     * passes meanwhile so resuming does not replay it as a burst of steps. */
    if (game_simulation_blocked(gs)) {
        gs->inspector.step_requested = 0;
        game_timing_restart_clock(gs);
        return 0;
    }
    if (!gs->debug_mode) return game_timing_take_steps(gs, frame_seconds);

    /* Debug inspection changes only how much real time reaches the
     * accumulator; every step it runs is still exactly GAME_FIXED_STEP. */
    int step = gs->inspector.step_requested;
    gs->inspector.step_requested = 0;
    if (step || gs->inspector.frozen) {
        game_timing_restart_clock(gs);
        return step ? 1 : 0;   /* F3 advances exactly one step */
    }
    static const float speeds[] = {1.0f, 0.25f, 0.1f};
    return game_timing_take_steps(gs, frame_seconds * speeds[gs->inspector.slow_mode]);
}

/*
 * describe_inspected — Text for the entity chosen with F10, or NULL when the
 * player is selected (the player already has its own bottom-right readout).
 * entity_index counts the player first, then fish, platforms and saws.
 */
static const char *describe_inspected(const GameState *gs, char *out, size_t size)
{
    int index = gs->inspector.entity_index - 1;
    if (index < 0) return NULL;
    if (index < gs->fish_count) {
        const Fish *fish = &gs->fish[index];
        snprintf(out, size, "FISH %d  y %.0f vy %.0f wait %.2f", index, fish->y, fish->vy, fish->jump_timer);
        return out;
    }
    index -= gs->fish_count;
    if (index < gs->float_platform_count) {
        const FloatPlatform *fp = &gs->float_platforms[index];
        snprintf(out, size, "PLATFORM %d  t %.2f fall %d stand %.2f", index, fp->t, fp->falling, fp->stand_timer);
        return out;
    }
    index -= gs->float_platform_count;
    if (index < gs->circular_saw_count) {
        const CircularSaw *saw = &gs->circular_saws[index];
        snprintf(out, size, "SAW %d  x %.0f dir %d angle %.0f", index, saw->x, saw->direction, saw->spin_angle);
        return out;
    }
    return NULL;
}

/*
 * game_inspector_render — The inspector's part of the debug overlay.
 *
 *   top left : one status line (LIVE / FROZEN / SLOW, recording or replay
 *              progress) plus, only when relevant, the movement value being
 *              tuned and the entity chosen with F10;
 *   centre   : the key list, only while F5 has it open.
 *
 * Performance, the player readout and the event log are drawn by
 * debug_render with the same panel style.
 */
void game_inspector_render(GameState *gs)
{
    if (!gs->debug_mode || !gs->hud.font) return;
    TextFont *font = gs->hud.font;
    const Color green = {120, 230, 120, 255}, cyan = {120, 210, 255, 255};
    const Color yellow = {255, 225, 90, 255}, red = {255, 110, 110, 255};
    const Color dim = {150, 155, 170, 255};

    /* ---- Status line --------------------------------------------- */
    static const char *slow_names[] = {"", "SLOW 0.25x", "SLOW 0.1x"};
    const GameExperiment *tape = gs->experiment;
    char status[64];
    Color status_color = green;
    const char *mode = "LIVE";
    if (gs->inspector.frozen) { mode = "FROZEN"; status_color = cyan; }
    else if (gs->inspector.slow_mode) { mode = slow_names[gs->inspector.slow_mode]; status_color = yellow; }
    if (tape && tape->replaying)
        snprintf(status, sizeof(status), "%s  REPLAY %d/%d", mode, tape->cursor, tape->count);
    else if (tape && tape->recording)
        snprintf(status, sizeof(status), "%s  REC %d", mode, tape->count);
    else
        snprintf(status, sizeof(status), "%s", mode);
    if (tape && tape->recording && !tape->replaying) status_color = red;

    /* ---- Tuning line: while being changed, or while it differs ----- */
    float values[INSPECTOR_PHYSICS_COUNT], authored[INSPECTOR_PHYSICS_COUNT];
    Player level_player = gs->player;
    level_apply_player_physics(&level_player, gs->runtime.current_level);
    game_inspector_physics(&gs->player, values, 0);
    game_inspector_physics(&level_player, authored, 0);
    int field = gs->inspector.physics_field;
    int tuned = values[field] != authored[field];
    char tuning[96];
    if (tuned)
        snprintf(tuning, sizeof(tuning), "%s %.0f (level %.0f)", fields[field].name,
                 values[field], authored[field]);
    else
        snprintf(tuning, sizeof(tuning), "%s %.0f", fields[field].name, values[field]);
    int show_tuning_line = tuned || clock_millis() < gs->inspector.tuning_visible_until;

    char entity[96];
    const char *inspected = describe_inspected(gs, entity, sizeof(entity));

    const char *lines[] = {status, show_tuning_line ? tuning : NULL, inspected,
                           gs->inspector.show_keys ? NULL : "F5 keys"};
    Color colors[] = {status_color, tuned ? yellow : WHITE, cyan, dim};
    debug_draw_panel(font, HUD_MARGIN, DEBUG_PANEL_TOP, 0, lines, colors, 4);

    /* ---- Key help, only while F5 has it open ----------------------
     * A two-column table: the font is proportional, so the action column
     * starts at a measured x instead of being padded with spaces. */
    if (gs->inspector.show_keys) {
        static const char *keys[] = {"F2", "F3", "F4", "F6", "- / +", "F7",
                                     "F8", "F9", "F10", "F5"};
        static const char *actions[] = {
            "freeze / resume", "step one 1/60 s", "speed 1x / 0.25x / 0.1x",
            "next movement field", "tune that field", "reset tuning",
            "record from level start", "export the recording",
            "inspect next entity", "close this help"};
        const int rows = (int)(sizeof(keys) / sizeof(keys[0]));
        const int gap = 10, pad = DEBUG_PANEL_PAD;
        int key_w = 0, action_w = 0, title_w = 0;
        for (int i = 0; i < rows; i++) {
            int w = 0;
            font_measure(font, keys[i], &w, NULL);
            if (w > key_w) key_w = w;
            font_measure(font, actions[i], &w, NULL);
            if (w > action_w) action_w = w;
        }
        font_measure(font, "DEBUG KEYS", &title_w, NULL);
        int box_w = pad * 2 + key_w + gap + action_w;
        int box_h = pad * 2 + (rows + 1) * DEBUG_PANEL_LINE_H;
        int left = (GAME_W - box_w) / 2, top = (GAME_H - box_h) / 2;
        debug_draw_box(left, top, box_w, box_h);
        font_draw(font, "DEBUG KEYS", left + (box_w - title_w) / 2, top + pad, yellow);
        for (int i = 0; i < rows; i++) {
            int row_y = top + pad + (i + 1) * DEBUG_PANEL_LINE_H;
            font_draw(font, keys[i], left + pad, row_y, cyan);
            font_draw(font, actions[i], left + pad + key_w + gap, row_y, WHITE);
        }
    }

    /* Highlight the resolved contact surface and show the physical foot point. */
    int x = (int)(gs->player.x + gs->player.w / 2) - (int)gs->camera.x;
    int y = (int)(gs->player.y + gs->player.h - PLAYER_FLOOR_SINK);
    DrawLine(x-5,y,x+5,y,(Color){gs->player.on_ground ? 0 : 255,255,255,255});
}

/* The inspector draws with the shared HUD font each frame and owns no
 * textures or heap memory; the lifecycle call stays for symmetry. */
void game_inspector_cleanup(GameState *gs)
{
    (void)gs;
}
