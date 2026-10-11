"""
Writes a game's state file (cg_control/*_state.json) for tests and agents to read while the game runs.

The write is atomic: the JSON goes to a temporary file, which then replaces the state file. On Windows that replace
fails while a reader has the state file open, so it is retried for a moment. If the reader still holds the file, this
write is skipped; the next write, a few frames later, replaces it. A reader race must never stop the game.
"""
import json
import os
import time


def write_json(path, data, attempts=20):
    """Write data as JSON to path. Returns False when the file stayed locked and the write was skipped."""
    folder = os.path.dirname(path)
    if folder:
        os.makedirs(folder, exist_ok=True)
    tmp = path + ".tmp"
    with open(tmp, "w") as f:
        json.dump(data, f)
    for _ in range(attempts):
        try:
            os.replace(tmp, path)
            return True
        except PermissionError:
            time.sleep(0.005)
    return False
