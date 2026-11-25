/*
 * render_snapshot.h — Pixel snapshots of whole frames, for the render tests.
 *
 * A snapshot test draws a fixed scene (same level, same steps, same seed),
 * reads the window back, and compares the pixels with what was recorded:
 *
 *   tests/snapshots/render_snapshots.txt   one line per snapshot: name,
 *                                          width, height, FNV-1a 64 hash
 *   tests/snapshots/<name>.png             the recorded frame, to look at
 *
 * Only raylib's Memory backend is compared. It is a software renderer that
 * runs the same code on every system, so its pixels are the same on Linux,
 * macOS and Windows; a real GPU (Mesa on the Linux CI leg, a laptop's
 * driver) may round an edge differently and is not compared.
 *
 * When a frame changes, the test fails with the snapshot's name, how many
 * pixels differ and where the first one is, and writes the new frame to
 * OUTDIR/snapshots/<name>.png. If the change is intended, record it:
 *
 *   make render-snapshots-update
 *
 * which reruns the snapshot cases with MANGO_SNAPSHOT_UPDATE=1 (the cases
 * then rewrite their line in render_snapshots.txt and write their PNG) and
 * copies the new PNGs into tests/snapshots/. Commit both with the change.
 */
#ifndef MANGO_TESTS_RENDER_SNAPSHOT_H
#define MANGO_TESTS_RENDER_SNAPSHOT_H

#include "shared/graphics.h"  /* raylib: Image */

/* Read the window's framebuffer as a top-down RGBA image. Call it after
 * drawing (and rlDrawRenderBatchActive) and before EndDrawing. The Memory
 * backend reads back bottom-up and in BGRA order; this undoes both. */
Image snapshot_grab_screen(void);

/* Compare `image` with the snapshot called `name` (letters, digits, '_').
 * Returns 0 when it matches, when MANGO_SNAPSHOT_UPDATE=1 recorded it, or
 * when this backend is not compared; 1 when it differs or is missing. */
int snapshot_check(const char *name, Image image);

#endif
