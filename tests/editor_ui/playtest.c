/* Included by tests/editor_ui_test.c. Not compiled alone: the functions are
 * static and use the helpers in that file.
 */
/* ------------------------------------------------------------------ */
/* Playtest process and native pickers (POSIX)                         */
/* ------------------------------------------------------------------ */

#ifndef _WIN32
/*
 * wait_until_exited — Block until the child has exited, but leave it
 * unreaped (WNOWAIT) so the editor's own status check still collects it and
 * reports the exit code or signal. No time limit: a slow host (sanitizers,
 * a busy CI runner) only makes the test slower, never wrong.
 */
static int wait_until_exited(pid_t child)
{
    siginfo_t info;
    int result;
    do {
        result = waitid(P_PID, (id_t)child, &info, WEXITED | WNOWAIT);
    } while (result != 0 && errno == EINTR);
    return result;
}

static int playtest_status_follows_the_game_process(void)
{
    int failed = 0;
    EditorState es;
    CHECK(open_editor(&es, NULL) == 0);

    /* While a playtest runs, edits are refused with a hint. The child
     * waits on a pipe so it is certainly still running during that frame. */
    int gate[2];
    CHECK(pipe(gate) == 0);
    pid_t child = fork();
    CHECK(child >= 0);
    if (child == 0) {
        char byte;
        (void)close(gate[1]);
        /* read() returns 0 once the parent closes its end. glibc marks
         * read() warn_unused_result, and a (void) cast does not silence
         * that under release flags, so the result is consumed by the if. */
        if (read(gate[0], &byte, 1) < 0) _exit(4);
        _exit(3);
    }
    (void)close(gate[0]);
    es.playing = 1;
    es.play_pid = (int)child;
    key_frame(&es, KEY_TWO, 0);
    /* The game exits with code 3: the status line says so. */
    (void)close(gate[1]);
    CHECK(wait_until_exited(child) == 0);
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(!es.playing && es.play_pid == 0);
    CHECK(strstr(es.status_message, "code 3") != NULL);
    CHECK(es.tool == TOOL_SELECT);

    /* A game killed by a signal reports the signal. */
    child = fork();
    CHECK(child >= 0);
    if (child == 0) { raise(SIGKILL); _exit(0); }
    es.playing = 1;
    es.play_pid = (int)child;
    CHECK(wait_until_exited(child) == 0);
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(!es.playing && strstr(es.status_message, "signal") != NULL);

    /* A child that something else already reaped ends the playtest too. */
    child = fork();
    CHECK(child >= 0);
    if (child == 0) _exit(0);
    CHECK(waitpid(child, NULL, 0) == child);
    es.playing = 1;
    es.play_pid = (int)child;
    ui_frame(&es, NEUTRAL_X, NEUTRAL_Y);
    CHECK(!es.playing && strstr(es.status_message, "already reaped") != NULL);

    /* The overlay's Stop button ends a running game. */
    child = fork();
    CHECK(child >= 0);
    if (child == 0) { for (;;) pause(); }
    es.playing = 1;
    es.play_pid = (int)child;
    for (int y = EDITOR_H / 2; y < EDITOR_H / 2 + 80 && es.playing; y += 4)
        click_frame(&es, EDITOR_W / 2, y);
    CHECK(!es.playing && es.play_pid == 0);
    CHECK(strstr(es.status_message, "stopped") != NULL);
    /* The child is gone: there is nothing left to reap. */
    CHECK(waitpid(child, NULL, WNOHANG) == -1 && errno == ECHILD);

    /* Starting a playtest of an invalid level is refused up front. */
    es.level.screen_count = -1;
    key_frame(&es, KEY_F5, 0);
    CHECK(!es.playing && strstr(es.status_message, "Playtest blocked") != NULL);
done:
    clear_dialog_seams();
    close_editor(&es);
    return failed;
}

/* The command line the last test launch would have run, one string per
 * argument, and how many launches there were. */
static char launched_args[8][EDITOR_PATH_MAX];
static int launched_argc;
static int launch_count;

/* The write end of the pipe the current stand-in game is waiting on, or -1
 * when none is running. Closing it is what lets that child exit. */
static int launch_gate = -1;

/* Stand-in for running the game: record the arguments and start a child
 * that waits on a pipe until the test lets it go, so the editor follows and
 * reaps a real process. A child that exited at once could be gone before
 * the frame that launched it had finished, and "is the playtest running"
 * would then depend on how busy the machine was. */
