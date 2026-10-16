"""Loopback multiplayer for the voxel sample (roadmap 18.2, 18.3): two game processes on one machine share their edits over a TCP
socket, and the listening one is the host, which owns the world.

One process listens (VOXEL_NET=listen:7790) and the other connects (VOXEL_NET=connect:127.0.0.1:7790). Both must run the same
world (the same VOXEL_SEED), because the terrain is generated in each process; only the edits and the player's position are
sent. The messages are JSON lines:
  client -> host: {"t": "req", "op": "hit", "c": cell, "swings": n}, {"t": "req", "op": "place", "c": cell, "id": id},
                  {"t": "req", "op": "prop", "c": cell, "s": support, "dir": dir}, {"t": "req", "op": "unprop", "c": cell}, {"t": "req", "op": "scoop", "c": cell}
  host -> client: {"t": "cells", "cells": [[x, y, z, block, damage, prop], ...]}, the state of each cell that changed
                  {"t": "ack", "rid": id, "ok": bool, "broke": bool}, the answer to a request that has an id (rid): whether the host made
                  the change, and whether a hit broke the block (the client counts its items from it, VoxelNetSync.settle_request)
  both ways:      {"t": "pos", "p": [x, y, z], "yaw": yaw}

The host applies a request with its own rules and then sends the state of the cell, so a request it refuses is undone on the
client. A new connection gets every edited cell at once. The socket is not blocking: poll() is called once a frame, reads what has
arrived, and returns it. A connecting process retries until the listener is up. Nothing here is secure or authenticated: it is for
two players on one machine, as a first step."""
import json
import socket
import time

RETRY_SECONDS = 1.0         # a connecting process tries again this often until the listener is up
CONNECT_TIMEOUT = 0.2       # seconds a connection attempt may block the frame
MAX_READS = 16              # socket reads per poll (64 KB each): a snapshot of many edits arrives over a few frames


def parse(spec):
    """VOXEL_NET: "listen:<port>" or "connect:<host>:<port>". Returns (mode, host, port), or None when it is unset or unknown."""
    if not spec:
        return None
    parts = spec.split(":")
    if parts[0] == "listen" and len(parts) == 2:
        return ("listen", "127.0.0.1", int(parts[1]))
    if parts[0] == "connect" and len(parts) == 3:
        return ("connect", parts[1], int(parts[2]))
    return None


class Link:
    def __init__(self, mode, host, port):
        self.mode = mode
        self.host = host
        self.port = port
        self.sock = None            # the connection to the other process, or None while there is none
        self.server = None          # the listening socket (listen mode)
        self.buffer = b""
        self.last_try = -1e9
        self.sent = 0
        self.received = 0
        if mode == "listen":
            self.server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self.server.bind((host, port))
            self.server.listen(1)
            self.server.setblocking(False)

    @property
    def connected(self):
        return self.sock is not None

    def poll(self):
        """Accept or connect if there is no connection yet, read what has arrived, and return the messages in it."""
        if self.sock is None:
            self.connect_or_accept()
        messages = []
        if self.sock is not None:
            try:
                for _ in range(MAX_READS):
                    data = self.sock.recv(65536)
                    if data == b"":
                        self.drop()
                        break
                    self.buffer += data
            except BlockingIOError:
                pass
            except OSError:
                self.drop()
        while b"\n" in self.buffer:
            line, self.buffer = self.buffer.split(b"\n", 1)
            if line:
                try:
                    messages.append(json.loads(line.decode("utf-8")))
                except ValueError:
                    pass   # a line that is not JSON is dropped: the other side is a different program, or a torn write
        self.received += len(messages)
        return messages

    def send(self, message):
        """Send one message. Returns False when there is no connection (the message is then not sent)."""
        if self.sock is None:
            return False
        data = (json.dumps(message) + "\n").encode("utf-8")
        try:
            self.sock.setblocking(True)
            self.sock.settimeout(0.5)
            self.sock.sendall(data)
        except OSError:
            self.drop()
            return False
        finally:
            if self.sock is not None:
                self.sock.settimeout(None)
                self.sock.setblocking(False)
        self.sent += 1
        return True

    def connect_or_accept(self):
        if self.server is not None:
            try:
                conn, _address = self.server.accept()
                conn.setblocking(False)
                self.sock = conn
                self.buffer = b""
            except BlockingIOError:
                pass
            return
        now = time.time()
        if now - self.last_try < RETRY_SECONDS:
            return
        self.last_try = now
        try:
            conn = socket.create_connection((self.host, self.port), timeout=CONNECT_TIMEOUT)
            conn.setblocking(False)
            self.sock = conn
            self.buffer = b""
        except OSError:
            pass

    def drop(self):
        """The connection ended: a listener waits for the next one, and a connecting process tries again."""
        if self.sock is not None:
            try:
                self.sock.close()
            except OSError:
                pass
        self.sock = None
        self.buffer = b""

    def close(self):
        self.drop()
        if self.server is not None:
            self.server.close()
            self.server = None
