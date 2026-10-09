/*
 * editor_playtest.c — Editor playtest process helpers.
 */

#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

#include "editor_playtest.h"

#include <stdio.h>     /* fprintf, snprintf, stderr */
#include <string.h>    /* memset */

#ifndef _WIN32
#include <errno.h>     /* errno, ECHILD */
#include <signal.h>    /* kill, SIGTERM, SIGKILL */
#include <sys/wait.h>  /* waitpid, WNOHANG */
#include <time.h>      /* nanosleep */
#include <unistd.h>    /* fork, execl, _exit */
#else
#include <errno.h>     /* errno */
#include <stdlib.h>   /* malloc, free */
#include <stdint.h>    /* intptr_t */
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#include <windows.h>   /* CreateProcessW, HANDLE */
#endif

#include "canvas.h"          /* canvas_contains, canvas_screen_to_world */
#include "editor_files.h"    /* private playtest snapshot lifecycle */
#include "editor_session.h" /* editor status/title/persist helpers */
#include "entity_meta.h"     /* editor_selection_count */
#include "../shared/serializer_io.h"

#if defined(MANGO_TESTING) && !defined(_WIN32)
static int (*editor_test_launch)(const char *const *argv);

void editor_test_set_play_launcher(int (*launch)(const char *const *argv))
{
    editor_test_launch = launch;
}
#endif

int editor_playtest_binary_path(char *path, size_t size)
{
    char *base = application_path();
    if (!base || !path || size == 0) { free(base); return -1; }
#ifdef _WIN32
    const char *suffix = ".exe";
#else
    const char *suffix = "";
#endif
    int length = snprintf(path, size, "%ssuper-mango%s", base, suffix);
    free(base);
    if (length < 0 || (size_t)length >= size) { path[0] = '\0'; return -1; }
    return 0;
}

int editor_playtest_arguments(const char *binary, const char *level, int debug,
                              const LevelStart *start, char *number,
                              const char **argv, int max)
{
    int n = 0;

    /* At most: binary, --level, path, --no-save, --debug, flag, value, NULL. */
    if (!binary || !level || !number || !argv || max < 8) return -1;
    argv[n++] = binary;
    argv[n++] = "--level";
    argv[n++] = level;
    argv[n++] = "--no-save";
    if (debug) argv[n++] = "--debug";
    if (start && start->kind == LEVEL_START_AT_X) {
        /* The flag takes whole pixels; x is already inside the level. */
        snprintf(number, 16, "%d", (int)(start->x + 0.5f));
        argv[n++] = "--start-x";
        argv[n++] = number;
    } else if (start && start->kind == LEVEL_START_AT_CHECKPOINT) {
        snprintf(number, 16, "%d", start->checkpoint);
        argv[n++] = "--start-checkpoint";
        argv[n++] = number;
    }
    argv[n] = NULL;
    return n;
}

int editor_playtest_start_here(EditorState *es, LevelStart *start)
{
    LevelStartPoint point;
    char why[128];

    if (!es || !start) return -1;
    memset(start, 0, sizeof(*start));
    if (editor_selection_count(es) == 1 && es->selection.type == ENT_CHECKPOINT) {
        start->kind = LEVEL_START_AT_CHECKPOINT;
        start->checkpoint = es->selection.index;
    } else if (canvas_contains(es->mouse_x, es->mouse_y)) {
        float world_x, world_y;
        canvas_screen_to_world(es, es->mouse_x, es->mouse_y, &world_x, &world_y);
        start->kind = LEVEL_START_AT_X;
        /* Whole pixels, as the game's flag takes them. */
        start->x = (float)(int)(world_x + 0.5f);
    } else {
        editor_set_status(es, "Playtest from here: point at the canvas or select a checkpoint");
        return -1;
    }
    if (level_start_resolve(&es->level, start, &point, why, sizeof(why)) != 0) {
        editor_set_status(es, "Playtest from here: %s", why);
        return -1;
    }
    return 0;
}

void editor_play_test_here(EditorState *es)
{
    LevelStart start;

    if (!es || !editor_finish_field_edit(es)) return;
    if (es->playing) return;
    if (editor_playtest_start_here(es, &start) != 0) return;
    editor_play_test_from(es, &start);
}

void editor_play_test(EditorState *es)
{
    editor_play_test_from(es, NULL);
}

