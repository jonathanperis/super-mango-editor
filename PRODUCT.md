# PRODUCT.md

## Product Identity

Super Mango is an open-source 2D side-scrolling pixel platformer and learning project. It is written in C11 with SDL2, runs natively on macOS, Linux, and Windows, and ships a browser-playable WebAssembly build.

The public website is not only a download page. It is the attract screen for a playable game, the front door to the codebase, and a guided entrance into learning C and SDL2 game development.

## Register

brand

## Primary Users

### Browser players

People who arrive from GitHub, social links, or a search result and want to try the game immediately. They are curious, impatient, and deciding within seconds whether the page is playable or only documentation.

### C and SDL2 learners

Developers who want a readable game codebase with real systems: input, game loop, collision, rendering, audio, assets, WebAssembly build, and level loading. They need proof that the project is approachable and well documented.

### Level builders and modders

Users who care about the visual editor, TOML level format, assets, and creating or modifying levels. They need a path from playing to understanding how levels are built.

### Open-source reviewers and contributors

Developers checking project quality before cloning, starring, filing issues, or contributing. They look for build clarity, CI confidence, source organization, and documentation depth.

## User State of Mind

The page should feel like a night-time arcade cabinet that also happens to be a workshop bench. Visitors should feel invited to insert a coin, then invited to open the cabinet and learn how it works.

## Core Value Propositions

1. **Play instantly in the browser.** The website hosts the WebAssembly build and loads the game into an embedded canvas.
2. **Learn from a real C11 and SDL2 game.** The project is positioned as an educational resource with commented code and architecture docs.
3. **Build and inspect levels.** Levels are TOML data files, and the repo includes a standalone visual level editor.
4. **Study a complete platformer stack.** The codebase includes rendering layers, physics, collision, enemies, hazards, pickups, overlays, audio, and native/WebAssembly builds.
5. **Run anywhere.** The project targets Linux, macOS, Windows, and browser play.

## Canonical Product Facts

Source-backed facts for design and copy:

- Language and stack: C11, SDL2, SDL2_image, SDL2_ttf, SDL2_mixer, vendored tomlc17, Emscripten/WebAssembly.
- Rendering: 400x300 logical resolution scaled to an 800x600 native window for chunky pixel rendering.
- Current browser payload callout: Play button advertises approximately 43 MB.
- Levels: 3 TOML levels, 23 screens total, 41 enemies, 64 hazards, 80 collectibles.
- Level names: Creator's Playground, Volcanic Depths 1, Volcanic Depths 2.
- Gameplay systems: one-way platforms, floating platforms, crumble bridges, floor gaps, animated water/lava, bouncepads, vines, ladders, ropes, coins, stars, hearts, lives, score, pause, game over, completion flow.
- Enemy set: spiders, jumping spiders, birds, faster birds, fish, faster fish.
- Hazard set: spike rows, spike blocks, spike platforms, circular saws, axe traps, blue flames, fire flames.
- Visual systems: parallax backgrounds, 32 render layers, scrolling camera, fog overlays, HUD, debug overlay.
- Level editor: standalone SDL2 editor with canvas, palette, select/place/delete tools, properties inspector, level config, undo/redo, recent files, autosave, validation, export, and play-test integration.
- Controls: keyboard controls are visible on the site: WASD or arrows to move, Space to jump. Docs also describe gamepad hot-plug support.
- Authors: Jonathan Peris and Fernando Santos.
- Art credit: Super Mango 2D Pixel Art Platformer Asset Pack by Juho.

Do not invent benchmark numbers, release counts, star counts, or performance claims unless generated from the repository or GitHub API during the task.

## Brand Voice

- **Arcade-native:** action labels should sound like a playable game screen, not generic SaaS copy.
- **Builder-friendly:** technical copy should be exact and source-backed.
- **Warmly mechanical:** the tone can be playful, but never vague.
- **Direct:** each section should tell visitors what they can do next.

Preferred copy moves:

