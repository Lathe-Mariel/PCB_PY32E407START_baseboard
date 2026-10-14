#!/usr/bin/env python3
"""
Convert a WAV file into a note table for the piezo buzzer music player
(sample/piezo). The table format matches melody_note.h: an array of
{freq_hz, ms} entries, with freq_hz == 0 marking a rest.

Method: detect zero crossings; between sign changes count complete
half-waves, measure their durations and take the median, so one noisy
half-wave cannot corrupt the pitch. Notes are merged across frames of
analysis (10 ms hop, 20 ms window); a run of frames with the same
quantised pitch becomes one entry. Silence -> rest.

Output: C source on stdout, stats on stderr.

Usage:
    python3 wav_conv.py <in.wav> <out.c> [max_notes]
"""

import sys
import wave
import struct
import math


def read_wav_mono(path):
    w = wave.open(path, "rb")
    nch = w.getnchannels()
    width = w.getsampwidth()
    rate = w.getframerate()
    frames = w.readframes(w.getnframes())
    w.close()

    if width == 2:
        fmt = "h"
    elif width == 1:
        fmt = "B"
    else:
        raise SystemExit("unsupported sample width %d" % width)
    size = struct.calcsize("<" + fmt)
    count = len(frames) // size
    data = struct.unpack("<%d%s" % (count, fmt), frames[0:count * size])

    if nch == 2:
        mono = [(data[i] + data[i + 1]) for i in range(0, count, 2)]
    else:
        mono = list(data)
    return mono, rate


def median(values):
    values = sorted(values)
    n = len(values)
    if n == 0:
        return 0
    return values[n // 2]


def detect_notes(samples, rate, hop_ms=10, win_ms=20):
    """Zero-crossing pitch detection with median smoothing.

    Each frame builds the list of half-wave durations from the zero
    crossings inside the window, drops the (partial) first and last
    one, and takes the *median* duration; so one noisy crossing cannot
    corrupt the pitch. A frame whose window is silent (peak below the
    gate) is marked as a rest.
    """
    hop = max(1, (hop_ms * rate) // 1000)
    win = max(hop, (win_ms * rate) // 1000)
    frames = []
    pos = 0
    while pos + win + 1 < len(samples):
        # peak amplitude in the window, for the silence gate
        peak = 0
        for i in range(pos, pos + win):
            a = samples[i] if samples[i] >= 0 else -samples[i]
            if a > peak:
                peak = a

        half = []
        if peak >= 600:
            # indices of zero crossings (sample goes from <0 to >=0 or back)
            crossings = []
            prev = samples[pos] >= 0
            for i in range(pos + 1, pos + win):
                cur = samples[i] >= 0
                if cur != prev:
                    crossings.append(i)
                    prev = cur
            # durations between consecutive crossings = half waves
            edges = [(pos) if samples[pos] >= 0 else pos]
            edges = [pos] + crossings
            for j in range(1, len(edges)):
                half.append(edges[j] - edges[j - 1])
            # drop partial half-waves at the window edges
            if len(half) >= 3:
                half = half[1:-1]
            if half:
                med = median(half)
                if med > 0:
                    frames.append(rate * 0.5 / med)
                    pos += hop
                    continue
        frames.append(0.0)
        pos += hop

    return frames


def quantise(f):
    if f <= 80.0:
        return 0  # silence / rest
    # snap to semitones: f * 2^(n/12)
    n = round(12.0 * math.log(f / 440.0) / math.log(2.0))
    return int(round(440.0 * (2.0 ** (n / 12.0))))


def run(in_path, out_path, max_notes=2048):
    samples, rate = read_wav_mono(in_path)
    frames = detect_notes(samples, rate)

    notes = []
    cur_q = None
    cur_len = 0
    total_ms = 0
    for f in frames:
        q = quantise(f) if f > 0 else 0
        if cur_q is None:
            cur_q = q
            cur_len = 1
        elif q == cur_q:
            cur_len += 1
        else:
            ms = cur_len * 10
            if ms >= 40:
                notes.append((cur_q, ms))
            cur_q = q
            cur_len = 1
    if cur_q is not None:
        ms = cur_len * 10
        if ms >= 40:
            notes.append((cur_q, ms))

    # Cleanup for a buzzer playback:
    # 1. drop everything above the audible piezo band (it will not sound
    #    anyway and only adds stutter) and everything below the useful range;
    # 2. merge consecutive entries of the same pitch;
    # 3. quantise durations to 50 ms so the timing noise of the detector
    #    does not show up as random note lengths;
    # 4. drop notes shorter than MIN_NOTE_LEN after quantisation.
    min_q, max_q = 150, 7000
    q_ms = 50
    min_note_len = 50
    cleaned = []
    for q, ms in notes:
        if q != 0 and not (min_q <= q <= max_q):
            q = 0                       # out of band -> treat as rest
        cleaned.append((q, ms))

    merged = []
    for q, ms in cleaned:
        ms = int(round(ms / float(q_ms))) * q_ms
        if merged and merged[-1][0] == q:
            merged[-1] = (q, merged[-1][1] + ms)
        else:
            merged.append((q, ms))
    notes = [n for n in merged if n[1] >= min_note_len]

    # cap length
    notes = notes[:max_notes]

    with open(out_path, "w") as fp:
        name = out_path.replace("\\", "/").split("/")[-1]
        fp.write("/**\n")
        fp.write("  * @file    %s\n" % name)
        fp.write("  * @brief   Note table generated from platform.wav by wav_conv.py.\n")
        fp.write("  */\n\n")
        fp.write('#include "platform_notes.h"\n\n')
        fp.write("/* Generated from platform.wav (%d samples @ %d Hz, %.2f s)\n"
                 % (len(samples), rate, len(samples) / float(rate)))
        fp.write("   pitch detection: zero-crossing median, 10 ms hop,\n")
        fp.write("   band limited to %d..%d Hz for piezo playback */\n\n"
                 % (min_q, max_q))
        fp.write("const MUSIC_Note s_melody[] =\n{\n")
        for q, ms in notes:
            if q == 0:
                fp.write("  { MUSIC_REST, %dU },\n" % ms)
            else:
                fp.write("  { %dU, %dU },\n" % (q, ms))
        fp.write("};\n\n")
        fp.write("const uint32_t s_melody_length = "
                 "sizeof(s_melody) / sizeof(s_melody[0]);\n")

    total_ms = sum(m for _, m in notes)
    sys.stderr.write("frames=%d notes=%d total=%.2fs\n"
                     % (len(frames), len(notes), total_ms / 1000.0))
    if len(notes) <= 2:
        qs = {}
        for f in frames:
            q = quantise(f) if f > 0 else 0
            qs[q] = qs.get(q, 0) + 1
        top = sorted(qs.items(), key=lambda kv: -kv[1])[:12]
        sys.stderr.write("top quantised pitch buckets: %r\n" % (top,))
        nz = [f for f in frames if f > 0]
        if nz:
            sys.stderr.write("nonzero frames=%d first10=%r\n"
                             % (len(nz), [round(x, 1) for x in nz[:10]]))
        else:
            sys.stderr.write("all frames are silent (amplitude gate failed?)\n")


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(__doc__, file=sys.stderr)
        sys.exit(2)
    run(sys.argv[1], sys.argv[2],
        int(sys.argv[3]) if len(sys.argv) > 3 else 2048)