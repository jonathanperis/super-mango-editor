# DESIGN.md

## Design Intent

Super Mango's website should feel like a playable arcade attract screen attached to a builder's manual. The overhaul should keep the current dark pixel-art atmosphere, mango-gold title energy, green terminal accents, and arcade-cabinet game frame while improving hierarchy, readability, accessibility, and conversion paths.

The target is not a new aesthetic. The target is a sharper version of the existing aesthetic: same world, clearer interface.

## Current Design Summary

The current landing page already communicates a retro platformer identity through:

- dark navy/black stage background;
- pixel title typography;
- mango-gold and neon-green accents;
- parallax hero layers;
- chunky pixel buttons;
- embedded WebAssembly game canvas framed as an arcade cabinet;
- card-based sections for features, docs, and downloads.

Audit conclusion: the look is memorable and appropriate, but the information hierarchy is too flat after the hero. The site should make three jobs easier:

1. start playing;
2. understand that this is an open-source C11/SDL2 learning project;
3. find the right docs or download path.

## Visual Principles

### 1. Preserve the arcade cabinet mood

Keep the page dark, luminous, and game-native. Preserve the title-screen feel, starfield/parallax scenery, pixel display type, glowing CTA treatment, cabinet bezel, scanline hints, and item-drop language.

### 2. Use arcade metaphor only when it clarifies

Labels like `INSERT COIN`, `START RUN`, `BUILDER MANUAL`, and `ITEM DROP` are desirable when paired with plain supporting text. Do not let the theme obscure task clarity.

Good:

- `ITEM DROP` with subtitle `Download builds for your platform`.
- `BUILDER MANUAL` with grouped docs.
- `INSERT COIN` with `Play in browser, ~43 MB`.

Avoid:

- replacing all navigation with obscure game lore;
- hiding docs behind unclear quest names;
- using playful labels without action labels.

### 3. Make source-backed facts visible

Use short HUD chips and scoreboard rows instead of long explanatory paragraphs where possible. The page has real proof points: C11, SDL2, WebAssembly, 3 levels, 23 screens, TOML levels, and a visual editor.

### 4. Improve readability without removing personality

Use Pixelify Sans for display and UI labels. Use DM Mono or another readable mono/body face for paragraphs, card descriptions, and metadata. Pixel display type should not carry dense body copy.

### 5. Every clickable panel should look clickable

Cards that navigate should include persistent action affordances, not only hover states. Use `READ DOC ->`, `OPEN GUIDE ->`, `PLAY ABOVE ->`, or `OPEN RELEASE ->` labels.

## Layout Direction

### Proposed page flow

1. **Hero / attract mode**
   - Large `SUPER MANGO` title.
   - Short value prop that includes browser play and open-source learning.
   - Primary action: `INSERT COIN` or `PLAY NOW`.
   - Secondary action: `BUILDER MANUAL` or `DOWNLOAD`.
   - HUD chips: `C11`, `SDL2`, `WASM`, `TOML`, `LEVEL EDITOR`.

2. **Cabinet / play panel**
   - Embedded game canvas remains the centerpiece.
   - Show clear payload and controls before load: `~43 MB`, `Keyboard recommended`, `WASD / Arrows`, `Space`.
   - Keep `Debug Mode`, but treat it as a developer affordance rather than a primary player action.

3. **Scoreboard proof strip**
   - Compact metrics: `3 levels`, `23 screens`, `41 enemies`, `64 hazards`, `80 collectibles`.
   - Stack facts on mobile.

4. **Choose your path**
   - Three large cards:
     - `Play` — browser and controls.
     - `Learn the Engine` — C11/SDL2 docs.
     - `Build Levels` — TOML levels and visual editor.

5. **Builder manual / docs**
   - Replace the flat 13-card landing-page list with grouped docs.
   - Keep the full docs page available for exhaustive indexing.

6. **Item drop / downloads**
   - Keep the `ITEM DROP` heading.
   - Add subtitle and action labels to each platform card.
   - Highlight WebAssembly as `No install` or `Play above`.

7. **Credits / source**
   - Keep GitHub, Releases, authors, and asset credit.

## Information Architecture

### Landing-page docs grouping

Use fewer, stronger cards on the landing page:

#### Start Here

- README
- Developer Guide
- Architecture

#### Core Systems

- Player Module
- Entities & Hazards
- Collectibles & Surfaces
- Constants Reference

#### World Builder

- Level Design
- Level Editor
- Assets
- Sounds

