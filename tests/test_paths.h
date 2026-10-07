/*
 * test_paths.h — Where native tests write their scratch files.
 *
 * The Makefile passes the build's OUTDIR as MANGO_TEST_OUTDIR, so `make test`,
 * `make sanitize` (out-sanitize/) and `make coverage` (out/coverage/) each
 * write into their own tree and can run at the same time in one checkout.
 * Built some other way, the tests fall back to out/.
 *
 * TEST_OUT ends in a slash and is a string literal, so C joins it with the
 * next literal at compile time: TEST_OUT "level.toml" -> "out/level.toml".
 */
#ifndef MANGO_TEST_PATHS_H
#define MANGO_TEST_PATHS_H

#ifndef MANGO_TEST_OUTDIR
#define MANGO_TEST_OUTDIR "out"
#endif

#define TEST_OUT MANGO_TEST_OUTDIR "/"

#endif /* MANGO_TEST_PATHS_H */
