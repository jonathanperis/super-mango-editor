# Security Policy

## Reporting a vulnerability

Please report suspected vulnerabilities privately through GitHub's
[private vulnerability reporting](https://github.com/jonathanperis/super-mango-editor/security/advisories/new)
(repository **Security** tab, then **Report a vulnerability**). Do not open a
public issue for a security problem.

Include the affected version or commit, platform (Linux, macOS, Windows or
browser), steps or a minimal file that reproduces the problem, and the impact
you observed. This is a volunteer-maintained learning project: expect an
acknowledgement within about a week. Fixes land on `main` and ship in the next
release; there are no backported security releases.

## Supported versions

Only the latest release and the current `main` branch receive fixes.

## Scope

In scope:

- **TOML level and campaign parsing**: the vendored `tomlc17` parser, the
  serializer/loader in `src/shared/` and `src/levels/`, and level validation.
  Crashes, memory-safety bugs, hangs or unbounded allocation from a crafted
  level file are in scope.
- **Player profiles and replay/experiment files**: native profile files in the
  OS preference directory or passed with `--profile`, and replay/experiment
  inputs.
- **Level editor file handling and dialogs**: open/save/recent/recovery paths,
  playtest snapshots and the native file and message dialogs.
- **Web build**: the Emscripten shell (`web/`), the docs-site page that hosts
  the game, and browser storage handling.
- **Release and CI integrity**: release archives, `SHA256SUMS`, build
  provenance attestations and the GitHub Actions workflows.

Out of scope: vulnerabilities in third-party dependencies that are already
fixed upstream but not yet re-pinned (please still tell us), behavior that
requires an attacker who already controls the local user account, and the
CodeQL path-injection findings already triaged as intentional local-user file
selection in [`.github/SECURITY_TRIAGE.md`](.github/SECURITY_TRIAGE.md).

## Web build storage and the shared origin

The browser game is served from GitHub Pages at
`https://jonathanperis.github.io/super-mango-editor/`. Browser storage is
scoped to the *origin* (`https://jonathanperis.github.io`), which is shared
with every other GitHub Pages site of the same account. Any script running on
that origin can read or write the game's storage. Without a custom domain this
cannot be isolated, so the game treats stored values as untrusted input:

- Keys are prefixed: `super-mango-profile-v2` (with read-only fallback to
  `super-mango-profile-v1`) in `localStorage`, and the one-shot
  `super-mango-replay-level` boot intent in `sessionStorage`.
- Profiles hold only settings, key bindings and level progress; no credentials
  or personal data. The browser bridge rejects text containing NUL or larger
  than the fixed profile buffer, then `game_profile_decode()` parses it with
  the same strict rules as native profiles: unknown keys, wrong types,
  out-of-range settings, malformed progress entries and duplicate entries
  reject the whole profile. The game then runs with defaults for that session
  only and leaves the stored text untouched (it is never overwritten). One
  exception keeps older profiles usable: a well-formed progress entry (or
  `last_level`) whose level key fails today's level-reference rule, such as
  `levels/con.toml`, is dropped with a warning while the rest loads. The next
  save then writes the profile without it, through the same compare-and-swap.
- The replay intent is removed as soon as it is read. Both pages that host the
  game (the standalone `super-mango.html` shell and the docs-site home page,
  `docs/src/components/home/Dashboard.astro`) forward it as a `--level`
  argument only when it matches a bundled level path (`levels/NAME.toml` or
  `levels/labs/NAME.toml`). The path must also fit the fixed level-path buffer, can only name
  files inside the Emscripten in-memory filesystem (the preloaded `assets/`
  and `levels/`), and the selected file still goes through full TOML schema
  and level validation before use. It does not reach the host filesystem.
- Profile writes use a compare-and-swap against the profile read at startup
  under a Web Lock, so a conflicting write from another tab is refused rather
  than silently overwritten.

Both pages that run the game ship a Content-Security-Policy `<meta>` tag
with no remote scripts and no JavaScript `eval` (`'wasm-unsafe-eval'` allows
WebAssembly compilation only):

- The docs-site home page (`index.html`), which players normally use.
  `docs/integrations/home-csp.mjs` adds the policy after `astro build`, with
  the SHA-256 hash of each inline script (Astro's inlined module script and the
  game bootstrap). The game's `super-mango.js`, `.wasm` and `.data` load from
  `'self'`. Styles allow `'self'`, inline styles (the game injects its
  touch-control CSS); fonts come only from `'self'`. The site's fonts are
  self-hosted, so no page contacts a font CDN, and `tools/check_docs_site.py`
  fails the build if a page or stylesheet references Google Fonts.
  `object-src`, `base-uri` and `form-action` are `'none'`.
  `tools/check_docs_site.py` fails the docs build if a hash is missing, if
  `script-src` gains `'unsafe-inline'` or a remote host, or if the tag no longer
  precedes the scripts. Astro's built-in `security.csp` is not used because it
  applies to every page and pins `style-src` to hashes, which would block the
  game's injected styles.
- The standalone `super-mango.html` shell, which `tools/web_csp.py` pins after
  linking.

A `<meta>` policy cannot set `frame-ancestors`, and GitHub Pages cannot send
CSP headers. Google Analytics, when configured, loads on manual pages but not
on the home page that hosts the game. `'self'` covers the whole
`jonathanperis.github.io` origin, and pages on that shared origin can still
access the game's storage, which is why the validation above is the security
boundary.

## Verifying releases

Each release attaches `SHA256SUMS` and a GitHub build provenance attestation
for every archive. See the
[release checklist](docs/wiki/release-checklist.md#verifying-a-download) for
the verification commands and the macOS Gatekeeper note.
