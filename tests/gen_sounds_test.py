"""Contract checks for tools/gen_sounds.py: deterministic, small, loopable."""
from pathlib import Path
import hashlib
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import gen_sounds
import qoa

SOUND_BUDGET = 1024 * 1024
LOOPS = {"levels/water.qoa", "levels/lava.qoa", "levels/winds.qoa"}


def decode(data):
    rate, pcm = qoa.decode(data)
    assert rate == gen_sounds.SAMPLE_RATE
    return pcm


def nudged(pcm, delta, count):
    """Copy samples with `count` spread-out samples shifted by `delta`."""
    out = list(pcm)
    for i in range(1, count + 1):
        j = i * len(out) // (count + 1)
        out[j] = max(-32767, min(32767, out[j] + delta))
    return out


def check_tolerance(pcm):
    # libm may round a few samples differently on another platform, which
    # can change a few encoded slices; --check must accept such a file.
    data = gen_sounds.encode(pcm)
    assert gen_sounds.qoa_matches(data, pcm, data)
    other_platform = gen_sounds.encode(nudged(pcm, 1, 8))
    assert gen_sounds.qoa_matches(other_platform, pcm, data), "1-LSB jitter rejected"
    # A louder recipe, other samples or another length must not pass.
    louder = gen_sounds.encode([max(-32767, min(32767, round(v * 1.06))) for v in pcm])
    assert not gen_sounds.qoa_matches(louder, pcm, data), "0.5 dB louder sound accepted"
    assert not gen_sounds.qoa_matches(gen_sounds.encode(pcm[::-1]), pcm, data)
    assert not gen_sounds.qoa_matches(data[:-8], pcm, data), "length must match"
    assert not gen_sounds.qoa_matches(b"RIFF" + data[4:], pcm, data), "not QOA"


def check_reference_vector():
    # SHA-256 of what raylib's own encoder (src/external/qoa.h in the pinned
    # raylib 6.0, qoa_encode) writes for these samples: a sawtooth over two
    # frames, then full-scale noise. qoa.py must produce the same bytes.
    samples, x = [((i * 2477) % 20000) - 10000 for i in range(5125)], 1
    for _ in range(600):
        x = (x * 1103515245 + 12345) & 0x7FFFFFFF
        samples.append((x >> 8) % 60000 - 30000)
    data = qoa.encode(samples, 22050)
    assert len(data) == 2352, len(data)
    assert hashlib.sha256(data).hexdigest() == (
        "2363abb800125a12421a3fae0a0b669df4d441a334873d48e3b142ff71b08179"), "qoa.h mismatch"
    rate, decoded = qoa.decode(data)
    assert rate == 22050 and len(decoded) == len(samples)


def check_write_skips_unchanged(pcm, rendered):
    # `make sounds` on a platform whose libm rounds differently must not
    # rewrite files that decode as well as this platform's encoding.
    path = "player/player_jump.qoa"
    subset, samples = {path: rendered[path]}, {path: pcm[path]}
    with tempfile.TemporaryDirectory() as tmp:
        out_dir = Path(tmp)
        assert gen_sounds.write_sounds(subset, samples, out_dir) == {path: "written"}
        assert (out_dir / path).read_bytes() == rendered[path]
        within = gen_sounds.encode(nudged(pcm[path], 1, 8))
        (out_dir / path).write_bytes(within)
        assert gen_sounds.write_sounds(subset, samples, out_dir) == {path: "unchanged"}
        assert (out_dir / path).read_bytes() == within, "1-LSB file rewritten"
        (out_dir / path).write_bytes(gen_sounds.encode(pcm[path][::-1]))
        assert gen_sounds.write_sounds(subset, samples, out_dir) == {path: "written"}
        assert (out_dir / path).read_bytes() == rendered[path]
        # A WAV left over from before the sounds became QOA is reported.
        (out_dir / "player/player_jump.wav").write_bytes(b"RIFF")
        assert gen_sounds.check(subset, samples, out_dir) == [
            "assets/sounds/player/player_jump.wav is not produced by tools/gen_sounds.py"]


def main():
    pcm = gen_sounds.render_all_pcm()
    assert pcm == gen_sounds.render_all_pcm(), "generation must be deterministic"
    rendered = gen_sounds.render_all(pcm)
    check_reference_vector()
    check_tolerance(pcm["player/player_hit.qoa"])
    check_write_skips_unchanged(pcm, rendered)
    # The C code loads these exact paths; the generator owns all of them.
    committed = sorted(p.relative_to(gen_sounds.SOUND_DIR).as_posix()
                       for p in gen_sounds.SOUND_DIR.rglob("*.qoa"))
    assert committed == sorted(rendered), (committed, sorted(rendered))
    assert sum(map(len, rendered.values())) < SOUND_BUDGET, "sound budget"
    for path, data in rendered.items():
        samples = pcm[path]
        peak = max(abs(v) for v in samples)
        assert 0.25 * 32767 < peak < 0.75 * 32767, (path, peak)  # -12..-2.5 dBFS
        duration = len(samples) / gen_sounds.SAMPLE_RATE
        # What raylib plays is the decoded file, so the seams are checked there.
        decoded = decode(data)
        assert len(decoded) == len(samples), path
        if path in LOOPS:
            assert 4.0 <= duration <= 8.0, (path, duration)
            # Wrapping from the last sample to the first must be no bigger
            # than a typical step inside the loop, i.e. no click.
            steps = [abs(decoded[i + 1] - decoded[i]) for i in range(len(decoded) - 1)]
            assert abs(decoded[0] - decoded[-1]) <= 2 * sum(steps) / len(steps), path
        else:
            assert duration <= 1.0, (path, duration)
            assert abs(samples[-1]) < 64, f"{path} must fade out, not cut off"
            assert abs(decoded[-1]) < 64, f"{path} must still fade out once encoded"
    assert gen_sounds.check(rendered, pcm) == [], "committed sounds are stale"
    print(f"gen sounds test: ok ({len(rendered)} sounds)")


if __name__ == "__main__":
    main()