static int record_launch(const char *const *argv)
{
    int gate[2];
    pid_t child;
    launched_argc = 0;
    for (int i = 0; argv[i] && i < 8; i++) {
        snprintf(launched_args[i], sizeof(launched_args[i]), "%s", argv[i]);
        launched_argc++;
    }
    launch_count++;
    if (pipe(gate) != 0) return -1;
    child = fork();
    if (child == 0) {
        char byte;
        /* The child holds a copy of the write end too; read() only sees
         * end-of-file once every copy is closed. */
        (void)close(gate[1]);
        if (read(gate[0], &byte, 1) < 0) _exit(4);
        _exit(0);
    }
    (void)close(gate[0]);
    if (child < 0) {
        (void)close(gate[1]);
        return -1;
    }
    launch_gate = gate[1];
    return (int)child;
}

/* Let the stand-in game exit, if one is still waiting. */
static void release_launched_game(void)
{
    if (launch_gate >= 0) (void)close(launch_gate);
    launch_gate = -1;
}

/* Is `flag` followed by `value` in the recorded command line? */
static int launched_with(const char *flag, const char *value)
{
    for (int i = 0; i + 1 < launched_argc; i++)
        if (strcmp(launched_args[i], flag) == 0 && strcmp(launched_args[i + 1], value) == 0)
            return 1;
    return 0;
}

/* Let the recorded child exit and the editor notice. */
static void finish_launched_game(EditorState *es)
{
    release_launched_game();
    if (es->play_pid > 0) (void)wait_until_exited((pid_t)es->play_pid);
    ui_frame(es, NEUTRAL_X, NEUTRAL_Y);
}

/*
 * Shift+F5 is "Playtest from here": the game gets --start-x for the spot
 * under the mouse, or --start-checkpoint for a selected checkpoint.  A
 * spot the game would refuse (over a floor gap) is refused before
 * anything is launched.  F5 alone still starts at the level's start.
 */
static int playtest_from_here_passes_the_start_point(void)
{
    int failed = 0;
    EditorState es;
    const char *prefs = TEST_OUT "ui-playtest-prefs";
    const int canvas_y = TOOLBAR_H + 100;
    CHECK(open_editor(&es, NULL) == 0);
    CHECK(mkdir(prefs, 0700) == 0 || errno == EEXIST);
    CHECK(editor_set_preference_root(&es, prefs) == 0);
    CHECK(editor_init_persistence_paths(&es) == 0);
    editor_test_set_play_launcher(record_launch);
    es.level.floor_gap_count = 1;
    es.level.floor_gaps[0] = 400;
    es.level.checkpoint_count = 1;
    es.level.checkpoints[0] = (CheckpointPlacement){800.0f, 252.0f};
    CHECK(level_is_valid(&es));
    es.camera.x = 0.0f;
    es.camera.y = 0.0f;
    es.camera.zoom = 1.0f;

    /* The mouse over world x 200, on the grass. */
    ui_frame(&es, 200, canvas_y);
    push_key(KEY_F5, INPUT_SHIFT);
    ui_frame(&es, 200, canvas_y);
    CHECK(launch_count == 1 && es.playing);
    CHECK(launched_with("--start-x", "200"));
    CHECK(launched_with("--level", es.playtest_path));
    CHECK(strstr(es.status_message, "from x 200") != NULL);
    finish_launched_game(&es);
    CHECK(!es.playing);

    /* Over the gap there is nothing to stand on: nothing is launched. */
    ui_frame(&es, 416, canvas_y);
    push_key(KEY_F5, INPUT_SHIFT);
    ui_frame(&es, 416, canvas_y);
    CHECK(launch_count == 1 && !es.playing);
    CHECK(strstr(es.status_message, "nothing to stand on") != NULL);

    /* A selected checkpoint wins over the mouse. */
    editor_select_only(&es, ENT_CHECKPOINT, 0);
    ui_frame(&es, 200, canvas_y);
    push_key(KEY_F5, INPUT_SHIFT);
    ui_frame(&es, 200, canvas_y);
    CHECK(launch_count == 2 && launched_with("--start-checkpoint", "0"));
    finish_launched_game(&es);

    /* Nothing selected and the mouse off the canvas: say what to do. */
    editor_select_none(&es);
    key_frame(&es, KEY_F5, INPUT_SHIFT);
    CHECK(launch_count == 2 && strstr(es.status_message, "point at the canvas") != NULL);

    /* Plain F5 starts where the level starts. */
    key_frame(&es, KEY_F5, 0);
    CHECK(launch_count == 3 && es.playing);
    for (int i = 0; i < launched_argc; i++)
        CHECK(strncmp(launched_args[i], "--start", 7) != 0);
    finish_launched_game(&es);
done:
    /* A check that failed part-way may have left a child waiting. */
    release_launched_game();
    editor_test_set_play_launcher(NULL);
    clear_dialog_seams();
    close_editor(&es);
    return failed;
}

#define FAKE_BIN TEST_OUT "fake-picker-bin"