void editor_play_test_from(EditorState *es, const LevelStart *start)
{
    char start_number[16];
    const char *args[8];

    if (!es || !editor_finish_field_edit(es)) return;
    if (es->playing) return;   /* already running */
    if (!editor_can_persist(es, "Playtest")) return;

    char save_path[EDITOR_PATH_MAX];
    char binary_path[EDITOR_PATH_MAX];
    if (editor_playtest_binary_path(binary_path, sizeof(binary_path)) != 0) {
        editor_set_status(es, "Play failed: cannot locate sibling game executable");
        return;
    }
    if (!serializer_file_exists_utf8(binary_path)
#if defined(MANGO_TESTING) && !defined(_WIN32)
        && !editor_test_launch   /* a test launcher needs no real game */
#endif
       ) {
        editor_set_status(es, "Play failed: build both executables with make builder");
        return;
    }

    if (editor_prepare_playtest_level(es, save_path, sizeof(save_path)) != 0) {
        fprintf(stderr, "Play: failed to prepare private level\n");
        return;
    }
    editor_set_status(es, "Play saved %s", save_path);
    if (editor_playtest_arguments(binary_path, save_path, es->debug_play, start,
                                  start_number, args, 8) < 0) {
        editor_retire_playtest_level(es);
        editor_set_status(es, "Play failed: command line");
        return;
    }

    fprintf(stderr, "Play: launching game...\n");

#ifndef _WIN32
#ifdef MANGO_TESTING
    pid_t pid = editor_test_launch ? (pid_t)editor_test_launch(args) : fork();
#else
    pid_t pid = fork();
#endif
    if (pid == 0) {
        /* execv wants a char *const[]; the strings are not changed. */
        execv(binary_path, (char *const *)args);
        _exit(1);
    } else if (pid > 0) {
        es->play_pid = (int)pid;
        es->playing = 1;
        if (start && start->kind == LEVEL_START_AT_X)
            editor_set_status(es, "Play launched from x %s", start_number);
        else if (start && start->kind == LEVEL_START_AT_CHECKPOINT)
            editor_set_status(es, "Play launched from checkpoint %s", start_number);
        else
            editor_set_status(es, "Play launched %s", save_path);
        SetWindowTitle("Super Mango Editor - Playing...");
    } else {
        fprintf(stderr, "Play: fork() failed\n");
        editor_retire_playtest_level(es);
        editor_set_status(es, "Play failed: fork");
    }
#else
    {
        wchar_t *wide_level = serializer_utf8_to_wide(save_path);
        wchar_t *wide_binary = serializer_utf8_to_wide(binary_path);
        wchar_t command[EDITOR_PATH_MAX * 2 + 128];
        STARTUPINFOW startup;
        PROCESS_INFORMATION process;
        int written;

        memset(&startup, 0, sizeof(startup));
        memset(&process, 0, sizeof(process));
        startup.cb = sizeof(startup);
        /* The same start flag editor_playtest_arguments put in args. */
        wchar_t start_flag[48] = L"";
        if (start && start->kind == LEVEL_START_AT_X)
            _snwprintf(start_flag, sizeof(start_flag) / sizeof(start_flag[0]),
                       L" --start-x %d", (int)(start->x + 0.5f));
        else if (start && start->kind == LEVEL_START_AT_CHECKPOINT)
            _snwprintf(start_flag, sizeof(start_flag) / sizeof(start_flag[0]),
                       L" --start-checkpoint %d", start->checkpoint);
        written = wide_level && wide_binary
                  ? _snwprintf(command, sizeof(command) / sizeof(command[0]),
                              L"\"%ls\" --no-save --level \"%ls\"%ls%ls",
                              wide_binary, wide_level,
                              es->debug_play ? L" --debug" : L"", start_flag)
                 : -1;
        if (written < 0 || (size_t)written >= sizeof(command) / sizeof(command[0]) ||
            !CreateProcessW(wide_binary, command, NULL, NULL, FALSE, 0, NULL, NULL,
                            &startup, &process)) {
            fprintf(stderr, "Play: launch failed for %s (errno=%d)\n",
                    save_path, errno);
            free(wide_level);
            free(wide_binary);
            editor_retire_playtest_level(es);
            editor_set_status(es, "Play failed: launch %s", save_path);
            return;
        }
        free(wide_level);
        free(wide_binary);
        CloseHandle(process.hThread);
        es->play_process = (intptr_t)process.hProcess;
    }
    es->playing = 1;
    if (start && start->kind == LEVEL_START_AT_X)
        editor_set_status(es, "Play launched from x %s", start_number);
    else if (start && start->kind == LEVEL_START_AT_CHECKPOINT)
        editor_set_status(es, "Play launched from checkpoint %s", start_number);
    else
        editor_set_status(es, "Play launched %s", save_path);
    SetWindowTitle("Super Mango Editor - Playing...");
#endif
}

#ifndef _WIN32
/*
 * PLAYTEST_STOP_WAIT_MS — how long Stop/quit waits for the game to exit
 * after SIGTERM before forcing it with SIGKILL.  The game normally exits
 * within a frame or two; the bound keeps a hung child from freezing the
 * editor.  Polled in PLAYTEST_STOP_POLL_MS steps.
 */
