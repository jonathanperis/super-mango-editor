"""Contract checks for tools/gen_sounds.py: deterministic, small, loopable."""
from pathlib import Path
import io
import struct
import sys
import wave

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import gen_sounds

SOUND_BUDGET = 2 * 1024 * 1024
LOOPS = {"levels/water.wav", "levels/lava.wav", "levels/winds.wav"}


def decode(data):
    with wave.open(io.BytesIO(data)) as clip:
        assert clip.getnchannels() == 1, "mono"
        assert clip.getsampwidth() == 2, "16-bit PCM"
        assert clip.getframerate() == gen_sounds.SAMPLE_RATE
        frames = clip.getnframes()
        return struct.unpack(f"<{frames}h", clip.readframes(frames))


def nudge(data, delta):
    """Copy a WAV with its middle sample shifted by `delta` int16 steps."""
    with wave.open(io.BytesIO(data)) as clip:
        frames = clip.getnframes()
    header = len(data) - 2 * frames
    offset = header + 2 * (frames // 2)
    value = struct.unpack_from("<h", data, offset)[0]
    value += delta if abs(value + delta) <= 32767 else -delta
    out = bytearray(data)
    struct.pack_into("<h", out, offset, value)
    return bytes(out)


def check_tolerance(data):
    # libm may differ by an ulp across platforms, so --check allows 1 LSB.
    assert gen_sounds.wav_matches(data, data)
    for delta in (1, -1):
        assert gen_sounds.wav_matches(nudge(data, delta), data), delta
    for delta in (2, -2):
        assert not gen_sounds.wav_matches(nudge(data, delta), data), delta
    assert not gen_sounds.wav_matches(data[:-2], data), "frame count must match"
    header_changed = bytearray(data)
    header_changed[24] ^= 1                     # sample-rate byte
    assert not gen_sounds.wav_matches(bytes(header_changed), data), "header"


def main():
    first = gen_sounds.render_all()
    assert first == gen_sounds.render_all(), "generation must be deterministic"
    check_tolerance(first["player/player_jump.wav"])
    # The C code loads these exact paths; the generator owns all of them.
    committed = sorted(p.relative_to(gen_sounds.SOUND_DIR).as_posix()
                       for p in gen_sounds.SOUND_DIR.rglob("*.wav"))
    assert committed == sorted(first), (committed, sorted(first))
    assert sum(map(len, first.values())) < SOUND_BUDGET, "sound budget"
    for path, data in first.items():
        pcm = decode(data)
        peak = max(abs(v) for v in pcm)
        assert 0.25 * 32767 < peak < 0.75 * 32767, (path, peak)  # -12..-2.5 dBFS
        duration = len(pcm) / gen_sounds.SAMPLE_RATE
        if path in LOOPS:
            assert 4.0 <= duration <= 8.0, (path, duration)
            # Wrapping from the last sample to the first must be no bigger
            # than a typical step inside the loop, i.e. no click.
            steps = [abs(pcm[i + 1] - pcm[i]) for i in range(len(pcm) - 1)]
            assert abs(pcm[0] - pcm[-1]) <= 2 * sum(steps) / len(steps), path
        else:
            assert duration <= 1.0, (path, duration)
            assert abs(pcm[-1]) < 64, f"{path} must fade out, not cut off"
    assert gen_sounds.main(["--check"]) == 0, "committed sounds are stale"
    print(f"gen sounds test: ok ({len(first)} sounds)")


if __name__ == "__main__":
    main()
