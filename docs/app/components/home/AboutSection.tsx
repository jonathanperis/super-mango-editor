export default function AboutSection() {
  return (
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
  );
}
