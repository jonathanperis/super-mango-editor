/*
 * hit_test.c — Entity rectangles and click hit-testing for the editor.
 *
 * Key design decisions:
 *   - Rectangles use display sizes (the visible art), not sprite frame
 *     sizes, so click targets match what the designer sees on screen.
 *   - Entities are tested in reverse render order (topmost first) so
 *     overlapping entities resolve intuitively.
 *   - Counts come from editor_entity_count, which clamps to each array's
 *     capacity, so even a hand-edited level with a bad count cannot make
 *     the loops read past the end of an array.
 */

#include "hit_test.h"

#include "entity_meta.h" /* display sizes, editor_entity_count, rail helpers */
#include "../game.h"     /* GAME_H, FLOOR_Y, TILE_SIZE, FLOOR_GAP_W         */

/*
 * Axe traps and circular saws store y = 0 to mean "use the default height".
 * These helpers return the height the game (and canvas) actually uses.
 */
float editor_axe_trap_y(const AxeTrapPlacement *at)
{
    return (at->y != 0.0f) ? at->y : (float)(FLOOR_Y - 3 * TILE_SIZE + 16);
}

float editor_circular_saw_y(const CircularSawPlacement *cs)
{
    return (cs->y != 0.0f) ? cs->y
                           : (float)(FLOOR_Y - 2 * TILE_SIZE + 16 - SAW_DISPLAY_H);
}

static EditorRect rect(float x, float y, float w, float h)
{
    EditorRect r;
    r.x = x;
    r.y = y;
    r.w = w;
    r.h = h;
    return r;
}

