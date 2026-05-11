/*
 * editor_playtest.c — Editor playtest process helpers.
 */

#include "editor_playtest.h"

#include <SDL.h>       /* SDL_SetWindowTitle */
#include <stdio.h>     /* fprintf, snprintf, stderr */

#ifndef _WIN32
#include <errno.h>     /* errno, ECHILD */
#include <signal.h>    /* kill, SIGTERM */
#include <sys/wait.h>  /* waitpid, WNOHANG */
#include <unistd.h>    /* fork, execl, _exit */
#else
#include <errno.h>     /* errno */
#include <process.h>   /* _spawnv, _P_NOWAIT */
#include <stdint.h>    /* intptr_t */
#endif

#include "editor_session.h" /* editor status/title/persist helpers */
#include "serializer.h"     /* level_save_toml */

void editor_play_test(EditorState *es)
{
    if (es->playing) return;   /* already running */
    if (!editor_can_persist(es, "Playtest")) return;

    const char *save_path = es->file_path[0] != '\0'
                          ? es->file_path
                          : "levels/_playtest.toml";

    if (level_save_toml(&es->level, save_path) != 0) {
        fprintf(stderr, "Play: failed to save %s\n", save_path);
        editor_set_status(es, "Play failed: save %s", save_path);
        return;
    }
    es->modified = 0;
    editor_set_status(es, "Play saved %s", save_path);

    fprintf(stderr, "Play: launching game...\n");

#ifndef _WIN32
    pid_t pid = fork();
    if (pid == 0) {
        if (es->debug_play)
            execl("./out/super-mango", "super-mango",
                  "--level", save_path, "--debug", (char *)NULL);
        else
            execl("./out/super-mango", "super-mango",
                  "--level", save_path, (char *)NULL);
        _exit(1);
    } else if (pid > 0) {
        es->play_pid = (int)pid;
        es->playing = 1;
        editor_set_status(es, "Play launched %s", save_path);
        SDL_SetWindowTitle(es->window, "Super Mango Editor - Playing...");
    } else {
        fprintf(stderr, "Play: fork() failed\n");
        editor_set_status(es, "Play failed: fork");
    }
#else
    {
        const char *argv_debug[] = {
            ".\\out\\super-mango.exe", "--level", save_path, "--debug", NULL
        };
        const char *argv_normal[] = {
            ".\\out\\super-mango.exe", "--level", save_path, NULL
        };
        const char *const *argv = es->debug_play ? argv_debug : argv_normal;
        intptr_t child = _spawnv(_P_NOWAIT, argv[0], argv);
        if (child == -1) {
            fprintf(stderr, "Play: launch failed for %s (errno=%d)\n",
                    save_path, errno);
            editor_set_status(es, "Play failed: launch %s", save_path);
            return;
        }
        es->play_pid = (int)child;
    }
    es->playing = 1;
    editor_set_status(es, "Play launched %s", save_path);
    SDL_SetWindowTitle(es->window, "Super Mango Editor - Playing...");
#endif
}

void editor_stop_play(EditorState *es)
{
    if (!es->playing) return;

#ifndef _WIN32
    if (es->play_pid > 0) {
        pid_t result;

        kill((pid_t)es->play_pid, SIGTERM);
        result = waitpid((pid_t)es->play_pid, NULL, WNOHANG);
        if (result > 0 || (result < 0 && errno == ECHILD)) {
            es->play_pid = 0;
        }
    }
#endif

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
                editor_set_status(es, "Play ended: process already reaped");
                editor_update_window_title(es);
            } else {
                editor_set_status(es, "Play status check failed");
            }
        }
    }
#else
    (void)es; /* Windows: no PID tracking in this simple implementation */
#endif
}
