/* Debug rendering uses the same integer hitbox helpers as gameplay. World
 * rectangles subtract camera X exactly once; HUD rectangles stay in screen
 * space. Per-draw colors do not leak into the following render layer. */
#include "debug.h"
#include "../shared/platform.h"  /* clock_millis */
#include "../game.h"
#include "../collision/game_collision.h"
#include "../player/player_climb.h"   /* the climbables' grab rectangles */
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#if defined(__APPLE__)
#include <mach/mach.h>
#elif defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#include <windows.h>
#include <psapi.h>
#endif

static float get_resident_mb(void)
{
    /* Resident memory is the process's current physical-memory footprint,
     * not the sum of our asset sizes. Each OS exposes it through a different
     * interface; normalize bytes/KiB to MiB for the compact overlay label. */
#if defined(__APPLE__)
    struct mach_task_basic_info info;
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &count) == KERN_SUCCESS)
        return (float)info.resident_size/(1024.0f*1024.0f);
#elif defined(__linux__)
    FILE *file = fopen("/proc/self/status", "r");
    if (file) {
        char line[128];
        while (fgets(line,sizeof(line),file)) {
            if (strncmp(line, "VmRSS:", 6) == 0) {
                char *end = NULL;
                long kb = strtol(line + 6, &end, 10);
                if (end == line + 6) continue;
                fclose(file);
                return (float)kb / 1024;
            }
        }
        fclose(file);
    }
#elif defined(_WIN32)
    PROCESS_MEMORY_COUNTERS memory;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &memory, sizeof(memory)))
        return (float)memory.WorkingSetSize/(1024.0f*1024.0f);
#endif
    return 0;
}

static void outline(IntRect r, int camera, Color color)
{
    DrawRectangleLines(r.x-camera,r.y,r.w,r.h,color);
}

/*
 * damage_source_color — The outline colour for one kind of thing that
 * hurts, looked up by the name its row has in game_collision.c's
 * s_damage_sources table. A kind added there without a colour here is
 * still outlined, in plain red, so it cannot be forgotten.
 */
static Color damage_source_color(const char *name)
{
    static const struct { const char *name; Color color; } colors[] = {
        { "spider",         {255,   0,   0, 255} },
        { "jspider",        {255,   0, 180, 255} },
        { "bird",           {255, 160,   0, 255} },
        { "fbird",          {255, 100, 200, 255} },
        { "fish",           {255,  50,  50, 255} },
        { "ffish",          {220, 100, 180, 255} },
        { "axe",            {200,   0,  50, 255} },
        { "saw",            {255, 140,   0, 255} },
        { "spike_block",    {255, 140,   0, 255} },
        { "spike",          {220, 180,   0, 255} },
        { "spike_platform", {180,   0, 120, 255} },
        { "blue_flame",     {255,  80,   0, 255} },
        { "fire_flame",     {255, 120,   0, 255} },
    };
    for (size_t i = 0; name && i < sizeof(colors) / sizeof(colors[0]); i++)
        if (strcmp(colors[i].name, name) == 0) return colors[i].color;
    return (Color){255, 0, 0, 255};
}

