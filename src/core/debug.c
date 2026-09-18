/* Debug rendering uses the same integer hitbox helpers as gameplay. World
 * rectangles subtract camera X exactly once; HUD rectangles stay in screen
 * space. Per-draw colors do not leak into the following render layer. */
#include "debug.h"
#include "../game.h"
#include "../collision/game_collision.h"
#include <stdio.h>
#include <stdarg.h>
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
            long kb;
            if (sscanf(line,"VmRSS: %ld kB",&kb) == 1) { fclose(file); return (float)kb/1024; }
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

static void draw_collision_boxes(const GameState *gs, int cam)
{
    /* Draw world-space collision extents, not entire transparent sprite slots.
     * The same helpers feed collision detection; outline applies camera X. */
    outline(player_get_hitbox(&gs->player),cam,(Color){0,255,0,255});
    for (int i=0;i<gs->floor_gap_count;i++)
        outline((IntRect){gs->floor_gaps[i],GAME_H-WATER_ART_H,FLOOR_GAP_W,WATER_ART_H},cam,(Color){0,50,200,255});
    for (int i=0;i<gs->platform_count;i++) {
        const Platform *p=&gs->platforms[i];
        outline((IntRect){(int)p->x,(int)p->y,p->w,p->h},cam,(Color){0,100,255,255});
    }
    for (int i=0;i<gs->float_platform_count;i++)
        if (gs->float_platforms[i].active) outline(float_platform_get_rect(&gs->float_platforms[i]),cam,(Color){0,200,180,255});
    for (int i=0;i<gs->bridge_count;i++) {
        const Bridge *b=&gs->bridges[i];
        for (int j=0;j<b->brick_count;j++) if (b->bricks[j].active)
            outline((IntRect){(int)b->x+j*BRIDGE_TILE_W,(int)(b->base_y+b->bricks[j].y_offset),BRIDGE_TILE_W,BRIDGE_TILE_H},cam,(Color){180,120,60,255});
    }
    for (int i=0;i<gs->coin_count;i++) if (gs->coins[i].active)
        outline((IntRect){(int)gs->coins[i].x,(int)gs->coins[i].y,COIN_DISPLAY_W,COIN_DISPLAY_H},cam,(Color){255,255,0,255});
    for (int i=0;i<gs->star_yellow_count;i++) if (gs->star_yellows[i].active)
        outline((IntRect){(int)gs->star_yellows[i].x,(int)gs->star_yellows[i].y,STAR_YELLOW_DISPLAY_W,STAR_YELLOW_DISPLAY_H},cam,(Color){255,0,255,255});
    for (int i=0;i<gs->star_green_count;i++) if (gs->star_greens[i].active)
        outline((IntRect){(int)gs->star_greens[i].x,(int)gs->star_greens[i].y,STAR_YELLOW_DISPLAY_W,STAR_YELLOW_DISPLAY_H},cam,(Color){0,200,0,255});
    for (int i=0;i<gs->star_red_count;i++) if (gs->star_reds[i].active)
        outline((IntRect){(int)gs->star_reds[i].x,(int)gs->star_reds[i].y,STAR_YELLOW_DISPLAY_W,STAR_YELLOW_DISPLAY_H},cam,(Color){200,0,0,255});

    /* Patrol extents follow visible art, rather than transparent sprite slots. */
    for (int i=0;i<gs->spider_count;i++) {
        const Spider *s=&gs->spiders[i];
        outline(spider_build_hitbox(s),cam,(Color){255,0,0,255});
        int y=FLOOR_Y-SPIDER_ART_H/2;
        DrawLine((int)s->patrol_x0+SPIDER_ART_X-cam,y,(int)s->patrol_x1-SPIDER_FRAME_W+SPIDER_ART_X+SPIDER_ART_W-cam,y,(Color){180,0,0,255});
    }
    for (int i=0;i<gs->jumping_spider_count;i++) {
        const JumpingSpider *s=&gs->jumping_spiders[i];
        outline(jumping_spider_build_hitbox(s),cam,(Color){255,0,180,255});
        int y=FLOOR_Y-JSPIDER_ART_H/2;
        DrawLine((int)s->patrol_x0+JSPIDER_ART_X-cam,y,(int)s->patrol_x1-JSPIDER_FRAME_W+JSPIDER_ART_X+JSPIDER_ART_W-cam,y,(Color){180,0,120,255});
    }
    for (int i=0;i<gs->bird_count;i++) {
        const Bird *b=&gs->birds[i];
        outline(bird_get_hitbox(b),cam,(Color){255,160,0,255});
        int y=(int)b->base_y+BIRD_ART_H/2;
        DrawLine((int)b->patrol_x0+BIRD_ART_X-cam,y,(int)b->patrol_x1-BIRD_FRAME_W+BIRD_ART_X+BIRD_ART_W-cam,y,(Color){180,120,0,255});
    }
    for (int i=0;i<gs->faster_bird_count;i++) {
        const FasterBird *b=&gs->faster_birds[i];
        outline(faster_bird_get_hitbox(b),cam,(Color){255,100,200,255});
        int y=(int)b->base_y+FBIRD_ART_H/2;
        DrawLine((int)b->patrol_x0+FBIRD_ART_X-cam,y,(int)b->patrol_x1-FBIRD_FRAME_W+FBIRD_ART_X+FBIRD_ART_W-cam,y,(Color){180,70,140,255});
    }
    for (int i=0;i<gs->fish_count;i++) {
        const Fish *f=&gs->fish[i];
        outline(fish_get_hitbox(f),cam,(Color){255,50,50,255});
        int y=(int)f->water_y+24;
        DrawLine((int)f->patrol_x0+FISH_HITBOX_PAD_X-cam,y,(int)f->patrol_x1-FISH_HITBOX_PAD_X-cam,y,(Color){180,50,50,255});
    }
    for (int i=0;i<gs->spike_block_count;i++) if (gs->spike_blocks[i].active)
        outline(spike_block_get_hitbox(&gs->spike_blocks[i]),cam,(Color){255,140,0,255});
    for (int i=0;i<gs->axe_trap_count;i++) if (gs->axe_traps[i].active) {
        outline(axe_trap_get_hitbox(&gs->axe_traps[i]),cam,(Color){200,0,50,255});
        int x=(int)gs->axe_traps[i].x-cam,y=(int)gs->axe_traps[i].y+4;
        DrawLine(x-3,y,x+3,y,(Color){200,0,50,255});
        DrawLine(x,y-3,x,y+3,(Color){200,0,50,255});
    }
    for (int i=0;i<gs->spike_row_count;i++) if (gs->spike_rows[i].active)
        outline(spike_row_get_rect(&gs->spike_rows[i]),cam,(Color){220,180,0,255});
    for (int i=0;i<gs->spike_platform_count;i++) if (gs->spike_platforms[i].active)
        outline(spike_platform_get_rect(&gs->spike_platforms[i]),cam,(Color){180,0,120,255});
    for (int i=0;i<gs->blue_flame_count;i++) if (gs->blue_flames[i].active && gs->blue_flames[i].state!=BLUE_FLAME_WAITING)
        outline(blue_flame_get_hitbox(&gs->blue_flames[i]),cam,(Color){255,80,0,255});
    for (int i=0;i<gs->fire_flame_count;i++) if (gs->fire_flames[i].active && gs->fire_flames[i].state!=BLUE_FLAME_WAITING)
        outline(blue_flame_get_hitbox(&gs->fire_flames[i]),cam,(Color){255,120,0,255});
    for (int i=0;i<gs->circular_saw_count;i++) if (gs->circular_saws[i].active)
        outline(circular_saw_get_hitbox(&gs->circular_saws[i]),cam,(Color){255,140,0,255});
    for (int i=0;i<gs->faster_fish_count;i++) outline(faster_fish_get_hitbox(&gs->faster_fish[i]),cam,(Color){220,100,180,255});
    if (gs->last_star.active) outline(last_star_get_hitbox(&gs->last_star),cam,(Color){255,215,0,255});
    /* Climbables show their full interaction spans, including tile overlap. */
    for (int i=0;i<gs->ladder_count;i++) {
        const LadderDecor *d=&gs->ladders[i];
        outline((IntRect){(int)d->x,(int)d->y,LADDER_W,(d->tile_count-1)*LADDER_STEP+LADDER_H},cam,(Color){180,120,60,255});
    }
    for (int i=0;i<gs->rope_count;i++) {
        const RopeDecor *r=&gs->ropes[i];
        outline((IntRect){(int)r->x,(int)r->y,ROPE_W,(r->tile_count-1)*ROPE_STEP+ROPE_H},cam,(Color){200,160,100,255});
    }
    const Bouncepad *pads[]={gs->bouncepads_medium,gs->bouncepads_small,gs->bouncepads_high};
    int counts[]={gs->bouncepad_medium_count,gs->bouncepad_small_count,gs->bouncepad_high_count};
    Color colors[]={{0,255,255,255},{0,200,0,255},{255,50,50,255}};
    for (int kind=0;kind<3;kind++) for (int i=0;i<counts[kind];i++) {
        const Bouncepad *p=&pads[kind][i];
        outline((IntRect){(int)p->x+BOUNCEPAD_ART_X,(int)p->y,BOUNCEPAD_ART_W,p->h},cam,colors[kind]);
    }
    for (int i=0;i<gs->vine_count;i++) {
        const VineDecor *v=&gs->vines[i];
        outline((IntRect){(int)v->x-4,(int)v->y,VINE_W+8,(v->tile_count-1)*VINE_STEP+VINE_H},cam,(Color){0,180,0,255});
    }
    for (int i=0;i<gs->rail_count;i++) for (int j=0;j<gs->rails[i].count;j++)
        outline((IntRect){gs->rails[i].tiles[j].x,gs->rails[i].tiles[j].y,RAIL_TILE_W,RAIL_TILE_H},cam,(Color){160,80,255,255});

    /* HUD outlines intentionally use camera=0 and the HUD's measured text. */
    for (int i=0;i<gs->hearts;i++) outline((IntRect){HUD_MARGIN+i*(HUD_HEART_SIZE+HUD_HEART_GAP),HUD_MARGIN,HUD_HEART_SIZE,HUD_HEART_SIZE},0,WHITE);
    int icon_x=HUD_MARGIN+MAX_HEARTS*(HUD_HEART_SIZE+HUD_HEART_GAP)+6;
    int y=HUD_MARGIN+(HUD_ROW_H-13)/2;
    outline((IntRect){icon_x,y,HUD_ICON_W,HUD_ICON_H},0,WHITE);
    char text[32]; int width=0;
    snprintf(text,sizeof(text),"x%d",gs->lives);
    font_measure(gs->hud.font,text,&width,NULL);
    outline((IntRect){icon_x+HUD_ICON_W+4,y,width,13},0,WHITE);
    snprintf(text,sizeof(text),"SCORE: %d",gs->score);
    font_measure(gs->hud.font,text,&width,NULL);
    int score_x=GAME_W-HUD_MARGIN-width-3-HUD_COIN_ICON_SIZE;
    outline((IntRect){score_x,y,width,13},0,WHITE);
    outline((IntRect){score_x+width+3,HUD_MARGIN+(HUD_ROW_H-HUD_COIN_ICON_SIZE)/2,HUD_COIN_ICON_SIZE,HUD_COIN_ICON_SIZE},0,WHITE);
}

