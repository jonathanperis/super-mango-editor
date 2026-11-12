/*
 * game_collision.h — Collision detection system public interface.
 *
 * This module handles all player-entity collision detection including
 * enemies, hazards, and collectibles. Damage application is separated
 * into collision_damage.c for cleaner dependency management.
 */
#pragma once

#include "../game_fwd.h"  /* GameState, used by pointer only */
#include "../entities/spider.h"
#include "../entities/jumping_spider.h"

/* ------------------------------------------------------------------ */
/* Collision detection                                                */
/* ------------------------------------------------------------------ */

/*
 * game_collide — Test all player–entity collisions for this frame.
 *
 * Counts down the invincibility timer then checks the player's physics
 * hitbox (AABB) against every active enemy, hazard, and collectible.
 * Damage is applied through apply_damage(); coins/stars are collected
 * directly. Enemies and hazards come from the s_damage_sources table in
 * game_collision.c, one row per kind, tested in that order.
 */
void game_collide(GameState *gs, float dt);

/* ------------------------------------------------------------------ */
/* Damage sources                                                     */
/* ------------------------------------------------------------------ */

/*
 * The kinds of enemy and hazard that hurt on touch are the rows of one
 * table in game_collision.c. These four calls let other code walk it:
 * the debug overlay draws every box collision would test, by asking for
 * each one here instead of working the rectangles out a second time.
 *
 *   game_damage_source_count()            how many kinds there are
 *   game_damage_source_name(source)       the kind's debug-log name
 *   game_damage_source_items(world, s)    how many items of it the level has
 *   game_damage_source_hitbox(world, s, i, &box)
 *       1 with *box set when item i can hurt right now; 0 when it cannot
 *       (inactive, a flame still below its gap) or the indexes are out of
 *       range. A spike row's box is the whole row.
 */
int game_damage_source_count(void);
const char *game_damage_source_name(int source);
int game_damage_source_items(const GameWorld *world, int source);
int game_damage_source_hitbox(const GameWorld *world, int source, int item, IntRect *box);

/* ------------------------------------------------------------------ */
/* Hitbox builders                                                    */
/* ------------------------------------------------------------------ */

/*
 * Spider hitbox builders — inline helpers since they need FLOOR_Y calculations.
 * These construct integer hitboxes from entity state for collision testing.
 */
IntRect spider_build_hitbox(const Spider *s);
IntRect jumping_spider_build_hitbox(const JumpingSpider *js);
