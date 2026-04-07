/*
 * exporter.c — Generate compilable C source files from a LevelDef.
 *
 * Writes two files that together define a const LevelDef:
 *   {var_name}.h  — #pragma once, #include "level.h", extern declaration
 *   {var_name}.c  — full designated-initialiser definition with every field
 *
 * The output style matches sandbox_00.c exactly: section separators, one array
 * entry per line, trailing commas, float literals with %.1ff formatting, and
 * enum identifiers instead of raw integers.
 *
 * All output is written with fprintf — no intermediate string buffers.
 */

#include "exporter.h"

#include "../levels/level.h"   /* LevelDef, all placement structs, enums */
#include "../game.h"           /* MAX_* constants (used by LevelDef)     */

#include <stdio.h>             /* FILE, fopen, fprintf, fclose           */
#include <string.h>            /* snprintf — path assembly               */

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
#include <sys/stat.h>          /* open, O_WRONLY, O_CREAT, O_TRUNC      */
#include <fcntl.h>
#endif

/* ------------------------------------------------------------------ */
/* Helper: enum-to-string mappers                                      */
/* ------------------------------------------------------------------ */

/*
 * rail_layout_str — Convert a RailLayout enum to its C identifier string.
 *
 * The exporter writes enum names (RAIL_LAYOUT_RECT) rather than raw integers
 * so the generated code is self-documenting and compiles against level.h.
 */
static const char *rail_layout_str(RailLayout layout)
{
    switch (layout) {
    case RAIL_LAYOUT_RECT:  return "RAIL_LAYOUT_RECT";
    case RAIL_LAYOUT_HORIZ: return "RAIL_LAYOUT_HORIZ";
    default:                return "RAIL_LAYOUT_RECT";
    }
}

/*
 * axe_mode_str — Convert an AxeTrapMode enum to its C identifier string.
 */
static const char *axe_mode_str(AxeTrapMode mode)
{
    switch (mode) {
    case AXE_MODE_PENDULUM: return "AXE_MODE_PENDULUM";
    case AXE_MODE_SPIN:     return "AXE_MODE_SPIN";
    default:                return "AXE_MODE_PENDULUM";
    }
}

/*
 * float_platform_mode_str — Convert a FloatPlatformMode to its C identifier.
 */
static const char *float_platform_mode_str(FloatPlatformMode mode)
{
    switch (mode) {
    case FLOAT_PLATFORM_STATIC:  return "FLOAT_PLATFORM_STATIC";
    case FLOAT_PLATFORM_CRUMBLE: return "FLOAT_PLATFORM_CRUMBLE";
    case FLOAT_PLATFORM_RAIL:    return "FLOAT_PLATFORM_RAIL";
    default:                     return "FLOAT_PLATFORM_STATIC";
    }
}

/*
 * bouncepad_type_str — Convert a BouncepadType to its C identifier.
 */
static const char *bouncepad_type_str(BouncepadType pad_type)
{
    switch (pad_type) {
    case BOUNCEPAD_GREEN: return "BOUNCEPAD_GREEN";
    case BOUNCEPAD_WOOD:  return "BOUNCEPAD_WOOD";
    case BOUNCEPAD_RED:   return "BOUNCEPAD_RED";
    default:              return "BOUNCEPAD_GREEN";
    }
}

/* ------------------------------------------------------------------ */
/* Helper: section separator                                           */
/* ------------------------------------------------------------------ */

/*
 * write_section — Print a visual separator comment between field groups.
 *
 * Matches the sandbox_00.c style: blank line, then "/ * ---- Name ---- * /"
 */
static void write_section(FILE *f, const char *name)
{
    fprintf(f, "\n    /* ---- %s ---- */\n", name);
}

/* ------------------------------------------------------------------ */
/* Header file writer                                                  */
/* ------------------------------------------------------------------ */

/*
 * write_header — Generate the {var_name}.h file.
 *
 * The header mirrors sandbox_00.h: pragma-once guard, include level.h,
 * and an extern declaration for the const LevelDef.
 */
