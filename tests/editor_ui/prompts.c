/* Included by tests/editor_ui_test.c. Not compiled alone: the functions are
 * static and use the helpers in that file.
 */
/* ------------------------------------------------------------------ */
/* In-window prompts                                                    */
/* ------------------------------------------------------------------ */

/* Queue a left-button press at one point and its release at another. */
static void push_press_release(int x0, int y0, int x1, int y1)
{
    push_event(INPUT_MOUSE_DOWN, MOUSE_BUTTON_LEFT, 0, x0, y0);
    push_event(INPUT_MOUSE_UP, MOUSE_BUTTON_LEFT, 0, x1, y1);
}

/*
 * The "Skip this undo step?" prompt, with no canned answer in the way and
 * its input queued as a designer would give it: Enter presses the
 * highlighted button, which starts on "Keep the step"; Esc and closing
 * the window cancel; Tab moves the highlight.  A button is chosen by a
 * press and release on it.  Only Skip drops the step, and a prompt nobody
 * can answer keeps it.
 */
static int campaign_skip_prompt_keeps_the_step_by_default(void)
{
    int failed = 0;
    EditorState es;
    char cwd[4096];
    int moved = 0;
    DialogLayout layout;
    CHECK(open_editor(&es, NULL) == 0);
    CHECK(enter_ui_campaign(cwd, sizeof(cwd)) == 0);
    moved = 1;
    CHECK(editor_campaign_open(&es, NULL) == 0);
    CHECK(editor_campaign_move(&es, 2, -1) == 0);               /* a c b */
    CHECK(editor_campaign_remove(&es, 0) == 0);                 /* c b */
    CHECK(remove("levels/a.toml") == 0);
    clear_dialog_seams();
    CHECK(editor_campaign_undo(&es) == -1);          /* refused; nothing asked */
    CHECK(editor_campaign_undo_blocked(&es) == 1);

    /* Nothing queued: the prompt is drawn, nobody answers, the step stays. */
    int frames = dialog_choice_test_frames();
    CHECK(editor_campaign_undo(&es) == -1);
    CHECK(strcmp(es.status_message, "Undo step kept: confirmation failed") == 0);
    CHECK(dialog_choice_test_frames() > frames);
    CHECK(strcmp(campaign_rows(&es), "cb") == 0 && editor_campaign_undo_steps(&es) == 2);
    layout = *dialog_choice_test_layout();
    CHECK(layout.button_count == 3);
    const int skip_x = layout.button_x[EDITOR_SKIP_STEP_SKIP] + layout.button_w[EDITOR_SKIP_STEP_SKIP] / 2;
    const int button_y = layout.button_y + layout.button_h / 2;

    /* Enter: the highlighted button, "Keep the step" to begin with. */
    push_key(KEY_ENTER, 0);
    CHECK(editor_campaign_undo(&es) == -1);
    CHECK(strstr(es.status_message, "Undo step kept: levels/a.toml") != NULL);
    CHECK(strcmp(campaign_rows(&es), "cb") == 0 && editor_campaign_undo_steps(&es) == 2);
    CHECK(editor_campaign_undo_blocked(&es) == 1);

    /* Esc, and closing the window, cancel. */
    input_clear();
    push_key(KEY_ESCAPE, 0);
    CHECK(editor_campaign_undo(&es) == -1);
    CHECK(strstr(es.status_message, "Undo step kept: levels/a.toml") != NULL);
    input_clear();
    push_event(INPUT_QUIT, 0, 0, 0, 0);
    CHECK(editor_campaign_undo(&es) == -1);
    CHECK(strcmp(campaign_rows(&es), "cb") == 0 && editor_campaign_undo_steps(&es) == 2);

    /* A press on Skip released elsewhere is no click; Tab twice from
     * Keep wraps round to Cancel, which Enter then presses. */
    input_clear();
    push_press_release(skip_x, button_y, skip_x, button_y - 60);
    push_key(KEY_TAB, 0);
    push_key(KEY_TAB, 0);
    push_key(KEY_ENTER, 0);
    CHECK(editor_campaign_undo(&es) == -1);
    CHECK(editor_campaign_undo_steps(&es) == 2);

    /* The Skip button, and nothing else, drops the step. */
    input_clear();
    push_press_release(skip_x, button_y, skip_x + 2, button_y + 1);
    CHECK(editor_campaign_undo(&es) == 2);
    CHECK(strcmp(es.status_message,
                 "Skipped: removed levels/a.toml (it stays out of the campaign)") == 0);
    CHECK(strcmp(campaign_rows(&es), "cb") == 0 && editor_campaign_undo_steps(&es) == 1);
    CHECK(editor_campaign_undo(&es) == 1);           /* c's move, one row higher */
    CHECK(strcmp(campaign_rows(&es), "bc") == 0);
done:
    input_clear();
    if (moved && test_change_folder(cwd) != 0) failed = 1;
    clear_dialog_seams();
    close_editor(&es);
    return failed;
}

