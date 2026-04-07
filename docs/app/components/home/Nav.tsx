import { RefObject } from "react";

interface NavProps {
  navRef: RefObject<HTMLElement | null>;
}

export default function Nav({ navRef }: NavProps) {
  return (
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
  );
}
