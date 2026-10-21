# raylib dependency

The game and editor use **raylib 6.2**, built from upstream commit
`02566453880e06bec97f5b8160fa6fe1e98397ca`.

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

## Gamepad mappings

The desktop input only uses a controller GLFW has a gamepad mapping for
(`glfwJoystickIsGamepad`), so GLFW's built-in table decides which pads work.
Upstream refreshes that table by hand: raylib 6.0 carried a 2025
regeneration from SDL_GameControllerDB (1837 entries), and 6.2's GLFW 3.5.1
update brought back GLFW's own copy (928 entries: 7 Xbox Series entries
instead of 42, 69 8BitDo instead of 212, no DualSense Edge).
`build_raylib.py` therefore always writes
`src/external/glfw/src/mappings.h` itself, the way GLFW's `update_mappings`
target (`CMake/GenerateMappings.cmake`) does: every Windows, macOS and Linux
line of `gamecontrollerdb/gamecontrollerdb.txt`, in file order, fills the
placeholders of the archive's own `mappings.h.in`, which keeps GLFW's notices
and its built-in XInput entries.

`gamecontrollerdb/` holds the database and its zlib license exactly as
published at SDL_GameControllerDB commit
`c1d5289a1f713b30a2c121e9fe6529d39360be0f` (2026-10-02). `manifest.json`
records that commit, the raw-file URL and the SHA-256, which the build checks
before use; `.gitattributes` keeps Git from changing its line endings. The
copy is vendored rather than downloaded so offline and CI builds need only the
raylib archive. The table is compiled in, so releases need no mapping file and
the game makes no `SetGamepadMappings` call. To refresh it, download
`gamecontrollerdb.txt` and `LICENSE` from a newer upstream commit, then update
the commit, URL and SHA-256 together. Lines GLFW cannot parse (output
modifiers such as `+leftx`) are skipped by GLFW at start-up, as before.

## Retained source fixes

The bootstrap applies the same exact replacements on every platform and
rejects unknown patch contexts. Repeated builds recognize the already-applied
changes.

- miniaudio's data-converter initialization passes a null heap through unchanged
  for a zero-allocation passthrough conversion instead of adding zero to null
  (found by UBSan).
- miniaudio's `ma_device_uninit` wakes the backend's own data loop before it
  joins the device thread. Upstream (0.11.25, and the 0.11.26 dev branch) no
  longer stops a started device there; it only signals the worker's wakeup
  event, which a PulseAudio worker blocked in `pa_mainloop_iterate` never
  sees, so `CloseAudioDevice` waited for PulseAudio's next timer: about 2 s on
  every game and editor exit on Linux, and per process in CI's scripted smoke.
  The patch calls the same `onDeviceDataLoopWakeup` that `ma_device_stop`
  uses; the worker already exits when it finds the device uninitialized.

One desktop leak is fixed the same way. Upstream `SetMouseCursor` on the GLFW
backend calls `glfwCreateStandardCursor` on every call and never frees the
result, so each cursor change (the editor's I-beam over a field, the arrows of a
drag) leaked one system cursor until exit. The patched function creates each of
the ten standard shapes once and reuses it; `glfwTerminate` frees them, and
`ClosePlatform` forgets them so a later window in the same process creates
fresh ones.

### WAV decoder hardening

raylib 6.2's `src/external/dr_wav.h` is an unmodified copy of mackron/dr_libs
`243e26ffa0` (2026-04-26). dr_libs fixed more malformed-file handling after it:
a short `fact` chunk, `fmt` chunk parsing, bounds and size checks on metadata
and `smpl` chunks against the file size, AIFF files with unusual bit depths,
an integer overflow in the AIFF block alignment, and sample conversion
errors. Levels may name WAV files, so the patch replaces the file hunk by hunk
with dr_libs `019e75ba8e` (2026-08-31, the last commit touching `dr_wav.h`):
the result equals that upstream file apart from a two-line marker in its
header comment. The public API and version (0.14.6) are unchanged. When
raylib vendors a newer dr_wav, drop these hunks.

These fixes retain the 6.2 public API, formats and playback behavior. Source
modifications are marked in the affected files. The session/audio regressions
exercise the real library, and Linux CI keeps leak detection enabled.

### Fixes now upstream

raylib 6.2 contains the fixes this project used to patch into 6.0, so those
patches are gone:

- `UnloadSoundAlias` frees the alias's converter cache
  ([raysan5/raylib#5857](https://github.com/raysan5/raylib/pull/5857)), and
  `UnloadMusicStream` frees the WAV decoder
  ([raysan5/raylib#5963](https://github.com/raysan5/raylib/pull/5963)); LeakSanitizer
  found both.
- `qoa.h` bounds a crafted `.qoa` file's channel count and frame size
  ([raysan5/raylib#6162](https://github.com/raysan5/raylib/pull/6162), for
  [#6161](https://github.com/raysan5/raylib/issues/6161)). The session test
  still feeds both crafted files to `LoadWaveFromMemory`.

6.2 also brings fixes the game relies on without a patch: `CloseAudioDevice`
stops the device before destroying its lock (#6039), window centring no longer
overflows on unsigned sizes (#6119), Windows borderless fullscreen is no longer
always on top and restores the window size when left (#5868, #6253), the
OpenGL ES default shaders use high precision (#6264), and the software
renderer used by the Memory tests clips scissor rectangles the right way up
and keeps triangle spans inside the framebuffer (#5976, #5849).

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

Upstream source and release: https://github.com/raysan5/raylib/releases/tag/6.2

Release packaging includes raylib's license, the GLFW license, and the
license-bearing external headers/source files from this pinned source tree.
The marked miniaudio fix leaves its license text intact, as do all other bundled
component notices; no license terms are extracted heuristically or rewritten.