- Use game-world language for section labels when clarity remains intact: `START RUN`, `INSERT COIN`, `BUILDER MANUAL`, `ITEM DROP`, `CHOOSE YOUR CARTRIDGE`.
- Pair playful labels with plain supporting text: `ITEM DROP` plus `Download builds for your platform`.
- Use concrete proof over adjectives: `3 levels`, `23 screens`, `TOML levels`, `C11 + SDL2`, `visual level editor`.

Avoid:

- Generic marketing language such as "seamless", "powerful", "beautiful", "next generation", or "unlock your creativity".
- Unsupported claims about speed, polish, or production readiness.
- Copy that makes the game sound like a SaaS product.
- Long centered paragraphs where a scoreboard, controls panel, or short builder note would work better.

## Current Aesthetic to Preserve

The current website already has a coherent identity. Future redesigns should preserve and deepen these elements:

- Dark night-sky arcade atmosphere.
- Pixel-art title treatment for `SUPER MANGO`.
- Mango-gold and bright green accent system.
- Starfield, silhouettes, parallax scenery, and ground layer.
- Embedded playable game framed like an arcade cabinet.
- Pixel buttons with hard shadows and pressed states.
- HUD-like labels, keyboard controls, and game-status copy.
- Card panels that feel like inventory, manual, or status modules.
- Documentation and source links as part of the learning promise.

## Current Website Evaluation

Design health from the audit: strong aesthetic, medium information architecture. Estimated heuristic score: **29/40**.

What works:

- The hero instantly reads as a pixel platformer.
- The playable WebAssembly canvas is the correct centerpiece.
- The palette and typography are coherent with the game.
- The page balances player and developer audiences better than a plain README would.

Priority issues:

1. **Play conversion is split.** The hero says `PLAY NOW`, but the actual launch happens after scrolling and clicking the cabinet button.
2. **Hero copy undersells the learning resource.** It mentions browser play but not the open-source C11/SDL2 learning angle or level editor.
3. **Documentation section is too flat.** Thirteen cards appear as equal peers, which makes it hard for new users to know where to start.
4. **Feature cards are visually repetitive.** They communicate facts, but not in a game-native way.
5. **Mobile navigation loses choices.** Nav links disappear under 600px without an equivalent compact menu or sticky CTA.
6. **Reduced-motion and focus states need hardening.** The page uses shimmer, twinkle, particles, parallax, and hover-heavy affordances without a documented reduced-motion/focus system.
7. **Some muted text colors are risky.** Dim green text should not carry important body copy unless contrast is verified.

## Same-Aesthetic Overhaul Direction

The overhaul should not replace the page with a modern app landing page. It should feel more like an arcade title screen connected to a builder's manual.

Recommended page story:

1. **Attract mode hero:** `SUPER MANGO`, browser-play CTA, short source-backed value prop.
2. **Cabinet play panel:** game canvas, payload size, controls, debug mode tucked into a developer drawer.
3. **Scoreboard proof strip:** C11, SDL2, WebAssembly, 3 levels, 23 screens, TOML levels, level editor.
4. **What you can do:** Play, learn the engine, build levels.
5. **Builder manual:** grouped docs by intent, not a flat list.
6. **Item drop:** native builds and WebAssembly route with clear platform actions.
7. **Credits and source:** GitHub, releases, authors, asset credit.

## A/B Testing Plan

Treat A/B testing as controlled variation inside the same arcade aesthetic. Do not test against a generic clean SaaS page.

### Test 1: Hero CTA wording

- **Hypothesis:** Arcade-native CTA copy increases play starts because it matches the page mood and feels like a game action.
- **A/control:** `PLAY NOW` and `DOWNLOAD`.
- **B:** `INSERT COIN` with sublabel `Play in browser, ~43 MB`.
- **C:** `START RUN` with secondary `OPEN BUILDER MANUAL`.
- **Primary metric:** Clicks from hero to the play section and actual game-load starts.
- **Recommendation:** Try B first. It keeps the same aesthetic and clarifies that the action loads a game.

### Test 2: Hero proof density

