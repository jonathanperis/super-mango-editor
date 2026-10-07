# Website and Builder Manual

Astro static site at <https://jonathanperis.github.io/super-mango-editor/>.
The landing page hosts the game; `/docs/` publishes the manual from `wiki/`.

## Requirements and Commands

Use Node.js **22.12+**, Bun and Python **3.11+**. CI pins Node **26.9.0** and Bun
**1.4.2**. Restore dependencies with `bun install --frozen-lockfile` in this directory.

The supported frontend versions are Astro **7.3.3**, `@astrojs/markdown-satteri`
**0.4.1**, `@astrojs/sitemap` **3.7.4**, `@astrojs/check` **0.9.10**, Tailwind CSS
and its Vite plugin **4.3.3**, and TypeScript **6.0.3**. TypeScript **7.0.2** is
newer, but Astro Check 0.9.10 requires `^5.0.0 || ^6.0.0`; retain 6.0.3 until the
checker supports 7.x. Keep peer constraints enforced. After dependency updates,
run frozen install, `bun audit`, lint, build and `check-site`.

| Command (from `docs/`) | Action |
|---|---|
| `bun run dev` | Start development server; routes have no repository base prefix |
| `bun run lint` | Run `astro check` |
| `bun run drift` | Run `make docs-drift`, including generated freshness and roadmap checks |
| `bun run build` | Build the production site to `docs/out/` with `/super-mango-editor/` base |
| `bun run check-site` | Check built routes, Markdown content, links, anchors, metadata, sitemap and the landing-page CSP |
| `bun run preview` | Serve the production build; open `/super-mango-editor/` |

For a documentation change, from the repository root:

```sh
make docs-drift
cd docs
bun run lint
bun run build
bun run check-site
```

`make docs-drift` already includes `make roadmap-quality`'s checks. When Bun is
unavailable and dependencies are already restored, `npm run lint`, `npm run build`
and `npm run check-site` run the same scripts. Keep `bun.lock` as the install
contract; do not generate an npm lockfile.

## Where to Edit

| Surface | Source |
|---|---|
| Landing copy / cabinet host | `src/components/home/`, `src/pages/index.astro` |
| Manual content | `wiki/*.md`, loaded as the `docs` content collection (`src/content.config.ts`); `wiki/index.md` renders on `/docs/` |
| Page titles, descriptions, categories and order | `src/lib/docsSidebar.ts` (optional `title`/`description` frontmatter overrides) |
| Manual layout / routes | `src/pages/docs/[...slug].astro` (`getCollection('docs')` + `render()`) |
| SEO and shared page shell | `src/layouts/BaseLayout.astro` |
| Styles | `src/styles/globals.css`, `src/styles/docs.css` |
| Production origin/base | `astro.config.mjs`; keep `public/robots.txt` consistent |
| Landing-page Content-Security-Policy | `integrations/home-csp.mjs` (hashes inline scripts after build; see `SECURITY.md`) |

Add a manual page in `wiki/`, register its ID/metadata/category in `docsSidebar.ts`,
then build and check it. The collection's glob loader picks up every `wiki/*.md`;
the build fails if a listed page has no file or a file is missing from
`SECTION_ORDER`, and frontmatter accepts only `title` and `description`. Links in `wiki/` target **published routes**, not GitHub
Markdown paths: use `../controls/` from a nested page and `controls/` from the
overview. Include fragment IDs only when the target heading exists.

### Generated Content

Run these from the repository root after changing their inputs:

| Command | Outputs |
|---|---|
| `make content-inventory` | `wiki/asset-inventory.md`, `src/generated/project.json` |
| `make level-catalog` | `wiki/level-catalog.md` from the campaign manifest |
| `make overlay-snapshots` | `wiki/overlay-snapshots.md` from overlay strings |

Do not hand-edit generated counts. `make docs-drift` checks freshness, TOML
example schema, public player declarations, CLI coverage and semantic references.
Astro compilation alone does not check links; `check-site` validates emitted HTML.

The manual and source comments are teaching material. Preserve the explanations
of ownership, units and frame order when updating library calls. Review examples
against their actual declarations and callers; do not only replace backend names.
`wiki/learning-path.md` gives the beginner's reading order and
`wiki/developer-guide.md` records the deliberately readable code style.

## Local Game Preview

Astro does **not** compile or copy WebAssembly. A docs-only preview renders the
site, but game startup needs the game artifacts. With Emscripten **6.0.9**,
build and assemble from the repository root:

```sh
make web
cd docs
bun run build
cd ..
cp out/super-mango{,-debug}.{html,js,wasm,data} docs/out/
cd docs
bun run preview
```

Reassemble after rebuilding Astro, which replaces its output. Touch-control code
is bundled into the generated game JavaScript. See the public
[Build System](https://jonathanperis.github.io/super-mango-editor/docs/build-system/)
for the CI-authoritative WebAssembly verification contract.

## Deployment and Analytics

`docs.yml` checks every `main` push and relevant pull requests (including
source, level, root-doc and workflow changes). It runs drift, frozen install, Astro lint, dependency audit,
build and built-site validation. It does not deploy.

On a main push or manual run on main, Build & Release ends with two Pages jobs.
`pages-build` checks out that run's commit, builds/checks the site with
read-only permissions, adds the same run's normal/debug WASM artifact and
performs HTTP assembly smoke checks; `pages-deploy`, the only job with Pages
and OIDC permissions, deploys `docs/out/`. Release tags are a separate publication path.

A passing documentation PR checks the proposed manual, not the deployed site.
Pages continues to serve the previous successful `main` revision until the PR
is merged and the matching build/deploy finishes. Keep website prose and WASM
from the same revision when verifying a migration.

Analytics is optional; see `.env.example`. With no `PUBLIC_GA_ID`, analytics
scripts are omitted. Production builds map the repository secret
`NEXT_PUBLIC_GA_ID` (a legacy Next.js-era name; renaming means creating a new
repository secret and updating the `pages-build` job in `build.yml`) to `PUBLIC_GA_ID`. Local
environment files stay untracked. Analytics loads only on manual pages
(`pageType="docs"`), never on the home page that hosts the game, so the
third-party script does not share a document with the running game. The
origin is still shared; see the repository `SECURITY.md`.

The browser game builds the same checksum-pinned raylib source as native builds,
using the pinned Emscripten SDK's Web/GLFW backend. `web/keyboard-scope.js` scopes
that SDK's keyboard handlers to the canvas; revisit its integration when changing
the SDK. Host contract tests do not replace real-browser/device verification.

See [AUDIT.md](AUDIT.md) for the dated audit and follow-up plan.
`AUDIT_IMPLEMENTATION.md` is the historical Sandbox School delivery report.
