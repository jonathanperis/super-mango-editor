# Release Checklist

<a id="home"></a>

Use this checklist before publishing a tagged or manually dispatched release. Run commands from the repository root unless noted.

## 1. Source and Content Gates

```sh
make docs-drift
make test CC=clang
make validate-levels
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy make smoke CC=clang SMOKE_FRAMES=5 SMOKE_SEED=1
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy make scripted-smoke CC=clang SMOKE_FRAMES=5 SMOKE_SEEDS="1 7 23"
```

Optional local hardening when the platform has sanitizer support:

```sh
make sanitize CC=clang
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy make sanitize-smoke CC=clang SMOKE_FRAMES=5 SMOKE_SEED=1
```

## 2. Documentation Gates

```sh
cd docs
bun run lint
bun run build
```

Confirm the release copy still points players to the current GitHub release page:

- `README.md` release section
- docs build/deploy notes
- `https://github.com/jonathanperis/super-mango-editor/releases/latest`

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

If a local/container Emscripten install fails inside SDL_ttf/HarfBuzz before compiling Super Mango code (for example Emscripten 3.1.58 plus newer Clang `-Wnontrivial-memcall`), do not block the release on that host-specific cache/toolchain failure. Confirm the latest GitHub Actions WebAssembly build and Pages WASM smoke are green instead.

## 4. Native Release Archive Gates

For each native platform produced by CI or a matching local runner:

```sh
RELEASE_PLATFORM=super-mango-native make dist-native
```

Inspect the zip and confirm it contains:

- `super-mango` executable, or `super-mango.exe` on Windows
- `assets/`
- `levels/`
- `README.txt`
- `LICENSE`
- Windows only: SDL2 runtime DLLs

## 5. CI and Publishing Gates

Before creating a release, confirm the latest `main` branch checks are green:

- Build & Release
- CodeQL
- Docs
- Deploy to GitHub Pages

Publishing rules:

- push a `v*` tag to create a tagged GitHub Release, or
- use `workflow_dispatch` for a manually versioned release.

Normal `main` pushes are build/deploy checks only; they do not publish a GitHub Release.

After publish, verify:

- the release asset table has Linux, macOS, Windows, and WebAssembly zip files;
- archive downloads match the expected platform names;
- Pages serves `/super-mango.js`, `/super-mango.wasm`, and `/super-mango.data`;
- the public docs site and badges do not report stale status.
