/*
 * platform_paths_test.c — The small OS helpers in shared/platform.c and
 * shared/asset_root.c, at their edges: buffers that are too small, text cut
 * in the middle of a character, paths in Windows spelling, a missing HOME,
 * a folder that cannot be created, and the game finding its files from the
 * folder its executable is in.
 *
 * Linked into session-test (see tests/session_test.c's case table), which
 * already links both files. The cases that change the working folder or the
 * environment put them back before returning, whatever happened, so the
 * order of the session cases does not matter (`--shuffle` checks that).
 */
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L  /* setenv, unsetenv, getcwd, chdir */
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>  /* _getcwd, _chdir, _mkdir, _rmdir */
#else
#include <sys/stat.h>  /* mkdir */
#include <unistd.h>    /* getcwd, chdir, rmdir */
#endif

#include "shared/asset_root.h"
#include "shared/platform.h"
#include "test_paths.h"
#include "check.h"

int platform_text_copies_never_overrun(void);
int platform_writable_paths_refuse_control_bytes(void);
int platform_preference_folders_are_created_or_refused(void);
int platform_executable_folder_reads_any_separator(void);
int asset_root_rejects_paths_it_cannot_hold(void);
int asset_root_is_found_from_the_test_program(void);

/* str_copy reports the length it needed; utf8_copy never splits a
 * character, so a name cut to fit a label stays valid UTF-8. */
int platform_text_copies_never_overrun(void)
{
    int failed = 0;
    char small[4] = "zzz";
    /* "h", e-acute (2 bytes), "llo": 6 bytes. */
    const char *hello = "h\xc3\xa9llo";

    CHECK(str_copy(small, "abcdef", 0) == 6 && strcmp(small, "zzz") == 0);
    CHECK(str_copy(small, "abcdef", sizeof(small)) == 6 && strcmp(small, "abc") == 0);
    CHECK(str_copy(small, "ab", sizeof(small)) == 2 && strcmp(small, "ab") == 0);

    CHECK(utf8_copy(small, hello, 0) == 0);
    CHECK(utf8_copy(small, hello, 3) == 1 && strcmp(small, "h") == 0);  /* not half an e-acute */
    CHECK(utf8_copy(small, hello, 4) == 3 && strcmp(small, "h\xc3\xa9") == 0);
    CHECK(utf8_copy(small, "\xc3\xa9\xc3\xa9", 2) == 0 && small[0] == '\0');
    CHECK(utf8_copy(small, "abc", sizeof(small)) == 3 && strcmp(small, "abc") == 0);
done:
    return failed;
}

/* path_copy_checked accepts only a path the program could write and show:
 * not empty, short enough, no control bytes. A refusal leaves out empty. */
int platform_writable_paths_refuse_control_bytes(void)
{
    int failed = 0;
    char out[8] = "junk";

    CHECK(path_copy_checked(NULL, "a", sizeof(out)) == -1);
    CHECK(path_copy_checked(out, "a", 0) == -1);
    CHECK(path_copy_checked(out, NULL, sizeof(out)) == -1 && out[0] == '\0');
    CHECK(path_copy_checked(out, "", sizeof(out)) == -1 && out[0] == '\0');
    CHECK(path_copy_checked(out, "a\tb", sizeof(out)) == -1 && out[0] == '\0');
    CHECK(path_copy_checked(out, "a\x7f", sizeof(out)) == -1 && out[0] == '\0');
    CHECK(path_copy_checked(out, "a\nb", sizeof(out)) == -1 && out[0] == '\0');
    CHECK(path_copy_checked(out, "1234567", sizeof(out)) == 0 && strcmp(out, "1234567") == 0);
    CHECK(path_copy_checked(out, "12345678", sizeof(out)) == -1 && out[0] == '\0');
    CHECK(path_copy_checked(out, "caf\xc3\xa9", sizeof(out)) == 0 &&
          strcmp(out, "caf\xc3\xa9") == 0);  /* UTF-8 is not a control byte */
done:
    return failed;
}

static void make_folder(const char *path)
{
#ifdef _WIN32
    (void)_mkdir(path);
#else
    (void)mkdir(path, 0700);
#endif
}

