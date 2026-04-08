"use client";

import { useEmscripten } from "@/hooks/useEmscripten";

export default function PlaySection() {
    const { canvasRef, statusRef, playBtnRef, debugBtnRef, startGame } = useEmscripten();

    return (
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
                        <button id="play-btn" ref={playBtnRef} onClick={() => startGame(false)}>
                            Play (~43 MB)
                        </button>
                        <button id="debug-btn" ref={debugBtnRef} onClick={() => startGame(true)}>
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
    );
}
