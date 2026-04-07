import { RefObject } from "react";

interface PlaySectionProps {
  canvasRef: RefObject<HTMLCanvasElement | null>;
  statusRef: RefObject<HTMLParagraphElement | null>;
  playBtnRef: RefObject<HTMLButtonElement | null>;
  debugBtnRef: RefObject<HTMLButtonElement | null>;
  onPlay: () => void;
  onDebug: () => void;
}

export default function PlaySection({
  canvasRef,
  statusRef,
  playBtnRef,
  debugBtnRef,
  onPlay,
  onDebug,
}: PlaySectionProps) {
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
            <button
              id="play-btn"
              ref={playBtnRef}
              onClick={onPlay}
            >
              Play (~43 MB)
            </button>
            <button
              id="debug-btn"
              ref={debugBtnRef}
              onClick={onDebug}
              style={{ marginLeft: "8px", fontSize: "0.85em", opacity: 0.7 }}
            >
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