int editor_entity_bounds(const LevelDef *level, EntityType type, int index,
                         EditorRect *out)
{
    /* Fish swim in the water strip at the bottom of the screen. */
    const float fish_y = (float)(GAME_H - WATER_ART_H) - FISH_FRAME_H / 2.0f;

    if (!level || !out || index < 0 || index >= editor_entity_count(level, type))
        return 0;

    switch (type) {
    /* ---- Enemies ---------------------------------------------------- */
    case ENT_SPIDER:  /* ground patrol: art sits on the floor */
        *out = rect(level->spiders[index].x, (float)(FLOOR_Y - SPIDER_ART_H),
                    SPIDER_FRAME_W, SPIDER_ART_H);
        return 1;
    case ENT_JUMPING_SPIDER:
        *out = rect(level->jumping_spiders[index].x,
                    (float)(FLOOR_Y - SPIDER_ART_H),
                    SPIDER_FRAME_W, SPIDER_ART_H);
        return 1;
    case ENT_BIRD:  /* sine-wave patrol around base_y */
        *out = rect(level->birds[index].x, level->birds[index].base_y,
                    BIRD_FRAME_W, BIRD_ART_H);
        return 1;
    case ENT_FASTER_BIRD:
        *out = rect(level->faster_birds[index].x,
                    level->faster_birds[index].base_y,
                    BIRD_FRAME_W, BIRD_ART_H);
        return 1;
    case ENT_FISH:
        *out = rect(level->fish[index].x, fish_y, FISH_FRAME_W, FISH_FRAME_H);
        return 1;
    case ENT_FASTER_FISH:
        *out = rect(level->faster_fish[index].x, fish_y,
                    FISH_FRAME_W, FISH_FRAME_H);
        return 1;

    /* ---- Hazards ---------------------------------------------------- */
    case ENT_AXE_TRAP: {
        const AxeTrapPlacement *at = &level->axe_traps[index];
        *out = rect(at->pillar_x, editor_axe_trap_y(at), AXE_FRAME_W, AXE_FRAME_H);
        return 1;
    }
    case ENT_CIRCULAR_SAW: {
        const CircularSawPlacement *cs = &level->circular_saws[index];
        *out = rect(cs->x, editor_circular_saw_y(cs), SAW_DISPLAY_W, SAW_DISPLAY_H);
        return 1;
    }
    case ENT_SPIKE_ROW: {
        const SpikeRowPlacement *sr = &level->spike_rows[index];
        *out = rect(sr->x, (float)(FLOOR_Y - SPIKE_TILE_H),
                    (float)sr->count * SPIKE_TILE_W, SPIKE_TILE_H);
        return 1;
    }
    case ENT_SPIKE_PLATFORM: {
        const SpikePlatformPlacement *sp = &level->spike_platforms[index];
        *out = rect(sp->x, sp->y, (float)sp->tile_count * SPIKE_PLAT_PIECE_W,
                    SPIKE_PLAT_SRC_H);
        return 1;
    }
    case ENT_SPIKE_BLOCK: {
        /* Rail rider: centred on its point along the rail. */
        const SpikeBlockPlacement *sb = &level->spike_blocks[index];
        float cx, cy;
        if (sb->rail_index < 0 || sb->rail_index >= level->rail_count) return 0;
        editor_rail_placement_position_at(&level->rails[sb->rail_index],
                                          sb->t_offset, &cx, &cy);
        *out = rect(cx - SPIKE_DISPLAY_W / 2.0f, cy - SPIKE_DISPLAY_H / 2.0f,
                    SPIKE_DISPLAY_W, SPIKE_DISPLAY_H);
        return 1;
    }
    case ENT_BLUE_FLAME:  /* x is the gap; the flame is centred inside it */
        *out = rect(level->blue_flames[index].x +
                        (float)(FLOOR_GAP_W - BLUE_FLAME_W) / 2.0f,
                    (float)(FLOOR_Y - BLUE_FLAME_H), BLUE_FLAME_W, BLUE_FLAME_H);
        return 1;
    case ENT_FIRE_FLAME:
        *out = rect(level->fire_flames[index].x +
                        (float)(FLOOR_GAP_W - FIRE_FLAME_W) / 2.0f,
                    (float)(FLOOR_Y - FIRE_FLAME_H), FIRE_FLAME_W, FIRE_FLAME_H);
        return 1;

    /* ---- Collectibles ----------------------------------------------- */
    case ENT_COIN:
        *out = rect(level->coins[index].x, level->coins[index].y,
                    COIN_DISPLAY_W, COIN_DISPLAY_H);
        return 1;
    case ENT_STAR_YELLOW:
        *out = rect(level->star_yellows[index].x, level->star_yellows[index].y,
                    YSTAR_DISPLAY_W, YSTAR_DISPLAY_H);
        return 1;
    case ENT_STAR_GREEN:
        *out = rect(level->star_greens[index].x, level->star_greens[index].y,
                    YSTAR_DISPLAY_W, YSTAR_DISPLAY_H);
        return 1;
    case ENT_STAR_RED:
        *out = rect(level->star_reds[index].x, level->star_reds[index].y,
                    YSTAR_DISPLAY_W, YSTAR_DISPLAY_H);
        return 1;
    case ENT_LAST_STAR:
        *out = rect(level->last_star.x, level->last_star.y,
                    LSTAR_DISPLAY_W, LSTAR_DISPLAY_H);
        return 1;
    case ENT_PLAYER_SPAWN:
        *out = rect(level->player_start_x, level->player_start_y,
                    PLAYER_SPAWN_W, PLAYER_SPAWN_H);
        return 1;

    /* ---- Surfaces --------------------------------------------------- */
    case ENT_BOUNCEPAD_SMALL:
        *out = rect(level->bouncepads_small[index].x, (float)(FLOOR_Y - BP_SRC_H),
                    BP_FRAME_W, BP_SRC_H);
        return 1;
    case ENT_BOUNCEPAD_MEDIUM:
        *out = rect(level->bouncepads_medium[index].x, (float)(FLOOR_Y - BP_SRC_H),
                    BP_FRAME_W, BP_SRC_H);
        return 1;
    case ENT_BOUNCEPAD_HIGH:
        *out = rect(level->bouncepads_high[index].x, (float)(FLOOR_Y - BP_SRC_H),
                    BP_FRAME_W, BP_SRC_H);
        return 1;
    case ENT_FLOAT_PLATFORM: {
        const FloatPlatformPlacement *fp = &level->float_platforms[index];
        float w = (float)fp->tile_count * FPLAT_PIECE_W;
        float x = fp->x;
        float y = fp->y;
        /* RAIL mode ignores x/y: the platform is centred on its rail point,
         * exactly as canvas.c draws it. */
        if (fp->mode == FLOAT_PLATFORM_RAIL &&
            fp->rail_index >= 0 && fp->rail_index < level->rail_count) {
            editor_rail_placement_position_at(&level->rails[fp->rail_index],
                                              fp->t_offset, &x, &y);
            x -= w * 0.5f;
            y -= FPLAT_PIECE_H * 0.5f;
        }
        *out = rect(x, y, w, FPLAT_PIECE_H);
        return 1;
    }
    case ENT_BRIDGE: {
        const BridgePlacement *br = &level->bridges[index];
        *out = rect(br->x, br->y, (float)br->brick_count * BRIDGE_TILE_W,
                    BRIDGE_TILE_H);
        return 1;
    }
    case ENT_VINE: {
        const VinePlacement *v = &level->vines[index];
        *out = rect(v->x, v->y, VINE_W,
                    (float)(v->tile_count - 1) * VINE_STEP + VINE_H);
        return 1;
    }
    case ENT_LADDER: {
        const LadderPlacement *ld = &level->ladders[index];
        *out = rect(ld->x, ld->y, LADDER_W,
                    (float)(ld->tile_count - 1) * LADDER_STEP + LADDER_H);
        return 1;
    }
    case ENT_ROPE: {
        const RopePlacement *rp = &level->ropes[index];
        *out = rect(rp->x, rp->y, ROPE_W,
                    (float)(rp->tile_count - 1) * ROPE_STEP + ROPE_H);
        return 1;
    }
    case ENT_RAIL: {  /* 16-px tile paths: a rectangle loop or a line */
        const RailPlacement *r = &level->rails[index];
        float h = (r->layout == RAIL_LAYOUT_RECT) ? (float)r->h * RAIL_TILE_H
                                                  : (float)RAIL_TILE_H;
        *out = rect((float)r->x, (float)r->y, (float)r->w * RAIL_TILE_W, h);
        return 1;
    }

    /* ---- World geometry --------------------------------------------- */
    case ENT_PLATFORM: {  /* ground pillar growing up from the floor */
        const PlatformPlacement *p = &level->platforms[index];
        int tile_w = (p->tile_width > 0) ? p->tile_width : 1;
        *out = rect(p->x, (float)FLOOR_Y - (float)p->tile_height * TILE_SIZE + 16.0f,
                    (float)tile_w * TILE_SIZE, (float)p->tile_height * TILE_SIZE);
        return 1;
    }
    case ENT_FLOOR_GAP:
        *out = rect((float)level->floor_gaps[index], (float)FLOOR_Y,
                    FLOOR_GAP_W, (float)(GAME_H - FLOOR_Y));
        return 1;
    case ENT_CHECKPOINT:
        /* Checkpoints are thin markers; keep a forgiving target around
         * the pole, which stands on (x, y) and rises 20 px above it. */
        *out = rect(level->checkpoints[index].x - 4.0f,
                    level->checkpoints[index].y - 20.0f, 9.0f, 24.0f);
        return 1;

    case ENT_COUNT:
        break;
    }
    return 0;
}

