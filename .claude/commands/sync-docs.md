---
description: "Audit the codebase and update all documentation to match the current state of the code."
---

# Sync Documentation — Super Mango Game

Scan the entire codebase and verify that all documentation accurately reflects the current state. Code is always the source of truth. Fix any drift found.

## Documentation Scope

This project's documentation lives in these locations:

| File / Directory | Purpose |
|------------------|---------|
| `README.md` | Public-facing project overview — description, screenshots, build instructions, credits |
| `CLAUDE.md` | Primary project guide — tech stack, build commands, structure, architecture, constants, guidelines |
| `.claude/references/entity-template.md` | Template for adding new entities |
| `.claude/references/coding-standards.md` | Comment style and coding conventions |
| `.claude/references/animation-sequences.md` | Sprite animation state machines and timing |
| `.claude/references/sprite-sheet-analysis.md` | Sprite sheet measurement and analysis guide |
| `.claude/commands/*.md` | Slash command definitions |
| `.specs/` | Feature specs, design docs, task breakdowns |
| `docs/index.html` | GitHub Pages landing page — game description, WebAssembly player, download links |
| `docs/docs/index.html` | GitHub Pages documentation site — mirrors wiki content as a single-page HTML reference |
| **Wiki repo** | `github.com/jonathanperis/super-mango-game.wiki.git` — detailed documentation pages |

## Steps

### 1. Gather current code state

Collect ground truth from the source code:

- **Project structure:** `find src/ -type f -name '*.c' -o -name '*.h' | sort` — all source files
- **Entity inventory:** scan `src/entities/`, `src/hazards/`, `src/collectibles/`, `src/surfaces/`, `src/effects/` for all `.h` files
- **GameState fields:** read `src/game.h` — all struct fields, constants, and `#define` values
- **LevelDef fields:** read `src/levels/level.h` — all placement structs and level config
- **Build system:** read `Makefile` — all targets (`make`, `make run`, `make editor`, `make run-editor`, `make run-level`, `make web`, `make clean`)
- **Assets:** `find assets/ -type f | sort` — all sprites, sounds, and fonts organized in categorized folders:
  - `assets/sprites/backgrounds/` — parallax background layers
  - `assets/sprites/foregrounds/` — fog and water foreground layers
  - `assets/sprites/collectibles/` — coins, stars
  - `assets/sprites/entities/` — enemies (spiders, birds, fish)
  - `assets/sprites/hazards/` — spikes, saws, flames
  - `assets/sprites/levels/` — floor tilesets, platform tilesets
  - `assets/sprites/player/` — player sprite
  - `assets/sprites/screens/` — HUD, start menu logo
  - `assets/sprites/surfaces/` — bouncepads, bridges, vines, ladders, ropes, rails
  - `assets/fonts/` — TTF fonts
  - `assets/sounds/collectibles/` — coin pickup
  - `assets/sounds/entities/` — enemy sounds
  - `assets/sounds/hazards/` — hazard sounds
  - `assets/sounds/levels/` — background music, ambient sounds
  - `assets/sounds/player/` — jump, hit
  - `assets/sounds/screens/` — UI confirm
  - `assets/sounds/surfaces/` — bouncepad
- **Level files:** `ls levels/*.toml` — TOML level data files
- **Editor:** scan `src/editor/` for all modules (editor, canvas, palette, properties, tools, ui, undo, serializer, exporter, file_dialog)
- **Vendor:** `ls vendor/` — tomlc17 (TOML parser)

### 2. Cross-reference CLAUDE.md against code

For each section in `CLAUDE.md`, verify:

| Section | Check against |
|---------|--------------|
| Tech Stack table | Actual includes and Makefile libs (tomlc17 replaced cJSON) |
| Build Commands | Makefile targets (make, make run, make editor, make run-editor, make run-level LEVEL=, make web, make clean) |
| Project Structure tree | Actual `src/` directory layout — includes `src/editor/` with 11 modules |
| Module responsibilities table | All subdirectories including editor modules |
| Architecture diagram | `main.c` → `game_init` → `game_loop` → `game_cleanup` call chain |
| Key Constants table | `#define` values in `game.h` (FLOOR_GAP_W, MAX_FLOOR_GAPS, etc.) |
| Current Game State bullets | Entity counts (8 enemy types, 7+ hazard types, 4 collectible types, 10+ surface types), editor features, TOML levels |
| Asset organization | Categorized folder structure under `assets/sprites/` and `assets/sounds/` |
| Level format | TOML-based (not JSON) — verify serializer references |
| Editor features | Standalone level editor with palette, properties, canvas, undo/redo |

Track every discrepancy: wrong values, missing entities, outdated counts, stale file paths, removed features, renamed modules.

### 3. Cross-reference .claude/references/ against code

For each reference document:

