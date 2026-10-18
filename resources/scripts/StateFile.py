"""
Writes a game's state file (cg_control/*_state.json) for tests and agents to read while the game runs.

The write is atomic: the JSON goes to a temporary file, which then replaces the state file. On Windows that replace
fails while a reader has the state file open, so it is retried for a moment. If the reader still holds the file, this
write is skipped; the next write, a few frames later, replaces it. A reader race must never stop the game.
"""
import json
import os
import threading
import time


def write_json(path, data, attempts=20):
    """Write data as JSON to path. Returns False when the file stayed locked and the write was skipped."""
    folder = os.path.dirname(path)
    if folder:
        os.makedirs(folder, exist_ok=True)
    tmp = path + ".tmp"
    for _ in range(attempts):
        # The temporary file can be taken away by another writer (two games in one folder), so it is written again on each try.
        try:
            with open(tmp, "w") as f:
                json.dump(data, f)
            os.replace(tmp, path)
            return True
        except (PermissionError, FileNotFoundError):
            time.sleep(0.005)
    return False


# The writer thread for write_json_async: one pending write per path, the newest, so a slow disk never queues up states.
_pending = {}
_pending_lock = threading.Lock()
_wake = threading.Event()
_writer = None


def _write_pending():
    while True:
        _wake.wait()
        _wake.clear()
        while True:
            with _pending_lock:
                if not _pending:
                    break
                path, text = _pending.popitem()
            _write_text(path, text)


def _write_text(path, text, attempts=20):
    folder = os.path.dirname(path)
    if folder:
        os.makedirs(folder, exist_ok=True)
    tmp = path + ".tmp"
    for _ in range(attempts):
        try:
            with open(tmp, "w") as f:
                f.write(text)
            os.replace(tmp, path)
            return True
        except (PermissionError, FileNotFoundError):
            time.sleep(0.005)
    return False


def write_json_async(path, data):
    """Like write_json, but the file is written on a background thread: the JSON is made here, the disk work is not. A state not yet
    written is replaced by the next one (the newest wins), so a slow disk never delays the game's frame."""
    global _writer
    text = json.dumps(data)
    with _pending_lock:
        _pending[path] = text
        if _writer is None:
            _writer = threading.Thread(target=_write_pending, name="state-file-writer", daemon=True)
            _writer.start()
    _wake.set()
