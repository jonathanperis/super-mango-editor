/*
 * serializer_io_test.c — shared/serializer_io.c when the file system says
 * no: a folder where a file was expected, a file one byte too big, a NUL in
 * the middle of a level, a stale temporary file from a crashed save, a
 * folder that does not exist, a target that appeared meanwhile, a drive
 * without hard links whose copy cannot be made, and names too long for the
 * buffers.
 *
 * Linked into level-serializer-test; serializer_io_edge_cases() runs every
 * case below and returns how many failed. Every scratch file lives under
 * TEST_OUT "sio_" and is removed again.
 */
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L  /* getpid, symlink, chdir, getcwd */
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>   /* _mkdir, _rmdir */
#include <process.h>  /* _getpid */
#else
#include <sys/stat.h>  /* mkdir */
#include <unistd.h>    /* getpid, rmdir, symlink, getcwd, chdir */
#endif

#include "shared/serializer_io.h"
#include "test_paths.h"
#include "check.h"

int serializer_io_edge_cases(void);

#define SIO_DIR  TEST_OUT "sio_dir"
#define SIO_FILE TEST_OUT "sio_file.toml"
#define SIO_COPY TEST_OUT "sio_copy.toml"

static int write_bytes(const char *path, const char *bytes, size_t size)
{
    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;
    size_t put = fwrite(bytes, 1, size, fp);
    return fclose(fp) == 0 && put == size ? 0 : -1;
}

static void make_dir(const char *path)
{
#ifdef _WIN32
    (void)_mkdir(path);
#else
    (void)mkdir(path, 0700);
#endif
}

static void remove_dir(const char *path)
{
#ifdef _WIN32
    (void)_rmdir(path);
#else
    (void)rmdir(path);
#endif
}

static unsigned long process_id(void)
{
#ifdef _WIN32
    return (unsigned long)_getpid();
#else
    return (unsigned long)getpid();
#endif
}

/*
 * Reading: a folder is not a level (it opens on POSIX, then the read
 * fails); a file of exactly max_bytes is too big, because its text and the
 * NUL after it would not both fit, and one byte less fits; a NUL inside
 * the text is refused, since every reader stops at it; max_bytes 0 is an
 * error, and a path through a file ("level.toml/x") is simply missing.
 */
static int reads_refuse_folders_oversize_and_nul_bytes(void)
{
    int failed = 0;
    char *text = NULL;
    size_t size = 99;
    int too_large = -1;

    make_dir(SIO_DIR);
    CHECK(serializer_read_bytes(SIO_DIR, 64, &text, &size, &too_large) == SERIALIZER_PATH_ERROR);
    CHECK(text == NULL && size == 0 && too_large == 0);
    CHECK(serializer_read_file(SIO_DIR, 64, &text) == SERIALIZER_PATH_ERROR);

    CHECK(write_bytes(SIO_FILE, "12345678", 8) == 0);
    CHECK(serializer_read_bytes(SIO_FILE, 8, &text, &size, &too_large) == SERIALIZER_PATH_ERROR);
    CHECK(text == NULL && too_large == 1);
    CHECK(serializer_read_bytes(SIO_FILE, 8, &text, NULL, NULL) == SERIALIZER_PATH_ERROR);
    CHECK(serializer_read_bytes(SIO_FILE, 9, &text, &size, &too_large) == SERIALIZER_PATH_EXISTING);
    CHECK(text && size == 8 && too_large == 0 && strcmp(text, "12345678") == 0);
    free(text);
    text = NULL;
    CHECK(serializer_read_bytes(SIO_FILE, 0, &text, &size, NULL) == SERIALIZER_PATH_ERROR);

    CHECK(write_bytes(SIO_FILE, "name = \"a\"\0tail\n", 16) == 0);
    CHECK(serializer_read_bytes(SIO_FILE, 64, &text, &size, NULL) == SERIALIZER_PATH_EXISTING);
    CHECK(size == 16);   /* the raw reader keeps every byte */
    free(text);
    text = NULL;
    CHECK(serializer_read_file(SIO_FILE, 64, &text) == SERIALIZER_PATH_ERROR && text == NULL);

#ifndef _WIN32
    CHECK(serializer_probe_path_utf8(SIO_FILE "/inside") == SERIALIZER_PATH_MISSING);  /* ENOTDIR */
#endif
    CHECK(serializer_probe_path_utf8("") == SERIALIZER_PATH_ERROR);
    CHECK(serializer_probe_path_utf8(NULL) == SERIALIZER_PATH_ERROR);
    CHECK(!serializer_file_exists_utf8(TEST_OUT "sio_never_written.toml"));
    CHECK(serializer_fopen_utf8(NULL, "rb") == NULL && serializer_fopen_utf8(SIO_FILE, NULL) == NULL);
    CHECK(serializer_remove_utf8(NULL) == -1);
done:
    free(text);
    remove(SIO_FILE);
    remove_dir(SIO_DIR);
    return failed;
}

