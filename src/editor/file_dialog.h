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

/*
 * file_dialog_open — Show a native file open dialog filtered to .toml files.
 *
 * buf      : buffer to receive the selected file path (null-terminated).
 * buf_size : size of the buffer in bytes.
 *
 * Returns 1 if the user selected a file (path written to buf),
 *         0 if the user cancelled or an error occurred (buf untouched).
 *
 * The dialog blocks until the user selects a file or cancels.  Because
 * it uses popen() to run an external process, the SDL event loop is
 * paused during this time — this is acceptable for a file dialog.
 */
int file_dialog_open(char *buf, int buf_size);
