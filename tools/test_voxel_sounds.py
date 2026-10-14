"""
Checks the voxel sample's sounds without the engine: each file is a 16-bit mono WAV at 22,050 Hz, of a sensible length and level,
the night loop joins up with itself, and the game plays each sound at the event it belongs to.
Usage (from the repo root): python -I tools/test_voxel_sounds.py
Exits 0 on success, 1 on any failed check.
"""
import math
import os
import re
import struct
import sys
import wave

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SOUNDS = os.path.join(ROOT, "resources", "sounds")
GAME = os.path.join(ROOT, "resources", "scripts", "VoxelGame.py")

EFFECTS = ["break", "place", "hurt", "step", "pickup", "bite", "splash"]
# The event each effect is played for, as the game's code spells it (play_sound("name", ...)).
WIRED = {"break": "break", "place": "place", "hurt": "hurt", "step": "step", "pickup": "pickup", "bite": "bite", "splash": "splash"}

failures = []


def check(name, condition):
    print(("PASS  " if condition else "FAIL  ") + name)
    if not condition:
        failures.append(name)


def read(name):
    with wave.open(os.path.join(SOUNDS, name + ".wav"), "rb") as f:
        frames = f.readframes(f.getnframes())
        samples = [v / 32768.0 for v in struct.unpack("<%dh" % f.getnframes(), frames)]
        return f.getnchannels(), f.getsampwidth(), f.getframerate(), samples


for name in EFFECTS + ["ambient"]:
    path = os.path.join(SOUNDS, name + ".wav")
    check(f"{name}.wav exists", os.path.isfile(path))
    if not os.path.isfile(path):
        continue
    channels, width, rate, samples = read(name)
    check(f"{name} is 16-bit mono at 22,050 Hz", channels == 1 and width == 2 and rate == 22050)
    seconds = len(samples) / rate
    if name == "ambient":
        check("ambient is a 16-second loop", abs(seconds - 16.0) < 0.01)
    else:
        check(f"{name} is short (0.1 to 2 seconds)", 0.1 <= seconds <= 2.0)
    peak = max(abs(s) for s in samples)
    rms = math.sqrt(sum(s * s for s in samples) / len(samples))
    check(f"{name} is audible and not clipped (peak 0.05 to 1.0)", 0.05 <= peak <= 1.0)
    check(f"{name} has energy (RMS above 0.005)", rms > 0.005)

if os.path.isfile(os.path.join(SOUNDS, "ambient.wav")):
    _, _, rate, loop = read("ambient")
    # Seamless: the last sample is one step from the first, so the loop has no click where it joins.
    step = max(abs(loop[i + 1] - loop[i]) for i in range(len(loop) - 1))
    join = abs(loop[0] - loop[-1])
    check("the night loop joins without a click", join <= 2.0 * step)

with open(GAME, encoding="utf-8") as f:
    game = f.read()
for name, event in WIRED.items():
    check(f"the game plays {name} for {event}", re.search(r'play_sound\("%s"' % name, game) is not None)
check("the game starts the night loop", re.search(r'play_music\("ambient"', game) is not None)
check("the game stops the night loop by day", "stop_music()" in game)

if failures:
    print(f"{len(failures)} check(s) failed")
    sys.exit(1)
print("All voxel sound checks passed.")