/* Write a stand-in for osascript/zenity: it prints MANGO_FAKE_PICK (when
 * set) and exits with MANGO_FAKE_STATUS, like a real picker would. */
static int install_fake_pickers(void)
{
    static const char script[] =
        "#!/bin/sh\n"
        "if [ -n \"$MANGO_FAKE_PICK\" ]; then printf '%s\\n' \"$MANGO_FAKE_PICK\"; fi\n"
        "exit \"${MANGO_FAKE_STATUS:-1}\"\n";
    static const char *const names[] = {"osascript", "zenity"};
    char path[512], cwd[2048], new_path[8192];
    if (mkdir(FAKE_BIN, 0755) != 0 && errno != EEXIST) return -1;
    for (size_t i = 0; i < 2; i++) {
        snprintf(path, sizeof(path), FAKE_BIN "/%s", names[i]);
        FILE *file = fopen(path, "w");
        if (!file) return -1;
        fputs(script, file);
        if (fclose(file) != 0 || chmod(path, 0755) != 0) return -1;
    }
    const char *old_path = getenv("PATH");
    if (!old_path) old_path = "";
    /* PATH needs a full path. An absolute OUTDIR (make test OUTDIR=/tmp/x)
     * already is one; a relative OUTDIR gets the working directory in front. */
    if (FAKE_BIN[0] == '/') {
        snprintf(new_path, sizeof(new_path), FAKE_BIN ":%s", old_path);
    } else {
        if (!getcwd(cwd, sizeof(cwd))) return -1;
        snprintf(new_path, sizeof(new_path), "%s/" FAKE_BIN ":%s", cwd, old_path);
    }
    return setenv("PATH", new_path, 1);
}

static int pick(int save, const char *output, const char *status, char *buf, int size)
{
    if (output) setenv("MANGO_FAKE_PICK", output, 1);
    else unsetenv("MANGO_FAKE_PICK");
    setenv("MANGO_FAKE_STATUS", status, 1);
    int result = save ? file_dialog_save(buf, size) : file_dialog_open(buf, size);
    unsetenv("MANGO_FAKE_PICK");
    setenv("MANGO_FAKE_STATUS", "1", 1);
    return result;
}

static int native_pickers_report_choice_cancel_and_failure(void)
{
    int failed = 0;
    char buf[256];
    clear_dialog_seams();

    /* A picked file comes back without its line ending. */
    CHECK(pick(0, "/levels/one.toml", "0", buf, sizeof(buf)) == FILE_DIALOG_SELECTED);
    CHECK(strcmp(buf, "/levels/one.toml") == 0);
    /* Cancel: no output and exit status 1 (osascript/zenity convention). */
    CHECK(pick(0, NULL, "1", buf, sizeof(buf)) == FILE_DIALOG_CANCELLED);
    /* No output and a successful exit is also a cancel (PowerShell's). */
    CHECK(pick(0, NULL, "0", buf, sizeof(buf)) == FILE_DIALOG_CANCELLED);
    /* Any other exit status is a failure, even with a path printed. */
    CHECK(pick(0, NULL, "2", buf, sizeof(buf)) == FILE_DIALOG_ERROR);
    CHECK(pick(0, "/levels/one.toml", "4", buf, sizeof(buf)) == FILE_DIALOG_ERROR);

    /* Save adds ".toml" when the name has no extension, keeps any case of
     * an existing one, and refuses a name with no room for it. */
    CHECK(pick(1, "/levels/new", "0", buf, sizeof(buf)) == FILE_DIALOG_SELECTED);
    CHECK(strcmp(buf, "/levels/new.toml") == 0);
    CHECK(pick(1, "/levels/Loud.TOML", "0", buf, sizeof(buf)) == FILE_DIALOG_SELECTED);
    CHECK(strcmp(buf, "/levels/Loud.TOML") == 0);
    CHECK(pick(1, "/levels.d/plain", "0", buf, sizeof(buf)) == FILE_DIALOG_SELECTED);
    CHECK(strcmp(buf, "/levels.d/plain.toml") == 0);
    CHECK(pick(1, "/levels/abcdefgh", "0", buf, 20) == FILE_DIALOG_ERROR);
    CHECK(pick(1, NULL, "1", buf, sizeof(buf)) == FILE_DIALOG_CANCELLED);
    CHECK(pick(1, "/levels/x", "3", buf, sizeof(buf)) == FILE_DIALOG_ERROR);

    /* The editor's Open command with a cancelled picker keeps the document. */
    {
        EditorState es;
        CHECK(open_editor(&es, "levels/labs/04_climbing.toml") == 0);
        uint64_t hash = doc_hash(&es);
        editor_open_level_file(&es);
        CHECK(doc_hash(&es) == hash && es.modified == 0);
        close_editor(&es);
    }
done:
    return failed;
}

#endif