static void remove_folder(const char *path)
{
#ifdef _WIN32
    (void)_rmdir(path);
#else
    (void)rmdir(path);
#endif
}

#ifndef _WIN32
/* Set or clear one environment variable; NULL clears it. */
static void put_env(const char *name, const char *value)
{
    if (value) setenv(name, value, 1);
    else unsetenv(name);
}

/* A copy of an environment variable (or NULL), to put back afterwards. */
static char *saved_env(const char *name)
{
    const char *value = getenv(name);
    char *copy = NULL;
    if (value && (copy = malloc(strlen(value) + 1))) strcpy(copy, value);
    return copy;
}
#endif

/*
 * preference_path_at creates each folder below the base, and gives up when
 * one cannot be created (here a plain file stands where the organization
 * folder should be). Backslashes separate folders too, as Windows paths
 * do. On POSIX, preference_path follows HOME (and XDG_DATA_HOME on Linux,
 * where an empty value means "unset") and refuses a root too long for its
 * buffer instead of cutting it.
 */
int platform_preference_folders_are_created_or_refused(void)
{
    int failed = 0;
    char *path = NULL;
    const char *base = TEST_OUT "prefs_edges";
#ifndef _WIN32
    char *home = saved_env("HOME");
    char *xdg = saved_env("XDG_DATA_HOME");
    static char long_root[5000];
#endif
    FILE *blocker = NULL;

    make_folder(MANGO_TEST_OUTDIR);
    make_folder(base);
    path = preference_path_at(base, "Org\\Team", "App");
    CHECK(path != NULL);
    CHECK(strcmp(path, TEST_OUT "prefs_edges/Org\\Team/App/") == 0);
    free(path);
    path = NULL;

    /* A file where a folder must go: the walk stops and reports it. */
    blocker = fopen(TEST_OUT "prefs_edges/Blocked", "wb");
    CHECK(blocker != NULL);
    fclose(blocker);
    blocker = NULL;
    CHECK(preference_path_at(base, "Blocked", "App") == NULL);

#ifndef _WIN32
    put_env("HOME", NULL);
    put_env("XDG_DATA_HOME", NULL);
    CHECK(preference_path("Org", "App") == NULL);   /* nowhere to put it */
    memset(long_root, 'x', sizeof(long_root) - 1);
    long_root[0] = '/';
    long_root[sizeof(long_root) - 1] = '\0';
    put_env("HOME", long_root);
    CHECK(preference_path("Org", "App") == NULL);   /* too long, not cut */
#if !defined(__APPLE__)
    put_env("HOME", NULL);
    put_env("XDG_DATA_HOME", long_root);
    CHECK(preference_path("Org", "App") == NULL);
    put_env("XDG_DATA_HOME", "");                   /* empty: use HOME */
    put_env("HOME", base);
    path = preference_path("Org", "App");
    CHECK(path && strcmp(path, TEST_OUT "prefs_edges/.local/share/Org/App/") == 0);
#endif
#endif
done:
#ifndef _WIN32
    put_env("HOME", home);
    put_env("XDG_DATA_HOME", xdg);
    free(home);
    free(xdg);
    remove_folder(TEST_OUT "prefs_edges/.local/share/Org/App");
    remove_folder(TEST_OUT "prefs_edges/.local/share/Org");
    remove_folder(TEST_OUT "prefs_edges/.local/share");
    remove_folder(TEST_OUT "prefs_edges/.local");
#endif
    free(path);
    remove(TEST_OUT "prefs_edges/Blocked");
    remove_folder(TEST_OUT "prefs_edges/Org\\Team/App");
    remove_folder(TEST_OUT "prefs_edges/Org\\Team");
    remove_folder(TEST_OUT "prefs_edges/Org");
    remove_folder(base);
    return failed;
}

/* The folder part of an executable path keeps its last separator, whether
 * that is '/' or '\\' (or both kinds appear); a bare name has no folder.
 * None of these files exist, so the path is used as given. */
