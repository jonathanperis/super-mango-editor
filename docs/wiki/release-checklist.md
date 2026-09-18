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

The authoritative WebAssembly gate is GitHub CI: the `build.yml` WebAssembly job runs `make web`, verifies artifacts with `tools/check_wasm_artifacts.py`, packages `super-mango-wasm.zip`, and `deploy.yml` smokes the unpacked Pages payload before publishing. Use local Emscripten only as an optional preflight when that host toolchain is healthy.

Optional local preflight:

```sh
make web
python3 tools/check_wasm_artifacts.py
RELEASE_PLATFORM=super-mango-wasm make dist-wasm
python3 tools/check_wasm_artifacts.py --zip dist/super-mango-wasm.zip
```

The verifier checks that `out/super-mango.html`, `.js`, `.wasm`, and `.data` exist, that the generated JavaScript references the expected asset basenames, that Node accepts the generated JavaScript syntax, and that `WebAssembly.compile` can compile the generated `.wasm` binary. When a zip exists, it also verifies the WebAssembly release archive includes HTML, JS, WASM, data, `README.txt`, and `LICENSE`.

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

- Build & Release
- CodeQL
- Docs (relevant PR check or an explicit manual run; it does not run on main pushes)
- Deploy to GitHub Pages

Publishing rules:

- push a `v*` tag to create a tagged GitHub Release, or
- use `workflow_dispatch` **on `main`** for a manually versioned release.

The release job creates a draft, uploads all four archives without overwriting
existing assets, then publishes it. A dispatch on another branch builds/checks
but does not publish. Verify the release event's own build matrix, not an older
green run.

Normal `main` pushes are build/deploy checks only; they do not publish a GitHub Release.

After publish, verify:

- the release asset table has Linux, macOS, Windows, and WebAssembly zip files;
- archive downloads match the expected platform names;
- Pages serves `/super-mango-editor/super-mango.js`, `/super-mango-editor/super-mango.wasm`, and `/super-mango-editor/super-mango.data` under the production project base;
- the public docs site and badges do not report stale status.
