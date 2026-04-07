import { useCallback, RefObject } from "react";

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

export function useEmscripten(
  canvasRef: RefObject<HTMLCanvasElement | null>,
  statusRef: RefObject<HTMLParagraphElement | null>,
  playBtnRef: RefObject<HTMLButtonElement | null>,
  debugBtnRef: RefObject<HTMLButtonElement | null>
) {
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

  return { startGame };
}
