/*
 * game_collision.h — Collision detection system public interface.
 *
 * This module handles all player-entity collision detection including
 * enemies, hazards, and collectibles. Damage application is separated
 * into collision_damage.c for cleaner dependency management.
 */
#pragma once

#include "../game.h"
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
 * directly. Refactored to use helper macros for readability.
 */
void game_collide(GameState *gs, float dt);

/* ------------------------------------------------------------------ */
/* Hitbox builders                                                    */
/* ------------------------------------------------------------------ */

/*
 * Spider hitbox builders — inline helpers since they need FLOOR_Y calculations.
 * These construct SDL_Rect hitboxes from entity state for collision testing.
 */
SDL_Rect spider_build_hitbox(const Spider *s);
SDL_Rect jumping_spider_build_hitbox(const JumpingSpider *js);
