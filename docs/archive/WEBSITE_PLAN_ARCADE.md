# Website Plan: the Arcade Cabinet (2026-05)

> **Historical document.** This is the website plan that `PRODUCT.md` carried
> from 2026-05-16 (commit `562df4f`) until 2026-11. It describes an earlier
> site framed as a night-time arcade cabinet, with "insert a coin" copy and an
> A/B testing plan. None of it is current: the site was rebuilt from the
> game's own sprites in 2026-10 (see [DESIGN.md](../../DESIGN.md)), and
> [PRODUCT.md](../../PRODUCT.md) describes the product as it is now. Nobody
> updates this file.

## Brand voice the plan used

- **Arcade-native:** action labels should sound like a playable game screen, not generic SaaS copy.
- **Builder-friendly:** technical copy should be exact and source-backed.
- **Warmly mechanical:** the tone can be playful, but never vague.
- **Direct:** each section should tell visitors what they can do next.

Preferred copy moves at the time:

- Use game-world language for section labels when clarity remains intact: `START RUN`, `INSERT COIN`, `BUILDER MANUAL`, `ITEM DROP`, `CHOOSE YOUR CARTRIDGE`.
- Pair playful labels with plain supporting text: `ITEM DROP` plus `Download builds for your platform`.

The page was meant to feel "like a night-time arcade cabinet that also happens
to be a workshop bench", with visitors "invited to insert a coin, then invited
to open the cabinet and learn how it works".

## The aesthetic the plan wanted to preserve

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

**Historical design assessment:** this section and the design/A-B proposals below
describe an earlier website. They are planning context, not current-state audit
findings or authoritative content counts. Current navigation, accessibility and
content facts must be checked against source and generated outputs.

Design health from the audit: strong aesthetic, medium information architecture. Estimated heuristic score: **29/40**.

What works:

- The hero instantly reads as a pixel platformer.
- The playable WebAssembly canvas is the correct centerpiece.
- The palette and typography are coherent with the game.
- The page balances player and developer audiences better than a plain README would.

Priority issues:

1. **Play conversion is split.** The hero says `PLAY NOW`, but the actual launch happens after scrolling and clicking the cabinet button.
2. **Hero copy undersells the learning resource.** It mentions browser play but not the open-source C11/raylib learning angle or level editor.
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
3. **Scoreboard proof strip:** C11, raylib, WebAssembly, the campaign's level and screen counts (read from `docs/src/generated/project.json`, never typed in), TOML levels, level editor.
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
- **B:** `Open-source C11 + raylib platformer. Play in browser, study the engine, build TOML levels.`
- **C:** Hero plus compact HUD chips: `C11`, `raylib`, `WASM`, `TOML`, `LEVEL EDITOR`.
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

## Page metrics the plan suggested

These were never collected: optional analytics loads only on manual (`/docs/`)
pages, never on the home page that hosts the game.

- Hero CTA click-through to `#play`.
- Actual WebAssembly load starts from the cabinet button.
- Debug mode starts as a separate developer signal.
- Docs clicks, especially Developer Guide, Architecture, Level Design, and Level Editor.
- GitHub repository clicks.
- Release clicks by platform card.
- Mobile clicks on Play, Docs, and GitHub.
- Scroll depth to Play, About, Docs, and Downloads.
