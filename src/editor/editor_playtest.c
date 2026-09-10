/*
 * editor_playtest.c — Editor playtest process helpers.
 */

#include "editor_playtest.h"

#include <SDL.h>       /* SDL_SetWindowTitle */
#include <stdio.h>     /* fprintf, snprintf, stderr */
#include <string.h>    /* memset */

#ifndef _WIN32
#include <errno.h>     /* errno, ECHILD */
#include <signal.h>    /* kill, SIGTERM */
#include <sys/wait.h>  /* waitpid, WNOHANG */
#include <unistd.h>    /* fork, execl, _exit */
#else
#include <errno.h>     /* errno */
#include <stdlib.h>   /* malloc, free */
#include <stdint.h>    /* intptr_t */
#include <windows.h>   /* CreateProcessW, HANDLE */
#endif

#include "editor_files.h"    /* private playtest snapshot lifecycle */
#include "editor_session.h" /* editor status/title/persist helpers */
#include "serializer_io.h"

int editor_playtest_binary_path(char *path, size_t size)
{
    char *base = SDL_GetBasePath();
    if (!base || !path || size == 0) { SDL_free(base); return -1; }
#ifdef _WIN32
    const char *suffix = ".exe";
#else
    const char *suffix = "";
#endif
    int length = snprintf(path, size, "%ssuper-mango%s", base, suffix);
    SDL_free(base);
    if (length < 0 || (size_t)length >= size) { path[0] = '\0'; return -1; }
    return 0;
}

void editor_play_test(EditorState *es)
{
    if (!es || !editor_finish_field_edit(es)) return;
    if (es->playing) return;   /* already running */
    if (!editor_can_persist(es, "Playtest")) return;

    char save_path[EDITOR_PATH_MAX];
    char binary_path[EDITOR_PATH_MAX];
    if (editor_playtest_binary_path(binary_path, sizeof(binary_path)) != 0) {
        editor_set_status(es, "Play failed: cannot locate sibling game executable");
        return;
    }

    if (editor_prepare_playtest_level(es, save_path, sizeof(save_path)) != 0) {
        fprintf(stderr, "Play: failed to prepare private level\n");
        return;
    }
    editor_set_status(es, "Play saved %s", save_path);

    fprintf(stderr, "Play: launching game...\n");

#ifndef _WIN32
    pid_t pid = fork();
    if (pid == 0) {
        if (es->debug_play)
            execl(binary_path, "super-mango",
                  "--level", save_path, "--debug", (char *)NULL);
        else
            execl(binary_path, "super-mango",
                  "--level", save_path, (char *)NULL);
        _exit(1);
    } else if (pid > 0) {
        es->play_pid = (int)pid;
        es->playing = 1;
        editor_set_status(es, "Play launched %s", save_path);
        SDL_SetWindowTitle(es->window, "Super Mango Editor - Playing...");
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
        written = wide_level && wide_binary
                  ? _snwprintf(command, sizeof(command) / sizeof(command[0]),
                              L"\"%ls\" --level \"%ls\"%ls",
                              wide_binary, wide_level,
                              es->debug_play ? L" --debug" : L"")
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
    editor_set_status(es, "Play launched %s", save_path);
    SDL_SetWindowTitle(es->window, "Super Mango Editor - Playing...");
#endif
}

void editor_stop_play(EditorState *es)
{
    int stopped = 1;

    if (!es->playing) return;

#ifndef _WIN32
    if (es->play_pid > 0) {
        pid_t result;

        if (kill((pid_t)es->play_pid, SIGTERM) < 0 && errno != ESRCH) {
            stopped = 0;
        }
        result = waitpid((pid_t)es->play_pid, NULL, WNOHANG);
        if (result == 0) {
            stopped = 0;
        } else if (result > 0 || (result < 0 && errno == ECHILD)) {
            es->play_pid = 0;
        } else {
            stopped = 0;
        }
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

    if (!stopped) {
        editor_set_status(es, "Stopping play...");
        return;
    }

    es->playing = 0;
    editor_retire_playtest_level(es);
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
