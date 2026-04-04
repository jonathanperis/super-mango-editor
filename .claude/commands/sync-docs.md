---
description: "Audit the codebase and update all documentation to match the current state of the code."
---

# Sync Documentation — Super Mango Game

Scan the entire codebase and verify that all documentation accurately reflects the current state. Code is always the source of truth. Fix any drift found.

## Documentation Scope

This project's documentation lives in these locations:

| File / Directory | Purpose |
|------------------|---------|
| `CLAUDE.md` | Primary project guide — tech stack, build commands, structure, architecture, constants, guidelines |
| `.claude/references/entity-template.md` | Template for adding new entities |
| `.claude/references/coding-standards.md` | Comment style and coding conventions |
| `.claude/references/animation-sequences.md` | Sprite animation state machines and timing |
| `.claude/references/sprite-sheet-analysis.md` | Sprite sheet measurement and analysis guide |
| `.claude/commands/*.md` | Slash command definitions |
| `docs/` | GitHub Pages site (index.html) |

## Steps

### 1. Gather current code state

Collect ground truth from the source code:

- **Project structure:** `ls -R src/` — list all subdirectories and source files
- **Entity inventory:** scan `src/entities/`, `src/hazards/`, `src/collectibles/`, `src/surfaces/`, `src/effects/` for all `.h` files
- **GameState fields:** read `src/game.h` — all struct fields, constants, and `#define` values
- **Build system:** read `Makefile` — all targets, source directories, compiler flags
- **Assets:** `ls assets/` and `ls sounds/` — all PNG sprites and WAV sounds
- **Level count:** count `src/levels/level_*.c` files
- **Screens:** scan `src/screens/` for all screen modules

### 2. Cross-reference CLAUDE.md against code

For each section in `CLAUDE.md`, verify:

| Section | Check against |
|---------|--------------|
| Tech Stack table | Actual includes and Makefile libs |
| Build Commands | Makefile targets (make, make run, make clean, etc.) |
| Project Structure tree | Actual `src/` directory layout and files |
| Module responsibilities table | Actual files in each subdirectory |
| Architecture diagram | `main.c` → `game_init` → `game_loop` → `game_cleanup` call chain |
| Key Constants table | `#define` values in `game.h` |
| Current Game State bullets | Actual entity counts, features, screen types |
| Adding a new entity section | Template in `.claude/references/entity-template.md` |
| Physics / collision pattern | Actual code in `src/player/player.c` |
| Adding sound effects section | Actual pattern in `game.c` init/cleanup |
| Sprite Sheet Reference | Actual assets and analysis script |

Track every discrepancy: wrong values, missing entities, outdated counts, stale file paths, removed features, renamed modules.

### 3. Cross-reference .claude/references/ against code

For each reference document:

- **entity-template.md**: Verify the template matches the actual pattern used by the most recently added entity. Check struct fields, function signatures, and wiring steps against a real example (pick the newest entity).
- **coding-standards.md**: Verify the documented comment style, error handling, and naming conventions match what the codebase actually uses.
- **animation-sequences.md**: Verify timing constants and state machine transitions match `src/player/player.c`.
- **sprite-sheet-analysis.md**: Verify the asset table (sheet sizes, grid layouts) matches actual PNG dimensions.

### 4. Cross-reference entity counts and types

This is the most common source of drift. Verify:

- Number of enemy types listed matches actual `.h` files in `src/entities/`
- Number of hazard types listed matches actual `.h` files in `src/hazards/`
- Number of collectible types matches `src/collectibles/`
- Number of surface types matches `src/surfaces/`
- Number of effect types matches `src/effects/`
- All entity names in docs use the correct snake_case names from the code
- "Current Game State" bullet points have accurate counts

### 5. Verify GitHub Pages docs

- Check that `docs/index.html` doesn't reference features or screenshots that no longer exist
- Verify any links in the docs site point to valid targets

### 6. Apply fixes

For each discrepancy:

- Update the documentation to match the code (code is the source of truth)
- Follow the project's existing doc style (markdown tables, code blocks with comments)
- Do NOT modify any source code — only fix documentation
- If a documented feature was removed, remove it from docs entirely
- Update counts, names, constants, and file paths to match current code exactly

### 7. Report

After all fixes are applied, output a summary:

- Number of documentation files checked
- Number of discrepancies found and fixed
- List of specific changes made (grouped by file)
- Any warnings about documentation that could not be automatically verified