static void draw_collision_boxes(const GameState *gs, int cam)
{
    /* Draw world-space collision extents, not entire transparent sprite slots.
     * The same helpers feed collision detection; outline applies camera X. */
    outline(player_get_hitbox(&gs->world.player),cam,(Color){0,255,0,255});
    for (int i = 0; i < gs->world.floor_gap_count; i++)
        outline((IntRect){gs->world.floor_gaps[i], GAME_H - WATER_ART_H, FLOOR_GAP_W, WATER_ART_H},
                cam, (Color){0, 50, 200, 255});
    for (int i = 0; i < gs->world.platform_count; i++) {
        const Platform *p = &gs->world.platforms[i];
        outline((IntRect){(int)p->x, (int)p->y, p->w, p->h}, cam, (Color){0, 100, 255, 255});
    }
    for (int i=0;i<gs->world.float_platform_count;i++)
        if (gs->world.float_platforms[i].active) outline(float_platform_get_rect(&gs->world.float_platforms[i]),cam,(Color){0,200,180,255});
    /* Crumbling bridges contain independently active bricks; use each brick's
     * current offset, just as collision does, rather than one large bridge box. */
    for (int i = 0; i < gs->world.bridge_count; i++) {
        const Bridge *b = &gs->world.bridges[i];
        for (int j = 0; j < b->brick_count; j++)
            if (b->bricks[j].active)
                outline((IntRect){(int)b->x + j*BRIDGE_TILE_W,
                                  (int)(b->base_y + b->bricks[j].y_offset),
                                  BRIDGE_TILE_W, BRIDGE_TILE_H}, cam, (Color){180,120,60,255});
    }
    for (int i=0;i<gs->world.coin_count;i++) if (gs->world.coins[i].active)
        outline((IntRect){(int)gs->world.coins[i].x,(int)gs->world.coins[i].y,COIN_DISPLAY_W,COIN_DISPLAY_H},cam,(Color){255,255,0,255});
    for (int i=0;i<gs->world.star_yellow_count;i++) if (gs->world.star_yellows[i].active)
        outline(health_star_get_hitbox(&gs->world.star_yellows[i]),cam,(Color){255,0,255,255});
    for (int i=0;i<gs->world.star_green_count;i++) if (gs->world.star_greens[i].active)
        outline(health_star_get_hitbox(&gs->world.star_greens[i]),cam,(Color){0,200,0,255});
    for (int i=0;i<gs->world.star_red_count;i++) if (gs->world.star_reds[i].active)
        outline(health_star_get_hitbox(&gs->world.star_reds[i]),cam,(Color){200,0,0,255});

    /* Everything that hurts on touch: the boxes game_collide tests, asked
     * for from the same table it walks (s_damage_sources), in the kind's
     * colour. An item that cannot hurt right now (inactive, a flame still
     * below its gap) has no box and is not outlined. */
    for (int source = 0; source < game_damage_source_count(); source++) {
        Color color = damage_source_color(game_damage_source_name(source));
        int items = game_damage_source_items(&gs->world, source);
        for (int i = 0; i < items; i++) {
            IntRect box;
            if (game_damage_source_hitbox(&gs->world, source, i, &box)) outline(box, cam, color);
        }
    }

    /* Patrol extents follow visible art, rather than transparent sprite
     * slots. These are ranges of movement, not boxes that hurt. */
    for (int i=0;i<gs->world.spider_count;i++) {
        const Spider *s=&gs->world.spiders[i];
        int y=FLOOR_Y-SPIDER_ART_H/2;
        DrawLine((int)s->patrol_x0+SPIDER_ART_X-cam,y,(int)s->patrol_x1-SPIDER_FRAME_W+SPIDER_ART_X+SPIDER_ART_W-cam,y,(Color){180,0,0,255});
    }
    for (int i=0;i<gs->world.jumping_spider_count;i++) {
        const JumpingSpider *s=&gs->world.jumping_spiders[i];
        int y=FLOOR_Y-JSPIDER_ART_H/2;
        DrawLine((int)s->patrol_x0+JSPIDER_ART_X-cam,y,(int)s->patrol_x1-JSPIDER_FRAME_W+JSPIDER_ART_X+JSPIDER_ART_W-cam,y,(Color){180,0,120,255});
    }
    for (int i=0;i<gs->world.bird_count;i++) {
        const Bird *b=&gs->world.birds[i];
        int y=(int)b->base_y+BIRD_ART_H/2;
        DrawLine((int)b->patrol_x0+BIRD_ART_X-cam,y,(int)b->patrol_x1-BIRD_FRAME_W+BIRD_ART_X+BIRD_ART_W-cam,y,(Color){180,120,0,255});
    }
    for (int i=0;i<gs->world.faster_bird_count;i++) {
        const FasterBird *b=&gs->world.faster_birds[i];
        int y=(int)b->base_y+FBIRD_ART_H/2;
        DrawLine((int)b->patrol_x0+FBIRD_ART_X-cam,y,(int)b->patrol_x1-FBIRD_FRAME_W+FBIRD_ART_X+FBIRD_ART_W-cam,y,(Color){180,70,140,255});
    }
    for (int i=0;i<gs->world.fish_count;i++) {
        const Fish *f=&gs->world.fish[i];
        int y=(int)f->water_y+24;
        DrawLine((int)f->patrol_x0+FISH_HITBOX_PAD_X-cam,y,(int)f->patrol_x1-FISH_HITBOX_PAD_X-cam,y,(Color){180,50,50,255});
    }
    /* An axe trap's pivot: a small cross where it hangs from. */
    for (int i = 0; i < gs->world.axe_trap_count; i++)
        if (gs->world.axe_traps[i].active) {
            int x = (int)gs->world.axe_traps[i].x - cam, y = (int)gs->world.axe_traps[i].y + 4;
            DrawLine(x - 3, y, x + 3, y, (Color){200,0,50,255});
            DrawLine(x, y - 3, x, y + 3, (Color){200,0,50,255});
        }
    if (gs->world.last_star.active) outline(last_star_get_hitbox(&gs->world.last_star),cam,(Color){255,215,0,255});
    /* Climbables: the very rectangles the grab test uses (player_climb.c),
     * a few pixels wider than the sprite on each side. Drawing a rectangle
     * worked out again here would show a box the game does not test. */
    for (int i = 0; i < gs->world.ladder_count; i++)
        outline(player_ladder_grab_rect(&gs->world.ladders[i]), cam, (Color){180,120,60,255});
    for (int i = 0; i < gs->world.rope_count; i++)
        outline(player_rope_grab_rect(&gs->world.ropes[i]), cam, (Color){200,160,100,255});
    const Bouncepad *pads[] = {gs->world.bouncepads_medium, gs->world.bouncepads_small, gs->world.bouncepads_high};
    int counts[] = {gs->world.bouncepad_medium_count, gs->world.bouncepad_small_count, gs->world.bouncepad_high_count};
    Color colors[] = {{0,255,255,255}, {0,200,0,255}, {255,50,50,255}};
    for (int kind = 0; kind < 3; kind++)
        for (int i = 0; i < counts[kind]; i++) {
            const Bouncepad *p = &pads[kind][i];
            outline((IntRect){(int)p->x + BOUNCEPAD_ART_X, (int)p->y, BOUNCEPAD_ART_W, p->h},
                    cam, colors[kind]);
        }
    for (int i = 0; i < gs->world.vine_count; i++)
        outline(player_vine_grab_rect(&gs->world.vines[i]), cam, (Color){0,180,0,255});
    for (int i=0;i<gs->world.rail_count;i++) for (int j=0;j<gs->world.rails[i].count;j++)
        outline((IntRect){gs->world.rails[i].tiles[j].x,gs->world.rails[i].tiles[j].y,RAIL_TILE_W,RAIL_TILE_H},cam,(Color){160,80,255,255});

    /* HUD outlines use camera=0 and the very rectangles hud_render draws. */
    HudLayout hud;
    hud_layout(&gs->screen.hud, gs->world.hearts, gs->world.lives, gs->world.score, &hud);
    for (int i = 0; i < hud.heart_count; i++) outline(hud.hearts[i], 0, WHITE);
    outline(hud.player_icon, 0, WHITE);
    outline(hud.lives_text, 0, WHITE);
    outline(hud.score_text, 0, WHITE);
    outline(hud.coin_icon, 0, WHITE);
}

