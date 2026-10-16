"""
Writes the night loop of the voxel sample (resources/sounds/ambient.wav). Made from sine waves, so the file is the same every
time this runs. Mono, 16-bit, 22,050 Hz.

The effects (break, place, hurt, step, pickup, bite, splash) are generated with the comfyui-audio skill, not here: a music or
effect model does not give a seamless loop, which is why the loop stays procedural. The footsteps of the ground blocks (step_grass,
step_dirt, step_stone, step_sand, step_gravel, step_snow) are made here from noise, so each one is the same file every time.
The loop is seamless: each tone runs a whole number of cycles over its length, and the LFO too, so the end meets the start.
Usage (from the repo root): python -I tools/make_voxel_sounds.py
"""
import math
import os
import random
import struct
import wave

RATE = 22050
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "resources", "sounds")


def write(name, samples):
    """Writes the samples (floats in -1 to 1) as a 16-bit mono WAV, scaled so the loudest sample is 0.8."""
    peak = max(1e-9, max(abs(s) for s in samples))
    scale = 0.8 / peak
    os.makedirs(OUT, exist_ok=True)
    path = os.path.join(OUT, name + ".wav")
    with wave.open(path, "wb") as f:
        f.setnchannels(1)
        f.setsampwidth(2)
        f.setframerate(RATE)
        f.writeframes(b"".join(struct.pack("<h", int(max(-1.0, min(1.0, s * scale)) * 32767)) for s in samples))
    return path


def seconds(duration):
    return int(duration * RATE)


def envelope(i, n, attack, decay):
    """A short attack, then an exponential decay over the sound's length."""
    t = i / RATE
    a = min(1.0, t / attack) if attack > 0 else 1.0
    return a * math.exp(-decay * t)


def tone(freq, i, phase=0.0):
    return math.sin(2.0 * math.pi * freq * i / RATE + phase)


def sweep(f0, f1, i, n):
    """A sine whose frequency glides from f0 to f1 over n samples."""
    t = i / RATE
    total = n / RATE
    return math.sin(2.0 * math.pi * (f0 * t + (f1 - f0) * t * t / (2.0 * total)))


def lowpass(samples, alpha):
    out, y = [], 0.0
    for s in samples:
        y += alpha * (s - y)
        out.append(y)
    return out


def make_ambient():
    """Sixteen seconds of night: a low drone of three tones, swelling slowly. Every frequency and the swell are whole cycles
    over the loop, so it repeats without a click."""
    seconds_long = 16
    n = seconds_long * RATE
    out = []
    for i in range(n):
        t = i / RATE
        swell = 0.6 + 0.4 * math.sin(2.0 * math.pi * (1.0 / 8.0) * t)   # one swell every 8 seconds: two in the loop
        drone = 0.5 * tone(55.0, i) + 0.3 * tone(82.5, i) + 0.2 * tone(110.0, i + 0) * (0.5 + 0.5 * math.sin(2.0 * math.pi * 0.125 * t))
        out.append(0.35 * swell * drone)
    return out


def smooth(samples, width):
    """A moving average: the high frequencies go, so the noise sounds softer."""
    out = []
    acc = 0.0
    for i, s in enumerate(samples):
        acc += s
        if i >= width:
            acc -= samples[i - width]
        out.append(acc / min(i + 1, width))
    return out


def highpass(samples):
    """The difference of neighbours: the low frequencies go, so the noise is harder and grainier."""
    return [samples[0]] + [samples[i] - samples[i - 1] for i in range(1, len(samples))]


def steps(kind):
    """A footstep on one ground: noise shaped for its material, the same samples each time (its own seed). The envelopes are
    times in seconds, so the step is about the same length at any rate."""
    seed = {"grass": 31, "dirt": 32, "stone": 33, "sand": 34, "gravel": 35, "snow": 36}[kind]
    rng = random.Random(seed)
    n = int(0.4 * RATE)
    noise = [rng.uniform(-1.0, 1.0) for _ in range(n)]
    out = [0.0] * n
    if kind == "stone":
        # A sharp click with a ring: fast decay, and a bright tone that dies sooner.
        for i in range(n):
            t = i / RATE
            out[i] = 0.7 * highpass(noise)[i] * math.exp(-t * 30.0) + 0.45 * math.sin(2.0 * math.pi * 1400.0 * t) * math.exp(-t * 45.0)
    elif kind == "dirt":
        # A low thump on soft ground, with a little grit on top.
        grit = highpass(noise)
        for i in range(n):
            t = i / RATE
            out[i] = 0.9 * math.sin(2.0 * math.pi * 70.0 * t) * math.exp(-t * 14.0) + 0.25 * grit[i] * math.exp(-t * 25.0)
    elif kind == "grass":
        # A soft rustle: low-passed noise with a short attack.
        soft = smooth(noise, 6)
        for i in range(n):
            t = i / RATE
            out[i] = 0.6 * soft[i] * min(1.0, t / 0.03) * math.exp(-t * 12.0)
    elif kind == "sand":
        # Grains: high-passed noise that swells and fades, with a crumble of clicks.
        grains = highpass(noise)
        for i in range(n):
            t = i / RATE
            out[i] = 0.5 * grains[i] * min(1.0, t / 0.02) * math.exp(-t * 8.0)
    elif kind == "gravel":
        # Crunch: short bursts of noise scattered through the step.
        for _ in range(28):
            start = rng.randrange(0, int(0.25 * RATE))
            amp = rng.uniform(0.3, 1.0)
            for j in range(int(0.006 * RATE)):
                if start + j < n:
                    out[start + j] += amp * noise[start + j] * math.exp(-j / (0.002 * RATE))
    else:   # snow: muffled and slow, a soft press that fades
        soft = smooth(noise, 16)
        for i in range(n):
            t = i / RATE
            out[i] = 0.8 * soft[i] * min(1.0, t / 0.08) * math.exp(-t * 6.0)
    return out


GROUND_STEPS = ("grass", "dirt", "stone", "sand", "gravel", "snow")


def lava():
    """A hiss with bubbles: filtered noise that swells and fades, with a few short low blips breaking through it."""
    rng = random.Random(41)
    n = int(0.9 * RATE)
    noise = smooth(highpass(highpass([rng.uniform(-1.0, 1.0) for _ in range(n)])), 3)
    out = [0.0] * n
    for i in range(n):
        t = i / RATE
        out[i] = 0.5 * noise[i] * math.sin(math.pi * min(1.0, t / 0.9)) ** 0.5
    for _ in range(4):
        start = rng.randrange(0, n - int(0.08 * RATE))
        freq = rng.uniform(180.0, 320.0)
        for j in range(int(0.08 * RATE)):
            t = j / RATE
            out[start + j] += 0.6 * math.sin(2.0 * math.pi * freq * (1.0 - 1.5 * t) * t) * math.exp(-t * 35.0)
    return out


def main():
    path = write("ambient", make_ambient())
    print("wrote", os.path.relpath(path, ROOT), os.path.getsize(path), "bytes")
    for kind in GROUND_STEPS:
        path = write("step_" + kind, steps(kind))
        print("wrote", os.path.relpath(path, ROOT), os.path.getsize(path), "bytes")
    path = write("lava", lava())
    print("wrote", os.path.relpath(path, ROOT), os.path.getsize(path), "bytes")


if __name__ == "__main__":
    main()
