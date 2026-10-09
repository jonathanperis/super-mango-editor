/*
 * game_resume.h — Turn a running level into a Continue point and back.
 *
 * The profile stores at most one Continue point (GameResume, defined in
 * game_profile.h with the rest of the file format). This module is the
 * bridge to gameplay: game_resume_capture reads one out of a GameState,
 * game_resume_apply checks it against the loaded level and puts the player
 * back there. AppSession decides when to do either.
 */
#pragma once

#include "../game.h"
#include "game_profile.h"

/* Fill resume from the running level: its profile key and content hash,
 * respawn point, score, lives, collected coins and level timer. */
void game_resume_capture(const GameState *gs, GameResume *resume);

/*
 * Restart the freshly loaded level gs at resume, as if the player had just
 * lost a life there: same respawn point, score, lives and collected coins,
 * with full hearts. Returns -1 and changes nothing when resume belongs to
 * another level, to another version of this level's file (level_hash), or
 * names a checkpoint, coin or position this level does not have.
 */
int game_resume_apply(GameState *gs, const GameResume *resume);
