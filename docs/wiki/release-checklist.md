# Release Checklist

<a id="home"></a>

Use this checklist before publishing a tagged or manually dispatched release. Run commands from the repository root unless noted.

## 1. Source and Content Gates

```sh
make docs-drift
make test CC=clang
make validate-levels
make smoke CC=clang SMOKE_FRAMES=5 SMOKE_SEED=1
make scripted-smoke CC=clang SMOKE_FRAMES=5 SMOKE_SEEDS="1 7 23"
```

Optional local hardening when the platform has sanitizer support:

```sh
make sanitize CC=clang
make sanitize-smoke CC=clang SMOKE_FRAMES=5 SMOKE_SEED=1
```

## 2. Documentation Gates

```sh
cd docs
bun run lint
bun run build
bun run check-site
```

Confirm the release copy still points players to the current GitHub release page:

- `README.md` release section
- docs build/deploy notes
- `https://github.com/jonathanperis/super-mango-editor/releases/latest`

Inspect the actual release assets, not just the workflow definition. Older
published releases may predate builder archives; avoid promising the current
editor/labs until a release containing them is published. Pages tracks successful
main builds independently of releases.

## 3. WebAssembly Gates

The authoritative WebAssembly gate is GitHub CI: the `build.yml` WebAssembly job runs `make web`, verifies artifacts with `tools/check_wasm_artifacts.py`, packages `super-mango-wasm.zip`, and the `pages-build` job in the same workflow smokes the unpacked Pages payload before `pages-deploy` publishes it. Use local Emscripten only as an optional preflight when that host toolchain is healthy.

Optional local preflight:

```sh
make web
python3 tools/check_wasm_artifacts.py
RELEASE_PLATFORM=super-mango-wasm make dist-wasm
python3 tools/check_wasm_artifacts.py --zip dist/super-mango-wasm.zip
```

The verifier checks that the normal and debug `out/super-mango*.html`, `.js`, `.wasm`, and `.data` files exist, that the generated JavaScript references the expected asset basenames and does not depend on `emscripten_sleep`, that each HTML shell's pinned Content-Security-Policy matches its inline boot script, that each page names its game files with one version (the script tag, both preload links and the boot script's `assetQuery` carry the same `?v=<commit>`), that Node accepts the generated JavaScript syntax, and that `WebAssembly.compile` can compile the generated `.wasm` binary. With `--zip`, it also verifies the WebAssembly release archive includes those eight files, `README.txt`, `LICENSE`, `THIRD_PARTY_NOTICES.md` and `licenses/tomlc17.txt`.

Report local SDK/cache failures separately from application failures and require green CI WebAssembly/artifact/Pages checks. Native rendered checks need graphics and audio services; Linux CI supplies Xvfb/Mesa and a PulseAudio null sink. A missing desktop device is an explicit verification gap, not a passing test.

## 4. Native Release Archive Gates

For each native platform produced by CI or a matching local runner:

```sh
RELEASE_PLATFORM=super-mango-native make dist-native
```

Inspect the zip and confirm it contains:

- `super-mango` executable, or `super-mango.exe` on Windows
- `super-mango-editor` executable, or `super-mango-editor.exe` on Windows
- playable `assets/` (no `unused/` reserve files)
- `levels/`, including campaign manifest and learning labs
- `README.txt`
- `LICENSE`
- `THIRD_PARTY_NOTICES.md` and `licenses/tomlc17.txt`
- `licenses/raylib.txt`, `licenses/raylib-dependencies/glfw.txt` and the bundled
  components' license-bearing source/header files, including marked source fixes
- Windows only: runtime DLLs and available package notices under `licenses/msys2/`

## 5. CI and Publishing Gates

Before creating a release, confirm checks for the intended source commit are green:

