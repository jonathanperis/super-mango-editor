/*
 * player_motion.h — Player horizontal acceleration and friction helpers.
 *
 * Input code writes Player.move_dir / is_running.  The update path calls this
 * module to turn that intent into smooth velocity using the tunable physics
 * fields stored on Player.
 */

#pragma once

#include "player.h"  /* Player */

/*
 * player_apply_horizontal_motion — Apply acceleration/friction to Player.vx.
 *
 * was_on_ground is the contact state from the previous frame.  Ground motion
 * uses live run input and stronger friction; air motion uses the run state
 * captured when the player left the ground.
 */
void player_apply_horizontal_motion(Player *player, float dt, int was_on_ground);