/*
 * Reverse render order: enemies and hazards are drawn last (on top), so
 * they are tested first; world geometry is drawn first and tested last.
 */
static const EntityType s_hit_order[] = {
    ENT_SPIDER, ENT_JUMPING_SPIDER, ENT_BIRD, ENT_FASTER_BIRD,
    ENT_FISH, ENT_FASTER_FISH,
    ENT_AXE_TRAP, ENT_CIRCULAR_SAW, ENT_SPIKE_ROW, ENT_SPIKE_PLATFORM,
    ENT_SPIKE_BLOCK, ENT_BLUE_FLAME, ENT_FIRE_FLAME,
    ENT_COIN, ENT_STAR_YELLOW, ENT_STAR_GREEN, ENT_STAR_RED,
    ENT_LAST_STAR, ENT_PLAYER_SPAWN,
    ENT_BOUNCEPAD_SMALL, ENT_BOUNCEPAD_MEDIUM, ENT_BOUNCEPAD_HIGH,
    ENT_FLOAT_PLATFORM, ENT_BRIDGE, ENT_VINE, ENT_LADDER, ENT_ROPE, ENT_RAIL,
    ENT_PLATFORM, ENT_FLOOR_GAP, ENT_CHECKPOINT
};

_Static_assert(sizeof(s_hit_order) / sizeof(s_hit_order[0]) == ENT_COUNT,
               "hit-test order must list every editor entity type");

Selection editor_hit_test(const LevelDef *level, float wx, float wy)
{
    Selection sel = { 0, -1 };
    size_t type_count = sizeof(s_hit_order) / sizeof(s_hit_order[0]);

    if (!level) return sel;

    for (size_t t = 0; t < type_count; t++) {
        EntityType type = s_hit_order[t];
        for (int i = editor_entity_count(level, type) - 1; i >= 0; i--) {
            EditorRect r;
            if (!editor_entity_bounds(level, type, i, &r)) continue;
            /* Half-open box: the right and bottom edges belong to the
             * neighbour, so two touching entities never both claim a pixel. */
            if (wx >= r.x && wx < r.x + r.w && wy >= r.y && wy < r.y + r.h) {
                sel.type = type;
                sel.index = i;
                return sel;
            }
        }
    }
    return sel;  /* no hit — index stays -1 */
}