- Build & Release, including the non-gating `Windows x86_64 extras (clang -Werror, Mesa desktop backend)` and `Coverage (Linux x86_64)` jobs
- CodeQL (`Analyze (c-cpp)`, `Analyze (actions)`, `Analyze (python)`, `Analyze (javascript-typescript)`)
- Docs (runs on pull requests that touch docs-related paths, weekly and on demand, never on `main` pushes; Build & Release's `Docs drift` job always runs, and on `main` its `Pages build` job lints, builds and checks the site)
- Pages build, Pages deploy and Live site check (main pushes and manual runs on main; the live check loads the published game in headless Chromium and fails on console errors, see [Testing](../testing/))

> **Required checks:** as of 2026-10-07 the `main` branch ruleset requires
> `Build (Linux x86_64)`, `Build (macOS arm64)`, `Build (Windows x86_64)`,
> `Build (WebAssembly)`, `Sanitizers (Linux x86_64)`, `Docs drift`,
> `Analyze (c-cpp)`, `Analyze (actions)`, `Analyze (python)`,
> `Analyze (javascript-typescript)` and `CodeQL`, and blocks merges on
> high-or-higher CodeQL security alerts. `Windows x86_64 extras (clang
> -Werror, Mesa desktop backend)`, `Coverage (Linux x86_64)`, `Live site
> check` and `Docs lint and build` (which only runs for docs-related pull
> requests) are not required; check that they are green before a release.

Publishing rules:

The version is [semver](https://semver.org/), recorded in `VERSION` at the
repository root (`MAJOR.MINOR.PATCH`, no `v` prefix). There is no version
string in the game or editor binaries; `VERSION` is the source of truth, and
`make docs-drift` checks that `build.yml` reads it.

`v1.0.845` was the last tag taken from a workflow run number. Tags after that
through `v1.0.994` were `v1.0.<commit count on main>`. That count is not a
version, and the series stops there. An older `v1.1.0` tag points at pre-rewrite
history and has no GitHub Release; leave it. From **2.0.0** onward:

- a routine release bumps the patch, so the line stays `2.0.x` (`2.0.1`, then
  `2.0.2`, and so on);
- bump the minor for a compatible set of player-visible changes worth calling
  out on their own;
- bump the major for an incompatible change.

To publish:

1. Set `VERSION` to the new number and merge that commit to `main` once its
   checks are green. Do not reuse a number that already has a `v*` tag.
2. Either push the tag `v` plus the contents of `VERSION` (the first tag
   under this rule is `v2.0.0`) onto that exact commit, or run
   `workflow_dispatch` **on `main`**.
   A dispatch reads `VERSION` and creates that tag. The release job refuses a
   pushed tag that does not match `VERSION`, and it refuses a dispatch when
   that tag already exists.
3. The GitHub Release is named `Super Mango` plus the tag. After it publishes,
   prepend a short summary of what changed since the previous release to the
   generated download and verification notes. The workflow writes those notes
   itself; the summary is the part a player reads first.

The release name follows the tag, so a `VERSION` of `2.0.0` publishes as
`Super Mango v2.0.0`.

A separate `Checksums and provenance` job (the only one holding
`attestations: write` and `id-token: write`) waits for the same gates as the
release (`build` and `Sanitizers (Linux x86_64)`), so no attestation is minted
for archives a failing gate keeps unpublished. It writes `SHA256SUMS` for the four
archives and creates a GitHub build provenance attestation for each. The
release job re-checks the archives against `SHA256SUMS`, creates a draft
release holding the four archives plus `SHA256SUMS` with the GitHub CLI
(`gh release create --draft`), then publishes it. It runs no third-party
action, and `gh release create` refuses a tag that already has a release, so
existing assets are never overwritten; after a failed run, delete the leftover
draft before dispatching again. A dispatch on another branch builds/checks
but does not publish. Verify the release event's own build matrix, not an older
green run.

Normal `main` pushes are build/deploy checks only; they do not publish a GitHub Release.

<a id="release-cadence"></a>

### Release cadence

Pages always shows the latest `main`, but the downloadable archives only
change when someone publishes a release, so they quietly fall behind. Aim to
cut a release:

- after any player-visible change (new level, mechanic, editor feature or
  control change) once it is green on `main`;
- after any security fix, as soon as its checks pass (`SECURITY.md` promises
  fixes in the latest release);
- otherwise at least every 50 commits on `main`.

On every `main` push a step of the `Docs drift` job (it used to be a separate
`Release freshness` job) counts the commits since the
latest `v*` tag that `main` contains and raises a warning annotation (it never
fails the run) once that is more than 50. Treat the warning as a prompt to
work through this checklist, not as something to silence.

After publish, verify:

- the release asset table has Linux, macOS, Windows, and WebAssembly zip files plus `SHA256SUMS`;
- `gh attestation verify` succeeds for a downloaded archive (see below);
- archive downloads match the expected platform names;
- Pages serves `/super-mango.js`, `/super-mango.wasm`, and `/super-mango.data` at the root of `https://super-mango-editor.jonathanperis.tech/`;
- the public docs site and badges do not report stale status.

<a id="verifying-a-download"></a>

## 6. Verifying a Download

Players and maintainers can check any downloaded archive against the release's
`SHA256SUMS` and its build provenance attestation:

```sh
# Linux (GNU coreutils)
sha256sum --check --ignore-missing SHA256SUMS
# macOS
shasum -a 256 --check --ignore-missing SHA256SUMS
# Windows PowerShell: compare with the matching SHA256SUMS line
Get-FileHash -Algorithm SHA256 super-mango-windows-x86_64.zip

# Any platform with the GitHub CLI: proves the archive was built by this
# repository's Build & Release workflow from the tagged commit
gh attestation verify super-mango-macos-arm64.zip --repo jonathanperis/super-mango-editor
```

macOS archives are ad-hoc signed (`codesign --sign -`), not signed with a
Developer ID or notarized, because the project has no Apple Developer account.
Gatekeeper therefore blocks the first launch of a downloaded copy. The old
right-click (Control-click) **Open** bypass no longer works on macOS 15
(Sequoia) and later. First verify `SHA256SUMS` or the attestation as shown
above, then either:

- try to launch `super-mango` (or `super-mango-editor`) once, open **System
  Settings › Privacy & Security**, and click **Open Anyway** next to the
  blocked-app message (macOS asks for an administrator password); or
- remove the quarantine attribute from the extracted folder:

```sh
xattr -dr com.apple.quarantine super-mango-macos-arm64
```

Fixing this properly requires a Developer ID certificate plus notarization in
the release job; until then, document the workaround in release notes.

<a id="hands-on-checks"></a>

## 7. Hands-on Checks CI Cannot Do

CI builds and tests Windows and macOS, but its runners cannot show what a
player sees: hosted macOS runners have no OpenGL, and Windows runners have no
audio device and a small virtual screen. Before a release, someone with the
machine runs the downloaded archive (not a local build) and ticks these off.
Until they do, Windows-only behaviour and a real macOS window are unverified.

**Windows (x86_64 archive)**

- [ ] Extract the zip and start `super-mango.exe`. SmartScreen shows "Windows
      protected your PC" for an unsigned download; **More info › Run anyway**
      starts it, and no other warning or missing-DLL error appears.
- [ ] Music and sound effects play, and still play after unplugging and
      reconnecting headphones.
- [ ] Resize and maximise the window, then turn on fullscreen in Settings
      (borderless windowed): the game scales without stretching, and the
      frame rate holds.
- [ ] A gamepad (Xbox or compatible) moves, jumps and opens the pause menu;
      unplugging it mid-level does not crash.
- [ ] Start `super-mango-editor.exe`, open a level with the file dialog (a
      PowerShell `OpenFileDialog`; it takes a moment to appear), change
      an entity, save it under a new name with the save dialog (including a
      folder with spaces and non-ASCII letters), and play-test it.
- [ ] Quit from the menu and with the window's close button; both exit
      without an error dialog.

**macOS (arm64 archive, a real window)**

- [ ] First launch is blocked by Gatekeeper as described in
      [Verifying a Download](#verifying-a-download); after **Open Anyway** (or
      removing the quarantine attribute) it starts.
- [ ] On a Retina display the pixel art is sharp (no blur from 2x scaling) and
      the HUD sits inside the window.
- [ ] Drag the window to a second display with a different refresh rate (for
      example 60 Hz and 120 Hz): the frame cap follows the new display and the
      game speed does not change.
- [ ] Music and sound effects play, and continue after switching the output
      device in Control Centre.
- [ ] The editor's open and save dialogs work, and Cmd+Q quits both programs.

Record the date, OS version and archive version in the release notes, with
anything that failed.
