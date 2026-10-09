/*
 * game_fwd.h — The name GameState, without its contents.
 *
 * A forward declaration tells the compiler "struct GameState exists" without
 * saying what is inside. That is enough to declare and pass a GameState *
 * pointer, which is all most module headers do:
 *
 *     void game_camera_snap(GameState *gs);
 *
 * Such a header includes this file instead of game.h. game.h pulls in every
 * entity header (GameState embeds all their arrays), so a header that
 * included game.h made each file including it recompile whenever GameState
 * or any entity struct changed. A .c file that reads gs->world... still
 * includes game.h itself, where the full definition is visible.
 *
 * C11 allows the same typedef to appear more than once, so game.h repeats
 * this line when it defines the struct.
 */
#pragma once

typedef struct GameState GameState;
