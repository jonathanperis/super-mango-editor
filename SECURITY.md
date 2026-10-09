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

The latest published release can lag well behind `main`. At the time of
writing it is `v1.0.845` (2026-09-17), which predates the move from SDL2 to
raylib and the editor/lab builder archives, so much of the code in `main`
(and in this policy's scope) has not shipped in any release yet. A fix to that
code reaches players through the website, which follows `main`, and through
the next release. Check the
[releases page](https://github.com/jonathanperis/super-mango-editor/releases)
for the current latest release.

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
  `super-mango-profile-v1`) and one `super-mango-ghost-v1:<level path>` entry
  per level with a time-trial ghost, all in `localStorage`. A ghost holds only
  positions and sprite frames; the browser bridge rejects text with NUL or over
  256 KB, `game_ghost_decode()` rejects any malformed field or sample count, and a
  ghost whose level hash does not match the loaded level is ignored. A full or
  denied storage only means the ghost is not saved. The game no longer uses
  `sessionStorage`: Replay restarts the level inside the running page, so the
  host pages read no boot intent (`tools/check_web_boot_contract.py` fails if
  one reappears).
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

Releases built by the current workflow attach `SHA256SUMS` and a GitHub
build provenance attestation for every archive. The workflow started
publishing them on 2026-10-06, so earlier releases, including `v1.0.845`
(2026-09-17), have neither: their archives can only be traced to the workflow
run that built them. See the
[release checklist](docs/wiki/release-checklist.md#verifying-a-download) for
the verification commands and the macOS Gatekeeper note.