- **Hypothesis:** Adding source-backed proof to the hero improves trust and scroll depth without weakening the arcade mood.
- **A/control:** `A 2D pixel art platformer. Play in your browser.`
- **B:** `Open-source C11 + SDL2 platformer. Play in browser, study the engine, build TOML levels.`
- **C:** Hero plus compact HUD chips: `C11`, `SDL2`, `WASM`, `TOML`, `LEVEL EDITOR`.
- **Primary metric:** Scroll to About, Docs clicks, GitHub clicks.
- **Recommendation:** Try C if the hero has room after spacing cleanup. Otherwise use B.

### Test 3: Cabinet placement

- **Hypothesis:** Showing the playable cabinet sooner improves game starts.
- **A/control:** Full hero, then cabinet section.
- **B:** Hero with cabinet preview peeking into the first viewport.
- **C:** Split title screen: hero copy on one side, cabinet frame on the other for desktop, stacked on mobile.
- **Primary metric:** Game-load starts and time to first game-load click.
- **Recommendation:** Prototype B first. It preserves the current hero while making play feel immediate.

### Test 4: Documentation information architecture

- **Hypothesis:** Grouping docs by user intent reduces cognitive load and increases meaningful docs clicks.
- **A/control:** 13 equal documentation cards.
- **B:** Three quest cards: `Play`, `Learn the Engine`, `Build Levels`, each linking to a small set of docs.
- **C:** Builder manual groups: `Start Here`, `Core Systems`, `World Builder`, `Build and Source`.
- **Primary metric:** Docs section click-through rate and distribution across beginner docs.
- **Recommendation:** Try C for the documentation page and B for a tighter landing-page summary.

### Test 5: Feature presentation

- **Hypothesis:** Game-native feature modules improve scannability compared with plain cards.
- **A/control:** Six equal feature cards.
- **B:** Scoreboard strip for key numbers plus three action cards.
- **C:** Inventory panel with pixel icons for enemies, hazards, surfaces, editor, WebAssembly, docs.
- **Primary metric:** Scroll depth through About and clicks to docs or GitHub.
- **Recommendation:** Try B first because it reduces text and foregrounds source-backed facts.

### Test 6: Mobile navigation

- **Hypothesis:** A persistent mobile action bar improves play and docs completion because the current nav disappears on small screens.
- **A/control:** Hide nav links below 600px.
- **B:** Sticky bottom pixel bar: `PLAY`, `DOCS`, `GITHUB`.
- **C:** Pixel hamburger plus persistent `PLAY` button.
- **Primary metric:** Mobile play-section visits, docs clicks, and bounce rate.
- **Recommendation:** Try B first. It keeps the action visible without adding menu complexity.

### Test 7: Download section framing

- **Hypothesis:** Clarifying platform card actions improves release clicks while preserving `ITEM DROP` charm.
- **A/control:** `ITEM DROP` with platform names.
- **B:** `ITEM DROP` plus subtitle `Download builds for your platform` and per-card action labels.
- **C:** `CHOOSE YOUR CARTRIDGE` with cards labeled `Open release`, `Download`, or `Play above`.
- **Primary metric:** Release-link clicks and WebAssembly return-to-play clicks.
- **Recommendation:** Try B first. It keeps the current label and adds clarity.

## Success Metrics

Track these where analytics allow:

- Hero CTA click-through to `#play`.
- Actual WebAssembly load starts from the cabinet button.
- Debug mode starts as a separate developer signal.
- Docs clicks, especially Developer Guide, Architecture, Level Design, and Level Editor.
- GitHub repository clicks.
- Release clicks by platform card.
- Mobile clicks on Play, Docs, and GitHub.
- Scroll depth to Play, About, Docs, and Downloads.

## Non-Goals

- Do not redesign the site into a generic tech landing page.
- Do not remove the playable cabinet from the core page story.
- Do not hide the educational/codebase promise behind only game marketing.
- Do not add unsourced claims or fake social proof.
- Do not implement broad visual changes inside context-documentation commits.