#### Build & Source

- Source Files
- Build System

For the landing page, this can be presented as four grouped cards. Each grouped card should list 2-4 deep links and include one strong action label.

### Full docs page

The full docs page can keep all deep links, but it should also use the same grouping taxonomy so the mental model stays consistent.

## Component Guidelines

### Hero

Keep:

- parallax layers;
- large pixel title;
- dark sky and silhouette scenery;
- green tech badge or HUD chips;
- bright primary CTA.

Change:

- add a richer but still short value prop;
- reduce visual ambiguity between primary and secondary buttons;
- consider a cabinet preview peeking into the first viewport;
- ensure enough vertical breathing room so the hero does not feel cropped.

Suggested copy candidates:

- `Open-source C11 + SDL2 platformer. Play in browser, study the engine, build TOML levels.`
- `A pixel platformer you can play, inspect, and rebuild.`
- `Play the run. Open the source. Build the next level.`

### Buttons

Primary button:

- filled mango-gold or bright green;
- hard pixel shadow;
- clear hover and pressed state;
- visible focus ring.

Secondary button:

- outlined with mango-gold border;
- no ambiguous trailing line artifacts;
- equal height with primary button;
- action-oriented label.

Focus state:

```css
:where(a, button):focus-visible {
  outline: 3px solid var(--accent);
  outline-offset: 4px;
  box-shadow: 0 0 0 6px rgba(255, 214, 10, 0.18);
}
```

### Cabinet

Keep the game embed as a large arcade frame. Add pre-load clarity:

- `Play in browser`;
- `~43 MB download`;
- `Keyboard recommended`;
- controls legend;
- optional `Debug Mode` marked as developer mode.

When the game loads, maintain focus intentionally and avoid trapping users. Provide a nearby note that explains how to resume page scrolling if keyboard controls capture arrows/space.

### Cards

Feature, doc, and download cards should read as game UI panels.

Recommended anatomy:

1. small top label (`FEATURE_01`, `QUEST_02`, `DROP_03`);
2. title;
3. concise body copy;
4. persistent action label;
5. optional pixel icon or glyph.

Interaction:

- hover: border brightens and card rises 1-2px;
- active: card depresses;
- focus-visible: same ring as buttons;
- no hidden-only-on-hover action text for mobile-critical tasks.

### Scoreboard strip

Use a compact HUD row for facts:

- `C11 + SDL2`
- `WebAssembly`
- `3 levels`
- `23 screens`
- `TOML worlds`
- `Visual editor`

This should be above or immediately after the cabinet to reduce reliance on long paragraphs.

### Documentation section

Replace the landing page's long list of equal cards with grouped cards. Make the `Developer Guide` a high-priority beginner route instead of an orphan final card.

Example grouped card:

```text
START HERE
README · Developer Guide · Architecture
New to the engine? Start with the walkthrough and architecture map.
OPEN GUIDE ->
```

### Downloads section

Keep the charming `ITEM DROP` heading but add clarity.

Example:

```text
ITEM DROP
Download builds for your platform or play the WebAssembly version above.

Linux       x86_64                 OPEN RELEASE ->
macOS       arm64 Apple Silicon     OPEN RELEASE ->
Windows     x86_64 + SDL2 DLLs      OPEN RELEASE ->
WebAssembly No install              PLAY ABOVE ->
```

## Accessibility Requirements

### Keyboard

- All buttons, nav links, cards, and download links must have visible focus styles.
- Cards that are links should not rely on nested interactive elements.
- Game canvas focus should be intentional and reversible.

### Motion

The landing page uses shimmer, twinkle, particles, parallax, pulse, and bounce. Add reduced-motion support:

```css
@media (prefers-reduced-motion: reduce) {
  *, *::before, *::after {
    animation-duration: 0.001ms !important;
    animation-iteration-count: 1 !important;
    scroll-behavior: auto !important;
    transition-duration: 0.001ms !important;
  }

  .parallax-layer,
  .particle,
  .scroll-hint,
  .shimmer-text {
    transform: none !important;
    animation: none !important;
  }
}
```

### Contrast

Use dim greens for decoration and borders, not critical text. Body text in cards should meet WCAG contrast on `--bg-card`.

Known audit concern:

- `--enhanced-text-muted #5a6a4a` on `--bg #060a14` is below normal-text contrast expectations.
- `--green-dim #1a7a2a` is risky for text.

### Mobile