#define PLAYTEST_STOP_WAIT_MS 1000
#define PLAYTEST_STOP_POLL_MS 10

/*
 * stop_child — Ask the game to exit, wait a bounded time, then force it.
 *
 * waitpid() must eventually be called for every child: until then the
 * kernel keeps a "zombie" entry for it.  WNOHANG makes each check return
 * at once; between checks we sleep briefly with nanosleep().  SIGKILL
 * cannot be caught or ignored, so the final blocking waitpid() returns
 * promptly.  Returns 1 once the child is gone (or was already reaped).
 */
static int stop_child(pid_t pid)
{
    struct timespec pause = { 0, PLAYTEST_STOP_POLL_MS * 1000000L };
    pid_t result;

    if (kill(pid, SIGTERM) < 0 && errno != ESRCH) return 0;
    for (int waited = 0; waited < PLAYTEST_STOP_WAIT_MS;
         waited += PLAYTEST_STOP_POLL_MS) {
        result = waitpid(pid, NULL, WNOHANG);
        if (result > 0 || (result < 0 && errno == ECHILD)) return 1;
        if (result < 0 && errno != EINTR) return 0;
        nanosleep(&pause, NULL);
    }

    fprintf(stderr, "Play: game did not exit after SIGTERM; sending SIGKILL\n");
    if (kill(pid, SIGKILL) < 0 && errno != ESRCH) return 0;
    do {
        result = waitpid(pid, NULL, 0);
    } while (result < 0 && errno == EINTR);
    return result > 0 || (result < 0 && errno == ECHILD);
}
#endif

void editor_stop_play(EditorState *es)
{
    int stopped = 1;

    if (!es->playing) return;

#ifndef _WIN32
    if (es->play_pid > 0) {
        if (stop_child((pid_t)es->play_pid)) es->play_pid = 0;
        else stopped = 0;
    }
#else
    if (es->play_process) {
        HANDLE process = (HANDLE)es->play_process;
        DWORD wait_result = WaitForSingleObject(process, 0);

        if (wait_result == WAIT_TIMEOUT) {
            if (!TerminateProcess(process, 1)) {
                stopped = 0;
            } else {
                wait_result = WaitForSingleObject(process, 2000);
                if (wait_result != WAIT_OBJECT_0) stopped = 0;
            }
        } else if (wait_result != WAIT_OBJECT_0) {
            stopped = 0;
        }
        if (stopped) {
            CloseHandle(process);
            es->play_process = 0;
        }
    }
#endif

    /* The private level snapshot is removed even if the child could not be
     * confirmed gone: on quit there is no later frame to clean it up.  (A
     * POSIX process that still has it open keeps reading its own copy.) */
    editor_retire_playtest_level(es);

    if (!stopped) {
        editor_set_status(es, "Stopping play...");
        return;
    }

    es->playing = 0;
    editor_set_status(es, "Play stopped");
    editor_update_window_title(es);
}

void editor_check_play_status(EditorState *es)
{
#ifndef _WIN32
    if (es->play_pid > 0) {
        int status;
        pid_t result = waitpid((pid_t)es->play_pid, &status, WNOHANG);
        if (result > 0) {
            es->play_pid = 0;
            es->playing = 0;
            editor_retire_playtest_level(es);
            if (WIFEXITED(status)) {
                editor_set_status(es, "Play exited: code %d", WEXITSTATUS(status));
            } else if (WIFSIGNALED(status)) {
                editor_set_status(es, "Play exited: signal %d", WTERMSIG(status));
            } else {
                editor_set_status(es, "Play ended");
            }
            editor_update_window_title(es);
        } else if (result < 0) {
            if (errno == ECHILD) {
                es->play_pid = 0;
                es->playing = 0;
                editor_retire_playtest_level(es);
                editor_set_status(es, "Play ended: process already reaped");
                editor_update_window_title(es);
            } else {
                editor_set_status(es, "Play status check failed");
            }
        }
    }
#else
    if (es->play_process) {
        HANDLE process = (HANDLE)es->play_process;
        DWORD wait_result = WaitForSingleObject(process, 0);

        if (wait_result == WAIT_OBJECT_0) {
            DWORD exit_code = 0;
            (void)GetExitCodeProcess(process, &exit_code);
            CloseHandle(process);
            es->play_process = 0;
            es->playing = 0;
            editor_retire_playtest_level(es);
            editor_set_status(es, "Play exited: code %lu",
                              (unsigned long)exit_code);
            editor_update_window_title(es);
        } else if (wait_result == WAIT_FAILED) {
            editor_set_status(es, "Play status check failed");
        }
    }
#endif
}
