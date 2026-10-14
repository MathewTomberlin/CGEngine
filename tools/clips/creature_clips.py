"""
Writes the creature's clips (roadmap 33.1) to resources/clips/: creature_walk and creature_idle (looping), creature_bite, creature_hurt
and creature_die (once). They key the bones of resources/models/voxel/creature.fbx (tools/blender/creature_model.py): Root, Spine,
Head, Tail and the four legs. Rotations are euler angles in radians, relative to the rest pose, as the knight's clips are.

    python -I tools/clips/creature_clips.py

The walk is a four-beat gait: the legs swing in diagonal pairs (front right with back left, front left with back right), so one
pair is always planted. The walker plays creature_walk while it moves and creature_idle while it stands.
"""
import json
import math
import os

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT = os.path.join(ROOT, "resources", "clips")
MODEL = "models/voxel/creature.fbx"
LEGS = ("LegFrontR", "LegBackL", "LegFrontL", "LegBackR")
# The pair that swings together: the front right with the back left, and the front left with the back right.
PHASE = {"LegFrontR": 0.0, "LegBackL": 0.0, "LegFrontL": math.pi, "LegBackR": math.pi}


def rot(bone, keys):
    return {"bone": bone, "rotation": [{"t": t, "euler": list(e)} for t, e in keys]}


def clip(name, duration, channels):
    return {"version": 1, "name": name, "model": MODEL, "durationSeconds": duration, "channels": channels}


def sampled(duration, samples, value):
    """Keys sampled from value(t) at samples evenly spaced times across the clip, the last one at the duration (looping clips
    repeat their first key at the end, so they meet without a jump)."""
    return [(duration * k / samples, value(duration * k / samples)) for k in range(samples + 1)]


def walk():
    duration, samples = 0.8, 8
    channels = []
    for leg in LEGS:
        def swing(t, leg=leg):
            angle = 0.45 * math.sin(2.0 * math.pi * t / duration + PHASE[leg])
            return (angle, 0.0, 0.0)
        channels.append(rot(leg, sampled(duration, samples, swing)))
    channels.append(rot("Spine", sampled(duration, samples, lambda t: (0.03 * math.sin(4.0 * math.pi * t / duration), 0.0, 0.0))))
    channels.append(rot("Head", sampled(duration, samples, lambda t: (0.05 * math.sin(4.0 * math.pi * t / duration), 0.0, 0.0))))
    channels.append(rot("Tail", sampled(duration, samples, lambda t: (0.0, 0.0, 0.25 * math.sin(2.0 * math.pi * t / duration)))))
    return clip("creature_walk", duration, channels)


def idle():
    duration, samples = 3.0, 6
    breathe = lambda t: (0.02 * math.sin(2.0 * math.pi * t / duration), 0.0, 0.0)
    graze = lambda t: (0.12 * math.sin(math.pi * min(1.0, t / duration) * 2.0) ** 2 * 0.5, 0.0, 0.0)
    return clip("creature_idle", duration, [
        rot("Spine", sampled(duration, samples, breathe)),
        rot("Head", sampled(duration, samples, graze)),
        rot("Tail", sampled(duration, 4, lambda t: (0.0, 0.0, 0.1 * math.sin(2.0 * math.pi * t / duration)))),
    ])


def bite():
    duration = 0.5
    # The head drops and lunges forward, then the jaw comes back: a bite is a quick lunge and a recover.
    head = [(0.0, (0.0, 0.0, 0.0)), (0.12, (0.45, 0.0, 0.0)), (0.22, (0.5, 0.0, 0.0)), (0.5, (0.0, 0.0, 0.0))]
    spine = [(0.0, (0.0, 0.0, 0.0)), (0.12, (0.15, 0.0, 0.0)), (0.5, (0.0, 0.0, 0.0))]
    return clip("creature_bite", duration, [rot("Head", head), rot("Spine", spine)])


def hurt():
    duration = 0.4
    flinch = [(0.0, (0.0, 0.0, 0.0)), (0.06, (-0.25, 0.0, 0.2)), (0.25, (0.05, 0.0, -0.05)), (0.4, (0.0, 0.0, 0.0))]
    spine = [(0.0, (0.0, 0.0, 0.0)), (0.06, (-0.12, 0.0, 0.0)), (0.4, (0.0, 0.0, 0.0))]
    return clip("creature_hurt", duration, [rot("Head", flinch), rot("Spine", spine)])


def die():
    duration = 1.2
    # The whole body rolls onto its side and the legs go slack: the spine tips over, the legs fold under.
    spine = [(0.0, (0.0, 0.0, 0.0)), (0.5, (0.0, 0.0, 0.0)), (1.2, (0.0, 0.0, 1.35))]
    head = [(0.0, (0.0, 0.0, 0.0)), (0.6, (0.3, 0.0, 0.0)), (1.2, (0.5, 0.0, 0.2))]
    folds = [(0.0, (0.0, 0.0, 0.0)), (0.7, (0.6, 0.0, 0.0)), (1.2, (0.9, 0.0, 0.0))]
    channels = [rot("Spine", spine), rot("Head", head)] + [rot(leg, folds) for leg in LEGS]
    return clip("creature_die", duration, channels)


def main():
    os.makedirs(OUT, exist_ok=True)
    for clip_data in (walk(), idle(), bite(), hurt(), die()):
        path = os.path.join(OUT, clip_data["name"] + ".json")
        with open(path, "w", encoding="utf-8") as f:
            json.dump(clip_data, f, indent=2)
        print("wrote", os.path.relpath(path, ROOT))


if __name__ == "__main__":
    main()
