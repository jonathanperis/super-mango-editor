#!/usr/bin/env python3
"""
qoa.py — A pure-Python QOA ("Quite OK Audio") encoder and decoder.

Ported from qoa.h, which carries this notice:
    Copyright (c) 2023, Dominic Szablewski - https://phoboslab.org
    SPDX-License-Identifier: MIT

QOA is the lossy audio format raylib reads natively (SUPPORT_FILEFORMAT_QOA,
src/external/qoa.h in the pinned raylib source). It stores 16-bit PCM at a
fixed 3.2 bits per sample, about a fifth of a 16-bit WAV, and decodes with a
few integer operations per sample.

This module is a line-by-line port of qoa.h's reference encoder (qoa_encode,
qoa_encode_frame) and decoder (qoa_decode_frame), so tools/gen_sounds.py can
write the game's sounds without building any C. For the same samples it
produces the same bytes as qoa.h. Python integers never overflow, so the few
places where qoa.h's 32-bit `int` arithmetic can wrap are wrapped explicitly
(see i32).

How QOA works, briefly:

    * Samples are grouped into slices of 20 and frames of 256 slices (5120
      samples). A frame header stores the predictor state, so every frame can
      be decoded on its own.
    * A 4-tap LMS (least mean squares) filter predicts each sample from the
      previous four reconstructed samples and adapts its weights as it goes.
    * Only the residual (sample minus prediction) is stored: divided by one of
      16 scalefactors chosen per slice, and quantized to 3 bits.
    * The encoder tries all 16 scalefactors on every slice and keeps the one
      with the smallest squared error (plus a penalty that stops the weights
      from growing large enough to click).
"""

SLICE_LEN = 20
SLICES_PER_FRAME = 256
FRAME_LEN = SLICE_LEN * SLICES_PER_FRAME
LMS_LEN = 4
MAGIC = 0x716F6166  # 'qoaf'

QUANT_TAB = (7, 7, 7, 5, 5, 3, 3, 1, 0, 0, 2, 2, 4, 4, 6, 6, 6)
RECIPROCAL_TAB = (65536, 9363, 3121, 1457, 781, 475, 311, 216,
                  156, 117, 90, 71, 57, 47, 39, 32)
DEQUANT_TAB = (
    (1, -1, 3, -3, 5, -5, 7, -7),
    (5, -5, 18, -18, 32, -32, 49, -49),
    (16, -16, 53, -53, 95, -95, 147, -147),
    (34, -34, 113, -113, 203, -203, 315, -315),
    (63, -63, 210, -210, 378, -378, 588, -588),
    (104, -104, 345, -345, 621, -621, 966, -966),
    (158, -158, 528, -528, 950, -950, 1477, -1477),
    (228, -228, 760, -760, 1368, -1368, 2128, -2128),
    (316, -316, 1053, -1053, 1895, -1895, 2947, -2947),
    (422, -422, 1405, -1405, 2529, -2529, 3934, -3934),
    (548, -548, 1828, -1828, 3290, -3290, 5117, -5117),
    (696, -696, 2320, -2320, 4176, -4176, 6496, -6496),
    (868, -868, 2893, -2893, 5207, -5207, 8099, -8099),
    (1064, -1064, 3548, -3548, 6386, -6386, 9933, -9933),
    (1286, -1286, 4288, -4288, 7718, -7718, 12005, -12005),
    (1536, -1536, 5120, -5120, 9216, -9216, 14336, -14336),
)
U64_MAX = (1 << 64) - 1


def i32(value):
    """Wrap an integer to C's 32-bit signed `int`, as qoa.h's arithmetic does."""
    if -0x80000000 <= value <= 0x7FFFFFFF:
        return value
    return ((value + 0x80000000) & 0xFFFFFFFF) - 0x80000000


def clamp_s16(value):
    return -32768 if value < -32768 else 32767 if value > 32767 else value


def u64(value):
    """Eight big-endian bytes, as qoa_write_u64 writes them."""
    return (value & U64_MAX).to_bytes(8, "big")


def quantize(residual, scalefactor):
    """qoa_div + clamp + quant_tab: the 3-bit code for a residual."""
    n = i32(residual * RECIPROCAL_TAB[scalefactor] + (1 << 15)) >> 16
    # Round away from zero, so a tiny non-zero residual never becomes 0.
    n += (residual > 0) - (residual < 0) - ((n > 0) - (n < 0))
    return QUANT_TAB[(-8 if n < -8 else 8 if n > 8 else n) + 8]


