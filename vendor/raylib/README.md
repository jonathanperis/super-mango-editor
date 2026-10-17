# raylib dependency

The game and editor use **raylib 6.0**, built from upstream commit
`dbc56a87da87d973a9c5baa4e7438a9d20121d28`.

`manifest.json` pins the HTTPS source archive and SHA-256. `tools/build_raylib.py`
verifies it before extraction, applies the documented source fixes in
`patches.json`, and builds a static library with CMake. Native builds use bundled
GLFW; browser builds use the Web backend
and the project's Emscripten 6.0.9 toolchain. Desktop SDL backends are not used.
raylib's automatic F12 screenshot shortcut is disabled because application
bindings and explicit exports belong to Super Mango.

## Trimmed feature set

`build_raylib.py` keeps raylib's `config.h` (CMake's `CUSTOMIZE_BUILD` stays
off) and switches individual features off with `-DSUPPORT_...=0` compiler flags.
`config.h` only supplies a default when a flag is not already defined, and the
sources test the value with `#if`, so each flag removes exactly that feature.
Super Mango is 2D, uses the built-in font, loads PNG images and WAV/QOA sounds,
and generates no images other than `GenImageColor` (which is outside the
image-generation block). These are compiled out:

| Flag set to 0 | Removes |
|---------------|---------|
| `SUPPORT_MODULE_RMODELS` | 3D models, meshes, materials and 3D drawing |
| `SUPPORT_FILEFORMAT_BMP`, `GIF`, `QOI`, `DDS` | Image formats other than PNG |
| `SUPPORT_FILEFORMAT_TTF`, `FNT` | Font files (the default font is built in) |
| `SUPPORT_FILEFORMAT_OGG`, `MP3`, `XM`, `MOD` | Audio formats other than WAV and QOA |
| `SUPPORT_IMAGE_EXPORT` | `ExportImage` and friends |
| `SUPPORT_IMAGE_GENERATION` | `GenImageGradient*`, `GenImageChecked`, noise, cellular, text images |
| `SUPPORT_COMPRESSION_API` | `CompressData`, Base64 and hash helpers |
| `SUPPORT_AUTOMATION_EVENTS` | Input recording and replay |
| `SUPPORT_GESTURES_SYSTEM` | Touch gesture detection (raw touch input remains) |
| `SUPPORT_CAMERA_SYSTEM` | `UpdateCamera` for 3D cameras |
| `SUPPORT_CLIPBOARD_IMAGE` | `GetClipboardImage` |
| `SUPPORT_SCREEN_CAPTURE` | The F12 screenshot shortcut |

PNG, WAV and QOA support stay on. Measured on Linux (clang release build and
the Web build): the static library shrank from 3.36 MB to 2.37 MB, the
stripped game from 1.99 MB to 1.65 MB, and `super-mango.wasm` from 635 KB to
481 KB (259 KB to 193 KB gzipped). Calling a removed function fails at link
time, so turning a feature back on means deleting its flag here; the build
script is a raylib input, so that change rebuilds every raylib tree and
invalidates CI's compiled-raylib caches.

Source and library outputs stay in the selected `OUTDIR`; no system raylib
installation is required. Keep native, web, debug, release and sanitizer build
directories separate. Windows dependency builds use MinGW Makefiles.

The explicit `RAYLIB_PLATFORM=memory` test build uses raylib's software framebuffer
and miniaudio's null backend. It is never selected automatically and cannot be
used for native release packaging. It supplements desktop/device verification.
Keyboard/text/mouse commands chain the selected GLFW backend's public callbacks
to retain event ordering and per-event modifiers; raylib still maintains sampled
device state and owns polling. The Memory suite verifies this adapter with
test-owned callback registration, independently of a display server.

## Retained source fixes

Linux ASan/LeakSanitizer/UBSan exposed three upstream audio defects. The bootstrap
applies the same exact replacements on every platform and rejects unknown patch
contexts. Repeated builds recognize the already-applied changes.

- `UnloadSoundAlias` releases its own converter residual cache while retaining
  the shared sample data until the owning sound is unloaded.
- `UnloadMusicStream` frees the allocated WAV decoder after `drwav_uninit` closes
  its internal resources, matching the ownership of other allocated decoders.
- miniaudio's data-converter initialization passes a null heap through unchanged
  for a zero-allocation passthrough conversion instead of adding zero to null.

One desktop leak is fixed the same way. Upstream `SetMouseCursor` on the GLFW
backend calls `glfwCreateStandardCursor` on every call and never frees the
result, so each cursor change (the editor's I-beam over a field, the arrows of a
drag) leaked one system cursor until exit. The patched function creates each of
the ten standard shapes once and reuses it; `glfwTerminate` frees them, and
`ClosePlatform` forgets them so a later window in the same process creates
fresh ones.

### QOA decoder bounds

raylib 6.0's bundled `src/external/qoa.h` predates two upstream bounds fixes,
so a crafted `.qoa` file passed to `LoadSound`/`LoadWave` (the shipped sounds
are QOA) could write out of bounds: a header with more than eight channels
overran `qoa_desc`'s decoder-state array on the stack, and a frame declaring
more samples than the file header overran the heap sample buffer
([raysan5/raylib#6161](https://github.com/raysan5/raylib/issues/6161)). The
patch applies the
[raysan5/raylib#6162](https://github.com/raysan5/raylib/pull/6162) re-vendor of
phoboslab/qoa `f73b4a36` hunk by hunk: the patched file equals raylib 6.2's
`qoa.h` apart from the marker comments. The header rejects more than
`QOA_MAX_CHANNELS`, each frame bounds its slices by its own data and by
`QOA_SLICES_PER_FRAME`, and the sample buffer is sized in whole frames with an
overflow guard. Valid files decode to the same samples; the encoder is
unchanged, so `tools/qoa.py` still matches it. The session test feeds both
crafted files to `LoadWaveFromMemory`.

These fixes retain the 6.0 public API, formats and playback behavior. Source
modifications are marked in the affected files. The session/audio regressions
exercise the real library, and Linux CI keeps leak detection enabled.

### Native macOS frame pacing

The macOS branch of raylib's partial-busy `WaitTime` wakes from `usleep` one
millisecond earlier. Its existing short busy wait then waits until the original
deadline. The upstream 5% margin alone allowed macOS sleep coalescing to overshoot
the 60 Hz target even when debug-game work took less than a millisecond.

This keeps normal `EndDrawing` timing, VSync hints, the 60 FPS cap and event
polling intact. Windows/Linux/Web wait code is unchanged; hidden smoke remains
uncapped. The tradeoff is up to roughly one additional millisecond of active
waiting per capped frame, not a full-frame busy loop. OS scheduling or expensive
frames can still miss a deadline; this is a target cadence, not a real-time guarantee.

Upstream source and release: https://github.com/raysan5/raylib/releases/tag/6.0

Release packaging includes raylib's license, the GLFW license, and the
license-bearing external headers/source files from this pinned source tree.
The marked miniaudio fix leaves its license text intact, as do all other bundled
component notices; no license terms are extracted heuristically or rewritten.
