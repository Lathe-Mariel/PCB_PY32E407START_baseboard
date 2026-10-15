#!/usr/bin/env python3
"""
Convert a WAV file into an unsigned 8 bit PCM array for the piezo buzzer
player (sample/piezo). The array is played back on the PWM duty cycle
(TIM1_CH2/CH2N on PA9/PA12), optionally through a fixed 6-band equaliser
that is applied here, offline, so the MCU needs no filter state.

Offline processing (all optional, enabled by flags):
  --eq            6-band IIR equaliser, gains in EQ_DB below (peaking
                  bands + high/low shelf), then peak-normalise to -1 dB.
                  This is where the speech intelligibility work happens:
                  raise the 300-4000 Hz voice band, cut the low rumble
                  and the shrill carrier hiss.
  --agc           dynamic range compression (simple soft-knee compressor)
                  then peak-normalise. Makes quiet syllables audible.

Usage:
    python3 wav2pcm.py <in.wav> <out.c> [--eq] [--agc] [--rate 16000]

Output format:  const unsigned char PLATFORM_PCM[] = { ... };
                const unsigned int  PLATFORM_PCM_LENGTH = sizeof(..);
                #define PLATFORM_PCM_RATE_HZ  <rate>
"""

import sys
import wave
import struct
import math
import array

# ---------------------------------------------------------------------------
# Equaliser definition: 6 biquad bands. This is the "frequency equaliser"
# requested for the project - edit the gains here and re-run the script.
#
# band: (type, freq, Q/steepness, gain_db)
#   lowshelf  : boosts/cuts everything below freq
#   peaking   : boosts/cuts a band around freq with width set by Q
#   highshelf : boosts/cuts everything above freq
#
# Tuning goal: speech intelligibility on a piezo buzzer. The raw speech band
# is dominated by two things:
#   - vowel energy 250-1000 Hz (the "body" of the voice)
#   - consonant energy 2-4 kHz   (what actually carries intelligibility)
# Low rumble < 200 Hz only wastes PWM span, hiss > 5-6 kHz sounds harsh.
# ----------------------------------------------------------------------------
EQ_DB = [
    ("lowshelf",  200.0, 0.7, -6.0),   # cut rumble below 200 Hz
    ("peaking",   300.0, 1.0, +3.0),   # vowel body
    ("peaking",   900.0, 1.0, +2.0),   # vowel body upper
    ("peaking",  2500.0, 1.0, +5.0),   # consonants (s, t, k, sh)
    ("peaking",  3500.0, 1.0, +3.0),   # presence / clarity
    ("highshelf", 6000.0, 0.7, -6.0),  # cut harsh hiss above 6 kHz
]