void debug_draw_box(int x, int y, int w, int h)
{
    DrawRectangle(x, y, w, h, (Color){8, 12, 20, 200});
    DrawRectangleLines(x, y, w, h, (Color){70, 80, 100, 220});
}

int debug_draw_panel(TextFont *font, int x, int y, int align_right,
                     const char *const *lines, const Color *colors, int count)
{
    const int pad = DEBUG_PANEL_PAD;
    int width = 0, shown = 0;

    if (!font || count <= 0) return 0;
    /* Size the box to the widest line so short readouts stay small. */
    for (int i = 0; i < count; i++) {
        int w = 0;
        if (!lines[i]) continue;
        font_measure(font, lines[i], &w, NULL);
        if (w > width) width = w;
        shown++;
    }
    if (shown == 0) return 0;

    int box_w = width + 2 * pad;
    int box_h = shown * DEBUG_PANEL_LINE_H + 2 * pad - 2;
    int left = align_right ? x - box_w : x;
    debug_draw_box(left, y, box_w, box_h);

    int row = 0;
    for (int i = 0; i < count; i++) {
        if (!lines[i]) continue;
        Color color = colors ? colors[i] : WHITE;
        font_draw(font, lines[i], left + pad, y + pad + row * DEBUG_PANEL_LINE_H, color);
        row++;
    }
    return box_h;
}

