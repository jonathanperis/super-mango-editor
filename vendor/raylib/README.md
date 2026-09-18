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

These fixes retain the 6.0 public API, formats and playback behavior. Source
modifications are marked in the affected files. The session/audio regressions
exercise the real library, and Linux CI keeps leak detection enabled.

Upstream source and release: https://github.com/raysan5/raylib/releases/tag/6.0

Release packaging includes raylib's license, the GLFW license, and the
license-bearing external headers/source files from this pinned source tree.
The marked miniaudio fix leaves its license text intact, as do all other bundled
component notices; no license terms are extracted heuristically or rewritten.
