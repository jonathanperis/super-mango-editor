export default function HomeSection({ visible }: { visible: boolean }) {
  return (
    <section id="home" className={`doc-section${!visible ? " hidden-section" : ""}`}>
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
  );
}
