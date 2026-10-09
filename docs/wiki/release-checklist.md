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

The verifier checks that the normal and debug `out/super-mango*.html`, `.js`, `.wasm`, and `.data` files exist, that the generated JavaScript references the expected asset basenames and does not depend on `emscripten_sleep`, that each HTML shell's pinned Content-Security-Policy matches its inline boot script, that Node accepts the generated JavaScript syntax, and that `WebAssembly.compile` can compile the generated `.wasm` binary. With `--zip`, it also verifies the WebAssembly release archive includes those eight files, `README.txt`, `LICENSE`, `THIRD_PARTY_NOTICES.md` and `licenses/tomlc17.txt`.

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

- Build & Release, including the non-gating `Windows x86_64 clang -Werror (rolling MSYS2 toolchain)`, `Desktop backend (Windows x86_64 Mesa)` and `Coverage (Linux x86_64)` jobs
- CodeQL (`Analyze (c-cpp)`, `Analyze (actions)`, `Analyze (python)`, `Analyze (javascript-typescript)`)
- Docs (runs on every `main` push; PRs run it only for docs-related paths, while Build & Release's `Docs drift` job always runs)
- Pages build and Pages deploy (main pushes and manual runs on main)

> **Required checks:** as of 2026-10-07 the `main` branch ruleset requires
> `Build (Linux x86_64)`, `Build (macOS arm64)`, `Build (Windows x86_64)`,
> `Build (WebAssembly)`, `Sanitizers (Linux x86_64)`, `Docs drift`,
> `Analyze (c-cpp)`, `Analyze (actions)`, `Analyze (python)`,
> `Analyze (javascript-typescript)` and `CodeQL`, and blocks merges on
> high-or-higher CodeQL security alerts. The Windows clang and `Desktop
> backend` jobs and `Docs lint and build` (which only runs for docs-related
> pull requests) are not required; check that they are green before a release.

Publishing rules:

- push a `v*` tag to create a tagged GitHub Release, or
- use `workflow_dispatch` **on `main`** for a manually versioned release.

A separate `Checksums and provenance` job (the only one holding
`attestations: write` and `id-token: write`) waits for the same gates as the
release (`build` and `Sanitizers (Linux x86_64)`), so no attestation is minted
for archives a failing gate keeps unpublished. It writes `SHA256SUMS` for the four
archives and creates a GitHub build provenance attestation for each. The
release job re-checks the archives against `SHA256SUMS`, creates a draft,
uploads the four archives plus `SHA256SUMS` without overwriting existing
assets, then publishes it. A dispatch on another branch builds/checks
but does not publish. Verify the release event's own build matrix, not an older
green run.

Normal `main` pushes are build/deploy checks only; they do not publish a GitHub Release.

After publish, verify:

- the release asset table has Linux, macOS, Windows, and WebAssembly zip files plus `SHA256SUMS`;
- `gh attestation verify` succeeds for a downloaded archive (see below);
- archive downloads match the expected platform names;
- Pages serves `/super-mango-editor/super-mango.js`, `/super-mango-editor/super-mango.wasm`, and `/super-mango-editor/super-mango.data` under the production project base;
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
