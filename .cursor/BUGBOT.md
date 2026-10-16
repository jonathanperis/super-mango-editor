# Bugbot review rules

Report real bugs, security issues and regressions. Skip style nitpicks
and preference-only suggestions.

## Generated and vendored files

Do not comment on these paths; they are generated, vendored or lockfiles:

- `**/dist/**`
- `**/out/**`
- `**/out-sanitize/**`
- `docs/node_modules/**`
- `docs/bun.lock`
- `docs/src/generated/**`
- `vendor/tomlc17/**`
- `**/*.min.js`

## Path-specific rules

### `src/**/*.{c,h}`

C11 game and level editor built on pinned raylib 6.0. Check memory
safety first: bounds on every array/string write (snprintf results,
strlen vs buffer size, fixed MAX_* capacities), integer overflow in
size math, NULL checks after allocation, ownership/lifetime of
textures, sounds and heap data on every error path, and use of data
loaded from TOML levels, profiles, replay scripts or dialogs before
validation. Keep `-std=c11 -Wall -Wextra -Wpedantic` warning-free.
This is a learning codebase: comments should explain why for a reader
new to C and game programming; flag removed or misleading teaching
comments and needlessly clever code. Keep `__EMSCRIPTEN__` paths free
of JavaScript eval (the web shell's CSP forbids it; use EM_JS).

### `tests/**`

Native regression tests and host checks. Flag assertions that cannot
fail, tests depending on wall-clock timing or host state, and fixtures
written outside out/.

### `tools/**/*.py`

Build, packaging, validation and docs-drift tooling run in CI on
Linux, macOS and Windows (MSYS2 Python). Flag bare except clauses,
unverified downloads (pins must stay SHA-256 checked), shell=True or
unquoted subprocess arguments, path handling that breaks on Windows,
and checks that silently pass when an input is missing.

### `web/**`

Emscripten shell and host scripts shared by the standalone page and
the docs site. Keep the Content-Security-Policy in shell.html
same-origin only (no remote origins, no 'unsafe-eval'); inline boot
script hashes are pinned by tools/web_csp.py. Treat sessionStorage
and localStorage values as untrusted input and keep keys prefixed
`super-mango-`.

### `docs/**`

Astro docs site and the wiki manual. Claims must match the current
source, Makefile and workflows (`make docs-drift` checks many of them);
flag stale SDL-era wording, broken internal links, and third-party
scripts added to the page that hosts the game.

### `.github/workflows/**`

Every action must be pinned to a full commit SHA with a version
comment. Keep least-privilege permissions per job (deploy/attestation
credentials only in jobs that run no third-party build code). Flag
user-controlled input interpolated directly in run: blocks (workflow
injection risk); recommend env: + quoting. Every job needs
timeout-minutes.
