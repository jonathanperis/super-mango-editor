/*
 * The editor's side panels as pixels (see tests/render_snapshot.h): the
 * palette, the properties of a selected spike block and the entity list,
 * drawn alone with the pointer parked in the status bar, so no hover or
 * tooltip is part of the frame. A fixture level keeps the content fixed.
 */
static int side_panels_match_their_snapshot(void)
{
    int failed = 0;
    EditorState es;
    Image frame = {0};

    CHECK(open_editor(&es, "tests/fixtures/runtime/hazards.toml") == 0);
    CHECK(es.level.spike_block_count > 0);
    editor_select_only(&es, ENT_SPIKE_BLOCK, 0);
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);

    es.ui.mouse_x = NEUTRAL_X;
    es.ui.mouse_y = NEUTRAL_Y;
    BeginDrawing();
    ClearBackground(BLACK);
    editor_render_side_panels(&es);
    rlDrawRenderBatchActive();
    frame = snapshot_grab_screen();
    EndDrawing();
    CHECK(snapshot_check("editor_side_panels", frame) == 0);
done:
    UnloadImage(frame);
    close_editor(&es);
    return failed;
}