/*
 * Fingerprints tell "unchanged" from "changed on disk": a missing file has
 * none (0, not valid), a folder cannot have one (-1), and two fingerprints
 * match only when both are valid and size and hash agree.
 */
static int fingerprints_compare_only_what_was_read(void)
{
    int failed = 0;
    SerializerFileFingerprint a, b, missing;

    CHECK(serializer_fingerprint_utf8(TEST_OUT "sio_never_written.toml", &missing) == 0);
    CHECK(!missing.valid);
    make_dir(SIO_DIR);
    CHECK(serializer_fingerprint_utf8(SIO_DIR, &a) == -1 && !a.valid);
    CHECK(serializer_fingerprint_utf8("", &a) == -1);
    CHECK(serializer_fingerprint_utf8(SIO_FILE, NULL) == -1);

    CHECK(write_bytes(SIO_FILE, "abc", 3) == 0);
    CHECK(serializer_fingerprint_utf8(SIO_FILE, &a) == 1 && a.valid && a.size == 3);
    serializer_fingerprint_bytes("abc", 3, &b);
    CHECK(serializer_fingerprint_equal(&a, &b));
    serializer_fingerprint_bytes("abd", 3, &b);           /* same size, other bytes */
    CHECK(!serializer_fingerprint_equal(&a, &b));
    serializer_fingerprint_bytes("abcd", 4, &b);          /* other size */
    CHECK(!serializer_fingerprint_equal(&a, &b));
    CHECK(!serializer_fingerprint_equal(&a, &missing));   /* never read */
    CHECK(!serializer_fingerprint_equal(&missing, &missing));
    CHECK(!serializer_fingerprint_equal(NULL, &a) && !serializer_fingerprint_equal(&a, NULL));
done:
    remove(SIO_FILE);
    remove_dir(SIO_DIR);
    return failed;
}

/*
 * Temporary files: a stale one left by a crashed save of this same process
 * id is stepped over (name.tmp.<pid>.1), never reused or truncated; a
 * folder that does not exist, or a buffer too small for the name, gives no
 * file and an empty name rather than a cut one.
 */
