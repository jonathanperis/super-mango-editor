#!/usr/bin/env python3
"""Study the semi-implicit Euler equations used by player.c, without graphics.

This is a numerical model, not a game performance benchmark or collision test.
Gravity is 800 px/s², initial velocity is zero, and the duration is one second.

The "variable dt" column is how the game used to run: one update per rendered
frame with dt = the frame's duration, so the result drifted with the display's
refresh rate. The game now uses the "fixed 60 Hz" column: an accumulator
(src/core/game_timing.c) collects real time and the simulation consumes it in
steps of exactly 1/60 s, however fast frames are rendered.
"""


def integrate(render_hz: int, fixed: bool) -> float:
    y = velocity = accumulator = 0.0
    for _ in range(render_hz):
        accumulator += 1 / render_hz
        while accumulator >= (1 / 60 - 1e-12) if fixed else accumulator > 0:
            dt = 1 / 60 if fixed else accumulator
            velocity += 800 * dt
            y += velocity * dt
            accumulator -= dt
    return y


if __name__ == "__main__":
    print("One-second free fall; exact displacement = 400 px")
    print("Render Hz | variable dt (px) | fixed 60 Hz (px, what the game does)")
    for hz in (30, 60, 144):
        print(f"{hz:9} | {integrate(hz, False):16.6f} | {integrate(hz, True):16.6f}")
    print("Variable steps change the trajectory with the render rate, so a jump would")
    print("peak at a different height on a 144 Hz display than on a 30 Hz one.")
    print("Fixed steps make it independent of render rate; integration error remains")
    print("(406.7 px instead of 400), but it is the same error on every machine.")
