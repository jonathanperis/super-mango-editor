/*
 * file_dialog.c — Native file dialog implementation using popen().
 *
 * Instead of linking a GUI toolkit or Objective-C framework, we shell out
 * to a platform-specific command that shows the native file picker.  The
 * command writes the selected path to stdout, which we read via popen().
 *
 * This approach has zero dependencies and works on macOS, Linux, and
 * Windows without any conditional compilation of GUI code.  The trade-off
 * is that popen() blocks the calling thread, but that's fine for a modal
 * file dialog — the user expects the editor to pause while they pick a file.
 */

#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdio.h>   /* popen, pclose, fgets, fprintf */
#include <string.h>  /* strlen, strchr */
#include <ctype.h>   /* tolower */
#if defined(__APPLE__) || defined(__unix__)
#include <sys/wait.h> /* WIFEXITED, WEXITSTATUS */
#endif
#include "file_dialog.h"

#define FILE_DIALOG_TEST_PATH_MAX 4096

static int file_dialog_finish(FILE *fp, char *buf, int buf_size);
static int file_dialog_or_prompt(int result, char *buf, int buf_size,
                                 const char *title, const char *request);

static int file_dialog_test_open_result = -1;
static int file_dialog_test_save_result = -1;
static char file_dialog_test_open_path[FILE_DIALOG_TEST_PATH_MAX];
static char file_dialog_test_save_path[FILE_DIALOG_TEST_PATH_MAX];

#ifdef MANGO_TESTING
void file_dialog_test_set_open_result(int result, const char *path)
{
    file_dialog_test_open_result = result;
    if (path) {
        strncpy(file_dialog_test_open_path, path,
                sizeof(file_dialog_test_open_path) - 1);
        file_dialog_test_open_path[sizeof(file_dialog_test_open_path) - 1] = '\0';
    } else {
        file_dialog_test_open_path[0] = '\0';
    }
}

void file_dialog_test_set_save_result(int result, const char *path)
{
    file_dialog_test_save_result = result;
    if (path) {
        strncpy(file_dialog_test_save_path, path,
                sizeof(file_dialog_test_save_path) - 1);
        file_dialog_test_save_path[sizeof(file_dialog_test_save_path) - 1] = '\0';
    } else {
        file_dialog_test_save_path[0] = '\0';
    }
}
#endif /* MANGO_TESTING */
/* ------------------------------------------------------------------ */

/*
 * file_dialog_open — Show a native file-open dialog and return the path.
 *
 * Platform detection uses predefined compiler macros:
 *   __APPLE__   → macOS     (osascript / AppleScript)
 *   _WIN32      → Windows   (PowerShell OpenFileDialog)
 *   otherwise   → Linux     (zenity)
 *
 * Each platform command is designed to:
 *   1. Show only .toml files by default (with a way to see all files).
 *   2. Print the selected absolute path to stdout.
 *   3. Exit with code 0 on selection, non-zero on cancel.
 *
 * Returns FILE_DIALOG_SELECTED, FILE_DIALOG_CANCELLED, FILE_DIALOG_ERROR,
 * or FILE_DIALOG_INVALID_PATH (see file_dialog_read_path).
 */
