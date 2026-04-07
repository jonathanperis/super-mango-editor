"use client";

import { useEffect, useRef, useCallback } from "react";

declare global {
  interface Window {
    Module?: EmscriptenModule;
  }

  interface EmscriptenModule {
    arguments: string[];
    canvas: HTMLCanvasElement;
    setStatus: (text: string) => void;
    monitorRunDependencies: (left: number) => void;
  }
}

export default function HomePage() {
  const heroRef = useRef<HTMLElement>(null);
  const navRef = useRef<HTMLElement>(null);
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const statusRef = useRef<HTMLParagraphElement>(null);
  const playBtnRef = useRef<HTMLButtonElement>(null);
  const debugBtnRef = useRef<HTMLButtonElement>(null);

  /* ---- Parallax scroll + nav intersection observer ---- */
  useEffect(() => {
    const handleScroll = () => {
      document.documentElement.style.setProperty("--scroll", String(window.scrollY));
    };

    window.addEventListener("scroll", handleScroll);

    let observer: IntersectionObserver | null = null;
    const heroEl = heroRef.current;
    const navEl = navRef.current;

    if (heroEl && navEl) {
      observer = new IntersectionObserver(([entry]) => {
        navEl.classList.toggle("visible", !entry.isIntersecting);
      });
      observer.observe(heroEl);
    }

    return () => {
      window.removeEventListener("scroll", handleScroll);
      if (observer && heroEl) {
        observer.unobserve(heroEl);
        observer.disconnect();
      }
    };
  }, []);

  /* ---- Emscripten game loader ---- */
  const startGame = useCallback((debug: boolean) => {
    const btn = playBtnRef.current;
    const dbgBtn = debugBtnRef.current;
    const statusEl = statusRef.current;
    const canvas = canvasRef.current;

    if (!btn || !statusEl || !canvas) return;

    btn.disabled = true;
    if (dbgBtn) dbgBtn.disabled = true;
    btn.textContent = "Loading...";

    statusEl.innerHTML = '<span class="spinner"></span>Downloading game data...';

    const args: string[] = [];
    if (debug) args.push("--debug");

    canvas.addEventListener(
      "webglcontextlost",
      (e) => {
        alert("WebGL context lost — please reload the page.");
        e.preventDefault();
      },
      false
    );

    const Module: EmscriptenModule = {
      arguments: args,
      canvas: canvas,
      setStatus: (text: string) => {
        if (!text) {
          statusEl.style.display = "none";
          btn.style.display = "none";
        } else {
          statusEl.innerHTML = text.includes("Downloading")
            ? '<span class="spinner"></span>' + text
            : text;
          statusEl.style.display = "";
        }
      },
      monitorRunDependencies: (left: number) => {
        if (left === 0) {
          Module.setStatus("");
          canvasRef.current?.focus();
        }
      },
    };

    window.Module = Module;

    /* Dynamically load the Emscripten-generated JS loader. */
    const script = document.createElement("script");
    script.src = "super-mango.js";
    script.onerror = () => {
      statusEl.innerHTML = "Failed to load game files. Try refreshing the page.";
      btn.style.display = "inline-block";
      btn.disabled = false;
      btn.textContent = "Retry (~43 MB)";
    };
    document.body.appendChild(script);
  }, []);

  return (
    <>
      {/* Floating particles */}
      <div className="particles">
        <div className="particle"></div>
        <div className="particle"></div>
        <div className="particle"></div>
        <div className="particle"></div>
        <div className="particle"></div>
        <div className="particle"></div>
      </div>

      {/* Parallax hero */}
      <section className="hero" ref={heroRef}>
        <div className="parallax-layer sky-layer"></div>
        <div className="parallax-layer mountains-layer"></div>
        <div className="parallax-layer trees-far-layer"></div>
        <div className="parallax-layer trees-near-layer"></div>
        <div className="parallax-layer ground-layer"></div>
        <div className="hero-content">
          <div className="hero-badge">C11 &middot; SDL2 &middot; WebAssembly</div>
          <h1 className="hero-title">
            <span className="shimmer-text">
              SUPER
              <br />
              MANGO
            </span>
          </h1>
          <p className="hero-sub">A 2D pixel art platformer. Play in your browser.</p>
          <div className="hero-ctas">
            <a href="#play" className="btn-play">
              &#9654; PLAY NOW
            </a>
            <a href="#downloads" className="btn-download">
              &darr; DOWNLOAD
            </a>
          </div>
        </div>
        <div className="scroll-hint">SCROLL &darr;</div>
      </section>

      {/* Sticky nav */}
      <nav className="site-nav" id="site-nav" ref={navRef}>
        <a href="#" className="nav-logo">
          Super Mango
        </a>
        <ul className="nav-links">
          <li>
            <a href="#play">Play</a>
          </li>
          <li>
            <a href="#about">About</a>
          </li>
          <li>
            <a href="#docs">Docs</a>
          </li>
          <li>
            <a href="/docs">Full Docs</a>
          </li>
          <li>
            <a href="#downloads">Downloads</a>
          </li>
        </ul>
      </nav>

      {/* Arcade cabinet game section */}
      <section id="play" className="play-section">
        <div className="cabinet-frame">
          <div className="cabinet-bezel">
            <canvas
              id="canvas"
              ref={canvasRef}
              onContextMenu={(e) => e.preventDefault()}
              tabIndex={-1}
            ></canvas>
            <p id="game-status" ref={statusRef}>
              <button
                id="play-btn"
                ref={playBtnRef}
                onClick={() => startGame(false)}
              >
                Play (~43 MB)
              </button>
              <button
                id="debug-btn"
                ref={debugBtnRef}
                onClick={() => startGame(true)}
                style={{ marginLeft: "8px", fontSize: "0.85em", opacity: 0.7 }}
              >
                Debug Mode
              </button>
            </p>
          </div>
          <div className="cabinet-controls">
            <kbd>WASD</kbd> / <kbd>Arrows</kbd> Move &nbsp;&middot;&nbsp;
            <kbd>Space</kbd> Jump &nbsp;&middot;&nbsp;
            <kbd>ESC</kbd> Quit
          </div>
          <div className="cabinet-base"></div>
        </div>
      </section>

      {/* About + Features */}
      <section id="about" className="about-section">
        <h2 className="section-title">ABOUT THE GAME</h2>
        <p>
          Super Mango is a classic side-scrolling platformer built as a{" "}
          <strong>learning resource</strong> for C and SDL2 game programming. Every line
          of code is commented to explain the <em>why</em>, not just the what — making it
          ideal for developers who want to learn how 2D games work under the hood.
        </p>
        <p>
          The game renders at 400&times;300 logical pixels scaled 2&times; to an
          800&times;600 window, uses delta-time physics for frame-rate-independent
          movement, and follows a clean init &rarr; loop &rarr; cleanup architecture.
        </p>

        <div className="feature-grid">
          <div className="feature-card">
            <h3>Platforming</h3>
            <p>
              One-way platforms, float platforms (static, crumble, rail), crumble
              bridges, and floor gaps with animated water.
            </p>
          </div>
          <div className="feature-card">
            <h3>Enemies</h3>
            <p>
              6 enemy types: spiders, jumping spiders, birds, faster birds, fish, and
              faster fish — each with unique behavior.
            </p>
          </div>
          <div className="feature-card">
            <h3>Hazards</h3>
            <p>
              Spike rows, spike platforms, axe traps, circular saws, blue flames, and
              spike blocks on rails.
            </p>
          </div>
          <div className="feature-card">
            <h3>Mechanics</h3>
            <p>
              Bouncepads (3 heights), climbable vines, ladders, ropes, collectible coins
              and stars, lives and hearts system.
            </p>
          </div>
          <div className="feature-card">
            <h3>Visual Polish</h3>
            <p>
              Parallax multi-layer scrolling, 5-state player animation, scrolling camera,
              fog overlay, and pixel-perfect rendering.
            </p>
          </div>
          <div className="feature-card">
            <h3>Cross-Platform</h3>
            <p>
              Builds natively for macOS, Linux, Windows, and WebAssembly. Play on desktop
              or in your browser.
            </p>
          </div>
        </div>
      </section>

      {/* Documentation */}
      <section id="docs" className="docs-section">
        <h2 className="section-title">DOCUMENTATION</h2>
        <div style={{ textAlign: "center", marginBottom: "2rem" }}>
          <a
            href="/docs"
            className="docs-view-btn"
            style={{
              display: "inline-block",
              padding: "0.75rem 2rem",
              background: "var(--accent)",
              color: "var(--bg)",
              fontFamily: "var(--pixel)",
              fontSize: "1.1rem",
              fontWeight: 700,
              textDecoration: "none",
              borderRadius: "6px",
              letterSpacing: "0.02em",
              transition: "background 0.2s",
            }}
          >
            View full documentation &rarr;
          </a>
        </div>
        <div className="docs-grid">
          <a
            className="doc-card"
            href="https://github.com/jonathanperis/super-mango-editor#readme"
            target="_blank"
            rel="noreferrer noopener"
            aria-label="Read the project README on GitHub"
          >
            <h3>README</h3>
            <p>
              Project overview, build instructions for all platforms, controls reference,
              architecture diagram, and full project structure.
            </p>
          </a>
          <a
            className="doc-card"
            href="https://github.com/jonathanperis/super-mango-editor/wiki"
            target="_blank"
            rel="noreferrer noopener"
            aria-label="Visit the Wiki home page"
          >
            <h3>Wiki &mdash; Home</h3>
            <p>
              Landing page with quick-start guide, project-at-a-glance, and navigation to
              all wiki pages.
            </p>
          </a>
          <a
            className="doc-card"
            href="https://github.com/jonathanperis/super-mango-editor/wiki/architecture"
            target="_blank"
            rel="noreferrer noopener"
            aria-label="Read the Architecture wiki page"
          >
            <h3>Wiki &mdash; Architecture</h3>
            <p>
              Startup sequence, game loop phases, coordinate system, GameState struct, and
              error handling strategy.
            </p>
          </a>
          <a
            className="doc-card"
            href="https://github.com/jonathanperis/super-mango-editor/wiki/build_system"
            target="_blank"
            rel="noreferrer noopener"
            aria-label="Read the Build System wiki page"
          >
            <h3>Wiki &mdash; Build System</h3>
            <p>
              Makefile overview, compiler flags, build targets, prerequisites, and adding
              new source files.
            </p>
          </a>
          <a
            className="doc-card"
            href="https://github.com/jonathanperis/super-mango-editor/wiki/developer_guide"
            target="_blank"
            rel="noreferrer noopener"
            aria-label="Read the Developer Guide wiki page"
          >
            <h3>Wiki &mdash; Developer Guide</h3>
            <p>
              Coding conventions, naming rules, memory safety, and step-by-step entity
              addition walkthrough.
            </p>
          </a>
          <a
            className="doc-card"
            href="https://github.com/jonathanperis/super-mango-editor/wiki/player_module"
            target="_blank"
            rel="noreferrer noopener"
            aria-label="Read the Player Module wiki page"
          >
            <h3>Wiki &mdash; Player Module</h3>
            <p>
              Player lifecycle, input handling, jump and climb logic, physics update,
              animation state machine.
            </p>
          </a>
          <a
            className="doc-card"
            href="https://github.com/jonathanperis/super-mango-editor/wiki/source_files"
            target="_blank"
            rel="noreferrer noopener"
            aria-label="Read the Source Files wiki page"
          >
            <h3>Wiki &mdash; Source Files</h3>
            <p>
              Complete file map of all 48 source files with descriptions of main.c,
              game.h, and game.c roles.
            </p>
          </a>
          <a
            className="doc-card"
            href="https://github.com/jonathanperis/super-mango-editor/wiki/assets"
            target="_blank"
            rel="noreferrer noopener"
            aria-label="Read the Assets wiki page"
          >
            <h3>Wiki &mdash; Assets</h3>
            <p>
              Asset catalog with 57 sprites, player sprite sheet analysis, animation row
              mapping, and unused assets.
            </p>
          </a>
          <a
            className="doc-card"
            href="https://github.com/jonathanperis/super-mango-editor/wiki/sounds"
            target="_blank"
            rel="noreferrer noopener"
            aria-label="Read the Sounds wiki page"
          >
            <h3>Wiki &mdash; Sounds</h3>
            <p>
              Sound effects catalog, audio configuration, Mix_Chunk vs Mix_Music, and
              adding new sound effects.
            </p>
          </a>
          <a
            className="doc-card"
            href="https://github.com/jonathanperis/super-mango-editor/wiki/constants_reference"
            target="_blank"
            rel="noreferrer noopener"
            aria-label="Read the Constants Reference wiki page"
          >
            <h3>Wiki &mdash; Constants</h3>
            <p>
              All compile-time constants: window, canvas, timing, tiles, physics, camera,
              and player locals.
            </p>
          </a>
        </div>
      </section>

      {/* Downloads */}
      <section id="downloads" className="downloads-section">
        <h2 className="section-title">ITEM DROP</h2>
        <p>
          Grab the latest release from{" "}
          <a
            href="https://github.com/jonathanperis/super-mango-editor/releases/latest"
            style={{ color: "var(--accent)" }}
            target="_blank"
            rel="noreferrer noopener"
          >
            GitHub Releases
          </a>
          . Native builds must be run from the repository root so they can find the{" "}
          <code>assets/</code> folder.
        </p>
        <div className="download-grid">
          <a
            className="download-card"
            href="https://github.com/jonathanperis/super-mango-editor/releases/latest"
            target="_blank"
            rel="noreferrer noopener"
            aria-label="Download Super Mango for Linux x86_64"
          >
            <div className="platform">Linux</div>
            <div className="arch">x86_64</div>
          </a>
          <a
            className="download-card"
            href="https://github.com/jonathanperis/super-mango-editor/releases/latest"
            target="_blank"
            rel="noreferrer noopener"
            aria-label="Download Super Mango for macOS Apple Silicon"
          >
            <div className="platform">macOS</div>
            <div className="arch">arm64 (Apple Silicon)</div>
          </a>
          <a
            className="download-card"
            href="https://github.com/jonathanperis/super-mango-editor/releases/latest"
            target="_blank"
            rel="noreferrer noopener"
            aria-label="Download Super Mango for Windows x86_64"
          >
            <div className="platform">Windows</div>
            <div className="arch">x86_64 (SDL2 DLLs included)</div>
          </a>
          <a
            className="download-card"
            href="#play"
            style={{ borderColor: "var(--green)" }}
          >
            <div className="platform" style={{ color: "var(--green)" }}>
              WebAssembly
            </div>
            <div className="arch">Play in browser above!</div>
          </a>
        </div>
      </section>

      {/* Footer */}
      <footer>
        <p>
          <a
            href="https://github.com/jonathanperis/super-mango-editor"
            target="_blank"
            rel="noreferrer noopener"
            aria-label="Super Mango GitHub Repository"
          >
            GitHub Repository
          </a>
          &nbsp;&middot;&nbsp;
          <a
            href="https://github.com/jonathanperis/super-mango-editor/wiki"
            target="_blank"
            rel="noreferrer noopener"
            aria-label="Super Mango Wiki"
          >
            Wiki
          </a>
          &nbsp;&middot;&nbsp;
          <a
            href="https://github.com/jonathanperis/super-mango-editor/releases/latest"
            target="_blank"
            rel="noreferrer noopener"
            aria-label="Super Mango Releases"
          >
            Releases
          </a>
        </p>
        <p>
          Created by{" "}
          <a
            href="https://github.com/jonathanperis"
            target="_blank"
            rel="noreferrer noopener"
          >
            jonathanperis
          </a>{" "}
          &amp;{" "}
          <a
            href="https://github.com/fersantos"
            target="_blank"
            rel="noreferrer noopener"
          >
            fersantos
          </a>
          .
        </p>
        <p>
          Art from{" "}
          <a href="https://juho.itch.io" target="_blank" rel="noreferrer noopener">
            Super Mango 2D Pixel Art Platformer Asset Pack
          </a>{" "}
          by Juho.
        </p>
      </footer>
    </>
  );
}
