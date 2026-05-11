/*
 * serializer_save.c — TOML writer for level definitions.
 */

#include <stdio.h>  /* FILE, fprintf, fclose */

#include "serializer.h"
#include "serializer_emit.h"
#include "serializer_io.h"
#include "serializer_types.h"

/* ================================================================== */
/* level_save_toml — Write a LevelDef to a human-readable TOML file    */
/* ================================================================== */

int level_save_toml(const LevelDef *def, const char *path) {
    if (!def || !path) return -1;

    FILE *fp = serializer_open_write(path);
    if (!fp) {
        fprintf(stderr, "serializer: cannot open '%s' for writing\n", path);
        return -1;
    }

    /* ---- Header fields ------------------------------------------- */

    write_toml_key_string(fp, "name", def->name[0] ? def->name : "Untitled");

    if (def->description[0] != '\0') {
        write_toml_key_string(fp, "description", def->description);
    }

    if (def->generated_by[0] != '\0') {
        write_toml_key_string(fp, "generated_by", def->generated_by);
    }

    fprintf(fp, "screen_count = %d\n", def->screen_count);

    /* ---- Level-wide configuration (must come before [[arrays]]) --- */

    fprintf(fp, "player_start_x = %s\n", fmt_float(def->player_start_x));
    fprintf(fp, "player_start_y = %s\n", fmt_float(def->player_start_y));
    write_toml_key_string(fp, "music_path", def->music_path);
    fprintf(fp, "music_volume = %d\n", def->music_volume);
    write_toml_key_string(fp, "floor_tile_path", def->floor_tile_path);
    fprintf(fp, "initial_hearts = %d\n", def->initial_hearts);
    fprintf(fp, "initial_lives = %d\n", def->initial_lives);
    fprintf(fp, "score_per_life = %d\n", def->score_per_life);
    fprintf(fp, "coin_score = %d\n", def->coin_score);

    /* ---- Floor gaps (plain integer array) ------------------------- */

    if (def->floor_gap_count > 0) {
        fprintf(fp, "floor_gaps = [");
        for (int i = 0; i < def->floor_gap_count; i++) {
            if (i > 0) fprintf(fp, ", ");
            fprintf(fp, "%d", def->floor_gaps[i]);
        }
        fprintf(fp, "]\n");
    }

    /* ---- Player movement physics ---------------------------------- */
    /*
     * Always written as a [physics] table so the level file documents the
     * current values.  The engine treats any value >= 0.0 as an override;
     * -1.0 means "use the #define default".  Set a field to -1.0 in the
     * editor to let the engine pick its own value for that parameter.
     */
    fprintf(fp, "\n[physics]\n");
    fprintf(fp, "walk_max_speed       = %s\n", fmt_float(def->physics.walk_max_speed));
    fprintf(fp, "run_max_speed        = %s\n", fmt_float(def->physics.run_max_speed));
    fprintf(fp, "walk_ground_accel    = %s\n", fmt_float(def->physics.walk_ground_accel));
    fprintf(fp, "run_ground_accel     = %s\n", fmt_float(def->physics.run_ground_accel));
    fprintf(fp, "ground_friction      = %s\n", fmt_float(def->physics.ground_friction));
    fprintf(fp, "ground_counter_accel = %s\n", fmt_float(def->physics.ground_counter_accel));
    fprintf(fp, "air_accel_walk       = %s\n", fmt_float(def->physics.air_accel_walk));
    fprintf(fp, "air_accel_run        = %s\n", fmt_float(def->physics.air_accel_run));
    fprintf(fp, "air_friction         = %s\n", fmt_float(def->physics.air_friction));
    fprintf(fp, "cam_lookahead_vx_factor = %s\n", fmt_float(def->physics.cam_lookahead_vx_factor));
    fprintf(fp, "cam_lookahead_max    = %s\n", fmt_float(def->physics.cam_lookahead_max));
    fprintf(fp, "\n");

    /* ---- Rails --------------------------------------------------- */

    for (int i = 0; i < def->rail_count; i++) {
        const RailPlacement *r = &def->rails[i];
        fprintf(fp, "[[rails]]\n");
        write_toml_key_string(fp, "layout", serializer_rail_layout_to_str(r->layout));
        fprintf(fp, "x = %d\n", r->x);
        fprintf(fp, "y = %d\n", r->y);
        fprintf(fp, "w = %d\n", r->w);
        fprintf(fp, "h = %d\n", r->h);
        fprintf(fp, "end_cap = %d\n", r->end_cap);
        fprintf(fp, "\n");
    }

    /* ---- Platforms ------------------------------------------------ */

    for (int i = 0; i < def->platform_count; i++) {
        const PlatformPlacement *p = &def->platforms[i];
        fprintf(fp, "[[platforms]]\n");
        fprintf(fp, "x = %s\n", fmt_float(p->x));
        fprintf(fp, "tile_height = %d\n", p->tile_height);
        fprintf(fp, "tile_width = %d\n", p->tile_width);
        if (p->tile_path[0] != '\0') {
            write_toml_key_string(fp, "tile_path", p->tile_path);
        }
        fprintf(fp, "\n");
    }

    /* ---- Coins --------------------------------------------------- */

    for (int i = 0; i < def->coin_count; i++) {
        const CoinPlacement *c = &def->coins[i];
        fprintf(fp, "[[coins]]\n");
        fprintf(fp, "x = %s\n", fmt_float(c->x));
        fprintf(fp, "y = %s\n", fmt_float(c->y));
        fprintf(fp, "\n");
    }

    /* ---- Star yellows --------------------------------------------- */

    for (int i = 0; i < def->star_yellow_count; i++) {
        const StarYellowPlacement *s = &def->star_yellows[i];
        fprintf(fp, "[[star_yellows]]\n");
        fprintf(fp, "x = %s\n", fmt_float(s->x));
        fprintf(fp, "y = %s\n", fmt_float(s->y));
        fprintf(fp, "\n");
    }

    /* ---- Star greens ---------------------------------------------- */

    for (int i = 0; i < def->star_green_count; i++) {
        const StarGreenPlacement *s = &def->star_greens[i];
        fprintf(fp, "[[star_greens]]\n");
        fprintf(fp, "x = %s\n", fmt_float(s->x));
        fprintf(fp, "y = %s\n", fmt_float(s->y));
        fprintf(fp, "\n");
    }

    /* ---- Star reds ------------------------------------------------ */

    for (int i = 0; i < def->star_red_count; i++) {
        const StarRedPlacement *s = &def->star_reds[i];
        fprintf(fp, "[[star_reds]]\n");
        fprintf(fp, "x = %s\n", fmt_float(s->x));
        fprintf(fp, "y = %s\n", fmt_float(s->y));
        fprintf(fp, "\n");
    }

    /* ---- Last star (single table, not array) --------------------- */

    {
        const LastStarPlacement *ls = &def->last_star;
        fprintf(fp, "[last_star]\n");
        fprintf(fp, "x = %s\n", fmt_float(ls->x));
        fprintf(fp, "y = %s\n", fmt_float(ls->y));
        if (def->next_phase[0] != '\0') {
            write_toml_key_string(fp, "next_phase", def->next_phase);
        }
        fprintf(fp, "\n");
    }

    /* ---- Spiders ------------------------------------------------- */

    for (int i = 0; i < def->spider_count; i++) {
        const SpiderPlacement *sp = &def->spiders[i];
        fprintf(fp, "[[spiders]]\n");
        fprintf(fp, "x = %s\n", fmt_float(sp->x));
        fprintf(fp, "vx = %s\n", fmt_float(sp->vx));
        fprintf(fp, "patrol_x0 = %s\n", fmt_float(sp->patrol_x0));
        fprintf(fp, "patrol_x1 = %s\n", fmt_float(sp->patrol_x1));
        fprintf(fp, "frame_index = %d\n", sp->frame_index);
        fprintf(fp, "\n");
    }

    /* ---- Jumping spiders ----------------------------------------- */

    for (int i = 0; i < def->jumping_spider_count; i++) {
        const JumpingSpiderPlacement *js = &def->jumping_spiders[i];
        fprintf(fp, "[[jumping_spiders]]\n");
        fprintf(fp, "x = %s\n", fmt_float(js->x));
        fprintf(fp, "vx = %s\n", fmt_float(js->vx));
        fprintf(fp, "patrol_x0 = %s\n", fmt_float(js->patrol_x0));
        fprintf(fp, "patrol_x1 = %s\n", fmt_float(js->patrol_x1));
        fprintf(fp, "\n");
    }

    /* ---- Birds --------------------------------------------------- */

    for (int i = 0; i < def->bird_count; i++) {
        const BirdPlacement *b = &def->birds[i];
        fprintf(fp, "[[birds]]\n");
        fprintf(fp, "x = %s\n", fmt_float(b->x));
        fprintf(fp, "base_y = %s\n", fmt_float(b->base_y));
        fprintf(fp, "vx = %s\n", fmt_float(b->vx));
        fprintf(fp, "patrol_x0 = %s\n", fmt_float(b->patrol_x0));
        fprintf(fp, "patrol_x1 = %s\n", fmt_float(b->patrol_x1));
        fprintf(fp, "frame_index = %d\n", b->frame_index);
        fprintf(fp, "\n");
    }

    /* ---- Faster birds -------------------------------------------- */

    for (int i = 0; i < def->faster_bird_count; i++) {
        const BirdPlacement *b = &def->faster_birds[i];
        fprintf(fp, "[[faster_birds]]\n");
        fprintf(fp, "x = %s\n", fmt_float(b->x));
        fprintf(fp, "base_y = %s\n", fmt_float(b->base_y));
        fprintf(fp, "vx = %s\n", fmt_float(b->vx));
        fprintf(fp, "patrol_x0 = %s\n", fmt_float(b->patrol_x0));
        fprintf(fp, "patrol_x1 = %s\n", fmt_float(b->patrol_x1));
        fprintf(fp, "frame_index = %d\n", b->frame_index);
        fprintf(fp, "\n");
    }

    /* ---- Fish ---------------------------------------------------- */

    for (int i = 0; i < def->fish_count; i++) {
        const FishPlacement *f = &def->fish[i];
        fprintf(fp, "[[fish]]\n");
        fprintf(fp, "x = %s\n", fmt_float(f->x));
        fprintf(fp, "vx = %s\n", fmt_float(f->vx));
        fprintf(fp, "patrol_x0 = %s\n", fmt_float(f->patrol_x0));
        fprintf(fp, "patrol_x1 = %s\n", fmt_float(f->patrol_x1));
        fprintf(fp, "\n");
    }

    /* ---- Faster fish --------------------------------------------- */

    for (int i = 0; i < def->faster_fish_count; i++) {
        const FishPlacement *f = &def->faster_fish[i];
        fprintf(fp, "[[faster_fish]]\n");
        fprintf(fp, "x = %s\n", fmt_float(f->x));
        fprintf(fp, "vx = %s\n", fmt_float(f->vx));
        fprintf(fp, "patrol_x0 = %s\n", fmt_float(f->patrol_x0));
        fprintf(fp, "patrol_x1 = %s\n", fmt_float(f->patrol_x1));
        fprintf(fp, "\n");
    }

    /* ---- Axe traps ----------------------------------------------- */

    for (int i = 0; i < def->axe_trap_count; i++) {
        const AxeTrapPlacement *a = &def->axe_traps[i];
        fprintf(fp, "[[axe_traps]]\n");
        fprintf(fp, "pillar_x = %s\n", fmt_float(a->pillar_x));
        fprintf(fp, "y = %s\n", fmt_float(a->y));
        write_toml_key_string(fp, "mode", serializer_axe_mode_to_str(a->mode));
        fprintf(fp, "\n");
    }

    /* ---- Circular saws ------------------------------------------- */

    for (int i = 0; i < def->circular_saw_count; i++) {
        const CircularSawPlacement *cs = &def->circular_saws[i];
        fprintf(fp, "[[circular_saws]]\n");
        fprintf(fp, "x = %s\n", fmt_float(cs->x));
        fprintf(fp, "y = %s\n", fmt_float(cs->y));
        fprintf(fp, "patrol_x0 = %s\n", fmt_float(cs->patrol_x0));
        fprintf(fp, "patrol_x1 = %s\n", fmt_float(cs->patrol_x1));
        fprintf(fp, "direction = %d\n", cs->direction);
        fprintf(fp, "\n");
    }

    /* ---- Spike rows ---------------------------------------------- */

    for (int i = 0; i < def->spike_row_count; i++) {
        const SpikeRowPlacement *sr = &def->spike_rows[i];
        fprintf(fp, "[[spike_rows]]\n");
        fprintf(fp, "x = %s\n", fmt_float(sr->x));
        fprintf(fp, "count = %d\n", sr->count);
        fprintf(fp, "\n");
    }

    /* ---- Spike platforms ----------------------------------------- */

    for (int i = 0; i < def->spike_platform_count; i++) {
        const SpikePlatformPlacement *sp = &def->spike_platforms[i];
        fprintf(fp, "[[spike_platforms]]\n");
        fprintf(fp, "x = %s\n", fmt_float(sp->x));
        fprintf(fp, "y = %s\n", fmt_float(sp->y));
        fprintf(fp, "tile_count = %d\n", sp->tile_count);
        fprintf(fp, "\n");
    }

    /* ---- Spike blocks -------------------------------------------- */

    for (int i = 0; i < def->spike_block_count; i++) {
        const SpikeBlockPlacement *sb = &def->spike_blocks[i];
        fprintf(fp, "[[spike_blocks]]\n");
        fprintf(fp, "rail_index = %d\n", sb->rail_index);
        fprintf(fp, "t_offset = %s\n", fmt_float(sb->t_offset));
        fprintf(fp, "speed = %s\n", fmt_float(sb->speed));
        fprintf(fp, "\n");
    }

    /* ---- Blue flames --------------------------------------------- */

    for (int i = 0; i < def->blue_flame_count; i++) {
        fprintf(fp, "[[blue_flames]]\n");
        fprintf(fp, "x = %s\n", fmt_float(def->blue_flames[i].x));
        fprintf(fp, "\n");
    }

    /* ---- Fire flames --------------------------------------------- */

    for (int i = 0; i < def->fire_flame_count; i++) {
        fprintf(fp, "[[fire_flames]]\n");
        fprintf(fp, "x = %s\n", fmt_float(def->fire_flames[i].x));
        fprintf(fp, "\n");
    }

    /* ---- Float platforms ----------------------------------------- */

    for (int i = 0; i < def->float_platform_count; i++) {
        const FloatPlatformPlacement *fl = &def->float_platforms[i];
        fprintf(fp, "[[float_platforms]]\n");
        write_toml_key_string(fp, "mode", serializer_float_mode_to_str(fl->mode));
        fprintf(fp, "x = %s\n", fmt_float(fl->x));
        fprintf(fp, "y = %s\n", fmt_float(fl->y));
        fprintf(fp, "tile_count = %d\n", fl->tile_count);
        fprintf(fp, "rail_index = %d\n", fl->rail_index);
        fprintf(fp, "t_offset = %s\n", fmt_float(fl->t_offset));
        fprintf(fp, "speed = %s\n", fmt_float(fl->speed));
        fprintf(fp, "\n");
    }

    /* ---- Bridges ------------------------------------------------- */

    for (int i = 0; i < def->bridge_count; i++) {
        const BridgePlacement *br = &def->bridges[i];
        fprintf(fp, "[[bridges]]\n");
        fprintf(fp, "x = %s\n", fmt_float(br->x));
        fprintf(fp, "y = %s\n", fmt_float(br->y));
        fprintf(fp, "brick_count = %d\n", br->brick_count);
        fprintf(fp, "\n");
    }

    /* ---- Bouncepads (small) -------------------------------------- */

    for (int i = 0; i < def->bouncepad_small_count; i++) {
        const BouncepadPlacement *bp = &def->bouncepads_small[i];
        fprintf(fp, "[[bouncepads_small]]\n");
        fprintf(fp, "x = %s\n", fmt_float(bp->x));
        fprintf(fp, "launch_vy = %s\n", fmt_float(bp->launch_vy));
        write_toml_key_string(fp, "pad_type", serializer_bouncepad_type_to_str(bp->pad_type));
        fprintf(fp, "\n");
    }

    /* ---- Bouncepads (medium) ------------------------------------- */

    for (int i = 0; i < def->bouncepad_medium_count; i++) {
        const BouncepadPlacement *bp = &def->bouncepads_medium[i];
        fprintf(fp, "[[bouncepads_medium]]\n");
        fprintf(fp, "x = %s\n", fmt_float(bp->x));
        fprintf(fp, "launch_vy = %s\n", fmt_float(bp->launch_vy));
        write_toml_key_string(fp, "pad_type", serializer_bouncepad_type_to_str(bp->pad_type));
        fprintf(fp, "\n");
    }

    /* ---- Bouncepads (high) --------------------------------------- */

    for (int i = 0; i < def->bouncepad_high_count; i++) {
        const BouncepadPlacement *bp = &def->bouncepads_high[i];
        fprintf(fp, "[[bouncepads_high]]\n");
        fprintf(fp, "x = %s\n", fmt_float(bp->x));
        fprintf(fp, "launch_vy = %s\n", fmt_float(bp->launch_vy));
        write_toml_key_string(fp, "pad_type", serializer_bouncepad_type_to_str(bp->pad_type));
        fprintf(fp, "\n");
    }

    /* ---- Vines --------------------------------------------------- */

    for (int i = 0; i < def->vine_count; i++) {
        const VinePlacement *v = &def->vines[i];
        fprintf(fp, "[[vines]]\n");
        fprintf(fp, "x = %s\n", fmt_float(v->x));
        fprintf(fp, "y = %s\n", fmt_float(v->y));
        fprintf(fp, "tile_count = %d\n", v->tile_count);
        if (v->vine_type != 0) {
            fprintf(fp, "vine_type = %d\n", v->vine_type);
        }
        fprintf(fp, "\n");
    }

    /* ---- Ladders ------------------------------------------------- */

    for (int i = 0; i < def->ladder_count; i++) {
        const LadderPlacement *l = &def->ladders[i];
        fprintf(fp, "[[ladders]]\n");
        fprintf(fp, "x = %s\n", fmt_float(l->x));
        fprintf(fp, "y = %s\n", fmt_float(l->y));
        fprintf(fp, "tile_count = %d\n", l->tile_count);
        fprintf(fp, "\n");
    }

    /* ---- Ropes --------------------------------------------------- */

    for (int i = 0; i < def->rope_count; i++) {
        const RopePlacement *rp = &def->ropes[i];
        fprintf(fp, "[[ropes]]\n");
        fprintf(fp, "x = %s\n", fmt_float(rp->x));
        fprintf(fp, "y = %s\n", fmt_float(rp->y));
        fprintf(fp, "tile_count = %d\n", rp->tile_count);
        fprintf(fp, "\n");
    }

    /* ---- Background layers --------------------------------------- */

    for (int i = 0; i < def->background_layer_count; i++) {
        fprintf(fp, "[[background_layers]]\n");
        write_toml_key_string(fp, "path", def->background_layers[i].path);
        fprintf(fp, "speed = %s\n", fmt_float(def->background_layers[i].speed));
        fprintf(fp, "\n");
    }

    /* ---- Foreground layers --------------------------------------- */

    for (int i = 0; i < def->foreground_layer_count; i++) {
        fprintf(fp, "[[foreground_layers]]\n");
        write_toml_key_string(fp, "path", def->foreground_layers[i].path);
        fprintf(fp, "speed = %s\n", fmt_float(def->foreground_layers[i].speed));
        fprintf(fp, "\n");
    }

    /* ---- Fog layers ------------------------------------------------- */

    for (int i = 0; i < def->fog_layer_count; i++) {
        fprintf(fp, "[[fog_layers]]\n");
        write_toml_key_string(fp, "path", def->fog_layers[i].path);
        fprintf(fp, "speed = %s\n", fmt_float(def->fog_layers[i].speed));
        fprintf(fp, "\n");
    }

    fclose(fp);
    return 0;
}

