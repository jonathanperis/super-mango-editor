/*
 * serializer_load_config.c — TOML level-wide configuration parsing.
 */

#include <stddef.h> /* size_t */
#include <string.h> /* strncpy */

#include "serializer_load_config.h"
#include "serializer_parse.h"

static void copy_config_string(char *dst, size_t dst_size, const char *src) {
    if (!dst || dst_size == 0) return;

    strncpy(dst, src ? src : "", dst_size - 1);
    dst[dst_size - 1] = '\0';
}

void serializer_load_config(toml_datum_t top, LevelDef *def) {
    if (!def) return;

    /* Player spawn */
    def->player_start_x = get_float(top, "player_start_x", 0);
    def->player_start_y = get_float(top, "player_start_y", 0);

    /* Music */
    {
        const char *mp = get_str(top, "music_path", "");
        copy_config_string(def->music_path, sizeof(def->music_path), mp);
    }
    def->music_volume = get_int(top, "music_volume", 0);

    /* Floor tile */
    {
        const char *ftp = get_str(top, "floor_tile_path", "");
        copy_config_string(def->floor_tile_path, sizeof(def->floor_tile_path), ftp);
    }

    /* Game rules */
    def->initial_hearts = get_int(top, "initial_hearts", 0);
    def->initial_lives  = get_int(top, "initial_lives", 0);
    def->score_per_life = get_int(top, "score_per_life", 0);
    def->coin_score     = get_int(top, "coin_score", 0);

    /* Player movement physics — [physics] sub-table; -1.0 = use engine default */
    {
        toml_datum_t ph = toml_get(top, "physics");
        if (ph.type == TOML_TABLE) {
            def->physics.walk_max_speed          = get_float(ph, "walk_max_speed",          -1.0f);
            def->physics.run_max_speed           = get_float(ph, "run_max_speed",           -1.0f);
            def->physics.walk_ground_accel       = get_float(ph, "walk_ground_accel",       -1.0f);
            def->physics.run_ground_accel        = get_float(ph, "run_ground_accel",        -1.0f);
            def->physics.ground_friction         = get_float(ph, "ground_friction",         -1.0f);
            def->physics.ground_counter_accel    = get_float(ph, "ground_counter_accel",    -1.0f);
            def->physics.air_accel_walk          = get_float(ph, "air_accel_walk",          -1.0f);
            def->physics.air_accel_run           = get_float(ph, "air_accel_run",           -1.0f);
            def->physics.air_friction            = get_float(ph, "air_friction",            -1.0f);
            def->physics.cam_lookahead_vx_factor = get_float(ph, "cam_lookahead_vx_factor", -1.0f);
            def->physics.cam_lookahead_max       = get_float(ph, "cam_lookahead_max",       -1.0f);
        }
    }
}
