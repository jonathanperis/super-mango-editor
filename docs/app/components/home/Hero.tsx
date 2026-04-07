import { RefObject } from "react";

interface HeroProps {
  heroRef: RefObject<HTMLElement | null>;
}

export default function Hero({ heroRef }: HeroProps) {
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
    </>
  );
}
