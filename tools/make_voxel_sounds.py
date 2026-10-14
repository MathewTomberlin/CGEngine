"""
Writes the night loop of the voxel sample (resources/sounds/ambient.wav). Made from sine waves, so the file is the same every
time this runs. Mono, 16-bit, 22,050 Hz.

The effects (break, place, hurt, step, pickup, bite, splash) are generated with the comfyui-audio skill, not here: a music or
effect model does not give a seamless loop, which is why the loop stays procedural.
The loop is seamless: each tone runs a whole number of cycles over its length, and the LFO too, so the end meets the start.
Usage (from the repo root): python -I tools/make_voxel_sounds.py
"""
import math
import os
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


def main():
    path = write("ambient", make_ambient())
    print("wrote", os.path.relpath(path, ROOT), os.path.getsize(path), "bytes")


if __name__ == "__main__":
    main()