int file_dialog_open(char *buf, int buf_size) {
    if (!buf || buf_size < 2) return FILE_DIALOG_ERROR;
    if (file_dialog_test_open_result != -1) {  /* -1 = no injected result */
        int result = file_dialog_test_open_result;
        file_dialog_test_open_result = -1;
        if (result == FILE_DIALOG_SELECTED) {
            strncpy(buf, file_dialog_test_open_path, (size_t)buf_size - 1);
            buf[buf_size - 1] = '\0';
        }
        return result;
    }

#if defined(__APPLE__)
    /*
     * macOS: use osascript to run an AppleScript that invokes NSOpenPanel.
     *
     * "choose file" shows the native macOS open dialog.
     * "of type {\"toml\"}" filters to .toml files.
     * "POSIX path of" converts the result from an AppleScript alias
     * (e.g. "Macintosh HD:Users:...") to a POSIX path ("/Users/...").
     *
     * If the user cancels, osascript exits with code 1 and popen returns
     * NULL or fgets returns NULL — both handled below.
     */
    const char *cmd =
        "osascript -e '"
        "set f to choose file of type {\"toml\", \"public.toml\"} "
        "with prompt \"Open Level TOML\"' "
        "-e 'POSIX path of f' 2>/dev/null";

#elif defined(_WIN32)
    /*
     * Windows: use PowerShell to show System.Windows.Forms.OpenFileDialog.
     *
     * Add-Type loads the WinForms assembly.  The dialog is configured to
     * filter for .toml files.  ShowDialog() returns "OK" if a file was
     * selected; the FileName property holds the path.
     */
    const char *cmd =
        "powershell -NoProfile -Command \""
        "[Console]::OutputEncoding = [Text.Encoding]::UTF8;"
        "Add-Type -AssemblyName System.Windows.Forms;"
        "$d = New-Object System.Windows.Forms.OpenFileDialog;"
        "$d.Filter = 'TOML files (*.toml)|*.toml|All files (*.*)|*.*';"
        "$d.Title = 'Open Level TOML';"
        "if ($d.ShowDialog() -eq 'OK') { $d.FileName }\"";

#else
    /*
     * Linux / other POSIX: use zenity, a GTK dialog utility commonly
     * installed on GNOME desktops, or kdialog, its KDE counterpart.
     *
     * `command -v NAME` is the shell's portable "is NAME installed?": it
     * prints nothing useful here (hence > /dev/null) and succeeds only
     * when the program exists.  With neither installed the snippet exits
     * with 127, the code a shell itself uses for "command not found", and
     * file_dialog_result turns that into FILE_DIALOG_UNAVAILABLE.  (popen
     * does not return NULL for a missing program: it starts `sh`, which
     * always exists, and sh is what fails to find the picker.)
     *
     * --file-selection shows the open dialog; --file-filter limits it to
     * .toml files.  kdialog takes the start folder and a filter instead.
     */
    const char *cmd =
        "if command -v zenity >/dev/null 2>&1; then "
        "zenity --file-selection "
        "--title='Open Level TOML' "
        "--file-filter='TOML files | *.toml' "
        "--file-filter='All files | *' "
        "2>/dev/null; "
        "elif command -v kdialog >/dev/null 2>&1; then "
        "kdialog --title 'Open Level TOML' --getopenfilename . "
        "'TOML files (*.toml)' 2>/dev/null; "
        "else exit 127; fi";
#endif

    /*
     * popen — launch the command in a child process and read its stdout.
     *
     * "r" opens the pipe for reading.  If the command fails to start
     * (e.g. osascript not found), popen returns NULL.
     */
    /* `cmd` is a fixed string in this function, not caller input. */
    FILE *fp = popen(cmd, "r"); /* NOLINT(cert-env33-c) */
    int result;

    if (!fp) {
        fprintf(stderr, "Warning: could not open file dialog\n");
        result = FILE_DIALOG_ERROR;
    } else {
        result = file_dialog_finish(fp, buf, buf_size);
    }
    return file_dialog_or_prompt(result, buf, buf_size, "Open Level",
                                 "Type the path of the level to open:");
}

