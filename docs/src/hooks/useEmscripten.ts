import { useCallback, useRef } from "react";

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

export function useEmscripten() {
    const canvasRef = useRef<HTMLCanvasElement>(null);
    const statusRef = useRef<HTMLParagraphElement>(null);
    const playBtnRef = useRef<HTMLButtonElement>(null);
    const debugBtnRef = useRef<HTMLButtonElement>(null);

    const startGame = useCallback((debug: boolean) => {
        const btn = playBtnRef.current;
        const dbgBtn = debugBtnRef.current;
        const statusEl = statusRef.current;
        const canvas = canvasRef.current;

        if (!btn || !statusEl || !canvas) return;

        btn.disabled = true;
        if (dbgBtn) dbgBtn.disabled = true;
        btn.textContent = "Loading...";

        // Set tabIndex programmatically so the canvas is only keyboard-focusable
        // once the game is actually interactive (avoids static a11y linter warning).
        canvas.tabIndex = -1;

        statusEl.innerHTML = '<span class="spinner"></span>Downloading game data...';

        // Always pass --sandbox to skip start menu and load the sandbox level.
        // For debug mode, also pass --debug.
        const args: string[] = ["--sandbox"];
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

        // Prevent browser from intercepting game keys (arrows, space, shift, WASD).
        const handleKeyDown = (e: KeyboardEvent) => {
            if ([32, 37, 38, 39, 40, 16, 65, 68, 83, 87].includes(e.keyCode)) {
                e.preventDefault();
            }
        };
        document.addEventListener("keydown", handleKeyDown, false);

        const script = document.createElement("script");
        script.src = "super-mango.js";
        script.onerror = () => {
            statusEl.innerHTML = "Failed to load game files. Try refreshing the page.";
            btn.style.display = "inline-block";
            btn.disabled = false;
            btn.textContent = "Retry (~43 MB)";
            document.removeEventListener("keydown", handleKeyDown);
        };
        document.body.appendChild(script);
    }, []);

    return { canvasRef, statusRef, playBtnRef, debugBtnRef, startGame };
}