Current nav links disappear below 600px. Replace or supplement with one of:

- sticky bottom pixel action bar: `PLAY`, `DOCS`, `GITHUB`;
- pixel hamburger plus persistent `PLAY` CTA;
- compact top nav with horizontal scroll.

Recommended first variant: sticky bottom action bar.

## Visual Token Direction

Preserve the existing token spirit:

- background: near-black navy;
- cards: slightly lighter blue-black;
- primary brand: mango-gold/yellow;
- action green: bright arcade green;
- body text: warm off-white or pale green-white;
- muted text: accessible desaturated green-gray;
- borders: green and gold glow accents.

Recommended additions:

```css
--focus: #ffd60a;
--body-readable: #d9e7c8;
--panel-border-strong: rgba(45, 226, 85, 0.55);
--gold-glow: rgba(255, 214, 10, 0.25);
--green-glow: rgba(45, 226, 85, 0.24);
```

Typography:

- Display/UI: Pixelify Sans.
- Body/metadata/code-adjacent copy: DM Mono.

## A/B Test Specifications

### A/B 1: CTA copy

- **A:** `PLAY NOW`
- **B:** `INSERT COIN` with `Play in browser, ~43 MB`
- **C:** `START RUN`
- **Metric:** hero CTA click-through and game-load starts.
- **Keep constant:** visual treatment, placement, and destination.

### A/B 2: Hero value prop

- **A:** current one-sentence browser-play copy.
- **B:** open-source learning copy.
- **C:** copy plus HUD chips.
- **Metric:** scroll depth to docs and GitHub/docs clicks.

### A/B 3: Cabinet proximity

- **A:** cabinet below hero.
- **B:** cabinet preview peeks into first viewport.
- **C:** desktop split hero/cabinet layout.
- **Metric:** time to first game-load click.

### A/B 4: Docs IA

- **A:** 13 equal doc cards.
- **B:** 3 path cards: Play, Learn, Build.
- **C:** 4 grouped manual cards: Start Here, Core Systems, World Builder, Build & Source.
- **Metric:** docs click-through rate and beginner-doc distribution.

### A/B 5: Feature proof

- **A:** six text feature cards.
- **B:** scoreboard strip plus three action cards.
- **C:** inventory grid with pixel icons.
- **Metric:** scroll depth and docs/GitHub clicks.

### A/B 6: Mobile navigation

- **A:** hidden nav links below 600px.
- **B:** sticky bottom bar: Play, Docs, GitHub.
- **C:** hamburger plus persistent Play.
- **Metric:** mobile play and docs completion.

### A/B 7: Downloads framing

- **A:** current Item Drop cards.
- **B:** Item Drop with subtitle and explicit action labels.
- **C:** Choose Your Cartridge framing.
- **Metric:** release-link clicks and WebAssembly return-to-play clicks.

## Implementation Roadmap

### Phase 1: Foundation cleanup

- Add focus-visible styles.
- Add reduced-motion rules.
- Remove inline styles from landing CTAs and use reusable classes.
- Fix class naming mismatch around docs CTA styles.
- Brighten body/card text colors where contrast is weak.

### Phase 2: Hierarchy overhaul

- Update hero copy and CTA pairing.
- Add scoreboard proof strip.
- Add pre-load clarity to cabinet.
- Convert feature cards to HUD/inventory panels.

### Phase 3: IA overhaul

- Replace landing docs grid with grouped docs cards.
- Promote Developer Guide as a first-step route.
- Add clear card action labels.
- Clarify download cards.

### Phase 4: A/B instrumentation

- Add stable event names for hero CTA, play start, debug start, docs cards, release clicks, GitHub clicks, and mobile sticky actions.
- Keep tests small and sequential to avoid mixing layout and copy effects.

### Phase 5: Visual expansion

- Add small pixel icons for feature/doc/download cards.
- Consider cabinet preview in hero.
- Add section dividers that feel like HUD separators.

## Acceptance Criteria for an Overhaul PR

- Same dark pixel-arcade aesthetic is preserved.
- Browser play remains the primary page action.
- Hero communicates both game play and open-source learning value.
- Docs are grouped by user intent, not a flat equal list.
- Developer Guide is easy to find.
- Download cards state what clicking does.
- All interactive elements have visible keyboard focus.
- Reduced-motion users do not get shimmer/parallax/particle motion.
- Mobile users retain visible Play and Docs routes.
- Astro lint/build pass.
- No unsupported product claims are introduced.
