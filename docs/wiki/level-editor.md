# Level Editor

<a id="home"></a>

---

Super Mango includes a standalone visual level editor (`out/super-mango-editor`) that lets designers place, move, and configure entities on a scrollable canvas, then save the result as a TOML level file that the game engine loads directly.

```sh
make editor        # build the editor binary → out/super-mango-editor
make run-editor    # build and launch
```

---

## Window Layout

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         Toolbar (32px)                                   │
├──────────────────────────────────────────────┬──────────────────────────┤
│                                              │                          │
│              Canvas  (896 × 656 px)          │    Panel  (384 × 656 px) │
│                                              │                          │
│   Scrollable level preview — WYSIWYG with    │  ┌────────────────────┐  │
│   the running game. Entities drawn at their  │  │   Entity Palette   │  │
│   exact game sizes and positions.            │  ├────────────────────┤  │
│                                              │  │   Properties       │  │
│                                              │  │   Inspector        │  │
│                                              │  ├────────────────────┤  │
│                                              │  │   Level Config     │  │
│                                              │  └────────────────────┘  │
├──────────────────────────────────────────────┴──────────────────────────┤
│                         Status Bar (32px)                                │
└─────────────────────────────────────────────────────────────────────────┘
  Total: 1280 × 720 px
```

| Area | Size | Contents |
|------|------|----------|
| Toolbar | 1280 × 32 px | Tool selector (Select / Place / Delete), File buttons (New, Open, Save, Save As), Play button |
| Canvas | 896 × 656 px | Scrollable level view with zoom. All entity types drawn at game-accurate sizes |
| Panel | 384 × 656 px | Entity palette, properties inspector, level config (collapsible sections) |
| Status bar | 1280 × 32 px | Cursor world coordinates, zoom level, current filename, modified indicator |

---

## Tools

Three interaction modes are available via the toolbar or keyboard shortcuts:

| Tool | Key | Behaviour |
|------|-----|-----------|
| **Select** | `S` | Click an entity on the canvas to select it. Drag to reposition. Selected entity appears in the Properties panel. |
| **Place** | `P` | Click empty canvas space to stamp a new entity of the type chosen in the palette. |
| **Delete** | `D` | Click an entity to remove it from the level immediately. |

---

## Entity Palette

The right panel lists all **30 placeable entity types**, grouped by category:

| Category | Entities |
|----------|----------|
| Layout | Floor Gap, Rail |
| Terrain | Platform, Float Platform, Bridge |
| Collectibles | Coin, Star Yellow, Star Green, Star Red, Last Star |
| Enemies | Spider, Jumping Spider, Bird, Faster Bird, Fish, Faster Fish |
| Hazards | Axe Trap, Circular Saw, Spike Row, Spike Platform, Spike Block, Blue Flame, Fire Flame |
| Surfaces | Bouncepad Small, Bouncepad Medium, Bouncepad High, Vine, Ladder, Rope |
| Special | Player Spawn |

Each type is shown with its in-game sprite thumbnail so the palette is visually identical to what appears in game.

---

## Properties Inspector

When an entity is selected with the Select tool, the Properties panel displays its editable fields. All fields match the TOML schema exactly — what you see in the inspector is what gets written to the file.

**Example — Spider:**
- `x`, `vx`, `patrol_x0`, `patrol_x1`, `frame_index`

**Example — Float Platform:**
- `mode` (dropdown: STATIC / CRUMBLE / RAIL)
- `x`, `y`, `tile_count`, `rail_index`, `t_offset`, `speed`

**Example — Axe Trap:**
- `pillar_x`, `y`
- `mode` (dropdown: PENDULUM / SPIN)

Changes take effect immediately on the canvas (WYSIWYG).

---

## Level Config Panel

The Level Config section in the right panel exposes the top-level TOML scalars:

- `name`, `description`, `generated_by`
- `screen_count` (world width = screen_count × 400 px)
- `player_start_x`, `player_start_y`
- `music_path`, `music_volume`
- `floor_tile_path`
- `initial_hearts`, `initial_lives`
- `score_per_life`, `coin_score`
- `floor_gaps` list (add / remove entries)

---

## Camera Controls

| Action | Input |
|--------|-------|
| Pan left / right | Click-drag on empty canvas area |
| Scroll horizontally | Mouse wheel |
| Zoom in | `+` or `Ctrl + Mouse Wheel Up` |
| Zoom out | `-` or `Ctrl + Mouse Wheel Down` |
| Toggle grid | `G` |

The canvas renders the level in WYSIWYG — entity positions and sizes match the game exactly at zoom 1.0 (logical pixel = 1 canvas pixel). At zoom 2.0 each logical pixel maps to 2 canvas pixels.

---

## Undo / Redo

The editor maintains a full undo stack for all placement, deletion, and property changes.

| Action | Shortcut |
|--------|----------|
| Undo | `Ctrl+Z` |
| Redo | `Ctrl+Y` (or `Ctrl+Shift+Z`) |

The undo stack is in-memory only — it is cleared when a new file is opened or created.

---

## Copy / Paste

| Action | Shortcut |
|--------|----------|
| Copy selected entity | `Ctrl+C` |
| Paste (offset from original) | `Ctrl+V` |

Only one entity can be in the clipboard at a time. The pasted entity appears offset from the original so it does not overlap.

---

## Play-Test Integration

The **Play** button in the toolbar launches the game engine as a child process, loading the currently open level file. The editor hides its window while the game is running. Clicking **Stop** (or closing the game window) returns to the editor.

```sh
# Equivalent to clicking Play in the editor:
make run-level LEVEL=levels/your_level.toml
```

Enabling **Debug Mode** in the toolbar adds `--debug` to the game launch, showing collision boxes, FPS counter, and the event log.

---

## File Operations

| Operation | Shortcut / Button | Notes |
|-----------|-------------------|-------|
| New | `Ctrl+N` / New button | Prompts to save if modified |
| Open | `Ctrl+O` / Open button | Opens a native file picker |
| Save | `Ctrl+S` / Save button | Overwrites the current file |
| Save As | `Ctrl+Shift+S` / Save As button | Native file picker for new path |

The title bar shows an asterisk (`*`) after the filename when there are unsaved changes. The editor prompts to save on quit if the level has been modified.

Saved files are plain TOML — they can be edited in any text editor and immediately reloaded in the editor or game.

---

## Architecture

The editor is built from these modules in `src/editor/`:

| File | Responsibility |
|------|---------------|
| `editor_main.c` | Entry point — SDL init, `EditorState` lifecycle |
| `editor.c` / `editor.h` | Core state struct, init/loop/cleanup, `EntityType` enum (30 types), `EditorTool`, `EditorCamera`, `Selection` |
| `canvas.c` / `canvas.h` | Level preview rendering, `canvas_screen_to_world`, grid overlay |
| `palette.c` / `palette.h` | Entity palette panel — thumbnails, type selection |
| `properties.c` / `properties.h` | Property inspector panel — per-type field editing |
| `tools.c` / `tools.h` | Mouse interaction for Select / Place / Delete tools |
| `ui.c` / `ui.h` | Immediate-mode UI widget library (buttons, dropdowns, text fields) |
| `undo.c` / `undo.h` | Undo stack and `PlacementData` clipboard union |
| `serializer.c` / `serializer.h` | TOML deserialization: file → `LevelDef` |
| `exporter.c` / `exporter.h` | TOML serialization: `LevelDef` → file |
| `file_dialog.c` / `file_dialog.h` | Native OS file picker (macOS / Linux / Windows) |

The `EditorState` struct mirrors the game's `GameState` design: one container passed by pointer to every function, owning the SDL window, renderer, font, entity textures, level data, camera, tool state, undo stack, and UI state.

---

## Relationship to the Game Engine

The editor shares the `LevelDef` struct and `level_loader` with the game. This means:

- Any level that loads correctly in the editor will load correctly in the game.
- The serializer (`serializer.c`) only needs to be updated in one place when a new entity type is added.
- The editor's canvas draws entities using the same sprite paths as the game — adding a new entity type requires adding its texture to `EntityTextures` and a render call in `canvas_render`.

See [Level Design — TOML Reference](#level-design) for the full schema of every entity type.