static void right_text(TextFont *font, const char *text, int y, Color color)
{
    int width=0;
    font_measure(font,text,&width,NULL);
    font_draw(font,text,GAME_W-HUD_MARGIN-width,y,color);
}

void debug_init(DebugOverlay *dbg)
{
    memset(dbg,0,sizeof(*dbg));
    dbg->fps_prev_ticks=clock_millis();
}

void debug_cleanup(DebugOverlay *dbg) { (void)dbg; }

void debug_update(DebugOverlay *dbg, float dt)
{
    dbg->frame_ms=dt*1000;
    dbg->fps_frame_count++;
    uint64_t now=clock_millis(),elapsed=now-dbg->fps_prev_ticks;
    if (elapsed>=DEBUG_FPS_SAMPLE_MS) {
        dbg->fps_display=(int)(dbg->fps_frame_count*1000/elapsed);
        dbg->fps_frame_count=0;
        dbg->fps_prev_ticks=now;
        dbg->frame_ms_display=dbg->frame_ms;
        dbg->cpu_percent=dbg->frame_ms_display/16.667f*100;
        dbg->mem_mb=get_resident_mb();
    }
    for (int i=0;i<dbg->log_count;i++) dbg->log[i].age+=dt;
}

void debug_log(DebugOverlay *dbg, const char *fmt, ...)
{
    DebugLogEntry *entry=&dbg->log[dbg->log_head];
    va_list args;
    va_start(args,fmt);
    vsnprintf(entry->text,sizeof(entry->text),fmt,args);
    va_end(args);
    entry->age=0;
    dbg->log_head=(dbg->log_head+1)%DEBUG_LOG_MAX_ENTRIES;
    if (dbg->log_count<DEBUG_LOG_MAX_ENTRIES) dbg->log_count++;
}

