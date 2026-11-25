/*
 * render_snapshot_test.c — The game's pixels, frame by frame.
 *
 * render/ is almost entirely executed by the other mechanics cases, but
 * executing a draw call says nothing about what it drew: a sprite drawn at
 * the wrong place, in the wrong colour or behind the wrong layer passes
 * them all. These cases draw a fixed scene and compare the frame with a
 * recorded snapshot (see render_snapshot.h): the HUD on its own, and one
 * whole level frame after a scripted run.
 *
 * Linked into gameplay-mechanics-test, which owns the window.
 */
#include <string.h>

#include "game.h"
#include "core/game_update.h"
#include "player/player.h"       /* PLAYER_INPUT_* */
#include "render/game_render.h"
#include "screens/hud.h"
#include "gameplay_mechanics_test.h"
#include "render_snapshot.h"
#include "check.h"

#include <rlgl.h>  /* rlDrawRenderBatchActive (after raylib.h) */

#define SNAPSHOT_LEVEL "tests/fixtures/runtime/hazards.toml"

/*
 * The HUD with every part showing: two of three hearts, a few lives, a
 * five-digit score and a checkpoint label still on screen. Drawn alone on
 * a plain background, so a change here is the HUD's and nothing else's.
 */
int hud_frame_matches_its_snapshot(void)
{
    int failed = 0;
    GameState gs;
    Image frame = {0};

    CHECK(mechanics_open_level(&gs, SNAPSHOT_LEVEL, 0) == 0);
    BeginDrawing();
    ClearBackground((Color){40, 60, 90, 255});
    hud_render(&gs.screen.hud, 2, 4, 12345, 1, 1, 5000u, 1000u);
    rlDrawRenderBatchActive();
    frame = snapshot_grab_screen();
    EndDrawing();
    CHECK(snapshot_check("hud", frame) == 0);
done:
    UnloadImage(frame);
    game_cleanup(&gs);
    return failed;
}

/*
 * A whole level frame: the hazards fixture after the player has run right
 * for a second and jumped, so the parallax, tiles, hazards mid-animation,
 * the player sprite and the HUD are all in it. game_render_frame draws it
 * into the logical target; it is then presented again here, the way
 * display_present does, inside a frame this test can read back.
 */
int level_frame_matches_its_snapshot(void)
{
    int failed = 0;
    GameState gs;
    Image frame = {0};

    CHECK(mechanics_open_level(&gs, SNAPSHOT_LEVEL, 0) == 0);
    mechanics_step(&gs, PLAYER_INPUT_RIGHT, 60);
    mechanics_step(&gs, PLAYER_INPUT_RIGHT | PLAYER_INPUT_JUMP, 12);
    CHECK(game_render_frame(&gs, (int)gs.world.camera.x, 0.0f) == 1);

    RenderTexture2D target = gs.screen.frame_target;
    BeginDrawing();
    ClearBackground(BLACK);
    DrawTexturePro(target.texture,
                   (Rectangle){0, 0, (float)target.texture.width, -(float)target.texture.height},
                   (Rectangle){0, 0, (float)target.texture.width, (float)target.texture.height},
                   (Vector2){0}, 0, WHITE);
    rlDrawRenderBatchActive();
    frame = snapshot_grab_screen();
    EndDrawing();
    CHECK(snapshot_check("level_hazards", frame) == 0);
done:
    UnloadImage(frame);
    game_cleanup(&gs);
    return failed;
}