void debug_init(DebugOverlay *dbg)
{
    memset(dbg, 0, sizeof(*dbg));
    dbg->fps_prev_ticks = clock_millis();
}

/* This overlay stores fixed-size values and borrows the HUD font at draw time;
 * it has no owned heap/GPU resource to release. Keep the lifecycle explicit. */
void debug_cleanup(DebugOverlay *dbg)
{
    (void)dbg;
}

void debug_update(DebugOverlay *dbg, float dt)
{
    /* dt arrives in seconds. Refresh readable counters over a short interval
     * instead of changing every label on every frame. This is an inspection
     * aid, not a CPU profiler: frame_ms is the interval between frames. */
    dbg->frame_ms = dt * 1000;
    dbg->fps_frame_count++;
    uint64_t now = clock_millis(), elapsed = now - dbg->fps_prev_ticks;
    if (elapsed >= DEBUG_FPS_SAMPLE_MS) {
        /* Show the nearest measured FPS: truncation makes a healthy 59.99 Hz
         * cadence read 59. This does not clamp slow frames to the target. */
        dbg->fps_display = (int)(dbg->fps_frame_count * 1000.0 / elapsed + 0.5);
        dbg->fps_frame_count = 0;
        dbg->fps_prev_ticks = now;
        dbg->frame_ms_display = dbg->frame_ms;
        dbg->mem_mb = get_resident_mb();
    }
    for (int i = 0; i < dbg->log_count; i++)
        dbg->log[i].age += dt;
}

void debug_log(DebugOverlay *dbg, const char *fmt, ...)
{
    /* A ring buffer bounds memory: overwrite the oldest slot when full.
     * va_list/vsnprintf accept printf-style arguments without an unbounded copy. */
    DebugLogEntry *entry = &dbg->log[dbg->log_head];
    va_list args;
    va_start(args, fmt);
    vsnprintf(entry->text, sizeof(entry->text), fmt, args);
    va_end(args);
    entry->age = 0;
    dbg->log_head = (dbg->log_head + 1) % DEBUG_LOG_MAX_ENTRIES;
    if (dbg->log_count < DEBUG_LOG_MAX_ENTRIES) dbg->log_count++;
}

