# Contributing

Short-lived branches off `main`. Name the branch with a topic prefix and a
short description (`docs/…`, `fix/…`, `refactor/…`, `test/…`, `tidy/…`,
`ci/…`, `perf/…`). Open a pull request whose title is one imperative
sentence in sentence case, with no `feat:` or `fix:` prefix. Merge with a
rebase merge, and only after the required checks below are green. Never
force-push `main`.

The manual lives in [`docs/wiki/`](docs/wiki/). Start with the
[developer guide](docs/wiki/developer-guide.md), the
[build system](docs/wiki/build-system.md) and
[testing](docs/wiki/testing.md). The
[release checklist](docs/wiki/release-checklist.md) is the list that stays
current for what `main` requires.

## Checks

Required before a rebase merge (the `main` ruleset, as recorded in the
release checklist):

- `Build (Linux x86_64)`, `Build (macOS arm64)`, `Build (Windows x86_64)`, `Build (WebAssembly)`
- `Sanitizers (Linux x86_64)`
- `Docs drift`
- `Analyze (c-cpp)`, `Analyze (actions)`, `Analyze (python)`, `Analyze (javascript-typescript)`, and `CodeQL`

Informational. Read them, and do not treat a red or noisy run as a merge
blocker unless the release checklist says otherwise:

- `Coverage (Linux x86_64)`, `clang-tidy (informational)`, the weekly fuzz
  workflow, `Windows x86_64 clang -Werror (rolling MSYS2 toolchain)`,
  `Desktop backend`, and `Docs lint and build` (that job only runs when the
  pull request touches the site)

## Building and testing

Headless, with no display and no audio device:

```sh
make test CC=clang OUTDIR=out/headless RAYLIB_PLATFORM=memory
```

`RAYLIB_PLATFORM=memory` is test-only. `release` and `dist-native` refuse
it. A desktop with a display but no playback device can use
`RAYLIB_AUDIO=null` the same way; that is test-only too.

## What not to change

`labs/debugging/` plants bugs on purpose (`coin_overflow`, `score_overflow`,
`texture_after_free`). They are the exercises in
[Debugging C](docs/wiki/debugging-c.md). Do not "fix" them.

When a comment, a wiki page, or this file disagrees with the code, the code
wins. Update the docs, and let `make docs-drift` catch the next stale claim.
Teaching comments (units, ownership, why a library call is there) stay; they
are part of the lesson.
