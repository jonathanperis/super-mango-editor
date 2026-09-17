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
| `bun run check-site` | Check built routes, Markdown content, links, anchors, metadata and sitemap |
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
| Manual content | `wiki/*.md`; `wiki/index.md` renders on `/docs/` |
| Page titles, descriptions, categories and order | `src/lib/docsSidebar.ts` |
| Manual layout / routes | `src/pages/docs/[...slug].astro` |
| SEO and shared page shell | `src/layouts/BaseLayout.astro` |
| Styles | `src/styles/globals.css`, `src/styles/docs.css` |
| Production origin/base | `astro.config.mjs`; keep `public/robots.txt` consistent |

Add a manual page in `wiki/`, register its ID/metadata/category in `docsSidebar.ts`,
then build and check it. Links in `wiki/` target **published routes**, not GitHub
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

`docs.yml` checks relevant pull requests (including source, level, root-doc and
workflow changes). It runs drift, frozen install, Astro lint, dependency audit,
build and built-site validation. It does not deploy.

`deploy.yml` runs after a successful same-repository main push/manual Build &
Release run. It checks out that run's exact commit, builds/checks the site, adds
the matching normal/debug WASM artifact, performs HTTP assembly smoke checks and
deploys `docs/out/` to Pages. Release tags are a separate publication path.

Analytics is optional; see `.env.example`. With no `PUBLIC_GA_ID`, analytics
scripts are omitted. Production builds map the repository secret
`NEXT_PUBLIC_GA_ID` to `PUBLIC_GA_ID`. Local environment files stay untracked.

The browser game uses the SDL2 companion ports shipped with the pinned Emscripten
SDK. Their versions can lag native packages; upgrading the SDK does not imply
every SDK-managed library matches the latest standalone release.

See [AUDIT.md](AUDIT.md) for the dated audit and follow-up plan.
`AUDIT_IMPLEMENTATION.md` is the historical Sandbox School delivery report.
