export default function DownloadsSection() {
  return (
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
  );
}
