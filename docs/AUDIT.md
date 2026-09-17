# Documentation and Website Audit

Checked **2026-09-17**, against `origin/main` at `16be4a1` and the local
`feature/documentation-refresh` changes. This is a dated audit, not a live status
dashboard. The earlier `AUDIT_IMPLEMENTATION.md` remains the Sandbox School
delivery report.

## Coverage

- Root README, PRODUCT, DESIGN and third-party notices.
- `docs/README.md`, all **23** `docs/wiki/` Markdown pages and the earlier report.
- Astro routes, navigation metadata, shared layout, landing components, analytics,
  styles, configuration, robots/sitemap, package scripts and lockfile contract.
- Make targets, four GitHub workflows, CLI/input/settings/editor implementations,
  player API, active update/render ordering, serializer schema and generated facts.
- GitHub About, Pages configuration, recent main CI/deployment status and the
  actual latest published release asset list, retrieved through `gh`.

Source/workflows are authoritative for behavior. Historical design proposals are
explicitly labeled in PRODUCT/DESIGN and are not current acceptance criteria.
Generated catalog, inventory and overlay snapshots were already fresh.

## Findings and Updates

| Area | Finding | Resolution |
|---|---|---|
| Level examples | 25 placement declarations used singleton `[name]` instead of `[[name]]` | Corrected enemy/surface/collectible examples; all TOML fences now pass the shared strict-v1 schema check |
| Schema guidance | Misleading root/table scoping, required-field claims, climbable dimensions and nonexistent triple-jump advice | Corrected against parser, validator and player source |
| Controls | Missing settings, persistence, touch and newer CLI flags; stale PRNG and pause-Back descriptions | Complete controls/settings/profile/experiment reference, checked against `main.c` flags |
| Editor | Tool keys documented as S/P/D instead of 1/2/3; unsupported zoom/pan shortcuts; shared code described as editor-owned | Updated actual controls, profile-isolated F5 playtest, recovery and module boundaries |
| Player and architecture | Missing physical/replay input parameters; outdated update-order diagram and fixed-world-width claims | Updated public declarations, active-frame ordering and runtime world width |
| Audio/assets | Browser music described as avoiding RAM use; theme variants conflated with selected assets | Explained WASM preload cost and linked generated inventory/provenance |
| Overview | `wiki/index.md` was loaded but never rendered on `/docs/` | Published its overview/quick start and restored learning routes |
| Search metadata | Every page reused the game's description and VideoGame structured data | Manual descriptions come from `DOCS_META`; manual pages use TechArticle data and branded titles |
| Sitemap | `robots.txt` pointed to nonexistent `sitemap.xml`; live HTTP returned 404 | Pointed it to Astro's generated `sitemap-index.xml` |
| Maintenance | Sparse docs setup, npm-first examples, redundant gates, missing WASM preview assembly | Expanded docs README with runtime requirements, source ownership, generated files, routes, preview and deployment |
| CI/release docs | Missing dependency audit, Actions CodeQL, exact deploy source and main-only manual release restriction | Aligned README/build/testing/release guides with workflows |
| Downloads | Copy implied the latest published release had current builder bundles | Distinguished current packaging from historical releases and linked source builds for current editor/labs |
| GitHub About | Description/topics presented only a platformer | Updated description and added editor, C11, TOML and learning topics through `gh`; verified readback |

## Recurrence Checks

`make docs-drift` now parses manual TOML examples with the existing validator,
compares campaign examples to the manifest, checks documented CLI flags against
`main.c`, and compares the player API block to `player.h`.

After `bun run build`, `bun run check-site` verifies emitted routes and Markdown
content, internal links/assets/fragments, canonical/Open Graph URLs, unique page
descriptions and complete sitemap coverage. It also runs table-driven negative
probes for the source and published-site failures found here. Both Docs CI and
Pages deployment run this gate; the Docs path filter includes its inputs.

## Verification

Passed locally:

- `make docs-drift` (including roadmap quality and three generated-freshness gates).
- `make validate-levels`: **9 levels**, **88 source assets**.
- `make web-host-contract`: boot wiring, JS host/storage/touch and Python archive contracts.
- `bun run lint`: **0 errors, 0 warnings, 0 hints** across 13 Astro/TS files.
- `bun run build`: **24 static pages** (landing plus 23 manual routes).
- `bun run check-site`: emitted-site checks and **9 negative mutations** in two table-driven tests.
- `bun audit`: **no vulnerabilities found** in the restored dependency set.
- Task-local value review: **296 literal numeric constant rows** compared with
  source definitions; **56 level TOML snippets** checked for asset paths and nested
  dimensions, in addition to the shipped schema gate. Root documentation links checked.
- `git diff --check`.

Regression scan: 23 callers checked, 30 assertions checked, 2 flagged/fixed.

Counts cover selected template, script, workflow and documentation consumers,
plus 25 existing host and five docs-test assertion sites. Review tightened the
campaign-example comparison and accepted the valid slashless project-root URL
in the link checker; affected drift/site checks were rerun. Counts are inspection
records, not coverage percentages.

Remote evidence before publication of this task:

- Main `16be4a1`: [Build & Release](https://github.com/jonathanperis/super-mango-editor/actions/runs/35245127186),
  [CodeQL](https://github.com/jonathanperis/super-mango-editor/actions/runs/35245127144)
  and [Pages](https://github.com/jonathanperis/super-mango-editor/actions/runs/35245873104) succeeded.
- Landing, manual, controls, learning path, sitemap index/shard and game JavaScript
  returned HTTP 200. The old advertised `sitemap.xml` returned HTTP 404.
- Latest published release was [v1.0.801](https://github.com/jonathanperis/super-mango-editor/releases/tag/v1.0.801),
  dated 2026-05-17. Linux/macOS assets were bare game executables; the other assets
  were Windows and WASM ZIPs. This does not establish availability of current
  game-and-editor builder bundles.

No new native/WASM compilation or interactive browser/device test was needed for
these documentation/template changes. Host contracts were run because the game
host template changed. Visual layout, real game interaction and the future remote
CI run are not verified by the HTML checks. Existing local dependencies were
used without installation. About is updated remotely; source changes require
publication before the website receives them.

## Enhancement / Follow-up Plan

| Priority | Next action | Completion evidence |
|---|---|---|
| High | Publish the verified documentation changes through the normal repository workflow | Relevant Docs/Build/CodeQL checks and matching Pages deployment pass; live robots points to the working sitemap |
| High | Prepare a new release containing current builder archives when ready to ship | All four platform archives inspected; game and editor, campaign/labs and notices present; release links verified |
| Medium | Perform an explicitly authorized browser/device documentation smoke | Desktop/mobile navigation, filtering, overview, keyboard focus, play/debug launch and touch controls observed on real supported browsers/devices |
| Medium | Resolve existing font/audio provenance gaps from original acquisition records | Documented source, author and license evidence; notices updated without inventing permissions |
| Ongoing | Regenerate content facts when levels/assets/overlay text change; run both source and emitted-site checks | `make docs-drift`, Astro lint/build and `bun run check-site` pass for the same source revision |

Broad redesign/A-B proposals in DESIGN remain historical ideas. They require a
fresh product decision and interaction evidence rather than being treated as
unfinished parts of this documentation refresh.