static int temporary_files_never_reuse_or_cut_a_name(void)
{
    int failed = 0;
    char stale[512], temp[512], expected[512];
    char tiny[8] = "junk";
    FILE *fp = NULL;

    snprintf(stale, sizeof(stale), "%s.tmp.%lu", SIO_FILE, process_id());
    snprintf(expected, sizeof(expected), "%s.tmp.%lu.1", SIO_FILE, process_id());
    CHECK(write_bytes(stale, "older save", 10) == 0);
    fp = serializer_open_temp(SIO_FILE, temp, sizeof(temp));
    CHECK(fp != NULL);
    CHECK(strcmp(temp, expected) == 0);
    fclose(fp);
    fp = NULL;
    {
        char *kept = NULL;
        CHECK(serializer_read_file(stale, 64, &kept) == SERIALIZER_PATH_EXISTING);
        CHECK(strcmp(kept, "older save") == 0);   /* the stale file is untouched */
        free(kept);
    }

    fp = serializer_open_temp(TEST_OUT "sio_no_such_folder/level.toml", temp, sizeof(temp));
    CHECK(fp == NULL && temp[0] == '\0');
    fp = serializer_open_temp(SIO_FILE, tiny, sizeof(tiny));
    CHECK(fp == NULL && tiny[0] == '\0');
    CHECK(serializer_open_temp(NULL, temp, sizeof(temp)) == NULL);
    CHECK(serializer_open_temp(SIO_FILE, temp, 0) == NULL);

    CHECK(serializer_make_temp_path(SIO_FILE, tiny, sizeof(tiny)) == -1 && tiny[0] == '\0');
    CHECK(serializer_make_temp_path(NULL, temp, sizeof(temp)) == -1);
    CHECK(serializer_flush(NULL) == -1);
done:
    if (fp) fclose(fp);
    remove(expected);
    remove(stale);
    return failed;
}

/*
 * Installing: replacing from a temporary file that is gone fails and
 * leaves the target alone; a create-only install refuses a target that
 * already exists (link() and, on a drive without hard links, the O_EXCL
 * copy both refuse it); a missing temporary file installs nothing.
 */
static int installs_fail_without_touching_the_target(void)
{
    int failed = 0;
    char *text = NULL;

    CHECK(write_bytes(SIO_FILE, "old", 3) == 0);
    CHECK(serializer_replace_file(TEST_OUT "sio_gone.tmp", SIO_FILE) == -1);
    CHECK(serializer_replace_file(NULL, SIO_FILE) == -1);
    CHECK(serializer_read_file(SIO_FILE, 64, &text) == SERIALIZER_PATH_EXISTING);
    CHECK(strcmp(text, "old") == 0);
    free(text);
    text = NULL;

    CHECK(write_bytes(SIO_COPY, "new", 3) == 0);
    CHECK(serializer_create_file(SIO_COPY, SIO_FILE) == -1);       /* target exists */
#ifndef _WIN32
    serializer_test_set_failure(SERIALIZER_TEST_FAILURE_NO_HARD_LINKS);
    CHECK(serializer_create_file(SIO_COPY, SIO_FILE) == -1);       /* the copy refuses too */
    serializer_test_set_failure(SERIALIZER_TEST_FAILURE_NO_HARD_LINKS);
    CHECK(serializer_create_file(TEST_OUT "sio_gone.tmp", TEST_OUT "sio_new.toml") == -1);
    CHECK(serializer_probe_path_utf8(TEST_OUT "sio_new.toml") == SERIALIZER_PATH_MISSING);
#endif
    serializer_test_set_failure(SERIALIZER_TEST_FAILURE_NONE);
    CHECK(serializer_create_file(NULL, SIO_FILE) == -1);
    CHECK(serializer_read_file(SIO_FILE, 64, &text) == SERIALIZER_PATH_EXISTING);
    CHECK(strcmp(text, "old") == 0);
    CHECK(serializer_install_temp(TEST_OUT "sio_gone.tmp", SIO_FILE, 0) == -1);
done:
    serializer_test_set_failure(SERIALIZER_TEST_FAILURE_NONE);
    free(text);
    remove(SIO_FILE);
    remove(SIO_COPY);
    remove(TEST_OUT "sio_new.toml");
    return failed;
}

/*
 * Names: same_file needs two real files (two spellings of one file match,
 * a missing one never does); resolve_save_target follows a link to its
 * file, replaces a dangling link itself, and refuses a buffer too small
 * for either answer.
 */