# ---------------------------------------------------------------------------
def read_wav(path):
    w = wave.open(path, "rb")
    nch = w.getnchannels()
    width = w.getsampwidth()
    rate = w.getframerate()
    frames = w.readframes(w.getnframes())
    w.close()

    if width == 2:
        fmt = "<%dh" % (len(frames) // 2)
        data = struct.unpack(fmt, frames)
    elif width == 1:
        fmt = "<%dB" % len(frames)
        data = struct.unpack(fmt, frames)
        data = [x - 128 for x in data]
    else:
        raise SystemExit("unsupported sample width %d" % width)

    if nch == 2:
        mono = [(data[i] + data[i + 1]) * 0.5 for i in range(0, len(data), 2)]
    else:
        mono = list(data)
    return mono, rate


def write_wav(path, samples, rate):
    """Write a 16 bit mono WAV (for the optional preview)."""
    w = wave.open(path, "wb")
    w.setnchannels(1)
    w.setsampwidth(2)
    w.setframerate(rate)
    frames = struct.pack("<%dh" % len(samples), *samples)
    w.writeframes(frames)
    w.close()


# ---------------------------------------------------------------------------
def resample_linear(samples, srate, drate):
    if srate == drate:
        return list(samples)
    ratio = srate / float(drate)
    out_len = int(len(samples) / ratio)
    out = []
    for i in range(out_len):
        x = i * ratio
        i0 = int(x)
        if i0 >= len(samples) - 1:
            out.append(samples[-1])
            continue
        frac = x - i0
        s = samples[i0] * (1.0 - frac) + samples[i0 + 1] * frac
        out.append(s)
    return out


def moving_average(x, n):
    """Integer-friendly low pass used before 8 bit quantisation."""
    if n <= 1:
        return list(x)
    acc = 0.0
    buf = [0.0] * n
    out = []
    for i, v in enumerate(x):
        acc += v - buf[i % n]
        buf[i % n] = v
        out.append(acc / n)
    return out


# ---------------------------------------------------------------------------
# RBJ audio EQ cookbook biquads
# ---------------------------------------------------------------------------
def biquad_lowshelf(f0, srate, q, gain_db):
    A = 10.0 ** (gain_db / 40.0)
    w0 = 2.0 * math.pi * f0 / srate
    alpha = math.sin(w0) / 2.0 * math.sqrt((A + 1.0 / A) * (1.0 / q - 1.0) + 2.0)
    cosw = math.cos(w0)
    sqA = 2.0 * math.sqrt(A) * alpha

    b0 = A * ((A + 1) - (A - 1) * cosw + sqA)
    b1 = 2 * A * ((A - 1) - (A + 1) * cosw)
    b2 = A * ((A + 1) - (A - 1) * cosw - sqA)
    a0 = (A + 1) + (A - 1) * cosw + sqA
    a1 = -2 * ((A - 1) + (A + 1) * cosw)
    a2 = (A + 1) + (A - 1) * cosw - sqA
    return [b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0]


def biquad_highshelf(f0, srate, q, gain_db):
    A = 10.0 ** (gain_db / 40.0)
    w0 = 2.0 * math.pi * f0 / srate
    alpha = math.sin(w0) / 2.0 * math.sqrt((A + 1.0 / A) * (1.0 / q - 1.0) + 2.0)
    cosw = math.cos(w0)
    sqA = 2.0 * math.sqrt(A) * alpha

    b0 = A * ((A + 1) + (A - 1) * cosw + sqA)
    b1 = -2 * A * ((A - 1) + (A + 1) * cosw)
    b2 = A * ((A + 1) + (A - 1) * cosw - sqA)
    a0 = (A + 1) - (A - 1) * cosw + sqA
    a1 = 2 * ((A - 1) - (A + 1) * cosw)
    a2 = (A + 1) - (A - 1) * cosw - sqA
    return [b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0]


def biquad_peaking(f0, srate, q, gain_db):
    A = 10.0 ** (gain_db / 40.0)
    w0 = 2.0 * math.pi * f0 / srate
    alpha = math.sin(w0) / (2.0 * q)
    cosw = math.cos(w0)

    b0 = 1 + alpha * A
    b1 = -2 * cosw
    b2 = 1 - alpha * A
    a0 = 1 + alpha / A
    a1 = -2 * cosw
    a2 = 1 - alpha / A
    return [b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0]


def apply_biquad(x, coeffs):
    b0, b1, b2, a1, a2 = coeffs
    x1 = x2 = y1 = y2 = 0.0
    y = []
    for v in x:
        outn = b0 * v + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2
        x2, x1 = x1, v
        y2, y1 = y1, outn
        y.append(outn)
    return y


def equalise(x, srate):
    for kind, f0, q, gdb in EQ_DB:
        if kind == "lowshelf":
            c = biquad_lowshelf(f0, srate, q, gdb)
        elif kind == "highshelf":
            c = biquad_highshelf(f0, srate, q, gdb)
        else:
            c = biquad_peaking(f0, srate, q, gdb)
        x = apply_biquad(x, c)
    return x


def compress(x, srate, threshold=0.25, ratio=4.0, attack=0.01, release=0.1):
    """Simple soft-knee feed-forward compressor, envelope follower per sample."""
    atk = math.exp(-1.0 / (attack * srate))
    rel = math.exp(-1.0 / (release * srate))
    env = 0.0
    out = []
    for v in x:
        a = abs(v)
        if a > env:
            env = atk * env + (1.0 - atk) * a
        else:
            env = rel * env + (1.0 - rel) * a
        if env > threshold:
            g = (threshold / env) ** (1.0 / ratio)
        else:
            g = 1.0
        out.append(v * g)
    return out


def normalise(x, target=0.94):
    peak = 0.0
    for v in x:
        a = abs(v)
        if a > peak:
            peak = a
    if peak == 0:
        return x
    k = target / peak
    return [v * k for v in x]


# ---------------------------------------------------------------------------
def main():
    args = sys.argv[1:]
    if len(args) < 2:
        print(__doc__, file=sys.stderr)
        sys.exit(2)

    in_path, out_path = args[0], args[1]
    do_eq = "--eq" in args
    do_agc = "--agc" in args

    rate = 16000
    if "--rate" in args:
        rate = int(args[args.index("--rate") + 1])

    mono, srate = read_wav(in_path)

    data = resample_linear(mono, srate, rate)

    if do_agc:
        data = compress(data, rate)
        data = normalise(data)
    if do_eq:
        data = equalise(data, rate)
        data = normalise(data)
        data = compress(data, rate, threshold=0.5, ratio=3.0)  # guard the peaks
        data = normalise(data)

    # DC removal + gentle low pass before quantisation
    # (removes the offset so silence sits at 128)
    n = 64
    dc = sum(data[: n]) / n
    data = [v - dc for v in data]
    data = moving_average(data, 3)

    pcm = []
    for v in data:
        # v is a -1..1 fraction (post-normalise); scale it to the full
        # unsigned 8 bit range FIRST, then round to an integer count.
        # (Rounding before scaling would collapse the whole signal into
        # 127..129 and produce silence.)
        s = int(round(v * 127)) + 128
        if s < 0:
            s = 0
        elif s > 255:
            s = 255
        pcm.append(s)

    name_base = out_path.replace("\\", "/").split("/")[-1].split(".")[0].upper()
    sym_samples = name_base + "_SAMPLES"
    sym_length = name_base + "_LENGTH"
    sym_rate = name_base + "_RATE_HZ"

    with open(out_path, "w") as fp:
        fp.write("/**\n")
        fp.write("  * @file    %s\n"
                 % out_path.replace("\\", "/").split("/")[-1])
        fp.write("  * @brief   %s as unsigned 8 bit PCM, %d Hz mono.\n"
                 % (in_path.replace("\\", "/").split("/")[-1], rate))
        fp.write("  *          Generated by tools/wav2pcm.py%s%s\n"
                 % (" --eq" if do_eq else "", " --agc" if do_agc else ""))
        fp.write("  */\n\n")
        fp.write('#include "%s.h"\n\n' % name_base.lower())
        # Unsized array + sizeof-derived length constant: the count and the
        # declared size can never disagree, no matter what the generator
        # emits (a sized array would warn thousands of "excess elements" if
        # the macro were ever out of sync).
        fp.write("const unsigned char %s[] =\n{\n" % sym_samples)
        for i in range(0, len(pcm), 16):
            fp.write("  " + ", ".join("%3d" % s for s in pcm[i:i + 16]) + ",\n")
        fp.write("};\n\n")
        fp.write("const unsigned int %s = sizeof(%s) / sizeof(%s[0]);\n"
                 % (sym_length, sym_samples, sym_samples))

    sys.stderr.write("input: %d Hz, %d samples (%.2f s)\n"
                     % (srate, len(mono), len(mono) / float(srate)))
    sys.stderr.write("output: %d Hz, %d samples (%.2f s), %d bytes\n"
                     % (rate, len(pcm), len(pcm) / float(rate), len(pcm)))


if __name__ == "__main__":
    main()