void debug_render(const DebugOverlay *dbg, TextFont *font, const void *state, int cam)
{
    const GameState *gs=state;
    draw_collision_boxes(gs,cam);
    Color green={0,255,0,255},yellow={255,255,0,255},red={255,80,80,255};

    /* ---- Top right: performance, one line ------------------------- */
    char perf[64];
    if (dbg->mem_mb > 0)
        snprintf(perf, sizeof(perf), "%d FPS  %.1fms  %.0fMB", dbg->fps_display,
                 (double)dbg->frame_ms_display, (double)dbg->mem_mb);
    else
        snprintf(perf, sizeof(perf), "%d FPS  %.1fms", dbg->fps_display,
                 (double)dbg->frame_ms_display);
    const char *perf_lines[] = {perf};
    Color perf_color = dbg->fps_display >= 55 ? green : dbg->fps_display >= 30 ? yellow : red;
    debug_draw_panel(font, GAME_W - HUD_MARGIN, DEBUG_PANEL_TOP, 1, perf_lines, &perf_color, 1);

    /* ---- Bottom right: the player, one line ------------------------
     * State, ground/air, facing and velocity always; checkpoint, riding
     * platform and hurt time only while they mean something. */
    const Player *p=&gs->world.player;
    static const char *states[]={"IDLE","WALK","JUMP","FALL","CLIMB"};
    static const char *climbs[]={" VINE"," LADDER"," ROPE"};
    char player_line[96], extras[48] = "";
    if (gs->world.checkpoint_index >= 0)
        snprintf(extras + strlen(extras), sizeof(extras) - strlen(extras), " CP%d", gs->world.checkpoint_index);
    if (gs->screen.loop.fp_prev_riding >= 0)
        snprintf(extras + strlen(extras), sizeof(extras) - strlen(extras), " FP%d", gs->screen.loop.fp_prev_riding);
    snprintf(player_line, sizeof(player_line), "%s %s %s%s  vx %.0f vy %.0f%s",
             states[p->anim_state], p->on_ground ? "GND" : "AIR", p->facing_left ? "<" : ">",
             p->on_vine ? climbs[p->climb_source] : "", p->vx, p->vy, extras);
    char hurt_line[24];
    snprintf(hurt_line, sizeof(hurt_line), "HURT %.1fs", p->hurt_timer);
    const char *player_lines[] = {p->hurt_timer > 0 ? hurt_line : NULL, player_line};
    Color player_colors[] = {red, WHITE};
    int player_rows = p->hurt_timer > 0 ? 2 : 1;
    debug_draw_panel(font, GAME_W - HUD_MARGIN,
                     GAME_H - HUD_MARGIN - player_rows * DEBUG_PANEL_LINE_H - 4, 1,
                     player_lines, player_colors, 2);

    /* The arrow illustrates a quarter-second of current velocity; it does
     * not advance simulation. Translate its world-space center by camera X. */
    IntRect hit = player_get_hitbox(p);
    int cx = hit.x + hit.w/2 - cam, cy = hit.y + hit.h/2;
    DrawLine(cx, cy, cx + (int)(p->vx/4), cy + (int)(p->vy/4), green);

    /* ---- Bottom left: recent events, newest at the bottom ---------- */
    const char *log_lines[DEBUG_LOG_MAX_ENTRIES];
    Color log_colors[DEBUG_LOG_MAX_ENTRIES];
    int shown = 0;
    /* Walk oldest-to-newest through the ring, skipping expired messages. */
    for (int k = dbg->log_count - 1; k >= 0; k--) {
        int index = (dbg->log_head - 1 - k + DEBUG_LOG_MAX_ENTRIES) % DEBUG_LOG_MAX_ENTRIES;
        const DebugLogEntry *entry = &dbg->log[index];
        if (entry->age >= DEBUG_LOG_DISPLAY_SEC) continue;
        log_lines[shown] = entry->text;
        log_colors[shown] = entry->age > DEBUG_LOG_DISPLAY_SEC - 1 ? (Color){160,160,160,255} : WHITE;
        shown++;
    }
    debug_draw_panel(font, HUD_MARGIN, GAME_H - HUD_MARGIN - shown * DEBUG_PANEL_LINE_H - 4, 0,
                     log_lines, log_colors, shown);

    font_draw_centered(font, "DEBUG MODE", GAME_W / 2, HUD_MARGIN, yellow);
}