static int file_identity_and_save_targets(void)
{
    int failed = 0;
    char buf[512];

    CHECK(write_bytes(SIO_FILE, "x", 1) == 0);
    CHECK(serializer_same_file_utf8(SIO_FILE, MANGO_TEST_OUTDIR "/./sio_file.toml"));
    CHECK(!serializer_same_file_utf8(SIO_FILE, TEST_OUT "sio_never_written.toml"));
    CHECK(!serializer_same_file_utf8(NULL, SIO_FILE) && !serializer_same_file_utf8(SIO_FILE, ""));
    CHECK(!serializer_path_is_symlink(NULL) && !serializer_path_is_symlink(""));

    CHECK(serializer_resolve_save_target(SIO_FILE, buf, sizeof(buf)) == 0);
    CHECK(strcmp(buf, SIO_FILE) == 0);
    CHECK(serializer_resolve_save_target(SIO_FILE, buf, 4) == -1);
    CHECK(serializer_resolve_save_target(NULL, buf, sizeof(buf)) == -1);
    CHECK(serializer_resolve_save_target(SIO_FILE, buf, 0) == -1);
#ifndef _WIN32
    remove(TEST_OUT "sio_link.toml");
    CHECK(symlink("sio_file.toml", TEST_OUT "sio_link.toml") == 0);
    CHECK(serializer_resolve_save_target(TEST_OUT "sio_link.toml", buf, sizeof(buf)) == 0);
    CHECK(strstr(buf, "sio_file.toml") != NULL && buf[0] == '/');   /* the real file */
    CHECK(serializer_resolve_save_target(TEST_OUT "sio_link.toml", buf, 8) == -1);
    remove(SIO_FILE);
    CHECK(serializer_resolve_save_target(TEST_OUT "sio_link.toml", buf, sizeof(buf)) == 0);
    CHECK(strcmp(buf, TEST_OUT "sio_link.toml") == 0);   /* dangling: the link itself */
#endif
done:
    remove(TEST_OUT "sio_link.toml");
    remove(SIO_FILE);
    return failed;
}

#ifndef _WIN32
/*
 * A durable save syncs the folder that holds the file, whatever its
 * spelling: a bare name lives in ".", so it is saved from inside TEST_OUT.
 */
static int durable_install_of_a_bare_name(void)
{
    int failed = 0;
    char home[4096];
    int moved = 0;

    CHECK(getcwd(home, sizeof(home)) != NULL);
    CHECK(chdir(MANGO_TEST_OUTDIR) == 0);
    moved = 1;
    serializer_set_durable(1);
    CHECK(write_bytes("sio_bare.tmp", "bare", 4) == 0);
    CHECK(serializer_install_temp("sio_bare.tmp", "sio_bare.toml", 0) == 0);
    CHECK(serializer_file_exists_utf8("sio_bare.toml"));
    CHECK(write_bytes("sio_bare.tmp", "fresh", 5) == 0);
    CHECK(serializer_install_temp("sio_bare.tmp", "sio_bare_new.toml", 1) == 0);
done:
    remove("sio_bare.tmp");
    remove("sio_bare.toml");
    remove("sio_bare_new.toml");
    if (moved && chdir(home) != 0) failed = 1;
    return failed;
}
#endif

int serializer_io_edge_cases(void)
{
    static const TestCase cases[] = {
        TEST_CASE(reads_refuse_folders_oversize_and_nul_bytes),
        TEST_CASE(fingerprints_compare_only_what_was_read),
        TEST_CASE(temporary_files_never_reuse_or_cut_a_name),
        TEST_CASE(installs_fail_without_touching_the_target),
        TEST_CASE(file_identity_and_save_targets),
#ifndef _WIN32
        TEST_CASE(durable_install_of_a_bare_name),
#endif
    };
    char *no_args[] = {NULL};
    return test_run_cases("serializer_io", cases, sizeof(cases) / sizeof(cases[0]),
                          (TestHooks){0}, 0, no_args);
}
