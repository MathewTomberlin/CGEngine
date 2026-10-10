#pragma once

namespace CGEngine {
    /// <summary>
    /// File-based control channel for AI agents and external tools.
    ///
    /// Each command is a JSON file in cg_control/inbox (name must end in .json), shaped
    /// {"command": "...", "params": {...}}. The engine polls the inbox every few frames, runs
    /// each command on the main thread, writes cg_control/outbox/&lt;name&gt;.result.json
    /// ({"ok": true, "result": ...} or {"ok": false, "error": "..."}), and removes the command.
    /// Write commands as name.json.tmp and rename to name.json so the engine never reads a partial file.
    /// See docs/ai/control-api.md for the command list.
    /// </summary>
    class ControlChannel {
    public:
        /// Create the inbox and outbox folders (relative to the working directory, the exe folder).
        static void initialize();
        /// Call once per frame, after rendering.
        static void poll();
        /// Frames rendered since startup, as counted by poll().
        static unsigned long long frameCount();
    };
}
