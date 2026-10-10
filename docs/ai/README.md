# CGEngine for AI agents: start here

CGEngine is a C++ engine for real-time 2D and 3D scenes. Agents can create and inspect content
without writing C++, using two interfaces:

| You want to... | Read |
|---|---|
| Describe a scene (materials, lights, model placements, primitives) | [scene-format.md](scene-format.md) |
| Drive a running engine (load scenes, change transforms and materials, remove bodies, take screenshots) | [control-api.md](control-api.md) |
| Run Python scripts in a running engine, or attach them to bodies | [script-hooks.md](script-hooks.md) |
| Know what is planned and what is done | [../ROADMAP.md](../ROADMAP.md) |

## Quick start

1. Build (see `CLAUDE.md`): `cmake --build build --config Debug --target main`.
2. Run `build/bin/Debug/main.exe` with `CGENGINE_SCENE=scenes/example.json` (or launch it and load a scene by command).
3. Write a command file to `cg_control/inbox/<name>.json.tmp`, rename it to `<name>.json`, then read `cg_control/outbox/<name>.result.json`.

Example command:

```json
{ "command": "describe_scene" }
```

## Conventions

- Scene and command paths are relative to the exe folder, unless absolute.
- Every command returns `{"ok": true, "result": ...}` or `{"ok": false, "error": "..."}`.
- Names of bodies, materials and lights come from the scene file. Keep them unique within their kind.
- Counts such as `get_stats` include engine default assets.

## Checking your work

- `tools/test_control_channel.ps1` runs the engine and checks every command, plus invalid-scene handling.
- `CGENGINE_SMOKE_FRAMES=120 main.exe` runs headless-friendly smoke frames and reports `avgFrameMs` and `drawCalls`.