int file_dialog_read_path(FILE *fp, char *buf, int buf_size)
{
    size_t length;
    int ended;

    if (!fp || !buf || buf_size < 2) return FILE_DIALOG_ERROR;
    buf[0] = '\0';

    /*
     * fgets — read one line from the command's stdout.
     *
     * The file dialog commands print the selected path as a single line.
     * If the user cancelled, there is no output at all.
     */
    if (!fgets(buf, buf_size, fp)) return FILE_DIALOG_CANCELLED;

    /* Strip the line ending: osascript and zenity print "path\n",
     * Windows PowerShell prints "path\r\n". */
    length = strlen(buf);
    ended = length > 0 && buf[length - 1] == '\n';
    if (ended) buf[--length] = '\0';
#ifdef _WIN32
    /* Only Windows ends lines with "\r\n", and its file names cannot hold
     * '\r'.  On macOS and Linux a name may really end in '\r'; stripping it
     * would open a different file, so it is left for the check below. */
    if (ended && length > 0 && buf[length - 1] == '\r') buf[--length] = '\0';
#endif

    /*
     * Anything after the first line means the picked name itself contained
     * a line break (or did not fit in buf).  Read the rest so the picker can
     * exit, then refuse: using just the first line would silently pick a
     * different file.
     */
    if (fgetc(fp) != EOF) {
        while (fgetc(fp) != EOF) { }
        return ended ? FILE_DIALOG_INVALID_PATH : FILE_DIALOG_ERROR;
    }
    if (!ended && length == (size_t)buf_size - 1) return FILE_DIALOG_ERROR;
    if (strchr(buf, '\r')) return FILE_DIALOG_INVALID_PATH;

    /* Empty string means no selection */
    if (buf[0] == '\0') return FILE_DIALOG_CANCELLED;
    return FILE_DIALOG_SELECTED;
}

int file_dialog_result(int read_result, int exited, int exit_code)
{
    /* The shell could not find any picker program. */
    if (exited && exit_code == 127) return FILE_DIALOG_UNAVAILABLE;

    if (read_result == FILE_DIALOG_CANCELLED) {
        /* PowerShell exits successfully without output on cancel; the
         * others exit with 1. */
        if (exited && (exit_code == 0 || exit_code == 1)) return FILE_DIALOG_CANCELLED;
        return FILE_DIALOG_ERROR;
    }
    if (read_result == FILE_DIALOG_SELECTED && !(exited && exit_code == 0))
        return FILE_DIALOG_ERROR;
    return read_result;
}

/*
 * file_dialog_finish — Read the picker's answer, then close the pipe.
 *
 * pclose() waits for the picker and returns its exit status, which is
 * how a cancel is told apart from a failure when nothing was printed.
 * On POSIX that status packs the exit code together with "killed by a
 * signal" information; WIFEXITED and WEXITSTATUS unpack it.  Windows'
 * _pclose returns the exit code itself.
 */
static int file_dialog_finish(FILE *fp, char *buf, int buf_size)
{
    int result = file_dialog_read_path(fp, buf, buf_size);
    int status = pclose(fp);
    int exited, exit_code;

#if defined(__APPLE__) || defined(__unix__)
    exited = status >= 0 && WIFEXITED(status);
    exit_code = exited ? WEXITSTATUS(status) : -1;
#else
    exited = status >= 0;
    exit_code = status;
#endif
    return file_dialog_result(result, exited, exit_code);
}

/*
 * file_dialog_or_prompt — Keep a picker's answer, or ask in the window.
 *
 * With no picker program installed (FILE_DIALOG_UNAVAILABLE), or one that
 * failed (FILE_DIALOG_ERROR: it could not start, crashed, or printed
 * something that is not one path), the editor could neither open a level
 * nor give a new one a name.  So the path is asked for in the editor's own
 * window instead (dialog_path), with a first line saying why.  `request`
 * is the rest of the question ("Type the path ...").  When that prompt
 * cannot be shown either (no window), the picker's own result is kept, so
 * the caller can say what is missing or what failed.
 */
static int file_dialog_or_prompt(int result, char *buf, int buf_size,
                                 const char *title, const char *request)
{
    char message[320];
    int typed;

    if (result == FILE_DIALOG_UNAVAILABLE) {
        fprintf(stderr, "Warning: no file picker found (zenity or kdialog); "
                        "asking for the path in the editor window\n");
        snprintf(message, sizeof(message), "No file picker was found (install "
                 "zenity or kdialog to browse). %s", request);
    } else if (result == FILE_DIALOG_ERROR) {
        fprintf(stderr, "Warning: the file picker failed; "
                        "asking for the path in the editor window\n");
        snprintf(message, sizeof(message), "The file picker did not work. %s", request);
    } else {
        return result;
    }
    buf[0] = '\0';
    typed = dialog_path(title, message, buf, buf_size);
    return typed == FILE_DIALOG_ERROR ? result : typed;
}

