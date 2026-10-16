/*
 * mangolib.h — Mangolib, Super Mango's engine layer, in one include.
 *
 * Mangolib sits between raylib and the game/editor, the way raylib.h sits
 * between an application and the platform. It depends only on raylib and the
 * C library; the game and the editor depend on it, never the reverse
 * (tools/check_layering.py fails the build otherwise).
 *
 *   mcore        monotonic clock, UTF-8 string copies, OS folders
 *   mtiming      fixed-step clock: frame time in, whole steps out
 *   mrandom      caller-owned reproducible random streams
 *   mangomath    integer hitboxes, half-open overlap, exact float compares
 *   mgfx         window, sprite drawing, logical-canvas presentation
 *   mtext        raylib's built-in font: measure and draw
 *   maudio       bounded sound voices and streamed music
 *   massets      reference-counted cache of textures, sounds and fonts
 *   masset_root  find the folder that holds assets/ and levels/
 *   mfile        UTF-8 paths, atomic saves, file fingerprints
 *   minput       raylib/GLFW input as ordered commands
 *   mangogui     immediate-mode widgets with staged edits
 *   mutf8        strict UTF-8 validation
 *
 * Including a single module header is fine too; this one is a convenience.
 */
#pragma once

#include "mcore.h"
#include "mtiming.h"
#include "mrandom.h"
#include "mangomath.h"
#include "mgfx.h"
#include "mtext.h"
#include "maudio.h"
#include "massets.h"
#include "masset_root.h"
#include "mfile.h"
#include "minput.h"
#include "mangogui.h"
#include "mutf8.h"