- **entity-template.md**: Verify the template matches the actual pattern used by the most recently added entity. Check struct fields, function signatures, and wiring steps against a real example.
- **coding-standards.md**: Verify the documented comment style, error handling, and naming conventions match what the codebase actually uses.
- **animation-sequences.md**: Verify timing constants and state machine transitions match `src/player/player.c`.
- **sprite-sheet-analysis.md**: Verify the asset table (sheet sizes, grid layouts) matches actual PNG dimensions. Asset paths should reference `assets/sprites/` not bare `assets/`.

### 4. Cross-reference entity counts and types

This is the most common source of drift. Verify:

- Number of enemy types listed matches actual `.h` files in `src/entities/` (spider, jumping_spider, bird, faster_bird, fish, faster_fish)
- Number of hazard types listed matches actual `.h` files in `src/hazards/` (spike, spike_block, spike_platform, circular_saw, axe_trap, blue_flame, fire_flame)
- Number of collectible types matches `src/collectibles/` (coin, star_yellow, star_green, star_red, last_star)
- Number of surface types matches `src/surfaces/` (platform, float_platform, bridge, bouncepad variants, vine, ladder, rope, rail)
- Number of effect types matches `src/effects/` (fog, parallax, water)
- All entity names use correct snake_case names from the code
- "Current Game State" bullet points have accurate counts
- Editor entity count matches (ENT_COUNT in editor.h)

### 5. Cross-reference README.md against code

Verify:

- Project description matches current feature set (editor, TOML levels, etc.)
- Build instructions are accurate (`make`, `make editor`, `make run-level LEVEL=`)
- Feature list matches actual implemented features
- Asset/dependency references are current (tomlc17, not cJSON)
- Screenshots or GIFs (if any) reflect current game/editor state
- Links to wiki, pages, or external resources are valid

### 6. Verify and update GitHub Pages docs

**Landing page (`docs/index.html`):**
- Check that feature descriptions, entity counts, and terminology match the code
- Verify the WebAssembly player buttons work (Play + Debug Mode)
- Check for stale references (JSON format, sea gaps, old entity names)

**Documentation site (`docs/docs/index.html`):**
- This single-page HTML site mirrors the wiki content and serves as the public documentation
- **Every wiki page's content must be reflected here** — when wiki pages are updated, this file must be regenerated or manually updated to match
- Cross-reference each section in `docs/docs/index.html` against the corresponding wiki page:
  - Architecture section ↔ `architecture.md`
  - Build System section ↔ `build_system.md`
  - Assets section ↔ `assets.md`
  - Sounds section ↔ `sounds.md`
  - Constants section ↔ `constants_reference.md`
  - Developer Guide section ↔ `developer_guide.md`
  - Player Module section ↔ `player_module.md`
  - Source Files section ↔ `source_files.md`
- Check for stale entity names, asset paths, build commands, and counts
- The HTML content should match what the wiki says — wiki is the source of truth for doc content, code is the source of truth for technical accuracy

### 7. Verify and update .specs/ documents

- Check `.specs/features/level-editor/spec.md`, `design.md`, `tasks.md` for accuracy
- Update references to JSON → TOML where applicable
- Update entity lists if new types were added since spec was written
- Update asset paths if they reference old locations

### 8. Update Wiki repository

The project wiki lives at `github.com/jonathanperis/super-mango-game.wiki.git`. After updating in-repo docs:

1. **Clone the wiki** (if not already cloned):
   ```sh
   git clone https://github.com/jonathanperis/super-mango-game.wiki.git /tmp/super-mango-wiki
   ```

2. **Sync key content to wiki pages:**
   - `Home.md` — project overview, quick start, links to other pages
   - `Architecture.md` — from CLAUDE.md architecture section
   - `Building.md` — build commands for all platforms (macOS, Linux, Windows, WebAssembly)
   - `Level-Editor.md` — editor features, controls, UI layout
   - `Level-Format.md` — TOML level file format reference with all fields
   - `Entity-Reference.md` — all entity types with placement fields
   - `Asset-Organization.md` — folder structure for sprites, sounds, fonts

3. **Commit and push wiki changes:**
   ```sh
   cd /tmp/super-mango-wiki
   git add -A
   git commit -m "docs: sync wiki with current codebase state"
   git push origin master
   ```

4. **Verify** wiki pages render correctly at `github.com/jonathanperis/super-mango-game/wiki`

### 9. Apply fixes

For each discrepancy:

- Update the documentation to match the code (code is the source of truth)
- Follow the project's existing doc style (markdown tables, code blocks with comments)
- Do NOT modify any source code — only fix documentation
- If a documented feature was removed, remove it from docs entirely
- Update counts, names, constants, and file paths to match current code exactly

### 10. Report

After all fixes are applied, output a summary:

- Number of documentation files checked
- Number of discrepancies found and fixed
- List of specific changes made (grouped by file)
- Wiki pages created or updated
- Any warnings about documentation that could not be automatically verified