static int file_dialog_add_toml_extension(char *buf, int buf_size)
{
    const char *slash;
    const char *backslash;
    const char *dot;
    size_t len;

    if (!buf || buf_size < 2 || buf[0] == '\0') return 0;
    slash = strrchr(buf, '/');
    backslash = strrchr(buf, '\\');
    dot = strrchr(buf, '.');
    if (backslash && (!slash || backslash > slash)) slash = backslash;

    if (dot && (!slash || dot > slash) && strlen(dot) == 5 &&
        tolower((unsigned char)dot[1]) == 't' &&
        tolower((unsigned char)dot[2]) == 'o' &&
        tolower((unsigned char)dot[3]) == 'm' &&
        tolower((unsigned char)dot[4]) == 'l') {
        return 1;
    }

    len = strlen(buf);
    if (len + 5 >= (size_t)buf_size) return 0;
    memcpy(buf + len, ".toml", 6);
    return 1;
}

int file_dialog_save(char *buf, int buf_size)
{
    if (!buf || buf_size < 2) return FILE_DIALOG_ERROR;
    if (file_dialog_test_save_result != -1) {  /* -1 = no injected result */
        int result = file_dialog_test_save_result;
        file_dialog_test_save_result = -1;
        if (result == FILE_DIALOG_SELECTED) {
            strncpy(buf, file_dialog_test_save_path, (size_t)buf_size - 1);
            buf[buf_size - 1] = '\0';
        }
        return result;
    }

#if defined(__APPLE__)
    const char *cmd =
        "osascript -e '"
        "set f to choose file name with prompt \"Save Level TOML\" "
        "default name \"untitled.toml\"' "
        "-e 'POSIX path of f' 2>/dev/null";
#elif defined(_WIN32)
    const char *cmd =
        "powershell -NoProfile -Command \""
        "[Console]::OutputEncoding = [Text.Encoding]::UTF8;"
        "Add-Type -AssemblyName System.Windows.Forms;"
        "$d = New-Object System.Windows.Forms.SaveFileDialog;"
        "$d.Filter = 'TOML files (*.toml)|*.toml|All files (*.*)|*.*';"
        "$d.DefaultExt = 'toml';"
        "$d.AddExtension = $true;"
        "$d.OverwritePrompt = $false;"
        "$d.Title = 'Save Level TOML';"
        "if ($d.ShowDialog() -eq 'OK') { $d.FileName }\"";
#else
    /* zenity, else kdialog, else 127 = "no picker" (see file_dialog_open). */
    const char *cmd =
        "if command -v zenity >/dev/null 2>&1; then "
        "zenity --file-selection --save "
        "--title='Save Level TOML' "
        "--file-filter='TOML files | *.toml' "
        "--file-filter='All files | *' 2>/dev/null; "
        "elif command -v kdialog >/dev/null 2>&1; then "
        "kdialog --title 'Save Level TOML' --getsavefilename untitled.toml "
        "'TOML files (*.toml)' 2>/dev/null; "
        "else exit 127; fi";
#endif

    /* `cmd` is a fixed string in this function, not caller input. */
    FILE *fp = popen(cmd, "r"); /* NOLINT(cert-env33-c) */
    int result;

    if (!fp) {
        fprintf(stderr, "Warning: could not open save file dialog\n");
        result = FILE_DIALOG_ERROR;
    } else {
        result = file_dialog_finish(fp, buf, buf_size);
    }
    result = file_dialog_or_prompt(result, buf, buf_size, "Save Level As",
                                   "Type the path to save the level to:");
    if (result != FILE_DIALOG_SELECTED) return result;
    if (!file_dialog_add_toml_extension(buf, buf_size)) return FILE_DIALOG_ERROR;
    return FILE_DIALOG_SELECTED;
}
