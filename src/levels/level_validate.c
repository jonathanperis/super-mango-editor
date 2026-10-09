#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "level_loader.h"
#include "level_ref.h"
#include "level_validate.h"
#include "../player/player.h"  /* JUMP_VY: the jump-height check */
#include "../screens/hud.h"    /* MAX_HEARTS: the initial_hearts limit */

#define MAX_INITIAL_LIVES 999
#define MAX_SCORE_PER_LIFE 999999
#define MAX_COIN_SCORE 999999

/* A gap that starts on the floor grid must also end on it, so the renderer
 * removes exactly the pieces the player can fall through. */
_Static_assert(FLOOR_GAP_W % FLOOR_PIECE_W == 0,
               "FLOOR_GAP_W must be a whole number of floor pieces");

static int fail_count(char *err, size_t err_size,
                      const char *field, int count, int max_count)
{
    if (err && err_size > 0) {
        snprintf(err, err_size, "%s has %d items (max %d)",
                 field, count, max_count);
    }
    return -1;
}

#define CHECK_COUNT(field, max_count) \
    do { \
        if (def->field < 0 || def->field > (max_count)) { \
            return fail_count(err, err_size, #field, def->field, (max_count)); \
        } \
    } while (0)

int level_validate_counts(const LevelDef *def, char *err, size_t err_size)
{
    if (!def) {
        if (err && err_size > 0) snprintf(err, err_size, "LevelDef is NULL");
        return -1;
    }

    CHECK_COUNT(floor_gap_count, MAX_FLOOR_GAPS);
    CHECK_COUNT(checkpoint_count, MAX_CHECKPOINTS);
    CHECK_COUNT(rail_count, MAX_RAILS);
    CHECK_COUNT(platform_count, MAX_PLATFORMS);

    CHECK_COUNT(coin_count, MAX_COINS);
    CHECK_COUNT(star_yellow_count, MAX_STAR_YELLOWS);
    CHECK_COUNT(star_green_count, MAX_STAR_GREENS);
    CHECK_COUNT(star_red_count, MAX_STAR_REDS);

    CHECK_COUNT(spider_count, MAX_SPIDERS);
    CHECK_COUNT(jumping_spider_count, MAX_JUMPING_SPIDERS);
    CHECK_COUNT(bird_count, MAX_BIRDS);
    CHECK_COUNT(faster_bird_count, MAX_FASTER_BIRDS);
    CHECK_COUNT(fish_count, MAX_FISH);
    CHECK_COUNT(faster_fish_count, MAX_FASTER_FISH);

    CHECK_COUNT(axe_trap_count, MAX_AXE_TRAPS);
    CHECK_COUNT(circular_saw_count, MAX_CIRCULAR_SAWS);
    CHECK_COUNT(spike_row_count, MAX_SPIKE_ROWS);
    CHECK_COUNT(spike_platform_count, MAX_SPIKE_PLATFORMS);
    CHECK_COUNT(spike_block_count, MAX_SPIKE_BLOCKS);
    CHECK_COUNT(blue_flame_count, MAX_BLUE_FLAMES);
    CHECK_COUNT(fire_flame_count, MAX_FIRE_FLAMES);

    CHECK_COUNT(float_platform_count, MAX_FLOAT_PLATFORMS);
    CHECK_COUNT(bridge_count, MAX_BRIDGES);
    CHECK_COUNT(bouncepad_small_count, MAX_BOUNCEPADS_SMALL);
    CHECK_COUNT(bouncepad_medium_count, MAX_BOUNCEPADS_MEDIUM);
    CHECK_COUNT(bouncepad_high_count, MAX_BOUNCEPADS_HIGH);

    CHECK_COUNT(vine_count, MAX_VINES);
    CHECK_COUNT(ladder_count, MAX_LADDERS);
    CHECK_COUNT(rope_count, MAX_ROPES);

    CHECK_COUNT(background_layer_count, MAX_BACKGROUND_LAYERS);
    CHECK_COUNT(foreground_layer_count, MAX_BACKGROUND_LAYERS);  /* shares layer limit */
    CHECK_COUNT(fog_layer_count, MAX_FOG_TEXTURES);

    if (err && err_size > 0) err[0] = '\0';
    return 0;
}

#undef CHECK_COUNT

static int fail_value(char *err, size_t err_size, const char *field,
                      const char *msg)
{
    if (err && err_size > 0) {
        snprintf(err, err_size, "%s %s", field, msg);
    }
    return -1;
}

static int fail_range(char *err, size_t err_size, const char *field,
                       int value, int lo, int hi)
{
    if (err && err_size > 0) {
        snprintf(err, err_size, "%s is %d (expected %d..%d)",
                 field, value, lo, hi);
    }
    return -1;
}

static int fail_float_range(char *err, size_t err_size, const char *field,
                            float value, float lo, float hi)
{
    if (err && err_size > 0) {
        snprintf(err, err_size, "%s is %.2f (expected %.2f..%.2f)",
                 field, value, lo, hi);
    }
    return -1;
}

/*
 * IssueSink — where the rule groups below send each error they find.
 *
 * The game only needs to know whether a level is valid, and why not, so
 * level_validate_runtime stops at the first error (report == NULL).  The
 * editor wants every error at once, so level_validate_runtime_each passes
 * a report function and the checks carry on after each one.  Either way a
 * check writes its message into err first and then calls stop_here.
 */
typedef struct {
    LevelIssueFn report;   /* NULL: stop at the first error */
    void        *context;  /* handed back to report          */
    int          errors;   /* errors found so far            */
} IssueSink;

/*
 * stop_here — A check just failed and wrote its message into err.  Count
 * it, pass it on in collect mode, and return 1 when the caller must stop
 * (single-error mode) or 0 to go on with the next check.
 */
static int stop_here(IssueSink *sink, const char *err)
{
    LevelIssueLocation where;

    sink->errors++;
    if (!sink->report) return 1;
    (void)level_issue_location_parse(err, &where);
    sink->report(sink->context, err, &where);
    return 0;
}

static int starts_with(const char *value, const char *prefix)
{
    size_t n = strlen(prefix);
    return strncmp(value, prefix, n) == 0;
}

static int ends_with(const char *value, const char *suffix)
{
    size_t value_len = strlen(value);
    size_t suffix_len = strlen(suffix);
    if (value_len < suffix_len) return 0;
    return strcmp(value + value_len - suffix_len, suffix) == 0;
}

int level_path_has_parent_segment(const char *value)
{
    const char *p = value;
    while (*p) {
        const char *start = p;
        const char *end;
        while (*p && *p != '/') p++;
        end = p;
        if ((end - start) == 2 && start[0] == '.' && start[1] == '.') {
            return 1;
        }
        if (*p == '/') p++;
    }
    return 0;
}

int level_path_has_control_char(const char *value)
{
    const unsigned char *p = (const unsigned char *)value;
    while (*p) {
        if (iscntrl(*p)) return 1;
        p++;
    }
    return 0;
}

static int validate_safe_repo_path(const char *field, const char *value,
                                   char *err, size_t err_size)
{
    if (!value || value[0] == '\0') return 0;

    if (value[0] == '/' || (value[0] == '\\' && value[1] == '\\') ||
        strchr(value, '\\') != NULL ||
        (isalpha((unsigned char)value[0]) && value[1] == ':')) {
        return fail_value(err, err_size, field,
                          "must be a repo-relative forward-slash path");
    }
    if (level_path_has_parent_segment(value)) {
        return fail_value(err, err_size, field,
                          "must not contain '..' path segments");
    }
    if (level_path_has_control_char(value)) {
        return fail_value(err, err_size, field,
                          "must not contain control characters");
    }

    return 0;
}

static int validate_typed_path(const char *field, const char *value,
                               const char *prefix, const char *suffix,
                               char *err, size_t err_size)
{
    if (!value || value[0] == '\0') return 0;
    if (validate_safe_repo_path(field, value, err, err_size) != 0) return -1;
    if (!starts_with(value, prefix) || !ends_with(value, suffix)) {
        if (err && err_size > 0) {
            snprintf(err, err_size, "%s must match %s*%s", field, prefix, suffix);
        }
        return -1;
    }
    return 0;
}

static int validate_level_paths(const LevelDef *def, char *err, size_t err_size,
                                IssueSink *sink)
{
    char field[64];

    if (validate_typed_path("music_path", def->music_path,
                            "assets/sounds/", ".wav", err, err_size) != 0 &&
        stop_here(sink, err))
        return -1;
    if (validate_typed_path("floor_tile_path", def->floor_tile_path,
                            "assets/sprites/levels/", ".png", err, err_size) != 0 &&
        stop_here(sink, err))
        return -1;
    /*
     * next_phase follows the shared level-reference rule (level_ref.h) so a
     * chained phase is always one the campaign and profile can also name.
     * Only one of the two problems is reported: the second rule is the
     * stricter form of the first.
     */
    if (def->next_phase[0] != '\0') {
        if (validate_safe_repo_path("next_phase", def->next_phase,
                                    err, err_size) != 0) {
            if (stop_here(sink, err)) return -1;
        } else if (!level_ref_valid(def->next_phase, strlen(def->next_phase))) {
            fail_value(err, err_size, "next_phase",
                       "must be levels/<name>.toml without subdirectories, "
                       "Windows-reserved characters or device names");
            if (stop_here(sink, err)) return -1;
        }
    }

    for (int i = 0; i < def->platform_count; i++) {
        snprintf(field, sizeof(field), "platforms[%d].tile_path", i);
        if (validate_typed_path(field, def->platforms[i].tile_path,
                                "assets/sprites/levels/", ".png", err, err_size) != 0 &&
            stop_here(sink, err))
            return -1;
    }
    for (int i = 0; i < def->background_layer_count; i++) {
        snprintf(field, sizeof(field), "background_layers[%d].path", i);
        if (validate_typed_path(field, def->background_layers[i].path,
                                "assets/sprites/", ".png", err, err_size) != 0 &&
            stop_here(sink, err))
            return -1;
    }
    for (int i = 0; i < def->foreground_layer_count; i++) {
        snprintf(field, sizeof(field), "foreground_layers[%d].path", i);
        if (validate_typed_path(field, def->foreground_layers[i].path,
                                "assets/sprites/", ".png", err, err_size) != 0 &&
            stop_here(sink, err))
            return -1;
    }
    for (int i = 0; i < def->fog_layer_count; i++) {
        snprintf(field, sizeof(field), "fog_layers[%d].path", i);
        if (validate_typed_path(field, def->fog_layers[i].path,
                                "assets/sprites/", ".png", err, err_size) != 0 &&
            stop_here(sink, err))
            return -1;
    }

    return 0;
}

static int validate_finite_float(char *err, size_t err_size,
                                 const char *field, float value)
{
    if (!isfinite(value)) {
        return fail_value(err, err_size, field, "must be finite");
    }
    return 0;
}

static int validate_motion(char *err, size_t err_size, const char *field, float value)
{
    if (!isfinite(value) || fabsf(value) > MAX_LEVEL_MOTION)
        return fail_float_range(err, err_size, field, value,
                                -MAX_LEVEL_MOTION, MAX_LEVEL_MOTION);
    return 0;
}

/*
 * validate_patrol_speed — An enemy's vx is its patrol speed for the whole
 * level (the sign only picks the first direction). 0 would leave it frozen,
 * and above MAX_PATROL_SPEED a spider could step over a floor gap between
 * two of its once-per-step gap checks.
 */
static int validate_patrol_speed(char *err, size_t err_size,
                                 const char *field, float vx)
{
    if (!isfinite(vx) || vx == 0.0f || fabsf(vx) > (float)MAX_PATROL_SPEED) {
        if (err && err_size > 0) {
            snprintf(err, err_size,
                     "%s is %.2f (must be nonzero and at most %d px/s either way)",
                     field, vx, MAX_PATROL_SPEED);
        }
        return -1;
    }
    return 0;
}

static int validate_world_x(char *err, size_t err_size,
                            const char *field, float x, float world_w)
{
    if (validate_finite_float(err, err_size, field, x) != 0) return -1;
    if (x < 0.0f || x > world_w) {
        return fail_float_range(err, err_size, field, x, 0.0f, world_w);
    }
    return 0;
}

static int validate_world_y(char *err, size_t err_size,
                            const char *field, float y)
{
    if (validate_finite_float(err, err_size, field, y) != 0) return -1;
    if (y < 0.0f || y > (float)GAME_H) {
        return fail_float_range(err, err_size, field, y, 0.0f, (float)GAME_H);
    }
    return 0;
}

static int validate_world_rect(char *err, size_t err_size,
                               const char *field, float x, float y,
                               float w, float h, float world_w)
{
    if (validate_finite_float(err, err_size, field, x) != 0) return -1;
    if (validate_finite_float(err, err_size, field, y) != 0) return -1;
    if (validate_finite_float(err, err_size, field, w) != 0) return -1;
    if (validate_finite_float(err, err_size, field, h) != 0) return -1;
    if (x < 0.0f) {
        return fail_float_range(err, err_size, field, x, 0.0f, world_w);
    }
    if (x + w > world_w) {
        return fail_float_range(err, err_size, field, x + w, 0.0f, world_w);
    }
    if (y < 0.0f) {
        return fail_float_range(err, err_size, field, y, 0.0f, (float)GAME_H);
    }
    if (y + h > (float)GAME_H) {
        return fail_float_range(err, err_size, field, y + h, 0.0f, (float)GAME_H);
    }
    return 0;
}

static int validate_gap_x(char *err, size_t err_size,
                          const char *field, float x, float world_w)
{
    if (validate_finite_float(err, err_size, field, x) != 0) return -1;
    if (x < 0.0f || x + (float)FLOOR_GAP_W > world_w) {
        return fail_float_range(err, err_size, field, x,
                                0.0f, world_w - (float)FLOOR_GAP_W);
    }
    return 0;
}

static int validate_climbable_rect(char *err, size_t err_size,
                                   const char *field, float x, float y,
                                   int tile_count, int width,
                                   int content_h, int step, float world_w)
{
    float h;

    if (tile_count < 1) {
        return fail_range(err, err_size, field, tile_count, 1, 999);
    }

    h = (float)content_h + (float)(tile_count - 1) * (float)step;
    return validate_world_rect(err, err_size, field, x, y,
                               (float)width, h, world_w);
}

static int validate_physics_finite(const LevelDef *def,
                                   char *err, size_t err_size, IssueSink *sink)
{
#define CHECK_PHYSICS_FIELD(field) \
    do { \
        if (validate_motion(err, err_size, "physics." #field, \
                            def->physics.field) != 0 && \
            stop_here(sink, err)) return -1; \
    } while (0)

    CHECK_PHYSICS_FIELD(walk_max_speed);
    CHECK_PHYSICS_FIELD(run_max_speed);
    CHECK_PHYSICS_FIELD(walk_ground_accel);
    CHECK_PHYSICS_FIELD(run_ground_accel);
    CHECK_PHYSICS_FIELD(ground_friction);
    CHECK_PHYSICS_FIELD(ground_counter_accel);
    CHECK_PHYSICS_FIELD(air_accel_walk);
    CHECK_PHYSICS_FIELD(air_accel_run);
    CHECK_PHYSICS_FIELD(air_friction);
    CHECK_PHYSICS_FIELD(cam_lookahead_vx_factor);
    CHECK_PHYSICS_FIELD(cam_lookahead_max);

    return 0;

#undef CHECK_PHYSICS_FIELD
}

static int validate_rail(const RailPlacement *rail, int index,
                         char *err, size_t err_size)
{
    char field[64];
    int tile_count;

    if (rail->layout == RAIL_LAYOUT_RECT) {
        if (rail->w < 2 || rail->w > MAX_RAIL_TILES) {
            snprintf(field, sizeof(field), "rails[%d].w", index);
            return fail_range(err, err_size, field, rail->w, 2, MAX_RAIL_TILES);
        }
        if (rail->h < 2 || rail->h > MAX_RAIL_TILES) {
            snprintf(field, sizeof(field), "rails[%d].h", index);
            return fail_range(err, err_size, field, rail->h, 2, MAX_RAIL_TILES);
        }

        tile_count = rail->w * 2 + (rail->h - 2) * 2;
        if (tile_count > MAX_RAIL_TILES) {
            snprintf(field, sizeof(field), "rails[%d]", index);
            return fail_range(err, err_size, field, tile_count, 1, MAX_RAIL_TILES);
        }
    } else if (rail->layout == RAIL_LAYOUT_HORIZ) {
        if (rail->w < 2 || rail->w > MAX_RAIL_TILES) {
            snprintf(field, sizeof(field), "rails[%d].w", index);
            return fail_range(err, err_size, field, rail->w, 2, MAX_RAIL_TILES);
        }
        if (rail->end_cap != 0 && rail->end_cap != 1) {
            snprintf(field, sizeof(field), "rails[%d].end_cap", index);
            return fail_range(err, err_size, field, rail->end_cap, 0, 1);
        }
    } else {
        snprintf(field, sizeof(field), "rails[%d].layout", index);
        return fail_value(err, err_size, field, "is invalid");
    }

    return 0;
}

static int validate_rail_index(char *err, size_t err_size,
                               const char *field, int value, int rail_count)
{
    if (rail_count <= 0) {
        if (err && err_size > 0) {
            snprintf(err, err_size, "%s references rail %d but no rails exist",
                     field, value);
        }
        return -1;
    }
    if (value < 0 || value >= rail_count) {
        return fail_range(err, err_size, field, value, 0, rail_count - 1);
    }
    return 0;
}

/*
 * validate_rail_speed — a rail rider must move forward, and no faster than
 * MAX_RAIL_SPEED (rail.h explains both limits).
 */
static int validate_rail_speed(char *err, size_t err_size, const char *field,
                               float speed)
{
    if (!isfinite(speed) || speed <= 0.0f || speed > (float)MAX_RAIL_SPEED) {
        if (err && err_size > 0) {
            snprintf(err, err_size,
                     "%s is %.2f (expected above 0 and at most %d tiles/s)",
                     field, speed, MAX_RAIL_SPEED);
        }
        return -1;
    }
    return 0;
}

/*
 * validate_bouncepads — shared rules for the three bouncepad arrays.
 *
 * Standing on a pad relaunches the player every step and never lets them
 * jump, so a pad weaker than a normal jump (or one pushing down) would trap
 * them in a buzzing hop with the spring sound replaying. launch_vy must be
 * at least as strong as JUMP_VY, and finite and bounded like any motion.
 */
static int validate_bouncepads(char *err, size_t err_size, const char *name,
                               const BouncepadPlacement *pads, int count,
                               float world_w, IssueSink *sink)
{
    char field[64];

    for (int i = 0; i < count; i++) {
        snprintf(field, sizeof(field), "%s[%d].x", name, i);
        if (validate_world_x(err, err_size, field, pads[i].x, world_w) != 0 &&
            stop_here(sink, err))
            return -1;
        snprintf(field, sizeof(field), "%s[%d].launch_vy", name, i);
        if (validate_motion(err, err_size, field, pads[i].launch_vy) != 0) {
            if (stop_here(sink, err)) return -1;
        } else if (pads[i].launch_vy > JUMP_VY) {
            if (err && err_size > 0) {
                snprintf(err, err_size,
                         "%s is %.2f (must be %.0f or lower, at least a normal jump upward)",
                         field, pads[i].launch_vy, JUMP_VY);
            }
            if (stop_here(sink, err)) return -1;
        }
        if (pads[i].pad_type != BOUNCEPAD_GREEN &&
            pads[i].pad_type != BOUNCEPAD_WOOD &&
            pads[i].pad_type != BOUNCEPAD_RED) {
            snprintf(field, sizeof(field), "%s[%d].pad_type", name, i);
            fail_value(err, err_size, field, "is invalid");
            if (stop_here(sink, err)) return -1;
        }
    }
    return 0;
}

/*
 * validate_patrol — x and its patrol range lie in the world, in order.
 *
 * entity_w is the width the patrol code turns the entity around with: it
 * turns when its right edge (x + entity_w) reaches patrol_x1 and its left
 * edge reaches patrol_x0. A range narrower than that snaps the entity from
 * one end to the other every step, so it is rejected. Saws bounce their x
 * itself between the bounds and pass 0.
 */
static int validate_patrol(char *err, size_t err_size, const char *field,
                           float x, float patrol_x0, float patrol_x1,
                           float entity_w, float world_w)
{
    char child[96];

    snprintf(child, sizeof(child), "%s.x", field);
    if (validate_finite_float(err, err_size, child, x) != 0) return -1;
    if (patrol_x0 > patrol_x1) {
        snprintf(child, sizeof(child), "%s.patrol", field);
        return fail_value(err, err_size, child, "has reversed bounds");
    }
    snprintf(child, sizeof(child), "%s.patrol_x0", field);
    if (validate_world_x(err, err_size, child, patrol_x0, world_w) != 0)
        return -1;
    snprintf(child, sizeof(child), "%s.patrol_x1", field);
    if (validate_world_x(err, err_size, child, patrol_x1, world_w) != 0)
        return -1;
    if (patrol_x1 - patrol_x0 < entity_w) {
        snprintf(child, sizeof(child), "%s.patrol", field);
        if (err && err_size > 0) {
            snprintf(err, err_size,
                     "%s is %.2f px wide (must be at least %.0f, the sprite width)",
                     child, patrol_x1 - patrol_x0, entity_w);
        }
        return -1;
    }
    if (x < patrol_x0 || x > patrol_x1) {
        snprintf(child, sizeof(child), "%s.x", field);
        return fail_float_range(err, err_size, child, x, patrol_x0, patrol_x1);
    }
    return 0;
}

static int validate_point(char *err, size_t err_size, const char *field,
                          float x, float y, float world_w)
{
    char child[96];

    snprintf(child, sizeof(child), "%s.x", field);
    if (validate_world_x(err, err_size, child, x, world_w) != 0) return -1;
    snprintf(child, sizeof(child), "%s.y", field);
    if (validate_world_y(err, err_size, child, y) != 0) return -1;
    return 0;
}

/*
 * validate_checkpoint — One authored checkpoint: after the start, inside
 * the world, clear of floor gaps, and not sharing its x with an earlier
 * one.  Each check builds on the one before, so only the first failure
 * is reported.
 */
static int validate_checkpoint(const LevelDef *def, int i, float start_x,
                               float world_w, char *err, size_t err_size)
{
    {
        char field[64];
        const CheckpointPlacement *checkpoint = &def->checkpoints[i];

        snprintf(field, sizeof(field), "checkpoints[%d].x", i);
        if (validate_finite_float(err, err_size, field, checkpoint->x) != 0)
            return -1;
        if (checkpoint->x <= start_x) {
            return fail_value(err, err_size, field,
                              "must be strictly after effective start x");
        }
        if (checkpoint->x < 0.0f ||
            checkpoint->x > world_w - (float)TILE_SIZE) {
            return fail_float_range(err, err_size, field, checkpoint->x,
                                    0.0f, world_w - (float)TILE_SIZE);
        }

        /*
         * The player respawns in the TILE_SIZE-wide column starting at x and
         * drops straight down. If that column touches a floor gap, the
         * player can respawn straight into the water and lose a life on
         * every respawn. Automatic checkpoints skip such columns
         * (game_checkpoint.c); authored ones must avoid them too.
         */
        for (int g = 0; g < def->floor_gap_count; g++) {
            float gap_x = (float)def->floor_gaps[g];
            if (checkpoint->x < gap_x + (float)FLOOR_GAP_W &&
                gap_x < checkpoint->x + (float)TILE_SIZE) {
                if (err && err_size > 0) {
                    snprintf(err, err_size,
                             "%s respawn column overlaps floor_gaps[%d] at %d",
                             field, g, def->floor_gaps[g]);
                }
                return -1;
            }
        }

        snprintf(field, sizeof(field), "checkpoints[%d].y", i);
        if (validate_world_y(err, err_size, field, checkpoint->y) != 0)
            return -1;

        for (int previous = 0; previous < i; previous++) {
            if (checkpoint->x == def->checkpoints[previous].x) {
                snprintf(field, sizeof(field), "checkpoints[%d].x", i);
                return fail_value(err, err_size, field,
                                  "has a duplicate x coordinate");
            }
        }
    }
    return 0;
}

static int validate_checkpoints(const LevelDef *def, char *err, size_t err_size,
                                float world_w, IssueSink *sink)
{
    float start_x;
    float start_y;

    if (def->checkpoint_count == 0) return 0;
    level_effective_spawn(def, &start_x, &start_y);
    (void)start_y;

    for (int i = 0; i < def->checkpoint_count; i++) {
        if (validate_checkpoint(def, i, start_x, world_w, err, err_size) != 0 &&
            stop_here(sink, err))
            return -1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Rule groups for level_validate_runtime                             */
/*                                                                    */
/* Each helper checks one family of fields.  At every failure it      */
/* fills err and asks stop_here whether to go on: the game's single-  */
/* error mode returns -1 at the first one, the editor's collect mode  */
/* reports it and carries on.  They appear in the order the runtime   */
/* checks call them, which is the order messages come out in: the     */
/* first one is what the game reports, so moving a call changes which */
/* message a broken level gets (tests compare them).                  */
/* ------------------------------------------------------------------ */

/*
 * validate_level_settings — Format version, world size, audio volume and
 * the hearts/lives/score limits.
 */
static int validate_level_settings(const LevelDef *def,
                                   char *err, size_t err_size, IssueSink *sink)
{
    if (def->format_version != LEVEL_FORMAT_VERSION) {
        if (err && err_size > 0) {
            snprintf(err, err_size, "format_version is %d (expected %d)",
                     def->format_version, LEVEL_FORMAT_VERSION);
        }
        if (stop_here(sink, err)) return -1;
    }

    if (def->screen_count < 0 || def->screen_count > MAX_LEVEL_SCREENS) {
        fail_range(err, err_size, "screen_count", def->screen_count,
                   0, MAX_LEVEL_SCREENS);
        if (stop_here(sink, err)) return -1;
    }

    if (def->music_volume < 0 || def->music_volume > LEVEL_MUSIC_VOLUME_MAX) {
        fail_range(err, err_size, "music_volume", def->music_volume, 0,
                   LEVEL_MUSIC_VOLUME_MAX);
        if (stop_here(sink, err)) return -1;
    }
    if (def->initial_hearts < 0 || def->initial_hearts > MAX_HEARTS) {
        fail_range(err, err_size, "initial_hearts", def->initial_hearts, 0, MAX_HEARTS);
        if (stop_here(sink, err)) return -1;
    }
    if (def->initial_lives < 0 || def->initial_lives > MAX_INITIAL_LIVES) {
        fail_range(err, err_size, "initial_lives", def->initial_lives,
                   0, MAX_INITIAL_LIVES);
        if (stop_here(sink, err)) return -1;
    }
    if (def->score_per_life < 0 || def->score_per_life > MAX_SCORE_PER_LIFE) {
        fail_range(err, err_size, "score_per_life", def->score_per_life,
                   0, MAX_SCORE_PER_LIFE);
        if (stop_here(sink, err)) return -1;
    }
    if (def->coin_score < 0 || def->coin_score > MAX_COIN_SCORE) {
        fail_range(err, err_size, "coin_score", def->coin_score,
                   0, MAX_COIN_SCORE);
        if (stop_here(sink, err)) return -1;
    }
    return 0;
}

float level_jumping_spider_min_gap_speed(void)
{
    /*
     * Count the fixed steps of one jump exactly as jumping_spiders_update
     * moves the spider: each step adds gravity to vy, then vy to y, until
     * y is back at the floor (0). With the shipped constants that is 39.
     */
    const float dt = 1.0f / (float)TARGET_FPS;   /* GAME_FIXED_STEP */
    float y = 0.0f, vy = JSPIDER_JUMP_VY;
    int air_steps = 0;
    do {
        vy += JSPIDER_GRAVITY * dt;
        y += vy * dt;
        air_steps++;
    } while (y < 0.0f);
    /*
     * The leap starts with the art centre snapped to the gap's near edge.
     * The spider then moves on every later step before it next looks for
     * a gap: the air steps after the first, plus the step after landing,
     * air_steps moves in all. They must carry the centre a whole
     * FLOOR_GAP_W, or it lands still over the gap, snaps back and leaps
     * again forever. Rounded up to the next 0.01 px/s, so a speed written
     * with two decimals that passes also clears the gap.
     */
    float speed = (float)FLOOR_GAP_W / ((float)air_steps * dt);
    return ceilf(speed * 100.0f) / 100.0f;
}

/*
 * validate_jumping_spider_gaps — A jumping spider whose patrol reaches a
 * floor gap must be fast enough to jump it
 * (level_jumping_spider_min_gap_speed). Its art centre, the point the gap
 * test uses, moves between these two x values.
 */
static int validate_jumping_spider_gaps(const LevelDef *def,
                                        char *err, size_t err_size, IssueSink *sink)
{
    const float min_speed = level_jumping_spider_min_gap_speed();
    const float centre = (float)JSPIDER_ART_X + (float)JSPIDER_ART_W / 2.0f;
    for (int i = 0; i < def->jumping_spider_count; i++) {
        const JumpingSpiderPlacement *p = &def->jumping_spiders[i];
        float lo = p->patrol_x0 + centre;
        float hi = p->patrol_x1 - (float)JSPIDER_FRAME_W + centre;
        int reaches_gap = 0;
        for (int g = 0; g < def->floor_gap_count; g++) {
            float gx = (float)def->floor_gaps[g];
            if (lo < gx + (float)FLOOR_GAP_W && gx <= hi) reaches_gap = 1;
        }
        if (!reaches_gap || !isfinite(p->vx) || fabsf(p->vx) >= min_speed) continue;
        if (err && err_size > 0)
            snprintf(err, err_size,
                     "jumping_spiders[%d].vx is %.2f; its patrol reaches a floor gap, "
                     "and jumping one needs at least %.2f px/s either way",
                     i, p->vx, min_speed);
        if (stop_here(sink, err)) return -1;
    }
    return 0;
}

/*
 * validate_entity_motion — Every per-entity speed must be finite and
 * bounded. Enemy patrol speeds follow the tighter MAX_PATROL_SPEED rule;
 * bouncepads, spike blocks and float platforms have their own speed rules
 * in their helpers below.
 */
static int validate_entity_motion(const LevelDef *def,
                                  char *err, size_t err_size, IssueSink *sink)
{
    char field[64];
    /* Every message names the element (spiders[2].vx), so the editor can
     * take the designer straight to it. */
#define CHECK_PATROL_SPEED_ARRAY(array, count) \
    for (int i = 0; i < def->count; i++) { \
        snprintf(field, sizeof(field), #array "[%d].vx", i); \
        if (validate_patrol_speed(err, err_size, field, def->array[i].vx) != 0 && \
            stop_here(sink, err)) return -1; \
    }
    CHECK_PATROL_SPEED_ARRAY(spiders, spider_count);
    CHECK_PATROL_SPEED_ARRAY(jumping_spiders, jumping_spider_count);
    CHECK_PATROL_SPEED_ARRAY(birds, bird_count);
    CHECK_PATROL_SPEED_ARRAY(faster_birds, faster_bird_count);
    CHECK_PATROL_SPEED_ARRAY(fish, fish_count);
    CHECK_PATROL_SPEED_ARRAY(faster_fish, faster_fish_count);
#undef CHECK_PATROL_SPEED_ARRAY

#define CHECK_MOTION_ARRAY(array, count, member) \
    for (int i = 0; i < def->count; i++) { \
        snprintf(field, sizeof(field), #array "[%d]." #member, i); \
        if (validate_motion(err, err_size, field, def->array[i].member) != 0 && \
            stop_here(sink, err)) return -1; \
    }
    CHECK_MOTION_ARRAY(background_layers, background_layer_count, speed);
    CHECK_MOTION_ARRAY(foreground_layers, foreground_layer_count, speed);
    CHECK_MOTION_ARRAY(fog_layers, fog_layer_count, speed);
#undef CHECK_MOTION_ARRAY
    return 0;
}

/* validate_player_start — An authored start point must lie inside the
 * world. 0,0 means "use the default start" and is not checked. */
static int validate_player_start(const LevelDef *def, char *err,
                                 size_t err_size, float world_w, IssueSink *sink)
{
    if (def->player_start_x != 0.0f || def->player_start_y != 0.0f) {
        if (validate_point(err, err_size, "player_start",
                           def->player_start_x, def->player_start_y, world_w) != 0 &&
            stop_here(sink, err))
            return -1;
    }
    return 0;
}

/* validate_floor_gaps — Each gap fits in the world and starts on the
 * floor-piece grid. */
static int validate_floor_gaps(const LevelDef *def, char *err,
                               size_t err_size, float world_w, IssueSink *sink)
{
    char field[64];

    for (int i = 0; i < def->floor_gap_count; i++) {
        snprintf(field, sizeof(field), "floor_gaps[%d]", i);
        if (def->floor_gaps[i] < 0 ||
            def->floor_gaps[i] > (int)world_w - FLOOR_GAP_W) {
            fail_range(err, err_size, field, def->floor_gaps[i],
                       0, (int)world_w - FLOOR_GAP_W);
            if (stop_here(sink, err)) return -1;
        } else if (def->floor_gaps[i] % FLOOR_PIECE_W != 0) {
            /* Off-grid gaps render partly covered by grass (see FLOOR_PIECE_W). */
            if (err && err_size > 0) {
                snprintf(err, err_size,
                         "%s is %d (must be a multiple of %d, the floor piece width)",
                         field, def->floor_gaps[i], FLOOR_PIECE_W);
            }
            if (stop_here(sink, err)) return -1;
        }
    }
    return 0;
}

/* validate_rails — Each rail has a valid shape (validate_rail) and its
 * whole track lies inside the world. */
static int validate_rails(const LevelDef *def, char *err, size_t err_size,
                          float world_w, IssueSink *sink)
{
    char field[64];

    for (int i = 0; i < def->rail_count; i++) {
        /* A rail with a broken shape or origin has no track to measure, so
         * the position checks below would only repeat the same problem. */
        if (validate_rail(&def->rails[i], i, err, err_size) != 0) {
            if (stop_here(sink, err)) return -1;
            continue;
        }
        if (def->rails[i].x < 0 || def->rails[i].y < 0) {
            snprintf(field, sizeof(field), "rails[%d]", i);
            fail_value(err, err_size, field, "has negative origin");
            if (stop_here(sink, err)) return -1;
            continue;
        }
        if (def->rails[i].layout == RAIL_LAYOUT_RECT) {
            snprintf(field, sizeof(field), "rails[%d]", i);
            if (validate_world_rect(err, err_size, field,
                                    (float)def->rails[i].x,
                                    (float)def->rails[i].y,
                                    (float)(def->rails[i].w * RAIL_TILE_W),
                                    (float)(def->rails[i].h * RAIL_TILE_H),
                                    world_w) != 0 &&
                stop_here(sink, err)) return -1;
        } else if (def->rails[i].layout == RAIL_LAYOUT_HORIZ) {
            snprintf(field, sizeof(field), "rails[%d]", i);
            if (validate_world_rect(err, err_size, field,
                                    (float)def->rails[i].x,
                                    (float)def->rails[i].y,
                                    (float)(def->rails[i].w * RAIL_TILE_W),
                                    (float)RAIL_TILE_H,
                                    world_w) != 0 &&
                stop_here(sink, err)) return -1;
        }
    }
    return 0;
}

/* validate_platforms — Ground pillars: a sane height and width, and a
 * footprint inside the world. */
static int validate_platforms(const LevelDef *def, char *err,
                              size_t err_size, float world_w, IssueSink *sink)
{
    char field[64];

    for (int i = 0; i < def->platform_count; i++) {
        const PlatformPlacement *p = &def->platforms[i];
        int tw = p->tile_width > 0 ? p->tile_width : 1;
        if (p->tile_height < 1 || p->tile_height > (FLOOR_Y + 16) / TILE_SIZE) {
            snprintf(field, sizeof(field), "platforms[%d].tile_height", i);
            fail_range(err, err_size, field, p->tile_height, 1, 999);
            if (stop_here(sink, err)) return -1;
        } else if (level_platform_top_y(p->tile_height) < 0.0f) {
            snprintf(field, sizeof(field), "platforms[%d].tile_height", i);
            fail_range(err, err_size, field, p->tile_height, 1,
                       (FLOOR_Y + 16) / TILE_SIZE);
            if (stop_here(sink, err)) return -1;
        }
        if (p->tile_width < 0 || p->tile_width > (int)world_w / TILE_SIZE) {
            snprintf(field, sizeof(field), "platforms[%d].tile_width", i);
            fail_range(err, err_size, field, p->tile_width, 0, 999);
            if (stop_here(sink, err)) return -1;
            continue;   /* the footprint check below needs a sane width */
        }
        snprintf(field, sizeof(field), "platforms[%d]", i);
        if (validate_world_rect(err, err_size, field, p->x, 0.0f,
                                (float)(tw * TILE_SIZE), 1.0f, world_w) != 0 &&
            stop_here(sink, err))
            return -1;
    }
    return 0;
}

/* validate_collectibles — Coins, stars and the last star lie inside the
 * world. A last star at 0,0 means the level has none. */
static int validate_collectibles(const LevelDef *def, char *err,
                                 size_t err_size, float world_w, IssueSink *sink)
{
    char field[64];

    for (int i = 0; i < def->coin_count; i++) {
        snprintf(field, sizeof(field), "coins[%d]", i);
        if (validate_point(err, err_size, field, def->coins[i].x,
                           def->coins[i].y, world_w) != 0 &&
            stop_here(sink, err)) return -1;
    }
    for (int i = 0; i < def->star_yellow_count; i++) {
        snprintf(field, sizeof(field), "star_yellows[%d]", i);
        if (validate_point(err, err_size, field, def->star_yellows[i].x,
                           def->star_yellows[i].y, world_w) != 0 &&
            stop_here(sink, err)) return -1;
    }
    for (int i = 0; i < def->star_green_count; i++) {
        snprintf(field, sizeof(field), "star_greens[%d]", i);
        if (validate_point(err, err_size, field, def->star_greens[i].x,
                           def->star_greens[i].y, world_w) != 0 &&
            stop_here(sink, err)) return -1;
    }
    for (int i = 0; i < def->star_red_count; i++) {
        snprintf(field, sizeof(field), "star_reds[%d]", i);
        if (validate_point(err, err_size, field, def->star_reds[i].x,
                           def->star_reds[i].y, world_w) != 0 &&
            stop_here(sink, err)) return -1;
    }
    if (def->last_star.x != 0.0f || def->last_star.y != 0.0f) {
        if (validate_point(err, err_size, "last_star", def->last_star.x,
                           def->last_star.y, world_w) != 0 &&
            stop_here(sink, err)) return -1;
    }
    return 0;
}

/*
 * validate_enemies — Each patrol range must contain its start x and be at
 * least as wide as the sprite that walks it; sprite frames must exist and
 * birds must fly inside the screen height.
 */
static int validate_enemies(const LevelDef *def, char *err, size_t err_size,
                            float world_w, IssueSink *sink)
{
    char field[64];

    for (int i = 0; i < def->spider_count; i++) {
        if (def->spiders[i].frame_index < 0 || def->spiders[i].frame_index >= SPIDER_FRAMES)
        {
            snprintf(field, sizeof(field), "spiders[%d].frame_index", i);
            fail_value(err, err_size, field, "is outside the sprite sheet");
            if (stop_here(sink, err)) return -1;
        }
        snprintf(field, sizeof(field), "spiders[%d]", i);
        if (validate_patrol(err, err_size, field, def->spiders[i].x,
                            def->spiders[i].patrol_x0,
                            def->spiders[i].patrol_x1,
                            (float)SPIDER_FRAME_W, world_w) != 0 &&
            stop_here(sink, err)) return -1;
    }
    for (int i = 0; i < def->jumping_spider_count; i++) {
        snprintf(field, sizeof(field), "jumping_spiders[%d]", i);
        if (validate_patrol(err, err_size, field, def->jumping_spiders[i].x,
                            def->jumping_spiders[i].patrol_x0,
                            def->jumping_spiders[i].patrol_x1,
                            (float)JSPIDER_FRAME_W, world_w) != 0 &&
            stop_here(sink, err)) return -1;
    }
    for (int i = 0; i < def->bird_count; i++) {
        if (def->birds[i].frame_index < 0 || def->birds[i].frame_index >= BIRD_FRAMES)
        {
            snprintf(field, sizeof(field), "birds[%d].frame_index", i);
            fail_value(err, err_size, field, "is outside the sprite sheet");
            if (stop_here(sink, err)) return -1;
        }
        snprintf(field, sizeof(field), "birds[%d]", i);
        if (validate_patrol(err, err_size, field, def->birds[i].x,
                            def->birds[i].patrol_x0,
                            def->birds[i].patrol_x1,
                            (float)BIRD_FRAME_W, world_w) != 0 &&
            stop_here(sink, err)) return -1;
        snprintf(field, sizeof(field), "birds[%d].base_y", i);
        if (validate_world_y(err, err_size, field,
                             def->birds[i].base_y) != 0 &&
            stop_here(sink, err)) return -1;
    }
    for (int i = 0; i < def->faster_bird_count; i++) {
        if (def->faster_birds[i].frame_index < 0 || def->faster_birds[i].frame_index >= FBIRD_FRAMES)
        {
            snprintf(field, sizeof(field), "faster_birds[%d].frame_index", i);
            fail_value(err, err_size, field, "is outside the sprite sheet");
            if (stop_here(sink, err)) return -1;
        }
        snprintf(field, sizeof(field), "faster_birds[%d]", i);
        if (validate_patrol(err, err_size, field, def->faster_birds[i].x,
                            def->faster_birds[i].patrol_x0,
                            def->faster_birds[i].patrol_x1,
                            (float)FBIRD_FRAME_W, world_w) != 0 &&
            stop_here(sink, err)) return -1;
        snprintf(field, sizeof(field), "faster_birds[%d].base_y", i);
        if (validate_world_y(err, err_size, field,
                             def->faster_birds[i].base_y) != 0 &&
            stop_here(sink, err)) return -1;
    }
    for (int i = 0; i < def->fish_count; i++) {
        snprintf(field, sizeof(field), "fish[%d]", i);
        if (validate_patrol(err, err_size, field, def->fish[i].x,
                            def->fish[i].patrol_x0,
                            def->fish[i].patrol_x1,
                            (float)FISH_RENDER_W, world_w) != 0 &&
            stop_here(sink, err)) return -1;
    }
    for (int i = 0; i < def->faster_fish_count; i++) {
        snprintf(field, sizeof(field), "faster_fish[%d]", i);
        if (validate_patrol(err, err_size, field, def->faster_fish[i].x,
                            def->faster_fish[i].patrol_x0,
                            def->faster_fish[i].patrol_x1,
                            (float)FISH_RENDER_W, world_w) != 0 &&
            stop_here(sink, err)) return -1;
    }
    return 0;
}

/* validate_spike_strips — Ground spike rows and spike platforms: a sane
 * tile count and their full width inside the world. */
static int validate_spike_strips(const LevelDef *def, char *err,
                                 size_t err_size, float world_w, IssueSink *sink)
{
    char field[64];

    /* The world-rect checks use the tile count for the width, so a bad
     * count skips them (continue) instead of reporting a second error. */
    for (int i = 0; i < def->spike_row_count; i++) {
        int n = def->spike_rows[i].count;
        if (n < 1 || n > MAX_SPIKE_TILES) {
            snprintf(field, sizeof(field), "spike_rows[%d].count", i);
            fail_range(err, err_size, field, n, 1, MAX_SPIKE_TILES);
            if (stop_here(sink, err)) return -1;
            continue;
        }
        snprintf(field, sizeof(field), "spike_rows[%d]", i);
        if (validate_world_rect(err, err_size, field, def->spike_rows[i].x,
                                (float)(FLOOR_Y - SPIKE_TILE_H),
                                (float)(n * SPIKE_TILE_W),
                                (float)SPIKE_TILE_H, world_w) != 0 &&
            stop_here(sink, err)) return -1;
    }

    for (int i = 0; i < def->spike_platform_count; i++) {
        int n = def->spike_platforms[i].tile_count;
        if (n < 1 || n > MAX_SPIKE_TILES) {
            snprintf(field, sizeof(field), "spike_platforms[%d].tile_count", i);
            fail_range(err, err_size, field, n, 1, MAX_SPIKE_TILES);
            if (stop_here(sink, err)) return -1;
            continue;
        }
        snprintf(field, sizeof(field), "spike_platforms[%d]", i);
        if (validate_world_rect(err, err_size, field, def->spike_platforms[i].x,
                                def->spike_platforms[i].y,
                                (float)(n * SPIKE_PLAT_PIECE_W),
                                (float)SPIKE_PLAT_SRC_H, world_w) != 0 &&
            stop_here(sink, err)) return -1;
    }
    return 0;
}

/*
 * rail_offset_is_on_track — Whether t_offset t names a point on the rail.
 *
 * t counts rail tiles from the start. A closed (RECT) loop accepts any t
 * in [0, tile count); an open (HORIZ) line ends on its last tile, so t
 * must also be at most count - 1.
 */
static int rail_offset_is_on_track(const RailPlacement *rail, float t)
{
    int count = rail->layout == RAIL_LAYOUT_RECT
              ? 2 * rail->w + 2 * (rail->h - 2) : rail->w;

    if (!isfinite(t) || t < 0.0f || t >= (float)count) return 0;
    if (rail->layout == RAIL_LAYOUT_HORIZ && t > (float)(count - 1)) return 0;
    return 1;
}

/* validate_spike_blocks — A spike block names a real rail, starts on it
 * and moves at a valid rail speed. */
static int validate_spike_blocks(const LevelDef *def, char *err,
                                 size_t err_size, IssueSink *sink)
{
    char field[64];

    for (int i = 0; i < def->spike_block_count; i++) {
        snprintf(field, sizeof(field), "spike_blocks[%d].rail_index", i);
        if (validate_rail_index(err, err_size, field,
                                def->spike_blocks[i].rail_index,
                                def->rail_count) != 0) {
            if (stop_here(sink, err)) return -1;
        } else if (!rail_offset_is_on_track(&def->rails[def->spike_blocks[i].rail_index],
                                            def->spike_blocks[i].t_offset)) {
            /* rails[rail_index] is only read once the index is known good. */
            snprintf(field, sizeof(field), "spike_blocks[%d].t_offset", i);
            fail_value(err, err_size, field, "must lie on the referenced rail");
            if (stop_here(sink, err)) return -1;
        }
        snprintf(field, sizeof(field), "spike_blocks[%d].speed", i);
        if (validate_rail_speed(err, err_size, field,
                                def->spike_blocks[i].speed) != 0 &&
            stop_here(sink, err)) return -1;
    }
    return 0;
}

/*
 * validate_float_platforms — A known mode and tile count. A rail platform
 * follows the same rail rules as a spike block; a fixed or crumbling one
 * must sit inside the world.
 */
static int validate_float_platforms(const LevelDef *def, char *err,
                                    size_t err_size, float world_w, IssueSink *sink)
{
    char field[64];

    for (int i = 0; i < def->float_platform_count; i++) {
        const FloatPlatformPlacement *fp = &def->float_platforms[i];
        /* An unknown mode or tile count says nothing reliable about which
         * of the checks below apply, so the platform's other checks are
         * skipped (continue). */
        if (fp->mode != FLOAT_PLATFORM_STATIC &&
            fp->mode != FLOAT_PLATFORM_CRUMBLE &&
            fp->mode != FLOAT_PLATFORM_RAIL) {
            snprintf(field, sizeof(field), "float_platforms[%d].mode", i);
            fail_value(err, err_size, field, "is invalid");
            if (stop_here(sink, err)) return -1;
            continue;
        }
        if (fp->tile_count < 1 || fp->tile_count > MAX_SPIKE_TILES) {
            snprintf(field, sizeof(field), "float_platforms[%d].tile_count", i);
            fail_range(err, err_size, field, fp->tile_count, 1, MAX_SPIKE_TILES);
            if (stop_here(sink, err)) return -1;
            continue;
        }
        if (fp->mode == FLOAT_PLATFORM_RAIL) {
            snprintf(field, sizeof(field), "float_platforms[%d].rail_index", i);
            if (validate_rail_index(err, err_size, field,
                                    fp->rail_index, def->rail_count) != 0) {
                if (stop_here(sink, err)) return -1;
            } else if (!rail_offset_is_on_track(&def->rails[fp->rail_index],
                                                fp->t_offset)) {
                /* rails[rail_index] is only read once the index is good. */
                snprintf(field, sizeof(field), "float_platforms[%d].t_offset", i);
                fail_value(err, err_size, field, "must lie on the referenced rail");
                if (stop_here(sink, err)) return -1;
            }
            snprintf(field, sizeof(field), "float_platforms[%d].speed", i);
            if (validate_rail_speed(err, err_size, field, fp->speed) != 0 &&
                stop_here(sink, err))
                return -1;
        } else {
            /* STATIC and CRUMBLE platforms never move, so speed is unused
             * and only has to be a sane number. */
            snprintf(field, sizeof(field), "float_platforms[%d].speed", i);
            if (validate_motion(err, err_size, field, fp->speed) != 0 &&
                stop_here(sink, err))
                return -1;
            snprintf(field, sizeof(field), "float_platforms[%d]", i);
            if (validate_world_rect(err, err_size, field, fp->x, fp->y,
                                    (float)(fp->tile_count * FLOAT_PLATFORM_PIECE_W),
                                    (float)FLOAT_PLATFORM_H,
                                    world_w) != 0 &&
                stop_here(sink, err)) return -1;
        }
    }
    return 0;
}

/* validate_bridges — A sane brick count and the whole bridge inside the
 * world. */
static int validate_bridges(const LevelDef *def, char *err, size_t err_size,
                            float world_w, IssueSink *sink)
{
    char field[64];

    for (int i = 0; i < def->bridge_count; i++) {
        int n = def->bridges[i].brick_count;
        if (n < 1 || n > MAX_BRIDGE_BRICKS) {
            snprintf(field, sizeof(field), "bridges[%d].brick_count", i);
            fail_range(err, err_size, field, n, 1, MAX_BRIDGE_BRICKS);
            if (stop_here(sink, err)) return -1;
            continue;   /* the width below comes from brick_count */
        }
        snprintf(field, sizeof(field), "bridges[%d]", i);
        if (validate_world_rect(err, err_size, field, def->bridges[i].x,
                                def->bridges[i].y, (float)(n * 16), 16.0f,
                                world_w) != 0 &&
            stop_here(sink, err)) return -1;
    }
    return 0;
}

/*
 * validate_blades — Axe traps and circular saws: positions in the world
 * (a y of 0 means "use the default height"), a known axe mode, and a saw
 * direction and patrol range it can bounce inside.
 */
static int validate_blades(const LevelDef *def, char *err, size_t err_size,
                           float world_w, IssueSink *sink)
{
    char field[64];

    for (int i = 0; i < def->axe_trap_count; i++) {
        snprintf(field, sizeof(field), "axe_traps[%d].pillar_x", i);
        if (validate_world_x(err, err_size, field,
                             def->axe_traps[i].pillar_x, world_w) != 0 &&
            stop_here(sink, err)) return -1;
        snprintf(field, sizeof(field), "axe_traps[%d].y", i);
        if (def->axe_traps[i].y != 0.0f &&
            validate_world_y(err, err_size, field,
                             def->axe_traps[i].y) != 0 &&
            stop_here(sink, err)) return -1;
        if (def->axe_traps[i].mode != AXE_MODE_PENDULUM &&
            def->axe_traps[i].mode != AXE_MODE_SPIN) {
            snprintf(field, sizeof(field), "axe_traps[%d].mode", i);
            fail_value(err, err_size, field, "is invalid");
            if (stop_here(sink, err)) return -1;
        }
    }
    for (int i = 0; i < def->circular_saw_count; i++) {
        snprintf(field, sizeof(field), "circular_saws[%d].y", i);
        if (def->circular_saws[i].y != 0.0f &&
            validate_world_y(err, err_size, field,
                             def->circular_saws[i].y) != 0 &&
            stop_here(sink, err)) return -1;
        if (def->circular_saws[i].direction != -1 &&
            def->circular_saws[i].direction != 1) {
            snprintf(field, sizeof(field), "circular_saws[%d].direction", i);
            fail_value(err, err_size, field, "must be -1 or 1");
            if (stop_here(sink, err)) return -1;
        }
        snprintf(field, sizeof(field), "circular_saws[%d]", i);
        if (validate_patrol(err, err_size, field, def->circular_saws[i].x,
                            def->circular_saws[i].patrol_x0,
                            def->circular_saws[i].patrol_x1,
                            0.0f, world_w) != 0 &&
            stop_here(sink, err)) return -1;
    }
    return 0;
}

/* same_position — Whether two coordinates are exactly the same value,
 * compared bit for bit as ui.c and serializer_emit.c do: the flame rule is
 * "on this gap", not "close to it".  Adding 0.0f first turns -0.0 into +0.0,
 * so a flame written as -0.0 still matches the gap at 0. */
static int same_position(float a, float b)
{
    uint32_t bits_a, bits_b;
    a += 0.0f;
    b += 0.0f;
    memcpy(&bits_a, &a, sizeof(bits_a));
    memcpy(&bits_b, &b, sizeof(bits_b));
    return bits_a == bits_b;
}

/* validate_flame_gap — A flame's x is the gap_x it erupts from (see
 * load_blue_flames), so it must be one of the level's floor gaps.  Without
 * this, a flame placed on solid floor rises out of the grass. */
static int validate_flame_gap(const LevelDef *def, char *err, size_t err_size,
                              const char *field, float x, float world_w)
{
    if (validate_gap_x(err, err_size, field, x, world_w) != 0) return -1;
    for (int g = 0; g < def->floor_gap_count; g++) {
        if (same_position(x, (float)def->floor_gaps[g])) return 0;
    }
    if (err && err_size > 0) {
        snprintf(err, err_size, "%s is %.2f (must match a floor gap x)",
                 field, x);
    }
    return -1;
}

/* validate_flames — Blue and fire flames erupt from a floor gap, so each
 * must sit on one (validate_flame_gap). */
static int validate_flames(const LevelDef *def, char *err, size_t err_size,
                           float world_w, IssueSink *sink)
{
    char field[64];

    for (int i = 0; i < def->blue_flame_count; i++) {
        snprintf(field, sizeof(field), "blue_flames[%d].x", i);
        if (validate_flame_gap(def, err, err_size, field,
                               def->blue_flames[i].x, world_w) != 0 &&
            stop_here(sink, err)) return -1;
    }
    for (int i = 0; i < def->fire_flame_count; i++) {
        snprintf(field, sizeof(field), "fire_flames[%d].x", i);
        if (validate_flame_gap(def, err, err_size, field,
                               def->fire_flames[i].x, world_w) != 0 &&
            stop_here(sink, err)) return -1;
    }
    return 0;
}

/* validate_all_bouncepads — The three bouncepad arrays share one rule set
 * (validate_bouncepads above); check small, medium, then high. */
static int validate_all_bouncepads(const LevelDef *def, char *err,
                                   size_t err_size, float world_w, IssueSink *sink)
{
    if (validate_bouncepads(err, err_size, "bouncepads_small",
                            def->bouncepads_small, def->bouncepad_small_count,
                            world_w, sink) != 0) return -1;
    if (validate_bouncepads(err, err_size, "bouncepads_medium",
                            def->bouncepads_medium, def->bouncepad_medium_count,
                            world_w, sink) != 0) return -1;
    if (validate_bouncepads(err, err_size, "bouncepads_high",
                            def->bouncepads_high, def->bouncepad_high_count,
                            world_w, sink) != 0) return -1;
    return 0;
}

/* validate_climbables — Vines, ladders and ropes: every stacked tile must
 * end inside the world, and a vine must have a known colour. */
static int validate_climbables(const LevelDef *def, char *err,
                               size_t err_size, float world_w, IssueSink *sink)
{
    char field[64];

    for (int i = 0; i < def->vine_count; i++) {
        if (def->vines[i].vine_type != VINE_GREEN &&
            def->vines[i].vine_type != VINE_BROWN) {
            snprintf(field, sizeof(field), "vines[%d].vine_type", i);
            fail_value(err, err_size, field, "is invalid");
            if (stop_here(sink, err)) return -1;
        }
        snprintf(field, sizeof(field), "vines[%d]", i);
        if (validate_climbable_rect(err, err_size, field,
                                    def->vines[i].x, def->vines[i].y,
                                    def->vines[i].tile_count,
                                    VINE_W, VINE_H, VINE_STEP,
                                    world_w) != 0 &&
            stop_here(sink, err))
            return -1;
    }
    for (int i = 0; i < def->ladder_count; i++) {
        snprintf(field, sizeof(field), "ladders[%d]", i);
        if (validate_climbable_rect(err, err_size, field,
                                    def->ladders[i].x, def->ladders[i].y,
                                    def->ladders[i].tile_count,
                                    LADDER_W, LADDER_H, LADDER_STEP,
                                    world_w) != 0 &&
            stop_here(sink, err))
            return -1;
    }
    for (int i = 0; i < def->rope_count; i++) {
        snprintf(field, sizeof(field), "ropes[%d]", i);
        if (validate_climbable_rect(err, err_size, field,
                                    def->ropes[i].x, def->ropes[i].y,
                                    def->ropes[i].tile_count,
                                    ROPE_W, ROPE_H, ROPE_STEP,
                                    world_w) != 0 &&
            stop_here(sink, err))
            return -1;
    }
    return 0;
}

/*
 * validate_runtime — Run every rule group in order, sending each error to
 * sink (see IssueSink).  Returns -1 when any rule failed, 0 otherwise.
 */
static int validate_runtime(const LevelDef *def, char *err, size_t err_size,
                            IssueSink *sink)
{
    int screens;
    float world_w;

    /* Level-wide rules: counts first, so every loop below stays inside its
     * array.  A bad count stops everything, in both modes: no later check
     * could safely walk that array. */
    if (level_validate_counts(def, err, err_size) != 0) {
        (void)stop_here(sink, err);
        return -1;
    }
    /* Then version, size, audio and scoring limits. */
    if (validate_level_settings(def, err, err_size, sink) != 0) return -1;

    /* Most rules below need the world width; 0 screens means the default 4. */
    screens = (def->screen_count > 0) ? def->screen_count : 4;
    world_w = (float)screens * (float)GAME_W;

    /* Asset paths, physics overrides and per-entity motion values. */
    if (validate_level_paths(def, err, err_size, sink) != 0) return -1;
    if (validate_physics_finite(def, err, err_size, sink) != 0) return -1;
    if (validate_entity_motion(def, err, err_size, sink) != 0) return -1;
    if (validate_jumping_spider_gaps(def, err, err_size, sink) != 0) return -1;

    /* Player start and checkpoints must lie inside the world. */
    if (validate_player_start(def, err, err_size, world_w, sink) != 0) return -1;
    if (validate_checkpoints(def, err, err_size, world_w, sink) != 0) return -1;

    /* World geometry: gaps, rails, ground pillars. */
    if (validate_floor_gaps(def, err, err_size, world_w, sink) != 0) return -1;
    if (validate_rails(def, err, err_size, world_w, sink) != 0) return -1;
    if (validate_platforms(def, err, err_size, world_w, sink) != 0) return -1;

    /* Everything placed in the world.  A rail rider reads the rail it
     * names only after checking that the index is in range. */
    if (validate_collectibles(def, err, err_size, world_w, sink) != 0) return -1;
    if (validate_enemies(def, err, err_size, world_w, sink) != 0) return -1;
    if (validate_spike_strips(def, err, err_size, world_w, sink) != 0) return -1;
    if (validate_spike_blocks(def, err, err_size, sink) != 0) return -1;
    if (validate_float_platforms(def, err, err_size, world_w, sink) != 0) return -1;
    if (validate_bridges(def, err, err_size, world_w, sink) != 0) return -1;
    if (validate_blades(def, err, err_size, world_w, sink) != 0) return -1;
    if (validate_flames(def, err, err_size, world_w, sink) != 0) return -1;
    if (validate_all_bouncepads(def, err, err_size, world_w, sink) != 0) return -1;
    if (validate_climbables(def, err, err_size, world_w, sink) != 0) return -1;

    return sink->errors > 0 ? -1 : 0;
}

/*
 * level_validate_runtime — Check every rule the runtime relies on.
 *
 * Runs on every level before it is used (game load, editor save/playtest),
 * so code elsewhere may index arrays and trust positions without rechecking.
 * The first failure writes "<field> <problem>" into err and returns -1; a
 * valid level returns 0 with err emptied.
 */
int level_validate_runtime(const LevelDef *def, char *err, size_t err_size)
{
    IssueSink first_only = { NULL, NULL, 0 };

    if (validate_runtime(def, err, err_size, &first_only) != 0) return -1;
    if (err && err_size > 0) err[0] = '\0';
    return 0;
}

int level_validate_runtime_each(const LevelDef *def, LevelIssueFn report,
                                void *context)
{
    /* Each message is written here before report sees it; 256 bytes holds
     * the longest message the checks above write. */
    char message[256];
    IssueSink every = { report, context, 0 };

    (void)validate_runtime(def, message, sizeof(message), &every);
    return every.errors;
}

/* ------------------------------------------------------------------ */
/* Where an error is                                                   */
/* ------------------------------------------------------------------ */

/*
 * Every message the validator writes starts with the TOML path of what is
 * wrong ("coins[3].x is ...", "rails[1] has ...", "screen_count is ...").
 * That is a promise to callers, kept by always passing that path as the
 * `field` of the fail_* helpers above, and checked by the tests.  Reading
 * the path back gives a structured location without threading one more
 * parameter through every check.
 */
static int is_path_char(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
}

int level_issue_location_parse(const char *message, LevelIssueLocation *where)
{
    const char *p = message;
    size_t n = 0;

    if (!where) return 0;
    memset(where, 0, sizeof(*where));
    where->index = -1;
    if (!message) return 0;

    /* The array or key name: "coins", "screen_count", "physics". */
    while (is_path_char(*p) && n + 1 < sizeof(where->path)) where->path[n++] = *p++;
    where->path[n] = '\0';
    if (n == 0) return 0;

    /* An optional element index: "[3]". */
    if (*p == '[') {
        long index = 0;
        int digits = 0;
        p++;
        while (*p >= '0' && *p <= '9' && digits < 9) {
            index = index * 10 + (*p - '0');
            digits++;
            p++;
        }
        if (*p != ']' || digits == 0) goto not_a_path;
        where->index = (int)index;
        p++;
    }

    /* An optional field inside it: ".x", ".tile_height". */
    if (*p == '.') {
        n = 0;
        p++;
        while (is_path_char(*p) && n + 1 < sizeof(where->field)) where->field[n++] = *p++;
        where->field[n] = '\0';
        if (n == 0) goto not_a_path;
    }

    /* The path ends where the message's words begin. */
    if (*p != ' ' && *p != '\0') goto not_a_path;
    return 1;

not_a_path:
    memset(where, 0, sizeof(*where));
    where->index = -1;
    return 0;
}

int level_validate_runtime_at(const LevelDef *def, char *err, size_t err_size,
                              LevelIssueLocation *where)
{
    int result = level_validate_runtime(def, err, err_size);

    if (where) {
        memset(where, 0, sizeof(*where));
        where->index = -1;
        if (result != 0 && err && err_size > 0)
            (void)level_issue_location_parse(err, where);
    }
    return result;
}