void debug_render(const DebugOverlay *dbg, TextFont *font, const void *state, int cam)
{
    const GameState *gs=state;
    draw_collision_boxes(gs,cam);
    char text[64];
    Color green={0,255,0,255},yellow={255,255,0,255},red={255,80,80,255};
    int y=HUD_MARGIN+HUD_ROW_H+2;
    snprintf(text,sizeof(text),"FPS: %d",dbg->fps_display);
    right_text(font,text,y,dbg->fps_display>=55?green:dbg->fps_display>=30?yellow:red);
    snprintf(text,sizeof(text),"Frame: %.1fms (%.0f%%)",(double)dbg->frame_ms_display,(double)dbg->cpu_percent);
    right_text(font,text,y+13,dbg->frame_ms_display<12?green:dbg->frame_ms_display<16.7f?yellow:red);
    if (dbg->mem_mb>0) {
        snprintf(text,sizeof(text),"MEM: %.1f MB",(double)dbg->mem_mb);
        right_text(font,text,y+26,(Color){100,220,255,255});
    }
    const Player *p=&gs->player;
    snprintf(text,sizeof(text),"vx:%.0f vy:%.0f",p->vx,p->vy);
    right_text(font,text,GAME_H-34,WHITE);
    static const char *states[]={"IDLE","WALK","JUMP","FALL","CLIMB"};
    static const char *climbs[]={" VINE"," LADDER"," ROPE"};
    snprintf(text,sizeof(text),"%s %s %s%s",states[p->anim_state],p->on_ground?"GND":"AIR",
             p->facing_left?"<-":"->",p->on_vine?climbs[p->climb_source]:"");
    right_text(font,text,GAME_H-48,WHITE);
    if (p->hurt_timer>0) {
        snprintf(text,sizeof(text),"HURT:%.1fs",p->hurt_timer);
        right_text(font,text,GAME_H-62,red);
    }
    IntRect hit=player_get_hitbox(p);
    int cx=hit.x+hit.w/2-cam,cy=hit.y+hit.h/2;
    DrawLine(cx,cy,cx+(int)(p->vx/4),cy+(int)(p->vy/4),green);
    int drawn=0;
    for (int k=0;k<dbg->log_count;k++) {
        int index=(dbg->log_head-1-k+DEBUG_LOG_MAX_ENTRIES)%DEBUG_LOG_MAX_ENTRIES;
        const DebugLogEntry *entry=&dbg->log[index];
        if (entry->age>=DEBUG_LOG_DISPLAY_SEC) continue;
        font_draw(font,entry->text,HUD_MARGIN,GAME_H-20-14*drawn++,entry->age>DEBUG_LOG_DISPLAY_SEC-1?(Color){180,180,180,255}:WHITE);
    }
    int width=0;
    font_measure(font,"DEBUG MODE",&width,NULL);
    font_draw(font,"DEBUG MODE",(GAME_W-width)/2,HUD_MARGIN,yellow);
}
