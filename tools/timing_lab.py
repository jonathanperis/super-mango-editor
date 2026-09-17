#!/usr/bin/env python3
"""Study the semi-implicit Euler equations used by player.c, without SDL.

This is a numerical model, not a game performance benchmark or collision test.
Gravity is 800 px/s², initial velocity is zero, and the duration is one second.
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
    print("Render Hz | variable dt (px) | fixed 60 Hz (px)")
    for hz in (30, 60, 144):
        print(f"{hz:9} | {integrate(hz, False):16.6f} | {integrate(hz, True):16.6f}")
    print("Fixed steps make this numerical trajectory independent of render rate; integration error remains.")
