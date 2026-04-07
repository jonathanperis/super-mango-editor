"use client";

import { useState, useEffect, useRef, useCallback } from "react";

export default function DocsPage() {
  const [menuOpen, setMenuOpen] = useState(false);
  const [searchQuery, setSearchQuery] = useState("");
  const [activeSection, setActiveSection] = useState("home");
  const sectionsRef = useRef<HTMLElement[]>([]);

  const toggleMenu = useCallback(() => {
    setMenuOpen((prev) => !prev);
  }, []);

  // Scrollspy
  useEffect(() => {
    const sections = document.querySelectorAll<HTMLElement>("section.doc-section");
    sectionsRef.current = Array.from(sections);

    const observerOptions: IntersectionObserverInit = {
      root: null,
      rootMargin: "-80px 0px -60% 0px",
      threshold: 0,
    };

    const observer = new IntersectionObserver((entries) => {
      entries.forEach((entry) => {
        if (entry.isIntersecting) {
          setActiveSection(entry.target.id);
        }
      });
    }, observerOptions);

    sections.forEach((section) => observer.observe(section));

    return () => observer.disconnect();
  }, []);

  // Scroll active nav item into view
  useEffect(() => {
    const activeItem = document.querySelector(".nav-item.active");
    if (activeItem) {
      activeItem.scrollIntoView({ behavior: "smooth", block: "nearest" });
    }
  }, [activeSection]);

  const sectionIds = [
    "home",
    "architecture",
    "assets",
    "build-system",
    "constants-reference",
    "developer-guide",
    "player-module",
    "sounds",
    "source-files",
  ];

  const sectionLabels: Record<string, string> = {
    home: "Home",
    architecture: "Architecture",
    assets: "Assets",
    "build-system": "Build System",
    "constants-reference": "Constants Reference",
    "developer-guide": "Developer Guide",
    "player-module": "Player Module",
    sounds: "Sounds",
    "source-files": "Source Files",
  };

  const isSectionVisible = (id: string) => {
    if (searchQuery === "") return true;
    const section = document.getElementById(id);
    if (!section) return true;
    return section.textContent?.toLowerCase().includes(searchQuery.toLowerCase());
  };

  return (
    <>
      <style>{`
        :root {
          --bg: #060a14;
          --sidebar-bg: #0d1520;
          --text-primary: #f0e6c8;
          --text-muted: #7a8a6a;
          --text-body: #d0c8a8;
          --accent: #ffb700;
          --accent-hover: #39d353;
          --code-bg: #0a1a0e;
          --code-text: #f0e6c8;
          --border: #1a3020;
          --code-border: #1a3020;
          --sidebar-width: 260px;
        }
        * { box-sizing: border-box; }
        html { scroll-behavior: smooth; font-family: 'DM Mono', monospace; }
        body {
          margin: 0; padding: 0;
          background: var(--bg);
          background-image: radial-gradient(circle at 50% 0%, rgba(255,183,0,0.05) 0%, transparent 50%);
          color: var(--text-primary);
          display: flex;
        }
        .sidebar {
          position: fixed; top: 0; left: 0;
          width: var(--sidebar-width); height: 100vh;
          background: var(--sidebar-bg);
          display: flex; flex-direction: column;
          border-right: 1px solid var(--border);
          z-index: 100; transition: transform 0.3s ease;
        }
        .sidebar-header { padding: 24px 24px 16px; }
        .repo-title {
          font-size: 18px; font-weight: 700;
          margin: 0 0 4px; color: var(--text-primary);
          letter-spacing: -0.01em;
          display: flex; align-items: center; gap: 8px;
        }
        .repo-title::before {
          content: ''; display: block;
          width: 16px; height: 16px;
          background: var(--accent); border-radius: 4px;
        }
        .repo-description {
          font-size: 13px; color: var(--text-muted); margin: 0 0 16px;
        }
        .search-box {
          width: 100%; background: #000;
          border: 1px solid var(--border);
          color: var(--text-primary); border-radius: 6px;
          padding: 8px 12px; font-size: 14px;
          outline: none; transition: border-color 0.2s;
        }
        .search-box:focus { border-color: var(--accent); }
        .sidebar-nav-container {
          flex: 1; overflow-y: auto; padding: 8px 16px 24px;
        }
        .sidebar-nav-container ul { list-style: none; padding: 0; margin: 0; }
        .sidebar-nav-container li { margin-bottom: 2px; }
        .nav-item {
          display: block; padding: 8px 12px;
          color: var(--text-muted); text-decoration: none;
          font-size: 14px; border-radius: 6px; transition: all 0.2s;
        }
        .nav-item:hover {
          color: var(--text-primary);
          background: rgba(255, 255, 255, 0.05);
        }
        .nav-item.active {
          color: var(--accent);
          background: rgba(255, 183, 0, 0.1);
          font-weight: 500;
        }
        .sidebar-footer {
          padding: 16px 24px;
          border-top: 1px solid var(--border);
          font-size: 12px; color: var(--text-muted);
        }
        .sidebar-footer a {
          color: var(--text-muted); text-decoration: none;
          display: block; margin-bottom: 8px;
        }
        .sidebar-footer a:hover { color: var(--text-primary); }
        .content-wrapper {
          margin-left: var(--sidebar-width); flex: 1;
          display: flex; justify-content: center;
          width: calc(100% - var(--sidebar-width));
        }
        .content { width: 100%; max-width: 860px; padding: 48px 64px; }
        h1 { font-size: 32px; font-weight: 700; letter-spacing: -0.02em; margin: 0 0 8px; color: var(--text-primary); }
        h2 { font-size: 22px; font-weight: 600; letter-spacing: -0.01em; margin: 48px 0 16px; padding-bottom: 8px; border-bottom: 1px solid var(--border); color: var(--text-primary); }
        h3 { font-size: 18px; font-weight: 600; margin: 32px 0 12px; color: var(--text-primary); }
        p { font-size: 15px; line-height: 1.75; color: var(--text-body); margin: 0 0 16px; }
        a { color: var(--accent); text-decoration: none; }
        a:hover { text-decoration: underline; }
        pre {
          background: var(--code-bg); border: 1px solid var(--code-border);
          border-radius: 8px; padding: 16px 20px;
          overflow-x: auto; margin: 24px 0; position: relative;
        }
        pre code { background: transparent; padding: 0; border-radius: 0; color: var(--code-text); font-size: 13px; }
        code { font-family: 'DM Mono', monospace; background: var(--code-bg); color: var(--code-text); border-radius: 4px; padding: 2px 6px; font-size: 13px; }
        table { width: 100%; border-collapse: collapse; border-radius: 8px; overflow: hidden; margin: 24px 0; font-size: 14px; border: 1px solid var(--border); }
        th, td { padding: 10px 16px; text-align: left; }
        th { background: var(--sidebar-bg); color: var(--text-primary); font-weight: 600; border-bottom: 1px solid var(--border); }
        td { border-bottom: 1px solid var(--border); color: var(--text-body); }
        tr:nth-child(even) { background: rgba(255, 255, 255, 0.02); }
        tr:last-child td { border-bottom: none; }
        blockquote { margin: 24px 0; padding: 12px 20px; background: var(--sidebar-bg); border-left: 3px solid var(--accent); border-radius: 0 6px 6px 0; color: var(--text-muted); }
        blockquote p { margin: 0; }
        hr { border: none; border-top: 1px solid var(--border); margin: 40px 0; }
        section.doc-section { scroll-margin-top: 80px; margin-bottom: 80px; }
        .hidden-section { display: none; }
        .mobile-header {
          display: none; position: sticky; top: 0;
          background: var(--bg); border-bottom: 1px solid var(--border);
          padding: 16px 24px; z-index: 90;
          align-items: center; gap: 16px;
        }
        .menu-toggle {
          background: none; border: none;
          color: var(--text-primary); cursor: pointer;
          padding: 0; display: flex; flex-direction: column; gap: 4px;
        }
        .menu-toggle span { display: block; width: 20px; height: 2px; background: currentColor; border-radius: 2px; }
        .overlay {
          display: none; position: fixed; inset: 0;
          background: rgba(0, 0, 0, 0.5);
          backdrop-filter: blur(2px); z-index: 95;
        }
        @media (max-width: 768px) {
          .sidebar { transform: translateX(-100%); }
          .sidebar.open { transform: translateX(0); }
          .content-wrapper { margin-left: 0; width: 100%; }
          .content { padding: 32px 24px; }
          .mobile-header { display: flex; }
          .overlay.open { display: block; }
        }
      `}</style>

      {/* Mobile Header */}
      <div className="mobile-header">
        <button className="menu-toggle" onClick={toggleMenu} aria-label="Toggle Menu">
          <span></span>
          <span></span>
          <span></span>
        </button>
        <div className="repo-title">Super Mango Editor</div>
      </div>

      <div
        className={`overlay${menuOpen ? " open" : ""}`}
        onClick={toggleMenu}
      ></div>

      {/* Sidebar */}
      <aside className={`sidebar${menuOpen ? " open" : ""}`}>
        <div className="sidebar-header">
          <h1 className="repo-title">Super Mango Editor</h1>
          <p className="repo-description">
            2D side-scrolling platformer written in C using SDL2 — browser-playable via WebAssembly
          </p>
          <input
            type="text"
            className="search-box"
            placeholder="Search documentation..."
            value={searchQuery}
            onChange={(e) => setSearchQuery(e.target.value)}
          />
        </div>
        <div className="sidebar-nav-container">
          <ul>
            {sectionIds.map((id) => (
              <li key={id} style={{ display: isSectionVisible(id) ? "" : "none" }}>
                <a
                  href={`#${id}`}
                  className={`nav-item${activeSection === id ? " active" : ""}`}
                  onClick={() => {
                    if (window.innerWidth <= 768) toggleMenu();
                  }}
                >
                  {sectionLabels[id]}
                </a>
              </li>
            ))}
          </ul>
        </div>
        <div className="sidebar-footer">
          <a href="/" className="back-home">&#8592; Back to home</a>
          <a href="https://github.com/jonathanperis/super-mango-editor" target="_blank" rel="noopener noreferrer">View on GitHub</a>
          <div>Built by Jonathan Peris</div>
        </div>
      </aside>

      {/* Main Content */}
      <div className="content-wrapper">
        <main className="content" id="mainContent">

{/* ============================================================ */}
{/* SECTION: HOME */}
{/* ============================================================ */}
<section id="home" className={`doc-section${!isSectionVisible("home") ? " hidden-section" : ""}`}>
  <h1 className="page-title">Home</h1>
  <blockquote>
    <p>2D side-scrolling platformer written in C using SDL2 -- browser-playable via WebAssembly</p>
  </blockquote>
  <p>Super Mango is a 2D platformer built in C11 with SDL2, designed as an educational project with well-commented source code for learning C + SDL2 game development. The game features TOML-based levels with configurable multi-screen stages, parallax backgrounds, enemies, hazards, collectibles, and delta-time physics with walk/run acceleration. It includes a standalone visual level editor and builds natively on macOS/Linux/Windows and as WebAssembly for browser play.</p>
  <hr />
  <h2>Quick Links</h2>
  <table>
    <thead><tr><th>Page</th><th>Description</th></tr></thead>
    <tbody>
      <tr><td><a href="architecture">Architecture</a></td><td>Game loop, init/loop/cleanup pattern, GameState container, render order</td></tr>
      <tr><td><a href="source_files">Source Files</a></td><td>Module-by-module reference for every <code>.c</code> / <code>.h</code> file</td></tr>
      <tr><td><a href="player_module">Player Module</a></td><td>Input, physics, animation -- deep dive into <code>player.c</code></td></tr>
      <tr><td><a href="build_system">Build System</a></td><td>Makefile, compiler flags, build targets, prerequisites</td></tr>
      <tr><td><a href="assets">Assets</a></td><td>All sprite sheets, tilesets, and fonts in <code>assets/</code></td></tr>
      <tr><td><a href="sounds">Sounds</a></td><td>All audio files in <code>sounds/</code></td></tr>
      <tr><td><a href="constants_reference">Constants Reference</a></td><td>Every <code>#define</code> in <code>game.h</code> and entity headers explained</td></tr>
      <tr><td><a href="developer_guide">Developer Guide</a></td><td>How to add new entities, sound effects, and features</td></tr>
    </tbody>
  </table>
  <hr />
  <h2>Key Features</h2>
  <ul>
    <li>2D side-scrolling platformer with TOML-based levels (dynamic world width via <code>screen_count</code>, default 1600px / 4 screens)</li>
    <li>35 render layers drawn back-to-front with configurable parallax scrolling background</li>
    <li>Delta-time physics with walk/run acceleration, momentum, and friction at 60 FPS</li>
    <li>Six enemy types, seven hazard types, collectibles (coins, 3 star colors, end-of-level star), bouncepads, climbable vines/ladders/ropes</li>
    <li>Standalone visual level editor (canvas, palette, tools, properties, undo, serializer, exporter)</li>
    <li>Start menu, HUD (hearts/lives/score), lives system, debug overlay</li>
    <li>Keyboard and gamepad (lazy-initialized, hot-plug) controls</li>
    <li>Per-level configurable physics, music, floor tilesets, and background layers</li>
    <li>Builds natively on macOS, Linux, Windows; WebAssembly via Emscripten</li>
  </ul>
  <p><strong><a href="https://jonathanperis.github.io/super-mango-editor/">Play in browser &#8594;</a></strong></p>
  <hr />
  <h2>Project at a Glance</h2>
  <table>
    <thead><tr><th>Item</th><th>Detail</th></tr></thead>
    <tbody>
      <tr><td>Language</td><td>C11</td></tr>
      <tr><td>Compiler</td><td><code>clang</code> (default), <code>gcc</code> compatible</td></tr>
      <tr><td>Window size</td><td>800 x 600 px (OS window)</td></tr>
      <tr><td>Logical canvas</td><td>400 x 300 px (2x pixel scale)</td></tr>
      <tr><td>Target FPS</td><td>60</td></tr>
      <tr><td>Audio</td><td>44100 Hz, stereo, 16-bit</td></tr>
      <tr><td>Level format</td><td>TOML (via vendored tomlc17 parser)</td></tr>
      <tr><td>Libraries</td><td>SDL2, SDL2_image, SDL2_ttf, SDL2_mixer, tomlc17 (vendored TOML parser)</td></tr>
    </tbody>
  </table>
  <hr />
  <h2>Quick Start</h2>
  <pre><code className="language-sh">{`# macOS -- install dependencies
brew install sdl2 sdl2_image sdl2_ttf sdl2_mixer

# Build and run the game
make run

# Build and run a specific level
make run-level LEVEL=levels/00_sandbox_01.toml

# Build and run the level editor
make run-editor`}</code></pre>
  <p>See <a href="build_system">Build System</a> for Linux and Windows instructions.</p>
</section>

{/* ============================================================ */}
{/* SECTION: ARCHITECTURE */}
{/* ============================================================ */}
<section id="architecture" className={`doc-section${!isSectionVisible("architecture") ? " hidden-section" : ""}`}>
  <h1 className="page-title">Architecture</h1>
  <p><a href="home">&#8592; Home</a></p>
  <hr />
  <h2>Overview</h2>
  <p>Super Mango follows a classic <strong>init &#8594; loop &#8594; cleanup</strong> pattern. A single <code>GameState</code> struct is the owner of every resource in the game and is passed by pointer to every function that needs to read or modify it.</p>
  <hr />
  <h2>Startup Sequence</h2>
  <pre><code>{`main()
  \u251C\u2500\u2500 SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)
  \u251C\u2500\u2500 IMG_Init(IMG_INIT_PNG)
  \u251C\u2500\u2500 TTF_Init()
  \u251C\u2500\u2500 Mix_OpenAudio(44100, stereo, 2048 buffer)
  \u2502
  \u251C\u2500\u2500 game_init(&gs)
  \u2502     \u251C\u2500\u2500 SDL_CreateWindow  \u2192 gs.window
  \u2502     \u251C\u2500\u2500 SDL_CreateRenderer \u2192 gs.renderer
  \u2502     \u251C\u2500\u2500 SDL_RenderSetLogicalSize(GAME_W, GAME_H)
  \u2502     \u2502
  \u2502     \u2502   \u2500\u2500 Load all textures (engine resources) \u2500\u2500
  \u2502     \u251C\u2500\u2500 parallax_init(&gs.parallax, gs.renderer)  (multi-layer background, configured per level)
  \u2502     \u251C\u2500\u2500 IMG_LoadTexture \u2192 gs.floor_tile        (sprites/levels/grass_tileset.png \u2014 fatal)
  \u2502     \u251C\u2500\u2500 IMG_LoadTexture \u2192 gs.platform_tex      (sprites/surfaces/Platform.png \u2014 fatal)
  \u2502     \u251C\u2500\u2500 water_init(&gs.water, gs.renderer)      (sprites/foregrounds/water.png)
  \u2502     \u251C\u2500\u2500 IMG_LoadTexture \u2192 gs.spider_tex        (sprites/entities/spider.png \u2014 fatal)
  \u2502     \u251C\u2500\u2500 IMG_LoadTexture \u2192 gs.jumping_spider_tex (sprites/entities/jumping_spider.png \u2014 fatal)
  \u2502     \u251C\u2500\u2500 IMG_LoadTexture \u2192 gs.bird_tex          (sprites/entities/bird.png \u2014 fatal)
  \u2502     \u251C\u2500\u2500 IMG_LoadTexture \u2192 gs.faster_bird_tex   (sprites/entities/faster_bird.png \u2014 fatal)
  \u2502     \u251C\u2500\u2500 IMG_LoadTexture \u2192 gs.fish_tex          (sprites/entities/fish.png \u2014 fatal)
  \u2502     \u251C\u2500\u2500 IMG_LoadTexture \u2192 gs.coin_tex          (sprites/collectibles/coin.png \u2014 fatal)
  \u2502     \u251C\u2500\u2500 IMG_LoadTexture \u2192 gs.bouncepad_medium_tex (sprites/surfaces/bouncepad_medium.png \u2014 fatal)
  \u2502     \u251C\u2500\u2500 IMG_LoadTexture \u2192 gs.vine_tex          (sprites/surfaces/vine.png \u2014 non-fatal)
  \u2502     \u251C\u2500\u2500 IMG_LoadTexture \u2192 gs.ladder_tex        (sprites/surfaces/ladder.png \u2014 non-fatal)
  \u2502     \u251C\u2500\u2500 IMG_LoadTexture \u2192 gs.rope_tex          (sprites/surfaces/rope.png \u2014 non-fatal)
  \u2502     \u251C\u2500\u2500 IMG_LoadTexture \u2192 gs.bouncepad_small_tex  (sprites/surfaces/bouncepad_small.png \u2014 non-fatal)
  \u2502     \u251C\u2500\u2500 IMG_LoadTexture \u2192 gs.bouncepad_high_tex   (sprites/surfaces/bouncepad_high.png \u2014 non-fatal)
  \u2502     \u251C\u2500\u2500 IMG_LoadTexture \u2192 gs.rail_tex          (sprites/surfaces/rail.png \u2014 non-fatal)
  \u2502     \u251C\u2500\u2500 IMG_LoadTexture \u2192 gs.spike_block_tex   (sprites/hazards/spike_block.png \u2014 non-fatal)
  \u2502     \u251C\u2500\u2500 IMG_LoadTexture \u2192 gs.float_platform_tex (sprites/surfaces/float_platform.png \u2014 non-fatal)
  \u2502     \u251C\u2500\u2500 IMG_LoadTexture \u2192 gs.bridge_tex        (sprites/surfaces/bridge.png \u2014 non-fatal)
  \u2502     \u251C\u2500\u2500 IMG_LoadTexture \u2192 gs.star_yellow_tex   (sprites/collectibles/star_yellow.png \u2014 non-fatal)
  \u2502     \u251C\u2500\u2500 IMG_LoadTexture \u2192 gs.star_green_tex    (sprites/collectibles/star_green.png \u2014 non-fatal)
  \u2502     \u251C\u2500\u2500 IMG_LoadTexture \u2192 gs.star_red_tex      (sprites/collectibles/star_red.png \u2014 non-fatal)
  \u2502     \u251C\u2500\u2500 IMG_LoadTexture \u2192 gs.axe_trap_tex      (sprites/hazards/axe_trap.png \u2014 non-fatal)
  \u2502     \u251C\u2500\u2500 IMG_LoadTexture \u2192 gs.circular_saw_tex  (sprites/hazards/circular_saw.png \u2014 non-fatal)
  \u2502     \u251C\u2500\u2500 IMG_LoadTexture \u2192 gs.blue_flame_tex    (sprites/hazards/blue_flame.png \u2014 non-fatal)
  \u2502     \u251C\u2500\u2500 IMG_LoadTexture \u2192 gs.fire_flame_tex    (sprites/hazards/fire_flame.png \u2014 non-fatal)
  \u2502     \u251C\u2500\u2500 IMG_LoadTexture \u2192 gs.faster_fish_tex   (sprites/entities/faster_fish.png \u2014 non-fatal)
  \u2502     \u251C\u2500\u2500 IMG_LoadTexture \u2192 gs.spike_tex         (sprites/hazards/spike.png \u2014 non-fatal)
  \u2502     \u251C\u2500\u2500 IMG_LoadTexture \u2192 gs.spike_platform_tex (sprites/hazards/spike_platform.png \u2014 non-fatal)
  \u2502     \u2502
  \u2502     \u2502   \u2500\u2500 Load all sound effects \u2500\u2500
  \u2502     \u251C\u2500\u2500 Mix_LoadWAV     \u2192 gs.snd_spring        (sounds/surfaces/bouncepad.wav \u2014 non-fatal)
  \u2502     \u251C\u2500\u2500 Mix_LoadWAV     \u2192 gs.snd_axe           (sounds/hazards/axe_trap.wav \u2014 non-fatal)
  \u2502     \u251C\u2500\u2500 Mix_LoadWAV     \u2192 gs.snd_flap          (sounds/entities/bird.wav \u2014 non-fatal)
  \u2502     \u251C\u2500\u2500 Mix_LoadWAV     \u2192 gs.snd_spider_attack (sounds/entities/spider.wav \u2014 non-fatal)
  \u2502     \u251C\u2500\u2500 Mix_LoadWAV     \u2192 gs.snd_dive          (sounds/entities/fish.wav \u2014 non-fatal)
  \u2502     \u251C\u2500\u2500 Mix_LoadWAV     \u2192 gs.snd_jump          (sounds/player/player_jump.wav \u2014 fatal)
  \u2502     \u251C\u2500\u2500 Mix_LoadWAV     \u2192 gs.snd_coin          (sounds/collectibles/coin.wav \u2014 non-fatal)
  \u2502     \u251C\u2500\u2500 Mix_LoadWAV     \u2192 gs.snd_hit           (sounds/player/player_hit.wav \u2014 non-fatal)
  \u2502     \u251C\u2500\u2500 Mix_LoadMUS     \u2192 gs.music             (per-level music_path \u2014 non-fatal)
  \u2502     \u251C\u2500\u2500 Mix_PlayMusic(-1)                      (loop forever, per-level volume)
  \u2502     \u2502
  \u2502     \u2502   \u2500\u2500 Initialise game objects \u2500\u2500
  \u2502     \u251C\u2500\u2500 player_init(&gs.player, gs.renderer)
  \u2502     \u251C\u2500\u2500 fog_init(&gs.fog, gs.renderer)         (fog_background_1.png, fog_background_2.png)
  \u2502     \u251C\u2500\u2500 hud_init(&gs.hud, gs.renderer)
  \u2502     \u251C\u2500\u2500 if (debug_mode) debug_init(&gs.debug)
  \u2502     \u251C\u2500\u2500 level_loader_load(&gs)                 (load level from TOML, entity inits + floor gap positions)
  \u2502     \u251C\u2500\u2500 hearts/lives/score/score_life_next initialisation
  \u2502     \u251C\u2500\u2500 ctrl_pending_init = 1 \u2014 deferred gamepad init (avoids antivirus/HID delays)
  \u2502     \u2514\u2500\u2500 gamepad subsystem initializes on first rendered frame via background thread
  \u2502
  \u251C\u2500\u2500 game_loop(&gs)          \u2190 see Game Loop section below
  \u2502
  \u2514\u2500\u2500 game_cleanup(&gs)       \u2190 reverse init order
        \u251C\u2500\u2500 SDL_GameControllerClose(gs->controller)  \u2190 if non-NULL
        \u251C\u2500\u2500 SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER)
        \u251C\u2500\u2500 hud_cleanup
        \u251C\u2500\u2500 fog_cleanup
        \u251C\u2500\u2500 player_cleanup
        \u251C\u2500\u2500 Mix_HaltMusic + Mix_FreeMusic
        \u251C\u2500\u2500 Mix_FreeChunk (snd_jump)
        \u251C\u2500\u2500 Mix_FreeChunk (snd_coin)
        \u251C\u2500\u2500 Mix_FreeChunk (snd_hit)
        \u251C\u2500\u2500 Mix_FreeChunk (snd_spring)
        \u251C\u2500\u2500 Mix_FreeChunk (snd_axe)
        \u251C\u2500\u2500 Mix_FreeChunk (snd_flap)
        \u251C\u2500\u2500 Mix_FreeChunk (snd_spider_attack)
        \u251C\u2500\u2500 Mix_FreeChunk (snd_dive)
        \u251C\u2500\u2500 water_cleanup
        \u251C\u2500\u2500 SDL_DestroyTexture (fire_flame_tex)
        \u251C\u2500\u2500 SDL_DestroyTexture (blue_flame_tex)
        \u251C\u2500\u2500 SDL_DestroyTexture (axe_trap_tex)
        \u251C\u2500\u2500 SDL_DestroyTexture (circular_saw_tex)
        \u251C\u2500\u2500 SDL_DestroyTexture (spike_platform_tex)
        \u251C\u2500\u2500 SDL_DestroyTexture (spike_tex)
        \u251C\u2500\u2500 SDL_DestroyTexture (spike_block_tex)
        \u251C\u2500\u2500 SDL_DestroyTexture (bridge_tex)
        \u251C\u2500\u2500 SDL_DestroyTexture (float_platform_tex)
        \u251C\u2500\u2500 SDL_DestroyTexture (rail_tex)
        \u251C\u2500\u2500 SDL_DestroyTexture (bouncepad_high_tex)
        \u251C\u2500\u2500 SDL_DestroyTexture (bouncepad_medium_tex)
        \u251C\u2500\u2500 SDL_DestroyTexture (bouncepad_small_tex)
        \u251C\u2500\u2500 SDL_DestroyTexture (rope_tex)
        \u251C\u2500\u2500 SDL_DestroyTexture (ladder_tex)
        \u251C\u2500\u2500 SDL_DestroyTexture (vine_tex)
        \u251C\u2500\u2500 last_star_cleanup
        \u251C\u2500\u2500 SDL_DestroyTexture (star_red_tex)
        \u251C\u2500\u2500 SDL_DestroyTexture (star_green_tex)
        \u251C\u2500\u2500 SDL_DestroyTexture (star_yellow_tex)
        \u251C\u2500\u2500 SDL_DestroyTexture (coin_tex)
        \u251C\u2500\u2500 SDL_DestroyTexture (faster_fish_tex)
        \u251C\u2500\u2500 SDL_DestroyTexture (fish_tex)
        \u251C\u2500\u2500 SDL_DestroyTexture (faster_bird_tex)
        \u251C\u2500\u2500 SDL_DestroyTexture (bird_tex)
        \u251C\u2500\u2500 SDL_DestroyTexture (jumping_spider_tex)
        \u251C\u2500\u2500 SDL_DestroyTexture (spider_tex)
        \u251C\u2500\u2500 SDL_DestroyTexture (platform_tex)
        \u251C\u2500\u2500 SDL_DestroyTexture (floor_tile)
        \u251C\u2500\u2500 parallax_cleanup
        \u251C\u2500\u2500 SDL_DestroyRenderer
        \u2514\u2500\u2500 SDL_DestroyWindow
  \u2502
  \u251C\u2500\u2500 Mix_CloseAudio
  \u251C\u2500\u2500 TTF_Quit
  \u251C\u2500\u2500 IMG_Quit
  \u2514\u2500\u2500 SDL_Quit`}</code></pre>
  <hr />
  <h2>Game Loop</h2>
  <p>The loop runs at <strong>60 FPS</strong>, capped via VSync + a manual <code>SDL_Delay</code> fallback. Each frame has four distinct phases:</p>
  <pre><code>{`while (gs.running) {
  1. Delta Time   \u2014 measure ms since last frame \u2192 dt (seconds)
  2. Events       \u2014 SDL_PollEvent (quit / ESC key)
                    SDL_CONTROLLERDEVICEADDED   \u2014 opens a newly plugged-in controller
                    SDL_CONTROLLERDEVICEREMOVED \u2014 closes and NULLs gs->controller when unplugged
                    SDL_CONTROLLERBUTTONDOWN (START) \u2014 sets gs->running = 0 to quit
  3. Update       \u2014 player_handle_input \u2192 player_update (incl. bouncepad, float-platform, bridge landing)
                    \u2192 bouncepad response (animation + spring sound)
                    \u2192 spiders_update \u2192 jumping_spiders_update \u2192 birds_update \u2192 faster_birds_update
                    \u2192 fish_update \u2192 faster_fish_update \u2192 spike_blocks_update \u2192 spikes_update
                    \u2192 spike_platforms_update \u2192 circular_saws_update \u2192 axe_traps_update
                    \u2192 blue_flames_update \u2192 fire_flames_update \u2192 float_platforms_update \u2192 bridges_update
                    \u2192 spider collision \u2192 jumping_spider collision \u2192 bird collision \u2192 faster_bird collision
                    \u2192 fish collision \u2192 faster_fish collision \u2192 spike_block collision (+ push impulse)
                    \u2192 spike collision \u2192 spike_platform collision \u2192 circular_saw collision
                    \u2192 axe_trap collision \u2192 blue_flame collision \u2192 fire_flame collision
                    \u2192 floor gap fall detection (instant death)
                    \u2192 coin\u2013player collision \u2192 star_yellow\u2013player collision
                    \u2192 star_green\u2013player collision \u2192 star_red\u2013player collision
                    \u2192 last_star\u2013player collision
                    \u2192 heart/lives/score_life_next logic
                    \u2192 water_update \u2192 fog_update \u2192 bouncepads_update (small, medium, high)
                    \u2192 debug_update (if --debug)
  4. Render       \u2014 clear \u2192 parallax background \u2192 platforms \u2192 floor tiles
                    \u2192 float platforms \u2192 spike rows \u2192 spike platforms \u2192 bridges
                    \u2192 bouncepads (medium, small, high) \u2192 rails
                    \u2192 vines \u2192 ladders \u2192 ropes \u2192 coins \u2192 yellow stars
                    \u2192 green stars \u2192 red stars \u2192 last star
                    \u2192 blue flames \u2192 fire flames \u2192 fish \u2192 faster fish \u2192 water
                    \u2192 spike blocks \u2192 axe traps \u2192 circular saws
                    \u2192 spiders \u2192 jumping spiders \u2192 birds \u2192 faster birds
                    \u2192 player \u2192 fog \u2192 hud
                    \u2192 debug overlay (if --debug) \u2192 present
}`}</code></pre>
  <h3>Delta Time</h3>
  <pre><code className="language-c">{`Uint64 now = SDL_GetTicks64();
float  dt  = (float)(now - prev) / 1000.0f;
prev = now;`}</code></pre>
  <p>All velocities are expressed in <strong>pixels per second</strong>. Multiplying by <code>dt</code> (seconds) gives the correct displacement per frame regardless of the actual frame rate.</p>
  <h3>Render Order (back to front)</h3>
  <table>
    <thead><tr><th>Layer</th><th>What</th><th>How</th></tr></thead>
    <tbody>
      <tr><td>1</td><td>Background</td><td>Up to 8 layers from <code>assets/sprites/backgrounds/</code> configured per level via <code>[[background_layers]]</code> in TOML, tiled horizontally, each scrolling at a different speed fraction of <code>cam_x</code></td></tr>
      <tr><td>2</td><td>Platforms</td><td><code>platform.png</code> 9-slice tiled pillar stacks (drawn before floor so pillars sink into ground)</td></tr>
      <tr><td>3</td><td>Floor</td><td><code>grass_tileset.png</code> 9-slice tiled across world width at <code>FLOOR_Y</code>, with floor-gap openings</td></tr>
      <tr><td>4</td><td>Float platforms</td><td><code>float_platform.png</code> 3-slice hovering surfaces (static, crumble, rail modes)</td></tr>
      <tr><td>5</td><td>Spike rows</td><td><code>spike.png</code> ground-level spike strips on the floor surface</td></tr>
      <tr><td>6</td><td>Spike platforms</td><td><code>spike_platform.png</code> elevated spike hazard surfaces</td></tr>
      <tr><td>7</td><td>Bridges</td><td><code>bridge.png</code> tiled crumble walkways</td></tr>
      <tr><td>8</td><td>Bouncepads (medium)</td><td><code>bouncepad_medium.png</code> standard-launch spring pads</td></tr>
      <tr><td>9</td><td>Bouncepads (small)</td><td><code>bouncepad_small.png</code> low-launch spring pads</td></tr>
      <tr><td>10</td><td>Bouncepads (high)</td><td><code>bouncepad_high.png</code> high-launch spring pads</td></tr>
      <tr><td>11</td><td>Rails</td><td><code>rail.png</code> bitmask tile tracks for spike blocks and float platforms</td></tr>
      <tr><td>12</td><td>Vines</td><td><code>vine.png</code> climbable plant decorations hanging from platforms</td></tr>
      <tr><td>13</td><td>Ladders</td><td><code>ladder.png</code> climbable ladder structures</td></tr>
      <tr><td>14</td><td>Ropes</td><td><code>rope.png</code> climbable rope segments</td></tr>
      <tr><td>15</td><td>Coins</td><td><code>coin.png</code> collectible sprites drawn on top of platforms</td></tr>
      <tr><td>16</td><td>Star yellows</td><td><code>star_yellow.png</code> collectible star pickups</td></tr>
      <tr><td>17</td><td>Star greens</td><td><code>star_green.png</code> collectible star pickups</td></tr>
      <tr><td>18</td><td>Star reds</td><td><code>star_red.png</code> collectible star pickups</td></tr>
      <tr><td>19</td><td>Last star</td><td>end-of-level star collectible (uses HUD star sprite)</td></tr>
      <tr><td>20</td><td>Blue flames</td><td><code>blue_flame.png</code> animated flame hazards erupting from floor gaps</td></tr>
      <tr><td>21</td><td>Fire flames</td><td><code>fire_flame.png</code> animated fire variant flame hazards erupting from floor gaps</td></tr>
      <tr><td>22</td><td>Fish</td><td><code>fish.png</code> animated jumping enemies, drawn before water for submerged look</td></tr>
      <tr><td>23</td><td>Faster fish</td><td><code>faster_fish.png</code> fast aggressive jumping fish enemies</td></tr>
      <tr><td>24</td><td>Water</td><td><code>water.png</code> animated scrolling strip at the bottom</td></tr>
      <tr><td>25</td><td>Spike blocks</td><td><code>spike_block.png</code> rotating rail-riding hazards</td></tr>
      <tr><td>26</td><td>Axe traps</td><td><code>axe_trap.png</code> swinging axe hazards</td></tr>
      <tr><td>27</td><td>Circular saws</td><td><code>circular_saw.png</code> spinning blade hazards</td></tr>
      <tr><td>28</td><td>Spiders</td><td><code>spider.png</code> animated ground patrol enemies</td></tr>
      <tr><td>29</td><td>Jumping spiders</td><td><code>jumping_spider.png</code> animated jumping patrol enemies</td></tr>
      <tr><td>30</td><td>Birds</td><td><code>bird.png</code> slow sine-wave sky patrol enemies</td></tr>
      <tr><td>31</td><td>Faster birds</td><td><code>faster_bird.png</code> fast aggressive sky patrol enemies</td></tr>
      <tr><td>32</td><td>Player</td><td>Animated sprite sheet, drawn on top of environment</td></tr>
      <tr><td>33</td><td>Fog</td><td><code>fog_background_1.png</code> / <code>fog_background_2.png</code> semi-transparent sliding overlay</td></tr>
      <tr><td>34</td><td>HUD</td><td><code>hud_render</code>: hearts, lives, score -- always drawn on top</td></tr>
      <tr><td>35</td><td>Debug</td><td><code>debug_render</code>: FPS counter, collision boxes, event log -- when <code>--debug</code> active</td></tr>
    </tbody>
  </table>
  <hr />
  <h2>Coordinate System</h2>
  <p>SDL&#39;s Y-axis increases <strong>downward</strong>. The origin (0, 0) is at the <strong>top-left</strong> of the logical canvas.</p>
  <pre><code>{`(0,0) \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u25BA x  (GAME_W = 400)
  \u2502
  \u2502   LOGICAL CANVAS (400 \u00d7 300)
  \u2502
  \u25BC
  y
(GAME_H = 300)
              \u250C\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2510
              \u2502 \u2190\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500 GAME_W (400 px) \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u25BA \u2502
  FLOOR_Y \u2500\u2500\u25BA\u2502\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2593\u2502
  (300-48=252)\u2502          Grass Tileset (48px tall)        \u2502
              \u2514\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2518`}</code></pre>
  <p><code>SDL_RenderSetLogicalSize(renderer, 400, 300)</code> makes SDL scale this canvas <strong>2x</strong> to fill the 800x600 OS window automatically, giving the chunky pixel-art look with no changes to game logic.</p>
  <hr />
  <h2>GameState Struct</h2>
  <p>Defined in <code>game.h</code>. The <strong>single container</strong> for every runtime resource.</p>
  <pre><code className="language-c">{`typedef struct {
    SDL_Window         *window;      // OS window handle
    SDL_Renderer       *renderer;    // GPU drawing context
    SDL_GameController *controller;  // first connected gamepad; NULL = none
    ParallaxSystem      parallax;    // multi-layer scrolling background

    SDL_Texture   *floor_tile;       // grass_tileset.png (GPU)
    SDL_Texture   *platform_tex;     // Shared tile for platform pillars (GPU)

    SDL_Texture   *spider_tex;       // Shared texture for all spiders (GPU)
    Spider         spiders[MAX_SPIDERS];
    int            spider_count;

    SDL_Texture   *jumping_spider_tex;  // Shared texture for jumping spiders (GPU)
    JumpingSpider  jumping_spiders[MAX_JUMPING_SPIDERS];
    int            jumping_spider_count;

    SDL_Texture   *bird_tex;         // Shared texture for Bird enemies (GPU)
    Bird           birds[MAX_BIRDS];
    int            bird_count;

    SDL_Texture   *faster_bird_tex;  // Shared texture for FasterBird (GPU)
    FasterBird     faster_birds[MAX_FASTER_BIRDS];
    int            faster_bird_count;

    SDL_Texture   *fish_tex;         // Shared texture for all fish enemies (GPU)
    Fish           fish[MAX_FISH];
    int            fish_count;

    SDL_Texture   *faster_fish_tex;  // Shared texture for faster fish enemies (GPU)
    FasterFish     faster_fish[MAX_FASTER_FISH];
    int            faster_fish_count;

    SDL_Texture   *coin_tex;         // Shared texture for all coin collectibles (GPU)
    Coin           coins[MAX_COINS];
    int            coin_count;

    SDL_Texture   *star_yellow_tex;  // Shared texture for star yellow pickups (GPU)
    StarYellow     star_yellows[MAX_STAR_YELLOWS];
    int            star_yellow_count;

    SDL_Texture   *star_green_tex;   // Shared texture for star green pickups (GPU)
    StarYellow     star_greens[MAX_STAR_YELLOWS];
    int            star_green_count;

    SDL_Texture   *star_red_tex;     // Shared texture for star red pickups (GPU)
    StarYellow     star_reds[MAX_STAR_YELLOWS];
    int            star_red_count;

    LastStar       last_star;        // Special end-of-level star collectible

    SDL_Texture   *vine_tex;         // Shared texture for vine decorations (GPU)
    VineDecor      vines[MAX_VINES];
    int            vine_count;

    SDL_Texture   *ladder_tex;       // Shared texture for ladders (GPU)
    LadderDecor    ladders[MAX_LADDERS];
    int            ladder_count;

    SDL_Texture   *rope_tex;         // Shared texture for ropes (GPU)
    RopeDecor      ropes[MAX_ROPES];
    int            rope_count;

    SDL_Texture   *bouncepad_small_tex;    // Shared texture for small bouncepads (GPU)
    Bouncepad      bouncepads_small[MAX_BOUNCEPADS_SMALL];
    int            bouncepad_small_count;

    SDL_Texture   *bouncepad_medium_tex;   // Shared texture for medium bouncepads (GPU)
    Bouncepad      bouncepads_medium[MAX_BOUNCEPADS_MEDIUM];
    int            bouncepad_medium_count;

    SDL_Texture   *bouncepad_high_tex;     // Shared texture for high bouncepads (GPU)
    Bouncepad      bouncepads_high[MAX_BOUNCEPADS_HIGH];
    int            bouncepad_high_count;

    SDL_Texture   *rail_tex;         // Shared texture for all rail tiles (GPU)
    Rail           rails[MAX_RAILS];
    int            rail_count;

    SDL_Texture   *spike_block_tex;  // Shared texture for spike blocks (GPU)
    SpikeBlock     spike_blocks[MAX_SPIKE_BLOCKS];
    int            spike_block_count;

    SDL_Texture   *spike_tex;        // Shared texture for ground spikes (GPU)
    SpikeRow       spike_rows[MAX_SPIKE_ROWS];
    int            spike_row_count;

    SDL_Texture   *spike_platform_tex;  // Shared texture for spike platforms (GPU)
    SpikePlatform  spike_platforms[MAX_SPIKE_PLATFORMS];
    int            spike_platform_count;

    SDL_Texture   *circular_saw_tex;    // Shared texture for circular saws (GPU)
    CircularSaw    circular_saws[MAX_CIRCULAR_SAWS];
    int            circular_saw_count;

    SDL_Texture   *axe_trap_tex;        // Shared texture for axe traps (GPU)
    AxeTrap        axe_traps[MAX_AXE_TRAPS];
    int            axe_trap_count;

    SDL_Texture   *blue_flame_tex;      // Shared texture for blue flames (GPU)
    BlueFlame      blue_flames[MAX_BLUE_FLAMES];
    int            blue_flame_count;

    SDL_Texture   *fire_flame_tex;      // Shared texture for fire flames (GPU)
    BlueFlame      fire_flames[MAX_BLUE_FLAMES];
    int            fire_flame_count;

    SDL_Texture   *float_platform_tex;  // float_platform.png 3-slice (GPU)
    FloatPlatform  float_platforms[MAX_FLOAT_PLATFORMS];
    int            float_platform_count;

    SDL_Texture   *bridge_tex;       // bridge.png tile (GPU)
    Bridge         bridges[MAX_BRIDGES];
    int            bridge_count;

    Mix_Chunk     *snd_jump;         // Player jump sound effect (WAV)
    Mix_Chunk     *snd_coin;         // Coin collect sound effect (WAV)
    Mix_Chunk     *snd_hit;          // Player hurt sound effect (WAV)
    Mix_Chunk     *snd_spring;       // Bouncepad spring sound effect (WAV)
    Mix_Chunk     *snd_axe;          // Axe trap swing sound effect (WAV)
    Mix_Chunk     *snd_flap;         // Bird flap sound effect (WAV)
    Mix_Chunk     *snd_spider_attack;// Spider attack sound effect (WAV)
    Mix_Chunk     *snd_dive;         // Fish dive sound effect (WAV)
    Mix_Music     *music;            // Background music stream (WAV)

    Player         player;           // Player data \u2014 stored by value
    Platform       platforms[MAX_PLATFORMS];
    int            platform_count;
    Water          water;            // Animated water strip at the bottom
    FogSystem      fog;              // Atmospheric fog overlay \u2014 topmost layer

    int            floor_gaps[MAX_FLOOR_GAPS];
    int            floor_gap_count;

    Hud            hud;              // HUD display: hearts, lives, score
    int            hearts;           // Current hit points (0-MAX_HEARTS)
    int            lives;            // Remaining lives; <0 triggers game over
    int            score;            // Cumulative score from collecting coins/stars
    int            score_life_next;  // Score threshold for next bonus life

    Camera         camera;           // Viewport scroll position; updated every frame
    int            running;          // Loop flag: 1 = keep going, 0 = quit
    int            paused;           // 1 = window lost focus; physics/music frozen
    int            debug_mode;       // 1 = debug overlays active (--debug flag)
    DebugOverlay   debug;            // FPS counter, collision vis, event log

    char           level_path[256];  // Path to loaded TOML level file
    const void    *current_level;    // Pointer to active LevelDef
    int            fog_enabled;      // 1 = fog rendering active
    int            water_enabled;    // 1 = water strip rendered
    int            world_w;          // Dynamic level width (set per level)
    int            score_per_life;   // Per-level score threshold for bonus life
    int            coin_score;       // Per-level points per coin

    // ---- Gamepad lazy init (deferred to avoid antivirus/HID delays) ----
    int            ctrl_pending_init;   // 0=idle, 1=first frame, 2=thread running
    SDL_Thread    *ctrl_init_thread;    // Background init thread
    volatile int   ctrl_init_done;     // Thread completion flag
    SDL_Texture   *ctrl_init_msg_tex;  // Cached HUD "Initializing gamepad..." texture

    // ---- Loop state (persists across frames for emscripten callback) ----
    Uint64         loop_prev_ticks;  // timestamp of previous frame
    int            fp_prev_riding;   // float platform player stood on last frame
} GameState;`}</code></pre>
  <p><strong>Key design decisions:</strong></p>
  <ul>
    <li><code>Player</code> is <strong>embedded by value</strong>, not a pointer. This avoids a heap allocation and keeps the struct self-contained. The same applies to <code>Platform</code>, <code>Water</code>, <code>FogSystem</code>, and all entity arrays.</li>
    <li>Every pointer is set to <code>NULL</code> after freeing, making accidental double-frees safe.</li>
    <li>Initialised with <code>GameState gs = {'{'}0{'}'}</code> so every field starts as <code>0</code> / <code>NULL</code>.</li>
  </ul>
  <hr />
  <h2>Error Handling Strategy</h2>
  <table>
    <thead><tr><th>Situation</th><th>Action</th></tr></thead>
    <tbody>
      <tr><td>SDL subsystem init failure (in <code>main</code>)</td><td><code>fprintf(stderr, ...)</code> &#8594; clean up already-inited subsystems &#8594; <code>return EXIT_FAILURE</code></td></tr>
      <tr><td>Resource load failure (in <code>game_init</code>)</td><td><code>fprintf(stderr, ...)</code> &#8594; destroy already-created resources &#8594; <code>exit(EXIT_FAILURE)</code></td></tr>
      <tr><td>Sound load failure (non-fatal pattern)</td><td><code>fprintf(stderr, ...)</code> then continue -- play is guarded by <code>if (snd_jump)</code></td></tr>
      <tr><td>Optional texture load failure (non-fatal)</td><td><code>fprintf(stderr, ...)</code> then continue -- render is guarded by <code>if (texture)</code></td></tr>
    </tbody>
  </table>
  <p>All SDL error strings are retrieved with <code>SDL_GetError()</code>, <code>IMG_GetError()</code>, or <code>Mix_GetError()</code> and printed to <code>stderr</code>.</p>
</section>

{/* ============================================================ */}
{/* SECTION: ASSETS */}
{/* ============================================================ */}
<section id="assets" className={`doc-section${!isSectionVisible("assets") ? " hidden-section" : ""}`}>
  <h1 className="page-title">Assets</h1>
  <p><a href="home">&#8592; Home</a></p>
  <hr />
  <p>All visual assets live in the <code>assets/sprites/</code> directory, organized by category (backgrounds, collectibles, entities, foregrounds, hazards, levels, player, screens, surfaces). They are PNG files (loaded via <code>SDL2_image</code>). Fonts live in <code>assets/fonts/</code>.</p>
  <blockquote><p><strong>Coordinate note:</strong> All game objects use <strong>logical space (400x300)</strong>. SDL scales to the 800x600 OS window 2x. A 48x48 sprite appears as 96x96 physical pixels on screen.</p></blockquote>
  <hr />
  <h2>Currently Used Assets</h2>
  <table>
    <thead><tr><th>File</th><th>GameState Field / Used By</th><th>Description</th></tr></thead>
    <tbody>
      <tr><td><code>sprites/backgrounds/*.png</code></td><td><code>parallax.c</code> (configured per level via <code>[[background_layers]]</code>)</td><td>Up to 8 parallax layers (sky_blue, sky_fire, clouds, glacial/volcanic_mountains, forest_leafs, castle_pillars, smoke variants, etc.) -- each with a scroll speed factor (0.0-1.0)</td></tr>
      <tr><td><code>sprites/levels/grass_tileset.png</code></td><td><code>gs-&gt;floor_tile</code></td><td>48x48 tile, 9-slice rendered across <code>FLOOR_Y</code> to form the floor (per-level configurable via <code>floor_tile_path</code>)</td></tr>
      <tr><td><code>sprites/surfaces/Platform.png</code></td><td><code>gs-&gt;platform_tex</code></td><td>48x48 tile, 9-slice rendered as one-way platform pillars</td></tr>
      <tr><td><code>sprites/player/player.png</code></td><td><code>player-&gt;texture</code></td><td>192x288 sprite sheet, 4 cols x 6 rows, 48x48 frames</td></tr>
      <tr><td><code>sprites/foregrounds/water.png</code></td><td><code>water-&gt;texture</code></td><td>384x64 sprite sheet, 8 frames of 48x64 with 16x31 art crop</td></tr>
      <tr><td><code>sprites/entities/spider.png</code></td><td><code>gs-&gt;spider_tex</code></td><td>Spider enemy sprite sheet (ground patrol)</td></tr>
      <tr><td><code>sprites/entities/jumping_spider.png</code></td><td><code>gs-&gt;jumping_spider_tex</code></td><td>Jumping spider enemy sprite sheet</td></tr>
      <tr><td><code>sprites/entities/bird.png</code></td><td><code>gs-&gt;bird_tex</code></td><td>Slow sine-wave bird enemy sprite sheet</td></tr>
      <tr><td><code>sprites/entities/faster_bird.png</code></td><td><code>gs-&gt;faster_bird_tex</code></td><td>Fast aggressive bird enemy sprite sheet</td></tr>
      <tr><td><code>sprites/entities/fish.png</code></td><td><code>gs-&gt;fish_tex</code></td><td>Jumping fish enemy sprite sheet</td></tr>
      <tr><td><code>sprites/entities/faster_fish.png</code></td><td><code>gs-&gt;faster_fish_tex</code></td><td>Faster fish enemy sprite sheet</td></tr>
      <tr><td><code>sprites/collectibles/coin.png</code></td><td><code>gs-&gt;coin_tex</code></td><td>16x16 coin collectible sprite</td></tr>
      <tr><td><code>sprites/collectibles/star_yellow.png</code></td><td><code>gs-&gt;star_yellow_tex</code></td><td>Star yellow collectible sprite</td></tr>
      <tr><td><code>sprites/collectibles/star_green.png</code></td><td><code>gs-&gt;star_green_tex</code></td><td>Star green collectible sprite</td></tr>
      <tr><td><code>sprites/collectibles/star_red.png</code></td><td><code>gs-&gt;star_red_tex</code></td><td>Star red collectible sprite</td></tr>
      <tr><td><code>sprites/collectibles/last_star.png</code></td><td><code>last_star.c</code></td><td>Last star goal collectible sprite</td></tr>
      <tr><td><code>sprites/hazards/spike.png</code></td><td><code>gs-&gt;spike_tex</code></td><td>Floor/ceiling spike hazard</td></tr>
      <tr><td><code>sprites/hazards/spike_block.png</code></td><td><code>gs-&gt;spike_block_tex</code></td><td>Rail-riding rotating hazard sprite</td></tr>
      <tr><td><code>sprites/hazards/spike_platform.png</code></td><td><code>gs-&gt;spike_platform_tex</code></td><td>Spiked platform hazard sprite</td></tr>
      <tr><td><code>sprites/hazards/circular_saw.png</code></td><td><code>gs-&gt;circular_saw_tex</code></td><td>Rotating saw blade hazard</td></tr>
      <tr><td><code>sprites/hazards/axe_trap.png</code></td><td><code>gs-&gt;axe_trap_tex</code></td><td>Swinging axe trap hazard</td></tr>
      <tr><td><code>sprites/hazards/blue_flame.png</code></td><td><code>gs-&gt;blue_flame_tex</code></td><td>Blue flame hazard sprite</td></tr>
      <tr><td><code>sprites/hazards/fire_flame.png</code></td><td><code>gs-&gt;fire_flame_tex</code></td><td>Fire flame hazard sprite (fire-colored variant)</td></tr>
      <tr><td><code>sprites/surfaces/float_platform.png</code></td><td><code>gs-&gt;float_platform_tex</code></td><td>48x16 sprite, 3-slice horizontal strip (left cap, centre fill, right cap)</td></tr>
      <tr><td><code>sprites/surfaces/bridge.png</code></td><td><code>gs-&gt;bridge_tex</code></td><td>16x16 single-frame brick tile for crumble walkways</td></tr>
      <tr><td><code>sprites/surfaces/bouncepad_small.png</code></td><td><code>gs-&gt;bouncepad_small_tex</code></td><td>Small bouncepad sprite (low launch)</td></tr>
      <tr><td><code>sprites/surfaces/bouncepad_medium.png</code></td><td><code>gs-&gt;bouncepad_medium_tex</code></td><td>Medium bouncepad sprite (standard launch)</td></tr>
      <tr><td><code>sprites/surfaces/bouncepad_high.png</code></td><td><code>gs-&gt;bouncepad_high_tex</code></td><td>High bouncepad sprite (max launch)</td></tr>
      <tr><td><code>sprites/surfaces/rail.png</code></td><td><code>gs-&gt;rail_tex</code></td><td>64x64 sprite sheet, 4x4 grid of 16x16 bitmask rail tiles</td></tr>
      <tr><td><code>sprites/surfaces/vine.png</code></td><td><code>gs-&gt;vine_tex</code></td><td>16x48 single-frame plant sprite for climbable vines</td></tr>
      <tr><td><code>sprites/surfaces/ladder.png</code></td><td><code>gs-&gt;ladder_tex</code></td><td>Climbable ladder sprite</td></tr>
      <tr><td><code>sprites/surfaces/rope.png</code></td><td><code>gs-&gt;rope_tex</code></td><td>Climbable rope sprite</td></tr>
      <tr><td><code>sprites/foregrounds/fog_background_1.png</code></td><td><code>fog.c</code> (<code>fog-&gt;textures[0]</code>)</td><td>Fog overlay layer, semi-transparent sliding effect</td></tr>
      <tr><td><code>sprites/foregrounds/fog_background_2.png</code></td><td><code>fog.c</code> (<code>fog-&gt;textures[1]</code>)</td><td>Fog overlay layer, semi-transparent sliding effect</td></tr>
      <tr><td><code>sprites/screens/hud_coins.png</code></td><td><code>hud.c</code></td><td>Coin count UI icon used in the HUD</td></tr>
      <tr><td><code>sprites/screens/start_menu_logo.png</code></td><td><code>start_menu.c</code></td><td>Game logo displayed on the start menu screen</td></tr>
      <tr><td><code>fonts/round9x13.ttf</code></td><td><code>hud.c</code> (<code>hud-&gt;font</code>)</td><td>Bitmap font for score and lives text in the HUD</td></tr>
    </tbody>
  </table>
  <hr />
  <h2>Player Sprite Sheet -- <code>player.png</code></h2>
  <p><strong>Sheet dimensions:</strong> 192 x 288 px<br /><strong>Grid:</strong> 4 columns x 6 rows<br /><strong>Frame size:</strong> 48 x 48 px</p>
  <h3>Animation Row Map</h3>
  <table>
    <thead><tr><th>Row</th><th>AnimState</th><th>Frame Count</th><th>Frame Duration</th><th>Notes</th></tr></thead>
    <tbody>
      <tr><td>0</td><td><code>ANIM_IDLE</code></td><td>4</td><td>150 ms/frame</td><td>Subtle breathing cycle</td></tr>
      <tr><td>1</td><td><code>ANIM_WALK</code></td><td>4</td><td>100 ms/frame</td><td>Looping run cycle</td></tr>
      <tr><td>2</td><td><code>ANIM_JUMP</code></td><td>2</td><td>150 ms/frame</td><td>Rising phase poses</td></tr>
      <tr><td>3</td><td><code>ANIM_FALL</code></td><td>1</td><td>200 ms/frame</td><td>Descent pose</td></tr>
      <tr><td>4</td><td><code>ANIM_CLIMB</code></td><td>2</td><td>100 ms/frame</td><td>Vine climbing cycle</td></tr>
      <tr><td>5</td><td><em>(unused)</em></td><td>--</td><td>--</td><td>Available for future states</td></tr>
    </tbody>
  </table>
  <h3>Frame Source Rect Formula</h3>
  <pre><code className="language-c">{`frame.x = anim_frame_index * FRAME_W;   // column x 48
frame.y = ANIM_ROW[anim_state] * FRAME_H; // row x 48`}</code></pre>
  <h3>Horizontal Flipping</h3>
  <p>When <code>player-&gt;facing_left == 1</code>, the sprite is drawn with <code>SDL_FLIP_HORIZONTAL</code> via <code>SDL_RenderCopyEx</code>. This means the same right-facing animation frames are used for both directions -- no duplicate assets needed.</p>
  <hr />
  <h2>Unused Assets</h2>
  <p>The following assets are stored in <code>assets/sprites/unused/</code> and are not loaded by the game. They are available as reserves for future use.</p>
  <table>
    <thead><tr><th>File</th><th>Description</th></tr></thead>
    <tbody>
      <tr><td><code>brick_oneway.png</code></td><td>One-way brick platform tile</td></tr>
      <tr><td><code>brick_tileset.png</code></td><td>Brick wall / platform tile</td></tr>
      <tr><td><code>castle_background_0.png</code></td><td>Castle/dungeon interior background</td></tr>
      <tr><td><code>cloud_tileset.png</code></td><td>Cloud platform tile</td></tr>
      <tr><td><code>clouds.png</code></td><td>Decorative cloud layer</td></tr>
      <tr><td><code>clouds_mg_1_lightened.png</code></td><td>Lightened midground cloud variant</td></tr>
      <tr><td><code>flame_1.png</code></td><td>Flame hazard variant</td></tr>
      <tr><td><code>forest_background_0.png</code></td><td>Forest scene background</td></tr>
      <tr><td><code>glacial_mountains_lightened.png</code></td><td>Lightened mountain variant</td></tr>
      <tr><td><code>grass_rock_oneway.png</code></td><td>One-way grass + rock platform</td></tr>
      <tr><td><code>grass_rock_tileset.png</code></td><td>Grass + rock mixed tileset</td></tr>
      <tr><td><code>lava.png</code></td><td>Lava hazard tile</td></tr>
      <tr><td><code>leaf_tileset.png</code></td><td>Leaf / foliage platform tile</td></tr>
      <tr><td><code>sky_background_0.png</code></td><td>Sky gradient background</td></tr>
      <tr><td><code>sky_lightened.png</code></td><td>Lightened sky variant</td></tr>
      <tr><td><code>stone_tileset.png</code></td><td>Stone floor / wall tile</td></tr>
    </tbody>
  </table>
  <hr />
  <h2>Sprite Sheet Analysis</h2>
  <p>To inspect any sprite sheet&#39;s exact dimensions and pixel layout:</p>
  <pre><code className="language-sh">{`python3 .claude/scripts/analyze_sprite.py assets/<sprite>.png`}</code></pre>
  <h3>Frame Math Reference</h3>
  <pre><code>{`Sheet width  = cols x frame_w
Sheet height = rows x frame_h

source_x = (frame_index % cols) * frame_w
source_y = (frame_index / cols) * frame_h`}</code></pre>
  <hr />
  <h2>Loading an Asset</h2>
  <pre><code className="language-c">{`// In game_init or an entity's init function:
SDL_Texture *tex = IMG_LoadTexture(gs->renderer, "assets/sprites/collectibles/coin.png");
if (!tex) {
    fprintf(stderr, "Failed to load coin.png: %s\\n", IMG_GetError());
    exit(EXIT_FAILURE);
}

// At cleanup (reverse init order):
if (tex) { SDL_DestroyTexture(tex); tex = NULL; }`}</code></pre>
</section>

{/* ============================================================ */}
{/* SECTION: BUILD SYSTEM */}
{/* ============================================================ */}
<section id="build-system" className={`doc-section${!isSectionVisible("build-system") ? " hidden-section" : ""}`}>
  <h1 className="page-title">Build System</h1>
  <p><a href="home">&#8592; Home</a></p>
  <hr />
  <h2>Makefile Overview</h2>
  <p>The project uses a <strong>GNU Makefile</strong> that auto-discovers source files via a wildcard -- no manual edits required when adding new <code>.c</code> files.</p>
  <pre><code className="language-makefile">{`CC      = clang
CFLAGS  = -std=c11 -Wall -Wextra -Wpedantic $(shell sdl2-config --cflags)
LIBS    = $(shell sdl2-config --libs) -lSDL2_image -lSDL2_ttf -lSDL2_mixer -lm
OUTDIR  = out
TARGET  = $(OUTDIR)/super-mango
SRCDIR  = src
SRCS    = $(wildcard $(SRCDIR)/*.c) \\
          $(wildcard $(SRCDIR)/collectibles/*.c) \\
          $(wildcard $(SRCDIR)/core/*.c) \\
          $(wildcard $(SRCDIR)/effects/*.c) \\
          $(wildcard $(SRCDIR)/entities/*.c) \\
          $(wildcard $(SRCDIR)/hazards/*.c) \\
          $(wildcard $(SRCDIR)/levels/*.c) \\
          $(wildcard $(SRCDIR)/player/*.c) \\
          $(wildcard $(SRCDIR)/screens/*.c) \\
          $(wildcard $(SRCDIR)/surfaces/*.c) \\
          $(SRCDIR)/editor/serializer.c \\
          vendor/tomlc17/tomlc17.c
OBJS    = $(SRCS:.c=.o)
DEPS    = $(OBJS:.o=.d)`}</code></pre>
  <h3>Key Variables</h3>
  <table>
    <thead><tr><th>Variable</th><th>Value</th><th>Description</th></tr></thead>
    <tbody>
      <tr><td><code>CC</code></td><td><code>clang</code></td><td>C compiler (override with <code>CC=gcc</code> if needed)</td></tr>
      <tr><td><code>CFLAGS</code></td><td>see below</td><td>Compiler flags</td></tr>
      <tr><td><code>LIBS</code></td><td>see below</td><td>Linker flags</td></tr>
      <tr><td><code>TARGET</code></td><td><code>out/super-mango</code></td><td>Output binary path</td></tr>
      <tr><td><code>SRCS</code></td><td><code>src/**/*.c</code></td><td>All C source files per subdirectory (auto-discovered via explicit wildcards)</td></tr>
      <tr><td><code>OBJS</code></td><td><code>src/*.o</code></td><td>Object files, placed next to sources</td></tr>
      <tr><td><code>DEPS</code></td><td><code>src/*.d</code></td><td>Auto-generated dependency files (tracks header changes)</td></tr>
    </tbody>
  </table>
  <h3>Compiler Flags Explained</h3>
  <table>
    <thead><tr><th>Flag</th><th>Meaning</th></tr></thead>
    <tbody>
      <tr><td><code>-std=c11</code></td><td>Compile as C11 (ISO/IEC 9899:2011)</td></tr>
      <tr><td><code>-Wall</code></td><td>Enable common warnings</td></tr>
      <tr><td><code>-Wextra</code></td><td>Enable extra warnings beyond <code>-Wall</code></td></tr>
      <tr><td><code>-Wpedantic</code></td><td>Strict ISO compliance warnings</td></tr>
      <tr><td><code>-MMD</code></td><td>Generate <code>.d</code> dependency files for each <code>.o</code> (tracks header changes) -- passed in compile rule, not in <code>CFLAGS</code></td></tr>
      <tr><td><code>-MP</code></td><td>Add phony targets for each dependency (prevents errors when headers are deleted) -- passed in compile rule, not in <code>CFLAGS</code></td></tr>
      <tr><td><code>$(shell sdl2-config --cflags)</code></td><td>SDL2 include paths (<code>-I/opt/homebrew/include/SDL2</code>)</td></tr>
    </tbody>
  </table>
  <h3>Linker Flags Explained</h3>
  <table>
    <thead><tr><th>Flag</th><th>Meaning</th></tr></thead>
    <tbody>
      <tr><td><code>$(shell sdl2-config --libs)</code></td><td>SDL2 core library (<code>-L/opt/homebrew/lib -lSDL2</code>)</td></tr>
      <tr><td><code>-lSDL2_image</code></td><td>PNG/JPG texture loading</td></tr>
      <tr><td><code>-lSDL2_ttf</code></td><td>TrueType font rendering</td></tr>
      <tr><td><code>-lSDL2_mixer</code></td><td>Audio mixing (WAV, MP3, OGG)</td></tr>
      <tr><td><code>-lm</code></td><td>Math library (<code>math.h</code> functions: <code>sinf</code>, <code>cosf</code>, <code>fmodf</code>, etc.)</td></tr>
    </tbody>
  </table>
  <hr />
  <h2>Build Targets</h2>
  <h3><code>make</code> / <code>make all</code></h3>
  <p>Compiles all <code>src/**/*.c</code> files (across all subdirectories) to <code>.o</code> objects, then links them into <code>out/super-mango</code>.</p>
  <pre><code className="language-sh">make</code></pre>
  <p><strong>Steps:</strong></p>
  <ol>
    <li>Creates <code>out/</code> directory if it does not exist</li>
    <li>Compiles each <code>src/**/*.c</code> &#8594; <code>src/**/*.o</code></li>
    <li>Links all <code>.o</code> files &#8594; <code>out/super-mango</code></li>
    <li>On macOS (<code>uname -s == Darwin</code>), ad-hoc code signs the binary with <code>codesign --force --sign - $@</code> (required on Apple Silicon to avoid <code>Killed: 9</code> errors). On other platforms this step is skipped</li>
  </ol>
  <h3><code>make run</code></h3>
  <p>Builds (if out of date) then immediately executes the binary (no CLI flags).</p>
  <pre><code className="language-sh">make run</code></pre>
  <p>The binary must be run from the <strong>repo root</strong> because asset paths are relative:</p>
  <pre><code className="language-c">{`IMG_LoadTexture(renderer, "assets/sprites/backgrounds/sky_blue.png");
Mix_LoadWAV("assets/sounds/player/player_jump.wav");`}</code></pre>
  <h3><code>make run-debug</code></h3>
  <p>Builds (if out of date) then runs the binary with the <code>--debug</code> flag, which enables the debug overlay: FPS counter, collision hitbox visualization, and scrolling event log.</p>
  <pre><code className="language-sh">make run-debug</code></pre>
  <h3><code>make run-level LEVEL=&lt;path&gt;</code></h3>
  <p>Builds (if out of date) then runs the binary with the <code>--level &lt;path&gt;</code> flag, which loads the specified TOML level file.</p>
  <pre><code className="language-sh">{`make run-level LEVEL=levels/00_sandbox_01.toml`}</code></pre>
  <h3><code>make run-level-debug LEVEL=&lt;path&gt;</code></h3>
  <p>Builds (if out of date) then runs the binary with both <code>--level &lt;path&gt;</code> and <code>--debug</code> flags, combining level loading with the debug overlay.</p>
  <pre><code className="language-sh">{`make run-level-debug LEVEL=levels/00_sandbox_01.toml`}</code></pre>
  <h3><code>make editor</code></h3>
  <p>Compiles the standalone visual level editor binary to <code>out/super-mango-editor</code>.</p>
  <pre><code className="language-sh">make editor</code></pre>
  <h3><code>make run-editor</code></h3>
  <p>Builds (if out of date) then launches the level editor.</p>
  <pre><code className="language-sh">make run-editor</code></pre>
  <h3><code>make web</code></h3>
  <p>Compiles the game to WebAssembly using the Emscripten SDK (<code>emcc</code>). Requires Emscripten to be installed and <code>emcc</code> on <code>PATH</code>.</p>
  <pre><code className="language-sh">make web</code></pre>
  <p>Produces <code>out/super-mango.html</code>, <code>.js</code>, <code>.wasm</code>, and <code>.data</code> (bundled assets/sounds). SDL2 ports are compiled from source by Emscripten on first build; subsequent builds reuse cached port libraries. Uses a custom shell template from <code>web/shell.html</code>.</p>
  <h3><code>make clean</code></h3>
  <p>Removes all build artifacts.</p>
  <pre><code className="language-sh">make clean</code></pre>
  <p>Deletes:</p>
  <ul>
    <li><code>src/**/*.o</code> -- all object files across subdirectories</li>
    <li><code>src/**/*.d</code> -- all generated dependency files</li>
    <li><code>out/</code> -- the output directory and binary</li>
  </ul>
  <hr />
  <h2>Prerequisites</h2>
  <h3>macOS (Apple Silicon / Intel)</h3>
  <pre><code className="language-sh">{`# Install Homebrew if needed: https://brew.sh
brew install sdl2 sdl2_image sdl2_ttf sdl2_mixer

# Xcode Command Line Tools (provides clang and make)
xcode-select --install`}</code></pre>
  <p>SDL2 libraries are installed to <code>/opt/homebrew/</code> on Apple Silicon. <code>sdl2-config</code> resolves the correct paths automatically.</p>
  <h3>Linux -- Debian / Ubuntu</h3>
  <pre><code className="language-sh">{`sudo apt update
sudo apt install build-essential clang \\
    libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev libsdl2-mixer-dev`}</code></pre>
  <h3>Linux -- Fedora / RHEL / CentOS</h3>
  <pre><code className="language-sh">{`sudo dnf install clang make \\
    SDL2-devel SDL2_image-devel SDL2_ttf-devel SDL2_mixer-devel`}</code></pre>
  <h3>Linux -- Arch Linux</h3>
  <pre><code className="language-sh">{`sudo pacman -S clang make sdl2 sdl2_image sdl2_ttf sdl2_mixer`}</code></pre>
  <h3>Windows (MSYS2)</h3>
  <ol>
    <li>Install <a href="https://www.msys2.org/">MSYS2</a></li>
    <li>Open the <strong>MSYS2 UCRT64</strong> terminal:</li>
  </ol>
  <pre><code className="language-sh">{`pacman -S mingw-w64-ucrt-x86_64-clang \\
          mingw-w64-ucrt-x86_64-make \\
          mingw-w64-ucrt-x86_64-SDL2 \\
          mingw-w64-ucrt-x86_64-SDL2_image \\
          mingw-w64-ucrt-x86_64-SDL2_ttf \\
          mingw-w64-ucrt-x86_64-SDL2_mixer`}</code></pre>
  <ol start={3}><li>Build:</li></ol>
  <pre><code className="language-sh">{`cd /c/path/to/super-mango-editor
make`}</code></pre>
  <ol start={4}><li>SDL2 DLLs must be in the same directory as the binary. Copy them from the MSYS2 prefix.</li></ol>
  <hr />
  <h2>CI/CD Pipelines</h2>
  <p>Three GitHub Actions workflows:</p>
  <table>
    <thead><tr><th>Workflow</th><th>File</th><th>Trigger</th><th>Purpose</th></tr></thead>
    <tbody>
      <tr><td>Build &amp; Release</td><td><code>build.yml</code></td><td>Push to <code>main</code>, pull requests</td><td>Multi-platform build (Linux x86_64, macOS arm64, Windows x86_64, WebAssembly); on main push: GitHub Release creation + Pages deployment of WebAssembly build</td></tr>
      <tr><td>CodeQL</td><td><code>codeql.yml</code></td><td>Push/PR to <code>main</code>, weekly</td><td>Automated code security and quality analysis</td></tr>
      <tr><td>Deploy</td><td><code>deploy.yml</code></td><td>Push to <code>main</code>, manual</td><td>Deploys <code>docs/</code> to GitHub Pages via actions/deploy-pages</td></tr>
    </tbody>
  </table>
  <p>All workflows install SDL2 dependencies per platform and compile with the project Makefile. The Deploy workflow handles static site deployment only.</p>
  <hr />
  <h2>Adding New Source Files</h2>
  <p>The Makefile uses per-subdirectory wildcards. Any new <code>.c</code> file placed in <code>src/</code> or its recognized subdirectories (<code>collectibles/</code>, <code>core/</code>, <code>effects/</code>, <code>entities/</code>, <code>hazards/</code>, <code>levels/</code>, <code>player/</code>, <code>screens/</code>, <code>surfaces/</code>) is compiled automatically. <strong>New subdirectories</strong> require adding a wildcard line to the Makefile.</p>
  <pre><code className="language-sh">{`# Example: adding a new enemy entity
touch src/entities/new_enemy.c src/entities/new_enemy.h
make   # new_enemy.c is compiled automatically`}</code></pre>
  <p>See <a href="developer_guide">Developer Guide</a> for the full new-entity workflow.</p>
  <hr />
  <h2>Output Structure</h2>
  <p>After a successful build:</p>
  <pre><code>{`out/
\u2514\u2500\u2500 super-mango          \u2190 the game binary
src/
\u251C\u2500\u2500 main.o
\u251C\u2500\u2500 game.o
\u251C\u2500\u2500 collectibles/        \u2190 coin.o, star_yellow.o, star_green.o, star_red.o, last_star.o
\u251C\u2500\u2500 core/                \u2190 debug.o, entity_utils.o
\u251C\u2500\u2500 effects/             \u2190 fog.o, parallax.o, water.o
\u251C\u2500\u2500 entities/            \u2190 spider.o, jumping_spider.o, bird.o, faster_bird.o, fish.o, faster_fish.o
\u251C\u2500\u2500 hazards/             \u2190 spike.o, spike_block.o, spike_platform.o, circular_saw.o, axe_trap.o, blue_flame.o
\u251C\u2500\u2500 levels/              \u2190 level_loader.o
\u251C\u2500\u2500 player/              \u2190 player.o
\u251C\u2500\u2500 screens/             \u2190 hud.o, start_menu.o
\u251C\u2500\u2500 surfaces/            \u2190 platform.o, float_platform.o, bridge.o, bouncepad.o, bouncepad_*.o, rail.o, vine.o, ladder.o, rope.o
\u251C\u2500\u2500 editor/              \u2190 serializer.o (shared with game build)
\u2514\u2500\u2500 (plus corresponding .d dependency files)
vendor/
\u2514\u2500\u2500 tomlc17/tomlc17.o`}</code></pre>
</section>

{/* ============================================================ */}
{/* SECTION: CONSTANTS REFERENCE */}
{/* ============================================================ */}
<section id="constants-reference" className={`doc-section${!isSectionVisible("constants-reference") ? " hidden-section" : ""}`}>
  <h1 className="page-title">Constants Reference</h1>
  <p><a href="home">&#8592; Home</a></p>
  <hr />
  <p>A complete reference for every compile-time constant in the codebase.</p>
  <hr />
  <h2><code>game.h</code> Constants</h2>
  <p>These are available to every file that <code>#include &quot;game.h&quot;</code>.</p>
  <h3>Window</h3>
  <table>
    <thead><tr><th>Constant</th><th>Value</th><th>Description</th></tr></thead>
    <tbody>
      <tr><td><code>WINDOW_TITLE</code></td><td><code>&quot;Super Mango&quot;</code></td><td>Text shown in the OS title bar</td></tr>
      <tr><td><code>WINDOW_W</code></td><td><code>800</code></td><td>OS window width in physical pixels</td></tr>
      <tr><td><code>WINDOW_H</code></td><td><code>600</code></td><td>OS window height in physical pixels</td></tr>
    </tbody>
  </table>
  <blockquote><p><strong>Do not use <code>WINDOW_W</code> / <code>WINDOW_H</code> for game object math.</strong> All game objects live in logical space.</p></blockquote>
  <h3>Logical Canvas</h3>
  <table>
    <thead><tr><th>Constant</th><th>Value</th><th>Description</th></tr></thead>
    <tbody>
      <tr><td><code>GAME_W</code></td><td><code>400</code></td><td>Internal canvas width in logical pixels</td></tr>
      <tr><td><code>GAME_H</code></td><td><code>300</code></td><td>Internal canvas height in logical pixels</td></tr>
    </tbody>
  </table>
  <p><code>SDL_RenderSetLogicalSize(renderer, GAME_W, GAME_H)</code> makes SDL scale every draw call from 400x300 up to 800x600 automatically, producing a <strong>2x pixel scale</strong> (each logical pixel = 2x2 physical pixels).</p>
  <h3>Timing</h3>
  <table>
    <thead><tr><th>Constant</th><th>Value</th><th>Description</th></tr></thead>
    <tbody><tr><td><code>TARGET_FPS</code></td><td><code>60</code></td><td>Desired frames per second</td></tr></tbody>
  </table>
  <p>Used to compute <code>frame_ms = 1000 / TARGET_FPS</code> (approximately 16 ms), which is the manual frame-cap duration when VSync is unavailable.</p>
  <h3>Tiles and Floor</h3>
  <table>
    <thead><tr><th>Constant</th><th>Value</th><th>Expression</th><th>Description</th></tr></thead>
    <tbody>
      <tr><td><code>TILE_SIZE</code></td><td><code>48</code></td><td>literal</td><td>Width and height of one grass tile (px)</td></tr>
      <tr><td><code>FLOOR_Y</code></td><td><code>252</code></td><td><code>GAME_H - TILE_SIZE</code></td><td>Y coordinate of the floor&#39;s top edge</td></tr>
    </tbody>
  </table>
  <p>The floor is drawn by repeating the 48x48 grass tile across the full <code>WORLD_W</code> at <code>y=FLOOR_Y</code>, with gaps cut out at each <code>floor_gaps[]</code> position.</p>
  <h3>Physics</h3>
  <table>
    <thead><tr><th>Constant</th><th>Value</th><th>Type</th><th>Description</th></tr></thead>
    <tbody>
      <tr><td><code>GRAVITY</code></td><td><code>800.0f</code></td><td><code>float</code></td><td>Downward acceleration in px/s^2</td></tr>
      <tr><td><code>FLOOR_GAP_W</code></td><td><code>32</code></td><td><code>int</code></td><td>Width of each floor gap in logical pixels</td></tr>
      <tr><td><code>MAX_FLOOR_GAPS</code></td><td><code>16</code></td><td><code>int</code></td><td>Maximum number of floor gaps per level</td></tr>
    </tbody>
  </table>
  <p>Every frame while airborne: <code>player-&gt;vy += GRAVITY * dt</code>.</p>
  <p>At 60 FPS (<code>dt</code> approximately 0.016s) gravity adds ~12.8 px/s per frame. The jump impulse (<code>-325.0f</code> px/s) produces a moderate arc.</p>
  <h3>Camera</h3>
  <table>
    <thead><tr><th>Constant</th><th>Value</th><th>Type</th><th>Description</th></tr></thead>
    <tbody>
      <tr><td><code>WORLD_W</code></td><td><code>1600</code></td><td><code>int</code></td><td>Total logical level width (4 x GAME_W)</td></tr>
      <tr><td><code>CAM_LOOKAHEAD_VX_FACTOR</code></td><td><code>0.20f</code></td><td><code>float</code></td><td>Pixels of lookahead per px/s of player velocity (dynamic lookahead)</td></tr>
      <tr><td><code>CAM_LOOKAHEAD_MAX</code></td><td><code>50.0f</code></td><td><code>float</code></td><td>Maximum forward-look offset in px</td></tr>
      <tr><td><code>CAM_SMOOTHING</code></td><td><code>8.0f</code></td><td><code>float</code></td><td>Lerp speed factor (per second); higher = snappier follow</td></tr>
      <tr><td><code>CAM_SNAP_THRESHOLD</code></td><td><code>0.5f</code></td><td><code>float</code></td><td>Sub-pixel distance at which the camera snaps exactly to target</td></tr>
    </tbody>
  </table>
  <p><code>WORLD_W</code> defines the full scrollable level width. The visible canvas is always <code>GAME_W</code> (400 px); the <code>Camera</code> struct tracks the left edge of the viewport in world coordinates.</p>
  <hr />
  <h2><code>player.c</code> Local Constants</h2>
  <p>These are <code>#define</code>s local to <code>player.c</code> (not visible to other files).</p>
  <table>
    <thead><tr><th>Constant</th><th>Value</th><th>Description</th></tr></thead>
    <tbody>
      <tr><td><code>FRAME_W</code></td><td><code>48</code></td><td>Width of one sprite frame in the sheet (px)</td></tr>
      <tr><td><code>FRAME_H</code></td><td><code>48</code></td><td>Height of one sprite frame in the sheet (px)</td></tr>
      <tr><td><code>FLOOR_SINK</code></td><td><code>16</code></td><td>Visual overlap onto the floor tile to prevent floating feet</td></tr>
      <tr><td><code>PHYS_PAD_X</code></td><td><code>15</code></td><td>Pixels trimmed from each horizontal side of the frame for the physics box (physics width = 48 - 30 = 18 px)</td></tr>
      <tr><td><code>PHYS_PAD_TOP</code></td><td><code>18</code></td><td>Pixels trimmed from the top of the frame for the physics box (physics height = 48 - 18 - 16 = 14; combined with FLOOR_SINK gives a 30 px tall box)</td></tr>
    </tbody>
  </table>
  <h3>Why <code>FLOOR_SINK</code>?</h3>
  <p>The <code>player.png</code> sprite sheet has transparent padding at the <strong>bottom</strong> of each 48x48 frame. Without the sink offset, the physics floor edge (<code>y + h = FLOOR_Y</code>) would leave the character visually floating 16 px above the grass. <code>FLOOR_SINK</code> compensates:</p>
  <pre><code>{`floor_snap = FLOOR_Y - player->h + FLOOR_SINK
           = 252      - 48        + 16
           = 220`}</code></pre>
  <p>The character&#39;s sprite appears to rest naturally on the grass at that Y.</p>
  <hr />
  <h2>Animation Tables in <code>player.c</code></h2>
  <p>Static arrays indexed by <code>AnimState</code> (0 = <code>ANIM_IDLE</code>, 1 = <code>ANIM_WALK</code>, 2 = <code>ANIM_JUMP</code>, 3 = <code>ANIM_FALL</code>, 4 = <code>ANIM_CLIMB</code>):</p>
  <pre><code className="language-c">{`static const int ANIM_FRAME_COUNT[5] = { 4,   4,   2,   1,   2   };
static const int ANIM_FRAME_MS[5]    = { 150, 100, 150, 200, 100 };
static const int ANIM_ROW[5]         = { 0,   1,   2,   3,   4   };`}</code></pre>
  <table>
    <thead><tr><th>Index</th><th>State</th><th>Frames</th><th>ms/frame</th><th>Sheet row</th></tr></thead>
    <tbody>
      <tr><td>0</td><td><code>ANIM_IDLE</code></td><td>4</td><td>150</td><td>Row 0</td></tr>
      <tr><td>1</td><td><code>ANIM_WALK</code></td><td>4</td><td>100</td><td>Row 1</td></tr>
      <tr><td>2</td><td><code>ANIM_JUMP</code></td><td>2</td><td>150</td><td>Row 2</td></tr>
      <tr><td>3</td><td><code>ANIM_FALL</code></td><td>1</td><td>200</td><td>Row 3</td></tr>
      <tr><td>4</td><td><code>ANIM_CLIMB</code></td><td>2</td><td>100</td><td>Row 4</td></tr>
    </tbody>
  </table>
  <hr />
  <h2>Movement Constants in <code>player.c</code></h2>
  <table>
    <thead><tr><th>Constant</th><th>Value</th><th>Description</th></tr></thead>
    <tbody>
      <tr><td><code>WALK_MAX_SPEED</code></td><td><code>100.0f</code></td><td>Maximum walking speed (px/s)</td></tr>
      <tr><td><code>RUN_MAX_SPEED</code></td><td><code>250.0f</code></td><td>Maximum running speed (px/s, Shift held)</td></tr>
      <tr><td><code>WALK_GROUND_ACCEL</code></td><td><code>750.0f</code></td><td>Ground acceleration while walking (px/s^2)</td></tr>
      <tr><td><code>RUN_GROUND_ACCEL</code></td><td><code>600.0f</code></td><td>Ground acceleration while running (px/s^2)</td></tr>
      <tr><td><code>GROUND_FRICTION</code></td><td><code>550.0f</code></td><td>Ground deceleration when no input (px/s^2)</td></tr>
      <tr><td><code>GROUND_COUNTER_ACCEL</code></td><td><code>100.0f</code></td><td>Extra deceleration when reversing direction (px/s^2)</td></tr>
      <tr><td><code>AIR_ACCEL_WALK</code></td><td><code>350.0f</code></td><td>Airborne acceleration while walking (px/s^2)</td></tr>
      <tr><td><code>AIR_ACCEL_RUN</code></td><td><code>180.0f</code></td><td>Airborne acceleration while running (px/s^2)</td></tr>
      <tr><td><code>AIR_FRICTION</code></td><td><code>80.0f</code></td><td>Airborne deceleration when no input (px/s^2)</td></tr>
      <tr><td><code>WALK_ANIM_MIN_VX</code></td><td><code>8.0f</code></td><td>Minimum horizontal speed to trigger walk animation (px/s)</td></tr>
    </tbody>
  </table>
  <p>The player uses an acceleration-based movement model. Hold Shift to run. Physics overrides for all these values can be configured per level in the TOML <code>[physics]</code> section.</p>
  <h2>Vine Climbing Constants in <code>player.c</code></h2>
  <table>
    <thead><tr><th>Constant</th><th>Value</th><th>Type</th><th>Description</th></tr></thead>
    <tbody>
      <tr><td><code>CLIMB_SPEED</code></td><td><code>80.0f</code></td><td><code>float</code></td><td>Vertical climbing speed on vines (px/s)</td></tr>
      <tr><td><code>CLIMB_H_SPEED</code></td><td><code>80.0f</code></td><td><code>float</code></td><td>Horizontal drift speed while on vine (px/s)</td></tr>
      <tr><td><code>VINE_GRAB_PAD</code></td><td><code>4</code></td><td><code>int</code></td><td>Extra pixels on each side of vine sprite that count as the grab zone (total grab width = VINE_W + 2 x 4 = 24 px)</td></tr>
    </tbody>
  </table>
  <hr />
  <h2>Audio Constants in <code>main.c</code></h2>
  <table>
    <thead><tr><th>Value</th><th>Description</th></tr></thead>
    <tbody>
      <tr><td><code>44100</code></td><td>Audio sample rate (Hz)</td></tr>
      <tr><td><code>MIX_DEFAULT_FORMAT</code></td><td>16-bit signed samples</td></tr>
      <tr><td><code>2</code></td><td>Stereo channels</td></tr>
      <tr><td><code>2048</code></td><td>Mixer buffer size (samples)</td></tr>
      <tr><td>per level</td><td>Music volume (0-128) configured via <code>music_volume</code> in level TOML (default 13, ~10%)</td></tr>
    </tbody>
  </table>
  <hr />
  <h2>Derived Values Quick Reference</h2>
  <table>
    <thead><tr><th>Expression</th><th>Result</th><th>Meaning</th></tr></thead>
    <tbody>
      <tr><td><code>GAME_W / WINDOW_W</code></td><td><code>2x</code></td><td>Pixel scale factor</td></tr>
      <tr><td><code>GAME_H / WINDOW_H</code></td><td><code>2x</code></td><td>Pixel scale factor</td></tr>
      <tr><td><code>1000 / TARGET_FPS</code></td><td><code>~16 ms</code></td><td>Frame budget</td></tr>
      <tr><td><code>GAME_H - TILE_SIZE</code></td><td><code>252</code></td><td><code>FLOOR_Y</code></td></tr>
      <tr><td><code>FLOOR_Y - FRAME_H + FLOOR_SINK</code></td><td><code>220</code></td><td>Player start / floor snap Y</td></tr>
      <tr><td><code>GAME_W / TILE_SIZE</code></td><td><code>~8.3</code></td><td>Tiles needed to fill the floor</td></tr>
      <tr><td><code>WATER_FRAMES x WATER_ART_W</code></td><td><code>128</code></td><td><code>WATER_PERIOD</code> -- seamless repeat distance</td></tr>
    </tbody>
  </table>
  <hr />
  <h2><code>platform.h</code> Constants</h2>
  <table><thead><tr><th>Constant</th><th>Value</th><th>Description</th></tr></thead><tbody><tr><td><code>MAX_PLATFORMS</code></td><td><code>32</code></td><td>Maximum number of platforms in the game</td></tr></tbody></table>
  <hr />
  <h2><code>water.h</code> Constants</h2>
  <table>
    <thead><tr><th>Constant</th><th>Value</th><th>Type</th><th>Description</th></tr></thead>
    <tbody>
      <tr><td><code>WATER_FRAMES</code></td><td><code>8</code></td><td><code>int</code></td><td>Total animation frames in <code>water.png</code></td></tr>
      <tr><td><code>WATER_FRAME_W</code></td><td><code>48</code></td><td><code>int</code></td><td>Full slot width per frame in the sheet (px)</td></tr>
      <tr><td><code>WATER_ART_DX</code></td><td><code>16</code></td><td><code>int</code></td><td>Left offset to visible art within each slot</td></tr>
      <tr><td><code>WATER_ART_W</code></td><td><code>16</code></td><td><code>int</code></td><td>Width of actual art pixels per frame</td></tr>
      <tr><td><code>WATER_ART_Y</code></td><td><code>17</code></td><td><code>int</code></td><td>First visible row within each frame</td></tr>
      <tr><td><code>WATER_ART_H</code></td><td><code>31</code></td><td><code>int</code></td><td>Height of visible art pixels</td></tr>
      <tr><td><code>WATER_PERIOD</code></td><td><code>128</code></td><td><code>int</code></td><td>Pattern repeat distance: <code>WATER_FRAMES x WATER_ART_W</code></td></tr>
      <tr><td><code>WATER_SCROLL_SPEED</code></td><td><code>40.0f</code></td><td><code>float</code></td><td>Rightward scroll speed (px/s)</td></tr>
    </tbody>
  </table>
  <hr />
  <h2><code>spider.h</code> Constants</h2>
  <table>
    <thead><tr><th>Constant</th><th>Value</th><th>Type</th><th>Description</th></tr></thead>
    <tbody>
      <tr><td><code>MAX_SPIDERS</code></td><td><code>16</code></td><td><code>int</code></td><td>Maximum simultaneous spider enemies</td></tr>
      <tr><td><code>SPIDER_FRAMES</code></td><td><code>3</code></td><td><code>int</code></td><td>Animation frames in <code>spider.png</code> (192/64 = 3)</td></tr>
      <tr><td><code>SPIDER_FRAME_W</code></td><td><code>64</code></td><td><code>int</code></td><td>Width of one frame slot in the sheet (px)</td></tr>
      <tr><td><code>SPIDER_ART_X</code></td><td><code>20</code></td><td><code>int</code></td><td>First visible col within each frame slot</td></tr>
      <tr><td><code>SPIDER_ART_W</code></td><td><code>25</code></td><td><code>int</code></td><td>Width of visible art (cols 20-44)</td></tr>
      <tr><td><code>SPIDER_ART_Y</code></td><td><code>22</code></td><td><code>int</code></td><td>First visible row within each frame slot</td></tr>
      <tr><td><code>SPIDER_ART_H</code></td><td><code>10</code></td><td><code>int</code></td><td>Height of visible art (rows 22-31)</td></tr>
      <tr><td><code>SPIDER_SPEED</code></td><td><code>50.0f</code></td><td><code>float</code></td><td>Walk speed (px/s)</td></tr>
      <tr><td><code>SPIDER_FRAME_MS</code></td><td><code>150</code></td><td><code>int</code></td><td>Milliseconds each animation frame is held</td></tr>
    </tbody>
  </table>
  <hr />
  <h2><code>fog.h</code> Constants</h2>
  <table><thead><tr><th>Constant</th><th>Value</th><th>Type</th><th>Description</th></tr></thead><tbody><tr><td><code>FOG_TEX_COUNT</code></td><td><code>2</code></td><td><code>int</code></td><td>Number of fog texture assets in rotation</td></tr><tr><td><code>FOG_MAX</code></td><td><code>4</code></td><td><code>int</code></td><td>Maximum concurrent fog instances</td></tr></tbody></table>
  <hr />
  <h2><code>parallax.h</code> Constants</h2>
  <table><thead><tr><th>Constant</th><th>Value</th><th>Type</th><th>Description</th></tr></thead><tbody><tr><td><code>MAX_BACKGROUND_LAYERS</code></td><td><code>8</code></td><td><code>int</code></td><td>Maximum number of background layers the system can hold</td></tr></tbody></table>
  <hr />
  <h2><code>coin.h</code> Constants</h2>
  <table>
    <thead><tr><th>Constant</th><th>Value</th><th>Type</th><th>Description</th></tr></thead>
    <tbody>
      <tr><td><code>MAX_COINS</code></td><td><code>64</code></td><td><code>int</code></td><td>Maximum simultaneous coins on screen</td></tr>
      <tr><td><code>COIN_DISPLAY_W</code></td><td><code>16</code></td><td><code>int</code></td><td>Render width in logical pixels</td></tr>
      <tr><td><code>COIN_DISPLAY_H</code></td><td><code>16</code></td><td><code>int</code></td><td>Render height in logical pixels</td></tr>
      <tr><td><code>COIN_SCORE</code></td><td><code>100</code></td><td><code>int</code></td><td>Score awarded per coin collected</td></tr>
      <tr><td><code>SCORE_PER_LIFE</code></td><td><code>1000</code></td><td><code>int</code></td><td>Score multiple that grants a bonus life</td></tr>
    </tbody>
  </table>
  <hr />
  <h2><code>vine.h</code> Constants</h2>
  <table>
    <thead><tr><th>Constant</th><th>Value</th><th>Type</th><th>Description</th></tr></thead>
    <tbody>
      <tr><td><code>MAX_VINES</code></td><td><code>24</code></td><td><code>int</code></td><td>Maximum number of vine instances</td></tr>
      <tr><td><code>VINE_W</code></td><td><code>16</code></td><td><code>int</code></td><td>Sprite width in logical pixels</td></tr>
      <tr><td><code>VINE_H</code></td><td><code>32</code></td><td><code>int</code></td><td>Content height after removing transparent padding</td></tr>
      <tr><td><code>VINE_SRC_Y</code></td><td><code>8</code></td><td><code>int</code></td><td>First pixel row with content in vine.png</td></tr>
      <tr><td><code>VINE_SRC_H</code></td><td><code>32</code></td><td><code>int</code></td><td>Height of content area in vine.png</td></tr>
      <tr><td><code>VINE_STEP</code></td><td><code>19</code></td><td><code>int</code></td><td>Vertical spacing between stacked tiles (px)</td></tr>
    </tbody>
  </table>
  <hr />
  <h2><code>fish.h</code> Constants</h2>
  <table>
    <thead><tr><th>Constant</th><th>Value</th><th>Type</th><th>Description</th></tr></thead>
    <tbody>
      <tr><td><code>MAX_FISH</code></td><td><code>16</code></td><td><code>int</code></td><td>Maximum simultaneous fish instances</td></tr>
      <tr><td><code>FISH_FRAMES</code></td><td><code>2</code></td><td><code>int</code></td><td>Horizontal frames in <code>fish.png</code> (96x48 sheet)</td></tr>
      <tr><td><code>FISH_FRAME_W</code></td><td><code>48</code></td><td><code>int</code></td><td>Width of one frame slot in the sheet (px)</td></tr>
      <tr><td><code>FISH_FRAME_H</code></td><td><code>48</code></td><td><code>int</code></td><td>Height of one frame slot in the sheet (px)</td></tr>
      <tr><td><code>FISH_RENDER_W</code></td><td><code>48</code></td><td><code>int</code></td><td>On-screen render width in logical pixels</td></tr>
      <tr><td><code>FISH_RENDER_H</code></td><td><code>48</code></td><td><code>int</code></td><td>On-screen render height in logical pixels</td></tr>
      <tr><td><code>FISH_SPEED</code></td><td><code>70.0f</code></td><td><code>float</code></td><td>Horizontal patrol speed (px/s)</td></tr>
      <tr><td><code>FISH_JUMP_VY</code></td><td><code>-280.0f</code></td><td><code>float</code></td><td>Upward jump impulse (px/s)</td></tr>
      <tr><td><code>FISH_JUMP_MIN</code></td><td><code>1.4f</code></td><td><code>float</code></td><td>Minimum seconds before next jump</td></tr>
      <tr><td><code>FISH_JUMP_MAX</code></td><td><code>3.0f</code></td><td><code>float</code></td><td>Maximum seconds before next jump</td></tr>
      <tr><td><code>FISH_HITBOX_PAD_X</code></td><td><code>16</code></td><td><code>int</code></td><td>Horizontal inset for fair AABB collision (hitbox width = 16 px)</td></tr>
      <tr><td><code>FISH_HITBOX_PAD_Y</code></td><td><code>13</code></td><td><code>int</code></td><td>Vertical inset for fair AABB collision (hitbox height = 19 px)</td></tr>
      <tr><td><code>FISH_FRAME_MS</code></td><td><code>120</code></td><td><code>int</code></td><td>Milliseconds per swim animation frame</td></tr>
    </tbody>
  </table>
  <hr />
  <h2><code>hud.h</code> Constants</h2>
  <table>
    <thead><tr><th>Constant</th><th>Value</th><th>Type</th><th>Description</th></tr></thead>
    <tbody>
      <tr><td><code>MAX_HEARTS</code></td><td><code>3</code></td><td><code>int</code></td><td>Maximum hearts the player can have</td></tr>
      <tr><td><code>DEFAULT_LIVES</code></td><td><code>3</code></td><td><code>int</code></td><td>Lives the player starts with</td></tr>
      <tr><td><code>HUD_MARGIN</code></td><td><code>4</code></td><td><code>int</code></td><td>Pixel margin from screen edges</td></tr>
      <tr><td><code>HUD_HEART_SIZE</code></td><td><code>16</code></td><td><code>int</code></td><td>Display size of each heart icon (px)</td></tr>
      <tr><td><code>HUD_HEART_GAP</code></td><td><code>2</code></td><td><code>int</code></td><td>Horizontal gap between heart icons (px)</td></tr>
      <tr><td><code>HUD_ICON_W</code></td><td><code>16</code></td><td><code>int</code></td><td>Display width of the player icon (px)</td></tr>
      <tr><td><code>HUD_ICON_H</code></td><td><code>13</code></td><td><code>int</code></td><td>Display height of the player icon (px)</td></tr>
      <tr><td><code>HUD_ROW_H</code></td><td><code>16</code></td><td><code>int</code></td><td>Row height for text alignment (font px)</td></tr>
      <tr><td><code>HUD_COIN_ICON_SIZE</code></td><td><code>12</code></td><td><code>int</code></td><td>Display size of the coin count icon (px)</td></tr>
    </tbody>
  </table>
  <hr />
  <h2><code>bouncepad.h</code> Constants</h2>
  <table>
    <thead><tr><th>Constant</th><th>Value</th><th>Type</th><th>Description</th></tr></thead>
    <tbody>
      <tr><td><code>MAX_BOUNCEPADS</code></td><td><code>16</code></td><td><code>int</code></td><td>Maximum simultaneous bouncepad instances (per variant)</td></tr>
      <tr><td><code>BOUNCEPAD_W</code></td><td><code>48</code></td><td><code>int</code></td><td>Display width of one bouncepad frame (px)</td></tr>
      <tr><td><code>BOUNCEPAD_H</code></td><td><code>48</code></td><td><code>int</code></td><td>Display height of one bouncepad frame (px)</td></tr>
      <tr><td><code>BOUNCEPAD_VY_SMALL</code></td><td><code>-380.0f</code></td><td><code>float</code></td><td>Small bouncepad launch impulse (px/s)</td></tr>
      <tr><td><code>BOUNCEPAD_VY_MEDIUM</code></td><td><code>-536.25f</code></td><td><code>float</code></td><td>Medium bouncepad launch impulse (px/s)</td></tr>
      <tr><td><code>BOUNCEPAD_VY_HIGH</code></td><td><code>-700.0f</code></td><td><code>float</code></td><td>High bouncepad launch impulse (px/s)</td></tr>
      <tr><td><code>BOUNCEPAD_VY</code></td><td><code>-536.25f</code></td><td><code>float</code></td><td>Default launch impulse (alias for <code>BOUNCEPAD_VY_MEDIUM</code>)</td></tr>
      <tr><td><code>BOUNCEPAD_FRAME_MS</code></td><td><code>80</code></td><td><code>int</code></td><td>Milliseconds per animation frame during release</td></tr>
      <tr><td><code>BOUNCEPAD_SRC_Y</code></td><td><code>14</code></td><td><code>int</code></td><td>First non-transparent row in the frame</td></tr>
      <tr><td><code>BOUNCEPAD_SRC_H</code></td><td><code>18</code></td><td><code>int</code></td><td>Height of the art region (rows 14-31)</td></tr>
      <tr><td><code>BOUNCEPAD_ART_X</code></td><td><code>16</code></td><td><code>int</code></td><td>First non-transparent col in the frame</td></tr>
      <tr><td><code>BOUNCEPAD_ART_W</code></td><td><code>16</code></td><td><code>int</code></td><td>Width of the art region (cols 16-31)</td></tr>
    </tbody>
  </table>
  <hr />
  <h2><code>rail.h</code> Constants</h2>
  <table>
    <thead><tr><th>Constant</th><th>Value</th><th>Type</th><th>Description</th></tr></thead>
    <tbody>
      <tr><td><code>RAIL_N</code></td><td><code>1 &lt;&lt; 0</code></td><td>bitmask</td><td>Tile opens upward</td></tr>
      <tr><td><code>RAIL_E</code></td><td><code>1 &lt;&lt; 1</code></td><td>bitmask</td><td>Tile opens rightward</td></tr>
      <tr><td><code>RAIL_S</code></td><td><code>1 &lt;&lt; 2</code></td><td>bitmask</td><td>Tile opens downward</td></tr>
      <tr><td><code>RAIL_W</code></td><td><code>1 &lt;&lt; 3</code></td><td>bitmask</td><td>Tile opens leftward</td></tr>
      <tr><td><code>RAIL_TILE_W</code></td><td><code>16</code></td><td><code>int</code></td><td>Width of one tile in the sprite sheet (px)</td></tr>
      <tr><td><code>RAIL_TILE_H</code></td><td><code>16</code></td><td><code>int</code></td><td>Height of one tile in the sprite sheet (px)</td></tr>
      <tr><td><code>MAX_RAIL_TILES</code></td><td><code>128</code></td><td><code>int</code></td><td>Maximum tiles in a single Rail path</td></tr>
      <tr><td><code>MAX_RAILS</code></td><td><code>16</code></td><td><code>int</code></td><td>Maximum Rail instances per level</td></tr>
    </tbody>
  </table>
  <hr />
  <h2><code>spike_block.h</code> Constants</h2>
  <table>
    <thead><tr><th>Constant</th><th>Value</th><th>Type</th><th>Description</th></tr></thead>
    <tbody>
      <tr><td><code>SPIKE_DISPLAY_W</code></td><td><code>24</code></td><td><code>int</code></td><td>On-screen width in logical pixels (16x16 scaled up)</td></tr>
      <tr><td><code>SPIKE_DISPLAY_H</code></td><td><code>24</code></td><td><code>int</code></td><td>On-screen height in logical pixels</td></tr>
      <tr><td><code>SPIKE_SPIN_DEG_PER_SEC</code></td><td><code>360.0f</code></td><td><code>float</code></td><td>Rotation speed -- one full turn per second</td></tr>
      <tr><td><code>SPIKE_SPEED_SLOW</code></td><td><code>1.5f</code></td><td><code>float</code></td><td>Rail traversal: 1.5 tiles/s</td></tr>
      <tr><td><code>SPIKE_SPEED_NORMAL</code></td><td><code>3.0f</code></td><td><code>float</code></td><td>Rail traversal: 3.0 tiles/s</td></tr>
      <tr><td><code>SPIKE_SPEED_FAST</code></td><td><code>6.0f</code></td><td><code>float</code></td><td>Rail traversal: 6.0 tiles/s</td></tr>
      <tr><td><code>SPIKE_PUSH_SPEED</code></td><td><code>220.0f</code></td><td><code>float</code></td><td>Horizontal push impulse magnitude (px/s)</td></tr>
      <tr><td><code>SPIKE_PUSH_VY</code></td><td><code>-150.0f</code></td><td><code>float</code></td><td>Upward push component on collision (px/s)</td></tr>
      <tr><td><code>MAX_SPIKE_BLOCKS</code></td><td><code>16</code></td><td><code>int</code></td><td>Maximum spike block instances per level</td></tr>
    </tbody>
  </table>
  <hr />
  <h2><code>debug.h</code> Constants</h2>
  <table>
    <thead><tr><th>Constant</th><th>Value</th><th>Type</th><th>Description</th></tr></thead>
    <tbody>
      <tr><td><code>DEBUG_LOG_MAX_ENTRIES</code></td><td><code>8</code></td><td><code>int</code></td><td>Maximum visible log messages</td></tr>
      <tr><td><code>DEBUG_LOG_MSG_LEN</code></td><td><code>64</code></td><td><code>int</code></td><td>Max characters per log message (incl. null)</td></tr>
      <tr><td><code>DEBUG_LOG_DISPLAY_SEC</code></td><td><code>4.0f</code></td><td><code>float</code></td><td>Seconds each log entry stays visible</td></tr>
      <tr><td><code>DEBUG_FPS_SAMPLE_MS</code></td><td><code>500</code></td><td><code>int</code></td><td>Milliseconds between FPS counter refreshes</td></tr>
    </tbody>
  </table>
  <hr />
  <h2><code>jumping_spider.h</code> Constants</h2>
  <table>
    <thead><tr><th>Constant</th><th>Value</th><th>Type</th><th>Description</th></tr></thead>
    <tbody>
      <tr><td><code>MAX_JUMPING_SPIDERS</code></td><td><code>16</code></td><td><code>int</code></td><td>Maximum simultaneous jumping spider instances</td></tr>
      <tr><td><code>JSPIDER_FRAMES</code></td><td><code>3</code></td><td><code>int</code></td><td>Animation frames in <code>jumping_spider.png</code> (192/64 = 3)</td></tr>
      <tr><td><code>JSPIDER_FRAME_W</code></td><td><code>64</code></td><td><code>int</code></td><td>Width of one frame slot in the sheet (px)</td></tr>
      <tr><td><code>JSPIDER_ART_X</code></td><td><code>20</code></td><td><code>int</code></td><td>First visible col within each frame</td></tr>
      <tr><td><code>JSPIDER_ART_W</code></td><td><code>25</code></td><td><code>int</code></td><td>Width of visible art (cols 20-44)</td></tr>
      <tr><td><code>JSPIDER_ART_Y</code></td><td><code>22</code></td><td><code>int</code></td><td>First visible row within each frame</td></tr>
      <tr><td><code>JSPIDER_ART_H</code></td><td><code>10</code></td><td><code>int</code></td><td>Height of visible art (rows 22-31)</td></tr>
      <tr><td><code>JSPIDER_SPEED</code></td><td><code>55.0f</code></td><td><code>float</code></td><td>Walk speed (px/s)</td></tr>
      <tr><td><code>JSPIDER_FRAME_MS</code></td><td><code>150</code></td><td><code>int</code></td><td>Milliseconds per animation frame</td></tr>
      <tr><td><code>JSPIDER_JUMP_VY</code></td><td><code>-200.0f</code></td><td><code>float</code></td><td>Upward jump impulse (px/s)</td></tr>
      <tr><td><code>JSPIDER_GRAVITY</code></td><td><code>600.0f</code></td><td><code>float</code></td><td>Downward acceleration while airborne (px/s^2)</td></tr>
    </tbody>
  </table>
  <hr />
  <h2><code>bird.h</code> Constants</h2>
  <table>
    <thead><tr><th>Constant</th><th>Value</th><th>Type</th><th>Description</th></tr></thead>
    <tbody>
      <tr><td><code>MAX_BIRDS</code></td><td><code>16</code></td><td><code>int</code></td><td>Maximum simultaneous bird instances</td></tr>
      <tr><td><code>BIRD_FRAMES</code></td><td><code>3</code></td><td><code>int</code></td><td>Animation frames in <code>bird.png</code> (144/48 = 3)</td></tr>
      <tr><td><code>BIRD_FRAME_W</code></td><td><code>48</code></td><td><code>int</code></td><td>Width of one frame slot in the sheet (px)</td></tr>
      <tr><td><code>BIRD_ART_X</code></td><td><code>17</code></td><td><code>int</code></td><td>First visible col within each frame</td></tr>
      <tr><td><code>BIRD_ART_W</code></td><td><code>15</code></td><td><code>int</code></td><td>Width of visible art (cols 17-31)</td></tr>
      <tr><td><code>BIRD_ART_Y</code></td><td><code>17</code></td><td><code>int</code></td><td>First visible row within each frame</td></tr>
      <tr><td><code>BIRD_ART_H</code></td><td><code>14</code></td><td><code>int</code></td><td>Height of visible art (rows 17-30)</td></tr>
      <tr><td><code>BIRD_SPEED</code></td><td><code>45.0f</code></td><td><code>float</code></td><td>Horizontal flight speed (px/s)</td></tr>
      <tr><td><code>BIRD_FRAME_MS</code></td><td><code>140</code></td><td><code>int</code></td><td>Milliseconds per wing animation frame</td></tr>
      <tr><td><code>BIRD_WAVE_AMP</code></td><td><code>20.0f</code></td><td><code>float</code></td><td>Sine-wave amplitude in logical pixels</td></tr>
      <tr><td><code>BIRD_WAVE_FREQ</code></td><td><code>0.015f</code></td><td><code>float</code></td><td>Sine cycles per pixel of horizontal travel</td></tr>
    </tbody>
  </table>
  <hr />
  <h2><code>faster_bird.h</code> Constants</h2>
  <table>
    <thead><tr><th>Constant</th><th>Value</th><th>Type</th><th>Description</th></tr></thead>
    <tbody>
      <tr><td><code>MAX_FASTER_BIRDS</code></td><td><code>16</code></td><td><code>int</code></td><td>Maximum simultaneous faster bird instances</td></tr>
      <tr><td><code>FBIRD_FRAMES</code></td><td><code>3</code></td><td><code>int</code></td><td>Animation frames in <code>faster_bird.png</code> (144/48 = 3)</td></tr>
      <tr><td><code>FBIRD_FRAME_W</code></td><td><code>48</code></td><td><code>int</code></td><td>Width of one frame slot in the sheet (px)</td></tr>
      <tr><td><code>FBIRD_ART_X</code></td><td><code>17</code></td><td><code>int</code></td><td>First visible col within each frame</td></tr>
      <tr><td><code>FBIRD_ART_W</code></td><td><code>15</code></td><td><code>int</code></td><td>Width of visible art (cols 17-31)</td></tr>
      <tr><td><code>FBIRD_ART_Y</code></td><td><code>17</code></td><td><code>int</code></td><td>First visible row within each frame</td></tr>
      <tr><td><code>FBIRD_ART_H</code></td><td><code>14</code></td><td><code>int</code></td><td>Height of visible art (rows 17-30)</td></tr>
      <tr><td><code>FBIRD_SPEED</code></td><td><code>80.0f</code></td><td><code>float</code></td><td>Horizontal speed -- nearly 2x the slow bird</td></tr>
      <tr><td><code>FBIRD_FRAME_MS</code></td><td><code>90</code></td><td><code>int</code></td><td>Faster wing animation (ms per frame)</td></tr>
      <tr><td><code>FBIRD_WAVE_AMP</code></td><td><code>15.0f</code></td><td><code>float</code></td><td>Tighter sine-wave amplitude (px)</td></tr>
      <tr><td><code>FBIRD_WAVE_FREQ</code></td><td><code>0.025f</code></td><td><code>float</code></td><td>Higher frequency -- more erratic curves</td></tr>
    </tbody>
  </table>
  <hr />
  <h2><code>float_platform.h</code> Constants</h2>
  <table>
    <thead><tr><th>Constant</th><th>Value</th><th>Type</th><th>Description</th></tr></thead>
    <tbody>
      <tr><td><code>FLOAT_PLATFORM_PIECE_W</code></td><td><code>16</code></td><td><code>int</code></td><td>Width of each 3-slice piece (px)</td></tr>
      <tr><td><code>FLOAT_PLATFORM_H</code></td><td><code>16</code></td><td><code>int</code></td><td>Height of the platform sprite (px)</td></tr>
      <tr><td><code>MAX_FLOAT_PLATFORMS</code></td><td><code>16</code></td><td><code>int</code></td><td>Maximum float platform instances per level</td></tr>
      <tr><td><code>CRUMBLE_STAND_LIMIT</code></td><td><code>0.75f</code></td><td><code>float</code></td><td>Seconds of standing before crumble-fall starts</td></tr>
      <tr><td><code>CRUMBLE_FALL_GRAVITY</code></td><td><code>250.0f</code></td><td><code>float</code></td><td>Downward acceleration during crumble fall (px/s^2)</td></tr>
      <tr><td><code>CRUMBLE_FALL_INITIAL_VY</code></td><td><code>20.0f</code></td><td><code>float</code></td><td>Initial downward velocity on crumble-start (px/s)</td></tr>
    </tbody>
  </table>
  <hr />
  <h2><code>bridge.h</code> Constants</h2>
  <table>
    <thead><tr><th>Constant</th><th>Value</th><th>Type</th><th>Description</th></tr></thead>
    <tbody>
      <tr><td><code>MAX_BRIDGES</code></td><td><code>16</code></td><td><code>int</code></td><td>Maximum bridge instances per level</td></tr>
      <tr><td><code>MAX_BRIDGE_BRICKS</code></td><td><code>16</code></td><td><code>int</code></td><td>Maximum bricks in a single bridge</td></tr>
      <tr><td><code>BRIDGE_TILE_W</code></td><td><code>16</code></td><td><code>int</code></td><td>Width of one bridge.png tile (px)</td></tr>
      <tr><td><code>BRIDGE_TILE_H</code></td><td><code>16</code></td><td><code>int</code></td><td>Height of one bridge.png tile (px)</td></tr>
      <tr><td><code>BRIDGE_FALL_DELAY</code></td><td><code>0.1f</code></td><td><code>float</code></td><td>Seconds between touch and first brick falling</td></tr>
      <tr><td><code>BRIDGE_CASCADE_DELAY</code></td><td><code>0.06f</code></td><td><code>float</code></td><td>Extra seconds between successive bricks cascading outward</td></tr>
      <tr><td><code>BRIDGE_FALL_GRAVITY</code></td><td><code>250.0f</code></td><td><code>float</code></td><td>Downward acceleration per brick during fall (px/s^2)</td></tr>
      <tr><td><code>BRIDGE_FALL_INITIAL_VY</code></td><td><code>20.0f</code></td><td><code>float</code></td><td>Initial downward velocity on fall-start (px/s)</td></tr>
    </tbody>
  </table>
  <hr />
  <h2><code>star_yellow.h</code> Constants</h2>
  <table>
    <thead><tr><th>Constant</th><th>Value</th><th>Type</th><th>Description</th></tr></thead>
    <tbody>
      <tr><td><code>MAX_STAR_YELLOWS</code></td><td><code>16</code></td><td><code>int</code></td><td>Maximum star instances per color per level</td></tr>
      <tr><td><code>STAR_YELLOW_DISPLAY_W</code></td><td><code>16</code></td><td><code>int</code></td><td>Display width (logical px)</td></tr>
      <tr><td><code>STAR_YELLOW_DISPLAY_H</code></td><td><code>16</code></td><td><code>int</code></td><td>Display height (logical px)</td></tr>
    </tbody>
  </table>
  <hr />
  <h2><code>last_star.h</code> Constants</h2>
  <table>
    <thead><tr><th>Constant</th><th>Value</th><th>Type</th><th>Description</th></tr></thead>
    <tbody>
      <tr><td><code>LAST_STAR_DISPLAY_W</code></td><td><code>24</code></td><td><code>int</code></td><td>Display width (logical px)</td></tr>
      <tr><td><code>LAST_STAR_DISPLAY_H</code></td><td><code>24</code></td><td><code>int</code></td><td>Display height (logical px)</td></tr>
    </tbody>
  </table>
  <hr />
  <h2><code>axe_trap.h</code> Constants</h2>
  <table>
    <thead><tr><th>Constant</th><th>Value</th><th>Type</th><th>Description</th></tr></thead>
    <tbody>
      <tr><td><code>AXE_FRAME_W</code></td><td><code>48</code></td><td><code>int</code></td><td>Source sprite width (px)</td></tr>
      <tr><td><code>AXE_FRAME_H</code></td><td><code>64</code></td><td><code>int</code></td><td>Source sprite height (px)</td></tr>
      <tr><td><code>AXE_DISPLAY_W</code></td><td><code>48</code></td><td><code>int</code></td><td>On-screen display width (logical px)</td></tr>
      <tr><td><code>AXE_DISPLAY_H</code></td><td><code>64</code></td><td><code>int</code></td><td>On-screen display height (logical px)</td></tr>
      <tr><td><code>AXE_SWING_AMPLITUDE</code></td><td><code>60.0f</code></td><td><code>float</code></td><td>Maximum pendulum angle from vertical (degrees)</td></tr>
      <tr><td><code>AXE_SWING_PERIOD</code></td><td><code>2.0f</code></td><td><code>float</code></td><td>Time for one full pendulum cycle (s)</td></tr>
      <tr><td><code>AXE_SPIN_SPEED</code></td><td><code>180.0f</code></td><td><code>float</code></td><td>Rotation speed for spin variant (degrees/s)</td></tr>
      <tr><td><code>MAX_AXE_TRAPS</code></td><td><code>16</code></td><td><code>int</code></td><td>Maximum axe trap instances per level</td></tr>
    </tbody>
  </table>
  <hr />
  <h2><code>circular_saw.h</code> Constants</h2>
  <table>
    <thead><tr><th>Constant</th><th>Value</th><th>Type</th><th>Description</th></tr></thead>
    <tbody>
      <tr><td><code>SAW_FRAME_W</code></td><td><code>32</code></td><td><code>int</code></td><td>Source sprite width (px)</td></tr>
      <tr><td><code>SAW_FRAME_H</code></td><td><code>32</code></td><td><code>int</code></td><td>Source sprite height (px)</td></tr>
      <tr><td><code>SAW_DISPLAY_W</code></td><td><code>32</code></td><td><code>int</code></td><td>On-screen display width (logical px)</td></tr>
      <tr><td><code>SAW_DISPLAY_H</code></td><td><code>32</code></td><td><code>int</code></td><td>On-screen display height (logical px)</td></tr>
      <tr><td><code>SAW_SPIN_DEG_PER_SEC</code></td><td><code>720.0f</code></td><td><code>float</code></td><td>Rotation speed (degrees/s)</td></tr>
      <tr><td><code>SAW_PATROL_SPEED</code></td><td><code>180.0f</code></td><td><code>float</code></td><td>Horizontal patrol speed (px/s)</td></tr>
      <tr><td><code>SAW_PUSH_SPEED</code></td><td><code>220.0f</code></td><td><code>float</code></td><td>Push impulse magnitude (px/s)</td></tr>
      <tr><td><code>SAW_PUSH_VY</code></td><td><code>-150.0f</code></td><td><code>float</code></td><td>Upward bounce component on collision (px/s)</td></tr>
      <tr><td><code>MAX_CIRCULAR_SAWS</code></td><td><code>16</code></td><td><code>int</code></td><td>Maximum circular saw instances per level</td></tr>
    </tbody>
  </table>
  <hr />
  <h2><code>blue_flame.h</code> Constants</h2>
  <table>
    <thead><tr><th>Constant</th><th>Value</th><th>Type</th><th>Description</th></tr></thead>
    <tbody>
      <tr><td><code>BLUE_FLAME_FRAME_W</code></td><td><code>48</code></td><td><code>int</code></td><td>Animation frame width (px)</td></tr>
      <tr><td><code>BLUE_FLAME_FRAME_H</code></td><td><code>48</code></td><td><code>int</code></td><td>Animation frame height (px)</td></tr>
      <tr><td><code>BLUE_FLAME_DISPLAY_W</code></td><td><code>48</code></td><td><code>int</code></td><td>On-screen display width (logical px)</td></tr>
      <tr><td><code>BLUE_FLAME_DISPLAY_H</code></td><td><code>48</code></td><td><code>int</code></td><td>On-screen display height (logical px)</td></tr>
      <tr><td><code>BLUE_FLAME_FRAME_COUNT</code></td><td><code>2</code></td><td><code>int</code></td><td>Number of animation frames</td></tr>
      <tr><td><code>BLUE_FLAME_ANIM_SPEED</code></td><td><code>0.1f</code></td><td><code>float</code></td><td>Seconds between frame advances</td></tr>
      <tr><td><code>BLUE_FLAME_LAUNCH_VY</code></td><td><code>-550.0f</code></td><td><code>float</code></td><td>Initial upward impulse (px/s)</td></tr>
      <tr><td><code>BLUE_FLAME_RISE_DECEL</code></td><td><code>800.0f</code></td><td><code>float</code></td><td>Deceleration during rise (px/s^2)</td></tr>
      <tr><td><code>BLUE_FLAME_APEX_Y</code></td><td><code>60.0f</code></td><td><code>float</code></td><td>World-space y coordinate at apex (px)</td></tr>
      <tr><td><code>BLUE_FLAME_FLIP_DURATION</code></td><td><code>0.12f</code></td><td><code>float</code></td><td>Time to rotate 180 degrees at apex (s)</td></tr>
      <tr><td><code>BLUE_FLAME_WAIT_DURATION</code></td><td><code>1.5f</code></td><td><code>float</code></td><td>Time hidden below floor before next eruption (s)</td></tr>
      <tr><td><code>MAX_BLUE_FLAMES</code></td><td><code>16</code></td><td><code>int</code></td><td>Maximum blue/fire flame instances per level</td></tr>
    </tbody>
  </table>
  <hr />
  <h2><code>faster_fish.h</code> Constants</h2>
  <table>
    <thead><tr><th>Constant</th><th>Value</th><th>Type</th><th>Description</th></tr></thead>
    <tbody>
      <tr><td><code>MAX_FASTER_FISH</code></td><td><code>16</code></td><td><code>int</code></td><td>Maximum faster fish instances per level</td></tr>
      <tr><td><code>FFISH_FRAMES</code></td><td><code>2</code></td><td><code>int</code></td><td>Number of animation frames</td></tr>
      <tr><td><code>FFISH_FRAME_W</code></td><td><code>48</code></td><td><code>int</code></td><td>Frame width (px)</td></tr>
      <tr><td><code>FFISH_FRAME_H</code></td><td><code>48</code></td><td><code>int</code></td><td>Frame height (px)</td></tr>
      <tr><td><code>FFISH_RENDER_W</code></td><td><code>48</code></td><td><code>int</code></td><td>Render width (logical px)</td></tr>
      <tr><td><code>FFISH_RENDER_H</code></td><td><code>48</code></td><td><code>int</code></td><td>Render height (logical px)</td></tr>
      <tr><td><code>FFISH_SPEED</code></td><td><code>120.0f</code></td><td><code>float</code></td><td>Patrol speed (px/s)</td></tr>
      <tr><td><code>FFISH_JUMP_VY</code></td><td><code>-420.0f</code></td><td><code>float</code></td><td>Jump impulse (px/s)</td></tr>
      <tr><td><code>FFISH_JUMP_MIN</code></td><td><code>1.0f</code></td><td><code>float</code></td><td>Minimum delay between jumps (s)</td></tr>
      <tr><td><code>FFISH_JUMP_MAX</code></td><td><code>2.2f</code></td><td><code>float</code></td><td>Maximum delay between jumps (s)</td></tr>
      <tr><td><code>FFISH_HITBOX_PAD_X</code></td><td><code>16</code></td><td><code>int</code></td><td>Horizontal hitbox inset (px)</td></tr>
      <tr><td><code>FFISH_HITBOX_PAD_Y</code></td><td><code>13</code></td><td><code>int</code></td><td>Vertical hitbox inset (px)</td></tr>
      <tr><td><code>FFISH_FRAME_MS</code></td><td><code>100</code></td><td><code>int</code></td><td>Frame animation duration (ms)</td></tr>
    </tbody>
  </table>
  <hr />
  <h2><code>spike.h</code> Constants</h2>
  <table>
    <thead><tr><th>Constant</th><th>Value</th><th>Type</th><th>Description</th></tr></thead>
    <tbody>
      <tr><td><code>MAX_SPIKE_ROWS</code></td><td><code>16</code></td><td><code>int</code></td><td>Maximum spike row instances per level</td></tr>
      <tr><td><code>MAX_SPIKE_TILES</code></td><td><code>16</code></td><td><code>int</code></td><td>Maximum tiles in a single spike row</td></tr>
      <tr><td><code>SPIKE_TILE_W</code></td><td><code>16</code></td><td><code>int</code></td><td>Spike tile width (px)</td></tr>
      <tr><td><code>SPIKE_TILE_H</code></td><td><code>16</code></td><td><code>int</code></td><td>Spike tile height (px)</td></tr>
    </tbody>
  </table>
  <hr />
  <h2><code>spike_platform.h</code> Constants</h2>
  <table>
    <thead><tr><th>Constant</th><th>Value</th><th>Type</th><th>Description</th></tr></thead>
    <tbody>
      <tr><td><code>MAX_SPIKE_PLATFORMS</code></td><td><code>16</code></td><td><code>int</code></td><td>Maximum spike platform instances per level</td></tr>
      <tr><td><code>SPIKE_PLAT_PIECE_W</code></td><td><code>16</code></td><td><code>int</code></td><td>Width of one 3-slice piece (px)</td></tr>
      <tr><td><code>SPIKE_PLAT_H</code></td><td><code>16</code></td><td><code>int</code></td><td>Full frame height (px)</td></tr>
      <tr><td><code>SPIKE_PLAT_SRC_Y</code></td><td><code>5</code></td><td><code>int</code></td><td>First content row in each piece (px)</td></tr>
      <tr><td><code>SPIKE_PLAT_SRC_H</code></td><td><code>11</code></td><td><code>int</code></td><td>Content height (rows 5-15, px)</td></tr>
    </tbody>
  </table>
  <hr />
  <h2><code>ladder.h</code> Constants</h2>
  <table>
    <thead><tr><th>Constant</th><th>Value</th><th>Type</th><th>Description</th></tr></thead>
    <tbody>
      <tr><td><code>MAX_LADDERS</code></td><td><code>16</code></td><td><code>int</code></td><td>Maximum ladder instances per level</td></tr>
      <tr><td><code>LADDER_W</code></td><td><code>16</code></td><td><code>int</code></td><td>Sprite width (px)</td></tr>
      <tr><td><code>LADDER_H</code></td><td><code>22</code></td><td><code>int</code></td><td>Content height after cropping padding (px)</td></tr>
      <tr><td><code>LADDER_SRC_Y</code></td><td><code>13</code></td><td><code>int</code></td><td>First pixel row with content</td></tr>
      <tr><td><code>LADDER_SRC_H</code></td><td><code>22</code></td><td><code>int</code></td><td>Height of content area (px)</td></tr>
      <tr><td><code>LADDER_STEP</code></td><td><code>8</code></td><td><code>int</code></td><td>Vertical overlap when tiling (px)</td></tr>
    </tbody>
  </table>
  <hr />
  <h2><code>rope.h</code> Constants</h2>
  <table>
    <thead><tr><th>Constant</th><th>Value</th><th>Type</th><th>Description</th></tr></thead>
    <tbody>
      <tr><td><code>MAX_ROPES</code></td><td><code>16</code></td><td><code>int</code></td><td>Maximum rope instances per level</td></tr>
      <tr><td><code>ROPE_W</code></td><td><code>12</code></td><td><code>int</code></td><td>Display width with padding (px)</td></tr>
      <tr><td><code>ROPE_H</code></td><td><code>36</code></td><td><code>int</code></td><td>Display height with padding (px)</td></tr>
      <tr><td><code>ROPE_SRC_X</code></td><td><code>2</code></td><td><code>int</code></td><td>Source crop x offset (px)</td></tr>
      <tr><td><code>ROPE_SRC_Y</code></td><td><code>6</code></td><td><code>int</code></td><td>Source crop y offset (px)</td></tr>
      <tr><td><code>ROPE_SRC_W</code></td><td><code>12</code></td><td><code>int</code></td><td>Source crop width (px)</td></tr>
      <tr><td><code>ROPE_SRC_H</code></td><td><code>36</code></td><td><code>int</code></td><td>Source crop height (px)</td></tr>
      <tr><td><code>ROPE_STEP</code></td><td><code>34</code></td><td><code>int</code></td><td>Vertical spacing between stacked tiles (px)</td></tr>
    </tbody>
  </table>
  <hr />
  <h2><code>bouncepad_small.h</code> / <code>bouncepad_medium.h</code> / <code>bouncepad_high.h</code> Constants</h2>
  <table>
    <thead><tr><th>Constant</th><th>Value</th><th>Type</th><th>Description</th></tr></thead>
    <tbody>
      <tr><td><code>MAX_BOUNCEPADS_SMALL</code></td><td><code>16</code></td><td><code>int</code></td><td>Maximum small bouncepad instances</td></tr>
      <tr><td><code>MAX_BOUNCEPADS_MEDIUM</code></td><td><code>16</code></td><td><code>int</code></td><td>Maximum medium bouncepad instances</td></tr>
      <tr><td><code>MAX_BOUNCEPADS_HIGH</code></td><td><code>16</code></td><td><code>int</code></td><td>Maximum high bouncepad instances</td></tr>
    </tbody>
  </table>
</section>

{/* ============================================================ */}
{/* SECTION: DEVELOPER GUIDE */}
{/* ============================================================ */}
<section id="developer-guide" className={`doc-section${!isSectionVisible("developer-guide") ? " hidden-section" : ""}`}>
  <h1 className="page-title">Developer Guide</h1>
  <p><a href="home">&#8592; Home</a></p>
  <hr />
  <p>This guide covers the patterns and conventions used in Super Mango and explains how to extend the game safely and consistently.</p>
  <hr />
  <h2>Coding Conventions</h2>
  <h3>Language and Standard</h3>
  <ul>
    <li><strong>C11</strong> (<code>-std=c11</code>)</li>
    <li>Compiler: <code>clang</code> (default), <code>gcc</code> compatible</li>
  </ul>
  <h3>Naming</h3>
  <table>
    <thead><tr><th>Category</th><th>Convention</th><th>Example</th></tr></thead>
    <tbody>
      <tr><td>Files</td><td><code>snake_case</code></td><td><code>player.c</code>, <code>coin.h</code></td></tr>
      <tr><td>Functions</td><td><code>module_verb</code></td><td><code>player_init</code>, <code>coin_update</code></td></tr>
      <tr><td>Struct types</td><td><code>PascalCase</code> via <code>typedef</code></td><td><code>Player</code>, <code>GameState</code>, <code>Coin</code></td></tr>
      <tr><td>Enum values</td><td><code>UPPER_SNAKE_CASE</code></td><td><code>ANIM_IDLE</code>, <code>ANIM_WALK</code></td></tr>
      <tr><td>Constants (<code>#define</code>)</td><td><code>UPPER_SNAKE_CASE</code></td><td><code>FLOOR_Y</code>, <code>TILE_SIZE</code></td></tr>
      <tr><td>Local variables</td><td><code>snake_case</code></td><td><code>dt</code>, <code>frame_ms</code>, <code>elapsed</code></td></tr>
      <tr><td>Assets</td><td><code>snake_case</code></td><td><code>player.png</code>, <code>coin.png</code>, <code>spider.png</code></td></tr>
      <tr><td>Sounds</td><td><code>component_descriptor.wav</code></td><td><code>player_jump.wav</code>, <code>coin.wav</code>, <code>bird.wav</code></td></tr>
    </tbody>
  </table>
  <h3>Memory and Safety Rules</h3>
  <ul>
    <li>Every pointer must be set to <code>NULL</code> <strong>immediately after freeing</strong>. (<code>SDL_Destroy*</code> and <code>free()</code> on <code>NULL</code> are no-ops, preventing double-free crashes.)</li>
    <li>Error paths call <code>SDL_GetError()</code> / <code>IMG_GetError()</code> / <code>Mix_GetError()</code> and write to <code>stderr</code>.</li>
    <li>Resources are <strong>always freed in reverse init order</strong>.</li>
    <li>Use <code>float</code> for positions and velocities; cast to <code>int</code> only at render time (<code>SDL_Rect</code> fields are <code>int</code>).</li>
  </ul>
  <h3>Coordinate System</h3>
  <p>All game-object positions and sizes live in <strong>logical space (400x300)</strong>. Never use <code>WINDOW_W</code> / <code>WINDOW_H</code> for game math -- SDL scales the logical canvas to the OS window automatically.</p>
  <p>See <a href="constants_reference">Constants Reference</a> for all defined constants.</p>
  <hr />
  <h2>Adding a New Entity</h2>
  <p>Every entity follows the same lifecycle pattern:</p>
  <pre><code>{`entity_init    -> load texture, set initial state
entity_update  -> move, apply physics, detect events
entity_render  -> draw to renderer
entity_cleanup -> SDL_DestroyTexture, set to NULL`}</code></pre>
  <p>And optionally:</p>
  <pre><code>{`entity_handle_input   -> if player-controlled
entity_animate        -> static helper, called from entity_update`}</code></pre>
  <h3>Step-by-Step</h3>
  <h4>1. Create the header -- <code>src/collectibles/coin.h</code></h4>
  <pre><code className="language-c">{`#pragma once
#include <SDL.h>

typedef struct {
    float        x, y;    /* logical position (top-left) */
    int          w, h;    /* display size in logical px  */
    int          active;  /* 1 = visible, 0 = collected  */
    SDL_Texture *texture;
} Coin;

void coin_init(Coin *coin, SDL_Renderer *renderer, float x, float y);
void coin_update(Coin *coin, float dt);
void coin_render(Coin *coin, SDL_Renderer *renderer);
void coin_cleanup(Coin *coin);`}</code></pre>
  <h4>2. Create the implementation -- <code>src/collectibles/coin.c</code></h4>
  <pre><code className="language-c">{`#include <SDL_image.h>
#include <stdio.h>
#include <stdlib.h>
#include "coin.h"

void coin_init(Coin *coin, SDL_Renderer *renderer, float x, float y) {
    coin->texture = IMG_LoadTexture(renderer, "assets/sprites/collectibles/coin.png");
    if (!coin->texture) {
        fprintf(stderr, "Failed to load coin.png: %s\\n", IMG_GetError());
        exit(EXIT_FAILURE);
    }
    coin->x = x;
    coin->y = y;
    coin->w = 48;
    coin->h = 48;
    coin->active = 1;
}

void coin_render(Coin *coin, SDL_Renderer *renderer) {
    if (!coin->active) return;
    SDL_Rect dst = { (int)coin->x, (int)coin->y, coin->w, coin->h };
    SDL_RenderCopy(renderer, coin->texture, NULL, &dst);
}

void coin_cleanup(Coin *coin) {
    if (coin->texture) {
        SDL_DestroyTexture(coin->texture);
        coin->texture = NULL;
    }
}`}</code></pre>
  <p>The Makefile picks up <code>coin.c</code> automatically -- <strong>no Makefile changes needed</strong>.</p>
  <h4>3. Add texture to <code>GameState</code> in <code>game.h</code></h4>
  <p>Textures are loaded in <code>game_init()</code> and stored in <code>GameState</code>. The entity array and count also live in <code>GameState</code>:</p>
  <pre><code className="language-c">{`#include "coin.h"

typedef struct {
    // ... existing fields ...
    SDL_Texture *tex_coin;    /* GPU texture, loaded in game_init */
    Coin coins[32];           /* fixed-size array -- simple and cache-friendly */
    int  coin_count;          /* how many are currently active */
} GameState;`}</code></pre>
  <h4>4. Wire up in <code>game.c</code></h4>
  <pre><code className="language-c">{`// game_init -- load texture and init entities:
gs->tex_coin = IMG_LoadTexture(gs->renderer, "assets/sprites/collectibles/coin.png");
coin_init(&gs->coins[0], gs->tex_coin, 200.0f, 100.0f);
gs->coin_count = 1;

// game_loop update section:
for (int i = 0; i < gs->coin_count; i++)
    coin_update(&gs->coins[i], dt);

// game_loop render section (correct layer order):
for (int i = 0; i < gs->coin_count; i++)
    coin_render(&gs->coins[i], gs->renderer);

// game_cleanup (before SDL_DestroyRenderer):
for (int i = 0; i < gs->coin_count; i++)
    coin_cleanup(&gs->coins[i]);`}</code></pre>
  <h4>5. Define entity placements in level TOML</h4>
  <p>Entity spawn positions are defined in level TOML files (e.g. <code>levels/00_sandbox_01.toml</code>). Add your entity placements there, or use the visual editor (<code>make run-editor</code>) to place entities graphically.</p>
  <h4>6. Add debug hitbox -- <code>src/core/debug.c</code></h4>
  <p>Every entity must have hitbox visualization in <code>debug.c</code>:</p>
  <pre><code className="language-c">{`// In debug_render:
for (int i = 0; i < gs->coin_count; i++) {
    if (!gs->coins[i].active) continue;
    SDL_Rect hb = { (int)gs->coins[i].x, (int)gs->coins[i].y,
                     gs->coins[i].w, gs->coins[i].h };
    SDL_SetRenderDrawColor(gs->renderer, 255, 255, 0, 128);
    SDL_RenderDrawRect(gs->renderer, &hb);
}`}</code></pre>
  <p>Also add <code>debug_log</code> calls in <code>game.c</code> for any significant entity events (collection, destruction, spawn).</p>
  <hr />
  <h2>Adding Physics to an Entity</h2>
  <p>Use the same pattern as <code>player_update</code>:</p>
  <pre><code className="language-c">{`/* Apply gravity while airborne */
if (!entity->on_ground) {
    entity->vy += GRAVITY * dt;
}

/* Integrate position */
entity->x += entity->vx * dt;
entity->y += entity->vy * dt;

/* Floor collision */
if (entity->y + entity->h >= FLOOR_Y) {
    entity->y        = (float)(FLOOR_Y - entity->h);
    entity->vy       = 0.0f;
    entity->on_ground = 1;
} else {
    entity->on_ground = 0;
}

/* Horizontal clamp */
if (entity->x < 0.0f)                entity->x = 0.0f;
if (entity->x > GAME_W - entity->w)  entity->x = (float)(GAME_W - entity->w);`}</code></pre>
  <p><code>GRAVITY</code>, <code>FLOOR_Y</code>, <code>GAME_W</code>, and <code>GAME_H</code> are all defined in <code>game.h</code> and available to any file that includes it. See <a href="constants_reference">Constants Reference</a> for values.</p>
  <hr />
  <h2>Adding a New Sound Effect</h2>
  <p>All sound files are <code>.wav</code> format, named with the convention <code>component_descriptor.wav</code>:</p>
  <table>
    <thead><tr><th>Sound</th><th>File</th></tr></thead>
    <tbody>
      <tr><td>Player jump</td><td><code>player_jump.wav</code></td></tr>
      <tr><td>Player hit</td><td><code>player_hit.wav</code></td></tr>
      <tr><td>Coin collect</td><td><code>coin.wav</code></td></tr>
      <tr><td>Bouncepad</td><td><code>bouncepad.wav</code></td></tr>
      <tr><td>Bird</td><td><code>bird.wav</code></td></tr>
      <tr><td>Fish</td><td><code>fish.wav</code></td></tr>
      <tr><td>Spider</td><td><code>spider.wav</code></td></tr>
      <tr><td>Axe trap</td><td><code>axe_trap.wav</code></td></tr>
    </tbody>
  </table>
  <p>Steps to add a new sound:</p>
  <ol>
    <li>Place <code>.wav</code> in <code>assets/sounds/&lt;category&gt;/</code>.</li>
    <li>Add <code>Mix_Chunk *snd_&lt;name&gt;;</code> to <code>GameState</code> in <code>game.h</code>.</li>
    <li>Load in <code>game_init</code> (non-fatal -- warn but continue):</li>
  </ol>
  <pre><code className="language-c">{`gs->snd_<name> = Mix_LoadWAV("assets/sounds/<category>/<name>.wav");
if (!gs->snd_<name>) {
    fprintf(stderr, "Warning: could not load <name>.wav: %s\\n", Mix_GetError());
}`}</code></pre>
  <ol start={4}><li>Free in <code>game_cleanup</code>:</li></ol>
  <pre><code className="language-c">{`if (gs->snd_<name>) { Mix_FreeChunk(gs->snd_<name>); gs->snd_<name> = NULL; }`}</code></pre>
  <ol start={5}><li>Play wherever needed:</li></ol>
  <pre><code className="language-c">{`if (gs->snd_<name>) Mix_PlayChannel(-1, gs->snd_<name>, 0);`}</code></pre>
  <p>See <a href="sounds">Sounds</a> for the full list of available sound files.</p>
  <hr />
  <h2>Adding Background Music</h2>
  <p>Background music is loaded via <code>Mix_LoadMUS</code> (not <code>Mix_LoadWAV</code>). The track path is configured per level via <code>music_path</code> in the TOML file:</p>
  <pre><code className="language-c">{`// Load (path from level TOML music_path field)
gs->music = Mix_LoadMUS(level->music_path);

// Play (looping)
Mix_PlayMusic(gs->music, -1);
Mix_VolumeMusic(level->music_volume);  // 0-128, configured per level

// Cleanup
Mix_HaltMusic();
Mix_FreeMusic(gs->music);
gs->music = NULL;`}</code></pre>
  <hr />
  <h2>Adding HUD / Text Rendering</h2>
  <p><code>SDL2_ttf</code> is already initialized in <code>main.c</code>. The font <code>round9x13.ttf</code> is in <code>assets/fonts/</code>.</p>
  <pre><code className="language-c">{`// Load font
TTF_Font *font = TTF_OpenFont("assets/fonts/round9x13.ttf", 13);
if (!font) { fprintf(stderr, "TTF_OpenFont: %s\\n", TTF_GetError()); }

// Render text to a surface, then upload to a texture
SDL_Color white = {255, 255, 255, 255};
SDL_Surface *surf = TTF_RenderText_Solid(font, "Score: 0", white);
SDL_Texture *tex  = SDL_CreateTextureFromSurface(renderer, surf);
SDL_FreeSurface(surf);

// Draw the texture
SDL_Rect dst = {10, 10, surf->w, surf->h};
SDL_RenderCopy(renderer, tex, NULL, &dst);

// Cleanup
SDL_DestroyTexture(tex);
TTF_CloseFont(font);`}</code></pre>
  <p>The HUD renders hearts (lives), life counter, and score. It is drawn after all game entities so it always appears on top.</p>
  <hr />
  <h2>Render Layer Order</h2>
  <p>Always draw in painter&#39;s algorithm order (back to front). The game currently uses 35 layers:</p>
  <pre><code>{` 1. Parallax background    (up to 8 layers from assets/sprites/backgrounds/, per level)
 2. Platforms              (platform.png 9-slice pillars)
 3. Floor tiles            (per-level tileset at FLOOR_Y, with floor-gap openings)
 4. Float platforms        (float_platform.png 3-slice hovering surfaces)
 5. Spike rows             (spike.png ground-level spike strips)
 6. Spike platforms        (spike_platform.png elevated spike hazards)
 7. Bridges                (bridge.png tiled crumble walkways)
 8. Bouncepads medium      (bouncepad_medium.png standard spring pads)
 9. Bouncepads small       (bouncepad_small.png low spring pads)
10. Bouncepads high        (bouncepad_high.png tall spring pads)
11. Rails                  (rail.png bitmask tile tracks)
12. Vines                  (vine.png climbable)
13. Ladders                (ladder.png climbable)
14. Ropes                  (rope.png climbable)
15. Coins                  (coin.png collectibles)
16. Star yellows           (star_yellow.png health pickups)
17. Star greens            (star_green.png health pickups)
18. Star reds              (star_red.png health pickups)
19. Last star              (end-of-level star using HUD star sprite)
20. Blue flames            (blue_flame.png erupting from floor gaps)
21. Fire flames            (fire_flame.png fire variant erupting from floor gaps)
22. Fish                   (fish.png jumping water enemies)
23. Faster fish            (faster_fish.png fast jumping enemies)
24. Water                  (water.png animated strip)
25. Spike blocks           (spike_block.png rail-riding hazards)
26. Axe traps              (axe_trap.png swinging hazards)
27. Circular saws          (circular_saw.png patrol hazards)
28. Spiders                (spider.png ground patrol)
29. Jumping spiders        (jumping_spider.png jumping patrol)
30. Birds                  (bird.png slow sine-wave)
31. Faster birds           (faster_bird.png fast sine-wave)
32. Player                 (player.png animated)
33. Fog                    (fog_background_1/2.png sliding overlay)
34. HUD                    (hearts, lives, score -- always on top)
35. Debug overlay          (FPS, hitboxes, event log -- when --debug)`}</code></pre>
  <p>See <a href="architecture">Architecture</a> for details on the render pipeline.</p>
  <hr />
  <h2>Sprite Sheet Workflow</h2>
  <p>To analyze a new sprite sheet:</p>
  <pre><code className="language-sh">{`python3 .claude/scripts/analyze_sprite.py assets/<sprite>.png`}</code></pre>
  <p>Frame math:</p>
  <pre><code>{`source_x = (frame_index % num_cols) * frame_w
source_y = (frame_index / num_cols) * frame_h`}</code></pre>
  <p>Standard animation row layout (most assets in this pack):</p>
  <table>
    <thead><tr><th>Row</th><th>Animation</th><th>Notes</th></tr></thead>
    <tbody>
      <tr><td>0</td><td>Idle</td><td>1-4 frames, subtle</td></tr>
      <tr><td>1</td><td>Walk / Run</td><td>6-8 frames, looping</td></tr>
      <tr><td>2</td><td>Jump (up)</td><td>2-4 frames, one-shot</td></tr>
      <tr><td>3</td><td>Fall / Land</td><td>2-4 frames</td></tr>
      <tr><td>4</td><td>Attack</td><td>4-8 frames, one-shot</td></tr>
      <tr><td>5</td><td>Death / Hurt</td><td>4-6 frames, one-shot</td></tr>
    </tbody>
  </table>
  <p>See <a href="assets">Assets</a> for sprite sheet dimensions and <a href="player_module">Player Module</a> for animation state machine details.</p>
  <hr />
  <h2>Checklist: Adding a New Entity</h2>
  <ul>
    <li><input type="checkbox" disabled /> Create <code>src/&lt;entity&gt;.h</code> with struct and function declarations</li>
    <li><input type="checkbox" disabled /> Create <code>src/&lt;entity&gt;.c</code> with init, update, render, cleanup</li>
    <li><input type="checkbox" disabled /> Add <code>#include &quot;&lt;entity&gt;.h&quot;</code> to <code>game.h</code></li>
    <li><input type="checkbox" disabled /> Add texture pointer, entity array, and count to <code>GameState</code> (by value, not pointer)</li>
    <li><input type="checkbox" disabled /> Load texture in <code>game_init</code> in <code>game.c</code></li>
    <li><input type="checkbox" disabled /> Call <code>&lt;entity&gt;_init</code> in <code>game_init</code></li>
    <li><input type="checkbox" disabled /> Call <code>&lt;entity&gt;_update</code> in <code>game_loop</code> update section</li>
    <li><input type="checkbox" disabled /> Call <code>&lt;entity&gt;_render</code> in <code>game_loop</code> render section (correct layer order)</li>
    <li><input type="checkbox" disabled /> Call <code>&lt;entity&gt;_cleanup</code> in <code>game_cleanup</code> (before <code>SDL_DestroyRenderer</code>)</li>
    <li><input type="checkbox" disabled /> Set all freed pointers to <code>NULL</code></li>
    <li><input type="checkbox" disabled /> Define entity spawn positions in a level TOML file or use the visual editor</li>
    <li><input type="checkbox" disabled /> Add hitbox visualization in <code>debug.c</code></li>
    <li><input type="checkbox" disabled /> Add <code>debug_log</code> calls in <code>game.c</code> for significant entity events</li>
    <li><input type="checkbox" disabled /> Build with <code>make</code> -- no Makefile changes needed</li>
    <li><input type="checkbox" disabled /> Test with <code>--debug</code> flag to verify hitboxes render correctly</li>
  </ul>
  <hr />
  <h2>Related Pages</h2>
  <ul>
    <li><a href="home">Home</a> -- project overview</li>
    <li><a href="architecture">Architecture</a> -- system design and game loop</li>
    <li><a href="build_system">Build System</a> -- compiling and running</li>
    <li><a href="source_files">Source Files</a> -- module-by-module reference</li>
    <li><a href="assets">Assets</a> -- sprite sheets and textures</li>
    <li><a href="sounds">Sounds</a> -- audio files and music</li>
    <li><a href="player_module">Player Module</a> -- player-specific details</li>
    <li><a href="constants_reference">Constants Reference</a> -- all defined constants</li>
  </ul>
</section>

{/* PLACEHOLDER_FINAL_SECTIONS */}

</main>
      </div>
    </>
  );
}