int platform_executable_folder_reads_any_separator(void)
{
    int failed = 0;
    char *folder = NULL;

    CHECK((folder = executable_folder("C:\\Games\\Mango\\super-mango.exe")) != NULL);
    CHECK(strcmp(folder, "C:\\Games\\Mango\\") == 0);
    free(folder);
    CHECK((folder = executable_folder("no/such/dir\\super-mango")) != NULL);
    CHECK(strcmp(folder, "no/such/dir\\") == 0);
    free(folder);
    CHECK((folder = executable_folder("no\\such/super-mango")) != NULL);
    CHECK(strcmp(folder, "no\\such/") == 0);
    free(folder);
    folder = NULL;
    CHECK(executable_folder("super-mango-no-folder") == NULL);
done:
    free(folder);
    return failed;
}

/* A path the asset root code cannot hold whole is refused, never cut:
 * a cut path could name some other folder. */
int asset_root_rejects_paths_it_cannot_hold(void)
{
    int failed = 0;
    static char huge[5000];
    char *joined = NULL;

    memset(huge, 'a', sizeof(huge) - 1);
    huge[sizeof(huge) - 1] = '\0';
    CHECK(asset_root_contains(NULL) == 0);
    CHECK(asset_root_contains(huge) == 0);
    CHECK(asset_root_enter_from(NULL) == -1);
    CHECK(asset_root_enter_from(huge) == -1);
    /* 4090 bytes: fits, but adding "../" for the parent would not. */
    huge[4090] = '\0';
    CHECK(asset_root_enter_from(huge) == -1);

    CHECK(!asset_root_path_is_absolute(NULL));
    CHECK(!asset_root_path_is_absolute(""));
    CHECK(asset_root_path_is_absolute("\\\\server\\share\\x.toml"));
    CHECK(asset_root_path_is_absolute("d:/levels/x.toml"));
    CHECK(!asset_root_path_is_absolute("C:levels"));   /* drive-relative */
    CHECK(!asset_root_path_is_absolute("1:/levels"));
    CHECK(!asset_root_path_is_absolute("C"));

    CHECK(asset_root_absolute(NULL) == NULL);
    CHECK(asset_root_absolute("") == NULL);
    CHECK((joined = asset_root_absolute("/abs/x.toml")) != NULL);
    CHECK(strcmp(joined, "/abs/x.toml") == 0);
done:
    free(joined);
    return failed;
}

static char *working_folder(void)
{
    static char buffer[4096];
#ifdef _WIN32
    if (!_getcwd(buffer, (int)sizeof(buffer))) return NULL;
#else
    if (!getcwd(buffer, sizeof(buffer))) return NULL;
#endif
    char *copy = malloc(strlen(buffer) + 1);
    if (copy) strcpy(copy, buffer);
    return copy;
}

static int enter_folder(const char *folder)
{
#ifdef _WIN32
    return _chdir(folder);
#else
    return chdir(folder);
#endif
}

/* How many folders OUTDIR is below the checkout ("out/headless" is 2). */
static int outdir_depth(void)
{
    int depth = 1;
    for (const char *p = MANGO_TEST_OUTDIR; *p; p++)
        if ((*p == '/' || *p == '\\') && p[1]) depth++;
    return depth;
}

/*
 * asset_root_enter starts from the folder of the running program. The
 * game looks there and up to two folders higher, which is where a build in
 * out/ or out/<tree>/ finds the checkout. The test program lives in OUTDIR:
 * out/headless/ finds it; out/headless/coverage/ (make coverage) is one
 * level too deep, so there the search fails and leaves the folder alone.
 * From the filesystem root, a relative path gets exactly one separator.
 */
int asset_root_is_found_from_the_test_program(void)
{
    int failed = 0;
    char *home = working_folder();
    char *at_root = NULL;
    const int reachable = outdir_depth() <= 2;

    CHECK(home != NULL);
    CHECK(enter_folder(TEST_OUT) == 0);
    CHECK(asset_root_contains("") == 0);
    CHECK(asset_root_enter() == (reachable ? 0 : -1));
    CHECK(asset_root_contains("") == reachable);

#ifndef _WIN32
    CHECK(enter_folder("/") == 0);
    CHECK((at_root = asset_root_absolute("x.toml")) != NULL);
    CHECK(strcmp(at_root, "/x.toml") == 0);
#endif
done:
    if (home && enter_folder(home) != 0) failed = 1;
    free(at_root);
    free(home);
    return failed;
}
