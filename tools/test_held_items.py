"""
Checks resources/held_items.json: the two hand offsets and each item's offsets are transforms with a position, three rotations and a
scale, every item names a hand ("R" or "L") or a bone, and the hands and items are the ones the game shows. Run: python -I tools/test_held_items.py
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PATH = os.path.join(HERE, "..", "resources", "held_items.json")
failures = []


def check(name, ok):
    print("PASS" if ok else "FAIL", name)
    if not ok:
        failures.append(name)


def is_transform(t):
    return (isinstance(t, dict) and len(t.get("position", [])) == 3 and len(t.get("rotation", [])) == 3
            and isinstance(t.get("scale"), (int, float)) and t["scale"] > 0)


with open(PATH, encoding="utf-8") as f:
    data = json.load(f)

check("the right hand has a held offset transform", is_transform(data.get("Held_Offset_R_Transform")))
check("the left hand has a held offset transform", is_transform(data.get("Held_Offset_L_Transform")))
items = data.get("items", {})
check("the pickaxe is held in the right hand", items.get("pickaxe", {}).get("hand") == "R")
check("the iron pickaxe is held in the right hand", items.get("iron pickaxe", {}).get("hand") == "R")
check("the sword is held in the right hand with its own offset",
      items.get("sword", {}).get("hand") == "R" and is_transform(items["sword"].get("Held_Item_Offset_Transform")))
check("the shield is held in the left hand, carried and raised",
      items.get("shield", {}).get("hand") == "L" and is_transform(items["shield"].get("Held_Item_Offset_Transform"))
      and is_transform(items["shield"].get("Raised_Held_Item_Offset_Transform")))
check("every item names a hand or a bone", all(entry.get("hand") in ("R", "L") or entry.get("bone") for entry in items.values()))
check("every item's own transforms are well formed", all(
    is_transform(entry[key]) for entry in items.values() for key in entry
    if key.endswith("Transform")))

if failures:
    print("%d check(s) failed" % len(failures))
    sys.exit(1)
print("All held item checks passed.")
