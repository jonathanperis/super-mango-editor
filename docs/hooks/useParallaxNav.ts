import { useEffect, RefObject } from "react";

export function useParallaxNav(
  heroRef: RefObject<HTMLElement | null>,
  navRef: RefObject<HTMLElement | null>
) {
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
}