/*
 * A prompt that opens in the middle of drawing (a button acts when it is
 * drawn) leaves the frame's target and clip and puts both back, so the
 * frame goes on as before.  A mouse release it read for a press made
 * before it opened goes back to the editor.  The default and cancel
 * buttons may be the same one.
 */
static int prompt_returns_to_the_interrupted_frame(void)
{
    int failed = 0;
    EditorState es;
    InputEvent event;
    const char *labels[] = {"No", "Yes"};
    int choice = -1, released = 0;
    CHECK(open_editor(&es, NULL) == 0);

    BeginDrawing();
    BeginTextureMode(tiny_target);
    es.drawing_target = &tiny_target;
    ui_clip_begin(&es.ui, 10, 20, 30, 40);
    push_event(INPUT_MOUSE_UP, MOUSE_BUTTON_LEFT, 0, 5, 5);  /* pressed before */
    push_key(KEY_TAB, 0);                                    /* Yes -> No */
    push_key(KEY_ENTER, 0);
    static const char message[] =
        "A message long enough to wrap onto a second line inside the box, with a "
        "path wider than the box itself: /home/someone/projects/super-mango/levels/"
        "campaigns/a-very-long-folder-name/another-folder/main-campaign-level.toml";
    CHECK(rlGetActiveFramebuffer() == tiny_target.id);
    int result = dialog_choice("Question", message, labels, 2, 1, 0, &choice);
    /* The frame's own target takes the draws again, not the screen the
     * prompt presented to. */
    CHECK(rlGetActiveFramebuffer() == tiny_target.id);
    CHECK(es.ui.clip_active && es.ui.clip_x == 10 && es.ui.clip_y == 20 &&
          es.ui.clip_w == 30 && es.ui.clip_h == 40);
    DrawRectangle(0, 0, 4, 4, ui_color(&es.ui, UI_COLOR_ACCENT));  /* still drawing */
    ui_clip_end(&es.ui);
    EndTextureMode();
    es.drawing_target = NULL;
    EndDrawing();
    CHECK(result == 0 && choice == 0);
    /* Wrapped at spaces, and the path, wider than the box, cut inside. */
    const DialogLayout *layout = dialog_choice_test_layout();
    CHECK(layout->line_count >= 3);
    for (int i = 0; i < layout->line_count; i++) {
        char line[256];
        CHECK(layout->line_length[i] > 0 && layout->line_length[i] < (int)sizeof(line));
        memcpy(line, message + layout->line_start[i], (size_t)layout->line_length[i]);
        line[layout->line_length[i]] = '\0';
        CHECK(ui_text_width(&es.ui, line) <= layout->box_w - 2 * (layout->text_x - layout->box_x));
    }
    CHECK(layout->line_start[layout->line_count - 1] + layout->line_length[layout->line_count - 1] ==
          (int)strlen(message));
    while (input_poll(&event))
        released |= event.type == INPUT_MOUSE_UP && event.button == MOUSE_BUTTON_LEFT;
    CHECK(released);

    /* Default and cancel on one button: Enter and Esc both choose it. */
    push_key(KEY_ESCAPE, 0);
    CHECK(dialog_choice("Question", "Message", labels, 2, 0, 0, &choice) == 0 && choice == 0);
    input_clear();

    /* A toolbar button's prompt opens mid-frame: New with unsaved changes.
     * Nothing answers it here, so nothing is lost and the frame finishes. */
    es.level.screen_count = 6;
    editor_refresh_dirty(&es);
    int frames = dialog_choice_test_frames();
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    editor_test_set_discard_choice(-1);
    push_click(EDITOR_W - 4 - 52 - 4 * 68 + 20, 16);         /* New */
    {
        /* ui_frame without arm_dialog_seams, which would answer for us. */
        InputEvent queued;
        ui_begin_frame(&es.ui);
        while (input_poll(&queued)) editor_handle_event(&es, &queued);
        es.ui.mouse_x = EDITOR_W - 4 - 52 - 4 * 68 + 20;
        es.ui.mouse_y = 16;
        BeginDrawing();
        BeginTextureMode(tiny_target);
        es.drawing_target = &tiny_target;
        editor_render_toolbar(&es);
        EndTextureMode();
        es.drawing_target = NULL;
        EndDrawing();
    }
    CHECK(dialog_choice_test_frames() > frames);
    CHECK(es.level.screen_count == 6 && es.modified);
    CHECK(strstr(es.status_message, "confirmation failed") != NULL);
done:
    input_clear();
    clear_dialog_seams();
    close_editor(&es);
    return failed;
}