static int write_header(const char *var_name, const char *dir_path)
{
    /*
     * snprintf — build the full file path into a stack buffer.
     * 512 bytes is generous for typical paths like "src/levels/exported/level_02.h".
     */
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.h", dir_path, var_name);

    /*
     * Open with restricted permissions (0644) on POSIX so generated files
     * are not world-writable.
     */
#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    FILE *f = fd >= 0 ? fdopen(fd, "w") : NULL;
#else
    FILE *f = fopen(path, "w");
#endif
    if (!f) {
        fprintf(stderr, "exporter: failed to open %s for writing\n", path);
        return -1;
    }

    /*
     * Write a minimal header:
     *   #pragma once
     *   #include "level.h"
     *   extern const LevelDef {var_name}_def;
     */
    fprintf(f, "#pragma once\n\n");
    fprintf(f, "#include \"../level.h\"\n\n");
    fprintf(f, "extern const LevelDef %s_def;\n", var_name);

    fclose(f);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Source file writer                                                   */
/* ------------------------------------------------------------------ */

/*
 * write_source — Generate the {var_name}.c file with the full LevelDef.
 *
 * This is the bulk of the exporter.  It writes the designated-initialiser
 * definition field by field, in the exact order they appear in the LevelDef
 * struct (level.h).  Empty arrays are represented as just ".count = 0,".
 *
 * The generated code includes all entity/hazard/surface headers so that
 * symbolic constants (SPIDER_SPEED, BOUNCEPAD_VY_MEDIUM, etc.) compile.
 */
static int write_source(const LevelDef *def, const char *var_name,
                         const char *dir_path)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.c", dir_path, var_name);

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
    int fds = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    FILE *f = fds >= 0 ? fdopen(fds, "w") : NULL;
#else
    FILE *f = fopen(path, "w");
#endif
    if (!f) {
        fprintf(stderr, "exporter: failed to open %s for writing\n", path);
        return -1;
    }

    /* ---- Include block ---- */
    /*
     * The include list matches sandbox_00.c exactly.  Every header that defines
     * a speed constant, enum, or display size used in the initialiser must be
     * present so the generated file compiles stand-alone.
     */
    fprintf(f, "#include \"%s.h\"\n\n", var_name);
    fprintf(f, "/* Pull in entity constants needed for SPEED / VY / MODE values */\n");
    fprintf(f, "#include \"../../entities/spider.h\"         /* SPIDER_SPEED */\n");
    fprintf(f, "#include \"../../entities/jumping_spider.h\" /* JSPIDER_SPEED */\n");
    fprintf(f, "#include \"../../entities/bird.h\"           /* BIRD_SPEED */\n");
    fprintf(f, "#include \"../../entities/faster_bird.h\"    /* FBIRD_SPEED */\n");
    fprintf(f, "#include \"../../entities/fish.h\"           /* FISH_SPEED */\n");
    fprintf(f, "#include \"../../entities/faster_fish.h\"    /* FFISH_SPEED */\n");
    fprintf(f, "#include \"../../collectibles/coin.h\"       /* COIN_DISPLAY_W, COIN_DISPLAY_H */\n");
    fprintf(f, "#include \"../../collectibles/star_yellow.h\"/* STAR_YELLOW_DISPLAY_W/H, MAX_STAR_YELLOWS */\n");
    fprintf(f, "#include \"../../collectibles/last_star.h\"  /* LAST_STAR_DISPLAY_W/H */\n");
    fprintf(f, "#include \"../../surfaces/bouncepad.h\"      /* BOUNCEPAD_VY_*, BouncepadType */\n");
    fprintf(f, "#include \"../../hazards/axe_trap.h\"        /* AxeTrapMode */\n");
    fprintf(f, "#include \"../../hazards/circular_saw.h\"    /* SAW_DISPLAY_W/H */\n");
    fprintf(f, "#include \"../../hazards/spike.h\"           /* SPIKE_TILE_H */\n");
    fprintf(f, "#include \"../../hazards/spike_block.h\"     /* SPIKE_SPEED_* */\n");
    fprintf(f, "#include \"../../surfaces/float_platform.h\" /* FLOAT_PLATFORM_*, CRUMBLE_STAND_LIMIT */\n");
    fprintf(f, "#include \"../../surfaces/vine.h\"           /* VINE_W */\n");
    fprintf(f, "#include \"../../game.h\"                    /* FLOOR_Y, TILE_SIZE, GAME_H */\n");

    /* ---- Open the LevelDef definition ---- */
    fprintf(f, "\n/* ------------------------------------------------------------------ */\n\n");
    fprintf(f, "const LevelDef %s_def = {\n", var_name);

    /* ---- 1. name + description ---- */
    fprintf(f, "    .name = \"%s\",\n", def->name[0] ? def->name : "Untitled");
    if (def->description[0] != '\0') {
        /* Escape newlines for C string literal */
        fprintf(f, "    .description = \"");
        for (const char *p = def->description; *p; p++) {
            if (*p == '\n')      fprintf(f, "\\n");
            else if (*p == '"')  fprintf(f, "\\\"");
            else if (*p == '\\') fprintf(f, "\\\\");
            else                 fputc(*p, f);
        }
        fprintf(f, "\",\n");
    }
    if (def->generated_by[0] != '\0') {
        fprintf(f, "    .generated_by = \"%s\",\n", def->generated_by);
    }

    /* ---- 2. Floor gaps ---- */
    write_section(f, "Floor gaps");
    if (def->floor_gap_count > 0) {
        fprintf(f, "    .floor_gaps      = {");
        for (int i = 0; i < def->floor_gap_count; i++) {
            fprintf(f, " %d", def->floor_gaps[i]);
            if (i < def->floor_gap_count - 1) fprintf(f, ",");
        }
        fprintf(f, " },\n");
    }
    fprintf(f, "    .floor_gap_count = %d,\n", def->floor_gap_count);

    /* ---- 3. Rails ---- */
    write_section(f, "Rails");
    if (def->rail_count > 0) {
        fprintf(f, "    .rails = {\n");
        for (int i = 0; i < def->rail_count; i++) {
            const RailPlacement *r = &def->rails[i];
            fprintf(f, "        { %s, .x = %d, .y = %d, .w = %d, .h = %d, .end_cap = %d },\n",
                    rail_layout_str(r->layout),
                    r->x, r->y, r->w, r->h, r->end_cap);
        }
        fprintf(f, "    },\n");
    }
    fprintf(f, "    .rail_count = %d,\n", def->rail_count);

    /* ---- 4. Platforms ---- */
    write_section(f, "Platforms");
    if (def->platform_count > 0) {
        fprintf(f, "    .platforms = {\n");
        for (int i = 0; i < def->platform_count; i++) {
            const PlatformPlacement *p = &def->platforms[i];
            fprintf(f, "        { .x = %.1ff, .tile_height = %d, .tile_width = %d },\n",
                    p->x, p->tile_height, p->tile_width);
        }
        fprintf(f, "    },\n");
    }
    fprintf(f, "    .platform_count = %d,\n", def->platform_count);

    /* ---- 5. Coins ---- */
    write_section(f, "Coins");
    if (def->coin_count > 0) {
        fprintf(f, "    .coins = {\n");
        for (int i = 0; i < def->coin_count; i++) {
            const CoinPlacement *c = &def->coins[i];
            fprintf(f, "        { %.1ff, %.1ff },\n", c->x, c->y);
        }
        fprintf(f, "    },\n");
    }
    fprintf(f, "    .coin_count = %d,\n", def->coin_count);

    /* ---- 6. Star yellows ---- */
    write_section(f, "Star yellows");
    if (def->star_yellow_count > 0) {
        fprintf(f, "    .star_yellows = {\n");
        for (int i = 0; i < def->star_yellow_count; i++) {
            const StarYellowPlacement *ys = &def->star_yellows[i];
            fprintf(f, "        { %.1ff, %.1ff },\n", ys->x, ys->y);
        }
        fprintf(f, "    },\n");
    }
    fprintf(f, "    .star_yellow_count = %d,\n", def->star_yellow_count);

    /* ---- 6b. Star greens ---- */
    write_section(f, "Star greens");
    if (def->star_green_count > 0) {
        fprintf(f, "    .star_greens = {\n");
        for (int i = 0; i < def->star_green_count; i++) {
            const StarGreenPlacement *gs = &def->star_greens[i];
            fprintf(f, "        { %.1ff, %.1ff },\n", gs->x, gs->y);
        }
        fprintf(f, "    },\n");
    }
    fprintf(f, "    .star_green_count = %d,\n", def->star_green_count);

    /* ---- 6c. Star reds ---- */
    write_section(f, "Star reds");
    if (def->star_red_count > 0) {
        fprintf(f, "    .star_reds = {\n");
        for (int i = 0; i < def->star_red_count; i++) {
            const StarRedPlacement *rs = &def->star_reds[i];
            fprintf(f, "        { %.1ff, %.1ff },\n", rs->x, rs->y);
        }
        fprintf(f, "    },\n");
    }
    fprintf(f, "    .star_red_count = %d,\n", def->star_red_count);

    /* ---- 7. Last star (single, not array) ---- */
    write_section(f, "Last star");
    fprintf(f, "    .last_star = { .x = %.1ff, .y = %.1ff },\n",
            def->last_star.x, def->last_star.y);

    /* ---- 8. Spiders ---- */
    write_section(f, "Spiders");
    if (def->spider_count > 0) {
        fprintf(f, "    .spiders = {\n");
        for (int i = 0; i < def->spider_count; i++) {
            const SpiderPlacement *s = &def->spiders[i];
            fprintf(f, "        { .x = %.1ff, .vx = %.1ff,\n"
                       "          .patrol_x0 = %.1ff, .patrol_x1 = %.1ff, .frame_index = %d },\n",
                    s->x, s->vx, s->patrol_x0, s->patrol_x1, s->frame_index);
        }
        fprintf(f, "    },\n");
    }
    fprintf(f, "    .spider_count = %d,\n", def->spider_count);

    /* ---- 9. Jumping spiders ---- */
    write_section(f, "Jumping spiders");
    if (def->jumping_spider_count > 0) {
        fprintf(f, "    .jumping_spiders = {\n");
        for (int i = 0; i < def->jumping_spider_count; i++) {
            const JumpingSpiderPlacement *js = &def->jumping_spiders[i];
            fprintf(f, "        { .x = %.1ff, .vx = %.1ff,\n"
                       "          .patrol_x0 = %.1ff, .patrol_x1 = %.1ff },\n",
                    js->x, js->vx, js->patrol_x0, js->patrol_x1);
        }
        fprintf(f, "    },\n");
    }
    fprintf(f, "    .jumping_spider_count = %d,\n", def->jumping_spider_count);

    /* ---- 10. Birds ---- */
    write_section(f, "Birds");
    if (def->bird_count > 0) {
        fprintf(f, "    .birds = {\n");
        for (int i = 0; i < def->bird_count; i++) {
            const BirdPlacement *b = &def->birds[i];
            fprintf(f, "        { .x = %.1ff, .base_y = %.1ff, .vx = %.1ff,\n"
                       "          .patrol_x0 = %.1ff, .patrol_x1 = %.1ff, .frame_index = %d },\n",
                    b->x, b->base_y, b->vx, b->patrol_x0, b->patrol_x1, b->frame_index);
        }
        fprintf(f, "    },\n");
    }
    fprintf(f, "    .bird_count = %d,\n", def->bird_count);

    /* ---- 11. Faster birds ---- */
    write_section(f, "Faster birds");
    if (def->faster_bird_count > 0) {
        fprintf(f, "    .faster_birds = {\n");
        for (int i = 0; i < def->faster_bird_count; i++) {
            const BirdPlacement *fb = &def->faster_birds[i];
            fprintf(f, "        { .x = %.1ff, .base_y = %.1ff, .vx = %.1ff,\n"
                       "          .patrol_x0 = %.1ff, .patrol_x1 = %.1ff, .frame_index = %d },\n",
                    fb->x, fb->base_y, fb->vx, fb->patrol_x0, fb->patrol_x1, fb->frame_index);
        }
        fprintf(f, "    },\n");
    }
    fprintf(f, "    .faster_bird_count = %d,\n", def->faster_bird_count);

    /* ---- 12. Fish ---- */
    write_section(f, "Fish");
    if (def->fish_count > 0) {
        fprintf(f, "    .fish = {\n");
        for (int i = 0; i < def->fish_count; i++) {
            const FishPlacement *fi = &def->fish[i];
            fprintf(f, "        { .x = %.1ff, .vx = %.1ff,\n"
                       "          .patrol_x0 = %.1ff, .patrol_x1 = %.1ff },\n",
                    fi->x, fi->vx, fi->patrol_x0, fi->patrol_x1);
        }
        fprintf(f, "    },\n");
    }
    fprintf(f, "    .fish_count = %d,\n", def->fish_count);

    /* ---- 13. Faster fish ---- */
    write_section(f, "Faster fish");
    if (def->faster_fish_count > 0) {
        fprintf(f, "    .faster_fish = {\n");
        for (int i = 0; i < def->faster_fish_count; i++) {
            const FishPlacement *ff = &def->faster_fish[i];
            fprintf(f, "        { .x = %.1ff, .vx = %.1ff,\n"
                       "          .patrol_x0 = %.1ff, .patrol_x1 = %.1ff },\n",
                    ff->x, ff->vx, ff->patrol_x0, ff->patrol_x1);
        }
        fprintf(f, "    },\n");
    }
    fprintf(f, "    .faster_fish_count = %d,\n", def->faster_fish_count);

    /* ---- 14. Axe traps ---- */
    write_section(f, "Axe traps");
    if (def->axe_trap_count > 0) {
        fprintf(f, "    .axe_traps = {\n");
        for (int i = 0; i < def->axe_trap_count; i++) {
            const AxeTrapPlacement *at = &def->axe_traps[i];
            fprintf(f, "        { .pillar_x = %.1ff, .mode = %s },\n",
                    at->pillar_x, axe_mode_str(at->mode));
        }
        fprintf(f, "    },\n");
    }
    fprintf(f, "    .axe_trap_count = %d,\n", def->axe_trap_count);

    /* ---- 15. Circular saws ---- */
    write_section(f, "Circular saws");
    if (def->circular_saw_count > 0) {
        fprintf(f, "    .circular_saws = {\n");
        for (int i = 0; i < def->circular_saw_count; i++) {
            const CircularSawPlacement *cs = &def->circular_saws[i];
            fprintf(f, "        { .x = %.1ff, .patrol_x0 = %.1ff, .patrol_x1 = %.1ff, .direction = %d },\n",
                    cs->x, cs->patrol_x0, cs->patrol_x1, cs->direction);
        }
        fprintf(f, "    },\n");
    }
    fprintf(f, "    .circular_saw_count = %d,\n", def->circular_saw_count);

    /* ---- 16. Spike rows ---- */
    write_section(f, "Spike rows");
    if (def->spike_row_count > 0) {
        fprintf(f, "    .spike_rows = {\n");
        for (int i = 0; i < def->spike_row_count; i++) {
            const SpikeRowPlacement *sr = &def->spike_rows[i];
            fprintf(f, "        { .x = %.1ff, .count = %d },\n",
                    sr->x, sr->count);
        }
        fprintf(f, "    },\n");
    }
    fprintf(f, "    .spike_row_count = %d,\n", def->spike_row_count);

    /* ---- 17. Spike platforms ---- */
    write_section(f, "Spike platforms");
    if (def->spike_platform_count > 0) {
        fprintf(f, "    .spike_platforms = {\n");
        for (int i = 0; i < def->spike_platform_count; i++) {
            const SpikePlatformPlacement *sp = &def->spike_platforms[i];
            fprintf(f, "        { .x = %.1ff, .y = %.1ff, .tile_count = %d },\n",
                    sp->x, sp->y, sp->tile_count);
        }
        fprintf(f, "    },\n");
    }
    fprintf(f, "    .spike_platform_count = %d,\n", def->spike_platform_count);

    /* ---- 18. Spike blocks ---- */
    write_section(f, "Spike blocks");
    if (def->spike_block_count > 0) {
        fprintf(f, "    .spike_blocks = {\n");
        for (int i = 0; i < def->spike_block_count; i++) {
            const SpikeBlockPlacement *sb = &def->spike_blocks[i];
            fprintf(f, "        { .rail_index = %d, .t_offset = %.1ff, .speed = %.1ff },\n",
                    sb->rail_index, sb->t_offset, sb->speed);
        }
        fprintf(f, "    },\n");
    }
    fprintf(f, "    .spike_block_count = %d,\n", def->spike_block_count);

    /* ---- 19. Blue flames ---- */
    write_section(f, "Blue flames");
    if (def->blue_flame_count > 0) {
        fprintf(f, "    .blue_flames = {\n");
        for (int i = 0; i < def->blue_flame_count; i++) {
            fprintf(f, "        { .x = %.1ff },\n", def->blue_flames[i].x);
        }
        fprintf(f, "    },\n");
    }
    fprintf(f, "    .blue_flame_count = %d,\n", def->blue_flame_count);

    /* ---- 20. Fire flames ---- */
    write_section(f, "Fire flames");
    if (def->fire_flame_count > 0) {
        fprintf(f, "    .fire_flames = {\n");
        for (int i = 0; i < def->fire_flame_count; i++) {
            fprintf(f, "        { .x = %.1ff },\n", def->fire_flames[i].x);
        }
        fprintf(f, "    },\n");
    }
    fprintf(f, "    .fire_flame_count = %d,\n", def->fire_flame_count);

    /* ---- 21. Float platforms ---- */
    write_section(f, "Float platforms");
    if (def->float_platform_count > 0) {
        fprintf(f, "    .float_platforms = {\n");
        for (int i = 0; i < def->float_platform_count; i++) {
            const FloatPlatformPlacement *fp = &def->float_platforms[i];
            fprintf(f, "        { .mode = %s, .x = %.1ff, .y = %.1ff, .tile_count = %d,\n"
                       "          .rail_index = %d, .t_offset = %.1ff, .speed = %.1ff },\n",
                    float_platform_mode_str(fp->mode),
                    fp->x, fp->y, fp->tile_count,
                    fp->rail_index, fp->t_offset, fp->speed);
        }
        fprintf(f, "    },\n");
    }
    fprintf(f, "    .float_platform_count = %d,\n", def->float_platform_count);

    /* ---- 21. Bridges ---- */
    write_section(f, "Bridges");
    if (def->bridge_count > 0) {
        fprintf(f, "    .bridges = {\n");
        for (int i = 0; i < def->bridge_count; i++) {
            const BridgePlacement *br = &def->bridges[i];
            fprintf(f, "        { .x = %.1ff, .y = %.1ff, .brick_count = %d },\n",
                    br->x, br->y, br->brick_count);
        }
        fprintf(f, "    },\n");
    }
    fprintf(f, "    .bridge_count = %d,\n", def->bridge_count);

    /* ---- 22. Bouncepads (small) ---- */
    write_section(f, "Bouncepads small");
    if (def->bouncepad_small_count > 0) {
        fprintf(f, "    .bouncepads_small = {\n");
        for (int i = 0; i < def->bouncepad_small_count; i++) {
            const BouncepadPlacement *bp = &def->bouncepads_small[i];
            fprintf(f, "        { .x = %.1ff, .launch_vy = %.1ff, .pad_type = %s },\n",
                    bp->x, bp->launch_vy, bouncepad_type_str(bp->pad_type));
        }
        fprintf(f, "    },\n");
    }
    fprintf(f, "    .bouncepad_small_count = %d,\n", def->bouncepad_small_count);

    /* ---- 23. Bouncepads (medium) ---- */
    write_section(f, "Bouncepads medium");
    if (def->bouncepad_medium_count > 0) {
        fprintf(f, "    .bouncepads_medium = {\n");
        for (int i = 0; i < def->bouncepad_medium_count; i++) {
            const BouncepadPlacement *bp = &def->bouncepads_medium[i];
            fprintf(f, "        { .x = %.1ff, .launch_vy = %.1ff, .pad_type = %s },\n",
                    bp->x, bp->launch_vy, bouncepad_type_str(bp->pad_type));
        }
        fprintf(f, "    },\n");
    }
    fprintf(f, "    .bouncepad_medium_count = %d,\n", def->bouncepad_medium_count);

    /* ---- 24. Bouncepads (high) ---- */
    write_section(f, "Bouncepads high");
    if (def->bouncepad_high_count > 0) {
        fprintf(f, "    .bouncepads_high = {\n");
        for (int i = 0; i < def->bouncepad_high_count; i++) {
            const BouncepadPlacement *bp = &def->bouncepads_high[i];
            fprintf(f, "        { .x = %.1ff, .launch_vy = %.1ff, .pad_type = %s },\n",
                    bp->x, bp->launch_vy, bouncepad_type_str(bp->pad_type));
        }
        fprintf(f, "    },\n");
    }
    fprintf(f, "    .bouncepad_high_count = %d,\n", def->bouncepad_high_count);

    /* ---- 25. Vines ---- */
    write_section(f, "Vines");
    if (def->vine_count > 0) {
        fprintf(f, "    .vines = {\n");
        for (int i = 0; i < def->vine_count; i++) {
            const VinePlacement *v = &def->vines[i];
            fprintf(f, "        { .x = %.1ff, .y = %.1ff, .tile_count = %d },\n",
                    v->x, v->y, v->tile_count);
        }
        fprintf(f, "    },\n");
    }
    fprintf(f, "    .vine_count = %d,\n", def->vine_count);

    /* ---- 26. Ladders ---- */
    write_section(f, "Ladders");
    if (def->ladder_count > 0) {
        fprintf(f, "    .ladders = {\n");
        for (int i = 0; i < def->ladder_count; i++) {
            const LadderPlacement *l = &def->ladders[i];
            fprintf(f, "        { .x = %.1ff, .y = %.1ff, .tile_count = %d },\n",
                    l->x, l->y, l->tile_count);
        }
        fprintf(f, "    },\n");
    }
    fprintf(f, "    .ladder_count = %d,\n", def->ladder_count);

    /* ---- 27. Ropes ---- */
    write_section(f, "Ropes");
    if (def->rope_count > 0) {
        fprintf(f, "    .ropes = {\n");
        for (int i = 0; i < def->rope_count; i++) {
            const RopePlacement *rp = &def->ropes[i];
            fprintf(f, "        { .x = %.1ff, .y = %.1ff, .tile_count = %d },\n",
                    rp->x, rp->y, rp->tile_count);
        }
        fprintf(f, "    },\n");
    }
    fprintf(f, "    .rope_count = %d,\n", def->rope_count);

    /* ---- 28. Level-wide configuration ---- */
    write_section(f, "Level-wide configuration");

    /* Background layers */
    if (def->background_layer_count > 0) {
        fprintf(f, "    .background_layers = {\n");
        for (int i = 0; i < def->background_layer_count; i++) {
            fprintf(f, "        { \"%s\", %.2ff },\n",
                    def->background_layers[i].path,
                    (double)def->background_layers[i].speed);
        }
        fprintf(f, "    },\n");
    }
    fprintf(f, "    .background_layer_count = %d,\n", def->background_layer_count);

    /* Foreground layers */
    if (def->foreground_layer_count > 0) {
        fprintf(f, "    .foreground_layers = {\n");
        for (int i = 0; i < def->foreground_layer_count; i++) {
            fprintf(f, "        { \"%s\", %.2ff },\n",
                    def->foreground_layers[i].path,
                    (double)def->foreground_layers[i].speed);
        }
        fprintf(f, "    },\n");
    }
    fprintf(f, "    .foreground_layer_count = %d,\n", def->foreground_layer_count);

    /* Fog layers */
    if (def->fog_layer_count > 0) {
        fprintf(f, "    .fog_layers = {\n");
        for (int i = 0; i < def->fog_layer_count; i++) {
            fprintf(f, "        { \"%s\", %.2ff },\n",
                    def->fog_layers[i].path,
                    (double)def->fog_layers[i].speed);
        }
        fprintf(f, "    },\n");
    }
    fprintf(f, "    .fog_layer_count = %d,\n", def->fog_layer_count);

    /* Player spawn */
    fprintf(f, "\n    .player_start_x = %.1ff,\n", (double)def->player_start_x);
    fprintf(f, "    .player_start_y = %.1ff,\n", (double)def->player_start_y);

    /* Music */
    fprintf(f, "\n    .music_path   = \"%s\",\n", def->music_path);
    fprintf(f, "    .music_volume = %d,\n", def->music_volume);

    /* Game rules */
    fprintf(f, "\n    .initial_hearts  = %d,\n", def->initial_hearts);
    fprintf(f, "    .initial_lives   = %d,\n", def->initial_lives);
    fprintf(f, "    .score_per_life  = %d,\n", def->score_per_life);
    fprintf(f, "    .coin_score      = %d,\n", def->coin_score);

    /* ---- Close the struct ---- */
    fprintf(f, "};\n");

    fclose(f);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

/*
 * level_export_c — Write a complete LevelDef as two compilable C files.
 *
 * Generates {dir_path}/{var_name}.h and {dir_path}/{var_name}.c.
 * The .c file uses designated initialisers and includes all entity headers
 * so that symbolic speed/mode constants resolve at compile time.
 *
 * Returns 0 on success, -1 if either file could not be opened.
 */
int level_export_c(const LevelDef *def, const char *var_name, const char *dir_path)
{
    /*
     * Write the header first — if it fails there's no point writing the .c
     * file, since both are needed for a compilable level.
     */
    if (write_header(var_name, dir_path) != 0) {
        return -1;
    }

    if (write_source(def, var_name, dir_path) != 0) {
        return -1;
    }

    return 0;
}
