/*
 * file_dialog.h — Native file dialog for opening TOML level files.
 *
 * Provides a cross-platform function that opens the OS file picker and
 * returns the selected file path.  Uses platform-specific commands:
 *   macOS  → osascript (AppleScript NSOpenPanel)
 *   Linux  → zenity --file-selection
 *   Windows → PowerShell System.Windows.Forms.OpenFileDialog
 *
 * These are launched via popen() — no library dependencies required.
 */
#pragma once

#include <stdio.h> /* FILE */

#define FILE_DIALOG_INVALID_PATH (-2) /* picked name contains a line break */
#define FILE_DIALOG_ERROR     (-1)
#define FILE_DIALOG_CANCELLED  0
#define FILE_DIALOG_SELECTED   1

/*
 * file_dialog_open — Show a native file open dialog filtered to .toml files.
 *
 * buf      : buffer to receive the selected file path (null-terminated).
 * buf_size : size of the buffer in bytes.
 *
 * Returns FILE_DIALOG_SELECTED if the user selected a file,
 *         FILE_DIALOG_CANCELLED if the user cancelled,
 *         FILE_DIALOG_ERROR if the dialog failed (buf untouched).
 *
 * The dialog blocks until the user selects a file or cancels.  Because
 * it uses popen() to run an external process, the editor event loop is
 * paused during this time — this is acceptable for a file dialog.
 */
int file_dialog_open(char *buf, int buf_size);

/* Show a native save picker filtered to .toml files, without overwrite UI. */
int file_dialog_save(char *buf, int buf_size);

/* Native two/three-button decision. Cancel/window-close selects cancel_index;
 * errors return -1 without granting a destructive action. */
int dialog_choice(const char *title, const char *message, const char *const *labels,
                  int count, int default_index, int cancel_index, int *selected);

/*
 * Quote text as one literal single-quoted argument.  Only one of these is
 * used on a given platform, but both are compiled everywhere so tests on
 * any OS can check them.
 *   dialog_quote_posix      : sh/osascript/zenity command lines
 *   dialog_quote_powershell : PowerShell scripts; also doubles the Unicode
 *                             quotation marks PowerShell treats as '
 * Returns a malloc'd string the caller frees, or NULL when out of memory.
 */
char *dialog_quote_posix(const char *text);
char *dialog_quote_powershell(const char *text);

/*
 * dialog_zenity_selection — Turn a finished zenity --question into a choice.
 *
 * exit_code 0 is the OK (default) button.  Exit code 1 is shared by Cancel
 * and --extra-button; only the extra button prints its label, so output
 * decides which.  Compiled everywhere so any OS can test it.  Returns the
 * selected index, or -1 for any other exit code (dialog failure).
 */
int dialog_zenity_selection(int exit_code, const char *output,
                            const char *const *labels, int count,
                            int default_index, int cancel_index, int extra_index);

/*
 * file_dialog_read_path — Read the one line a native picker prints.
 *
 * Stores the path without its line ending in buf.  Returns
 * FILE_DIALOG_SELECTED, FILE_DIALOG_CANCELLED (no output),
 * FILE_DIALOG_ERROR (longer than buf), or FILE_DIALOG_INVALID_PATH when the
 * file name itself contains a line break: the picker prints such a name
 * across several lines, and keeping only the first would silently open or
 * overwrite a different file.  Reads fp to its end; does not close it.
 */
int file_dialog_read_path(FILE *fp, char *buf, int buf_size);

/* Deterministic picker seam used by editor workflow tests. */
void file_dialog_test_set_open_result(int result, const char *path);
void file_dialog_test_set_save_result(int result, const char *path);
