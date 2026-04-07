"use client";
import { useRef } from "react";
import { useParallaxNav } from "@/hooks/useParallaxNav";
import { useEmscripten } from "@/hooks/useEmscripten";
import Hero from "@/app/components/home/Hero";
import Nav from "@/app/components/home/Nav";
import PlaySection from "@/app/components/home/PlaySection";
import AboutSection from "@/app/components/home/AboutSection";
import DocsSection from "@/app/components/home/DocsSection";
import DownloadsSection from "@/app/components/home/DownloadsSection";
import Footer from "@/app/components/home/Footer";

export default function HomePage() {
  const heroRef = useRef<HTMLElement>(null);
  const navRef = useRef<HTMLElement>(null);
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const statusRef = useRef<HTMLParagraphElement>(null);
  const playBtnRef = useRef<HTMLButtonElement>(null);
  const debugBtnRef = useRef<HTMLButtonElement>(null);

  useParallaxNav(heroRef, navRef);
  const { startGame } = useEmscripten(canvasRef, statusRef, playBtnRef, debugBtnRef);

  return (
    <>
      <Hero heroRef={heroRef} />
      <Nav navRef={navRef} />
      <PlaySection
        canvasRef={canvasRef}
        statusRef={statusRef}
        playBtnRef={playBtnRef}
        debugBtnRef={debugBtnRef}
        onPlay={() => startGame(false)}
        onDebug={() => startGame(true)}
      />
      <AboutSection />
      <DocsSection />
      <DownloadsSection />
      <Footer />
    </>
  );
}