def _encode_slice(samples, start, end, history, weights, scalefactor, best_rank):
    """Try one scalefactor on one slice. Returns (rank, slice bits, history,
    weights); the rank is only exact when it does not exceed best_rank."""
    h0, h1, h2, h3 = history
    w0, w1, w2, w3 = weights
    dequant = DEQUANT_TAB[scalefactor]
    bits = scalefactor
    rank = 0
    for si in range(start, end):
        sample = samples[si]
        predicted = i32(w0 * h0 + w1 * h1 + w2 * h2 + w3 * h3) >> 13
        residual = sample - predicted
        quantized = quantize(residual, scalefactor)
        dequantized = dequant[quantized]
        reconstructed = clamp_s16(predicted + dequantized)
        # Large weights make the filter ring; qoa.h penalizes them.
        penalty = (i32(w0 * w0 + w1 * w1 + w2 * w2 + w3 * w3) >> 18) - 0x8FF
        if penalty < 0:
            penalty = 0
        error = sample - reconstructed
        rank += error * error + penalty * penalty
        if rank > best_rank:
            break
        delta = dequantized >> 4
        w0 += -delta if h0 < 0 else delta
        w1 += -delta if h1 < 0 else delta
        w2 += -delta if h2 < 0 else delta
        w3 += -delta if h3 < 0 else delta
        h0, h1, h2, h3 = h1, h2, h3, reconstructed
        bits = (bits << 3) | quantized
    return rank, bits, (h0, h1, h2, h3), (w0, w1, w2, w3)


def encode(samples, samplerate):
    """Encode mono 16-bit samples (a sequence of ints) as QOA file bytes."""
    count = len(samples)
    if count == 0 or not 0 < samplerate <= 0xFFFFFF:
        raise ValueError("QOA needs at least one sample and a 24-bit sample rate")
    out = bytearray(u64((MAGIC << 32) | count))
    # Initial weights {0, 0, -1, 2} (in .13 fixed point) predict the first
    # milliseconds better than all zeros.
    history, weights = (0, 0, 0, 0), (0, 0, -(1 << 13), 1 << 14)
    for frame_start in range(0, count, FRAME_LEN):
        frame_len = min(FRAME_LEN, count - frame_start)
        slices = (frame_len + SLICE_LEN - 1) // SLICE_LEN
        frame_size = 8 + LMS_LEN * 4 + 8 * slices
        out += u64((1 << 56) | (samplerate << 32) | (frame_len << 16) | frame_size)
        packed_history = packed_weights = 0
        for i in range(LMS_LEN):
            packed_history = (packed_history << 16) | (history[i] & 0xFFFF)
            packed_weights = (packed_weights << 16) | (weights[i] & 0xFFFF)
        out += u64(packed_history) + u64(packed_weights)
        previous = 0
        for start in range(frame_start, frame_start + frame_len, SLICE_LEN):
            end = min(start + SLICE_LEN, frame_start + frame_len)
            best = (U64_MAX, 0, history, weights)
            best_scalefactor = 0
            for step in range(16):
                # Neighbouring slices tend to share a scalefactor: try the
                # previous slice's first, so the early exit above triggers more.
                scalefactor = (step + previous) & 15
                trial = _encode_slice(samples, start, end, history, weights,
                                      scalefactor, best[0])
                if trial[0] < best[0]:
                    best, best_scalefactor = trial, scalefactor
            previous = best_scalefactor
            _, bits, history, weights = best
            # A short final slice is left-aligned in its 64 bits.
            out += u64(bits << ((SLICE_LEN - (end - start)) * 3))
    return bytes(out)


def decode(data):
    """Decode mono QOA file bytes. Returns (samplerate, list of int samples)."""
    if len(data) < 16 or int.from_bytes(data[:4], "big") != MAGIC:
        raise ValueError("not a QOA file")
    count = int.from_bytes(data[4:8], "big")
    out = []
    samplerate = None
    p = 8
    while len(out) < count:
        header = int.from_bytes(data[p:p + 8], "big")
        channels, rate = header >> 56, (header >> 32) & 0xFFFFFF
        frame_len, frame_size = (header >> 16) & 0xFFFF, header & 0xFFFF
        if channels != 1 or (samplerate is not None and rate != samplerate):
            raise ValueError("only mono QOA files with one sample rate are supported")
        samplerate = rate
        packed_history = int.from_bytes(data[p + 8:p + 16], "big")
        packed_weights = int.from_bytes(data[p + 16:p + 24], "big")

        def s16(packed, i):
            value = (packed >> (48 - 16 * i)) & 0xFFFF
            return value - 0x10000 if value & 0x8000 else value
        history = [s16(packed_history, i) for i in range(LMS_LEN)]
        weights = [s16(packed_weights, i) for i in range(LMS_LEN)]
        q = p + 24
        for start in range(0, frame_len, SLICE_LEN):
            bits = int.from_bytes(data[q:q + 8], "big")
            q += 8
            dequant = DEQUANT_TAB[bits >> 60]
            for i in range(min(SLICE_LEN, frame_len - start)):
                predicted = i32(sum(w * h for w, h in zip(weights, history))) >> 13
                dequantized = dequant[(bits >> (57 - 3 * i)) & 7]
                reconstructed = clamp_s16(predicted + dequantized)
                out.append(reconstructed)
                delta = dequantized >> 4
                weights = [w - delta if h < 0 else w + delta for w, h in zip(weights, history)]
                history = history[1:] + [reconstructed]
        if q - p != frame_size:
            raise ValueError("QOA frame size does not match its slices")
        p = q
    return samplerate, out
