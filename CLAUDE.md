# CGEngine — agent guide

C++17 scripting engine for real-time 2D/3D scenes on OpenGL 3.3 (core). Uses SFML 3 for window/input, GLEW for GL loading, Assimp for mesh/model import, and embedded Python (pybind11) for Python-driven scripts. Alpha software; APIs change freely.

## Build and run (Windows, MSVC x64)

Prerequisites: Visual Studio 2022 Community with the C++ workload (VS 2026 "18" is also installed on the dev machine; CMake picks 2022), CMake 3.28+, Git, and network access (CMake fetches SFML 3.0.0 via FetchContent on first configure).

```bash
git submodule update --init external/pybind11
cmake -S . -B build -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DINSTALL_MANPAGES=OFF -DBUILD_DOC=OFF
cmake --build build --config Debug --target main
```

Run `build/bin/Debug/main.exe` from its own folder. The post-build steps copy `resources/`, `resources/scripts`, the Python scripts in `src/Core/Interpreter/Scripts`, and `external/assimp/lib/assimp-vc143-mt.dll` next to the exe.

Why the extra flags (CMake 4.x + bundled deps):
- `-DCMAKE_POLICY_VERSION_MINIMUM=3.5`: FreeType and Vorbis in SFML's fetched deps declare CMake < 3.5 minimums that CMake 4 rejects.
- `-DINSTALL_MANPAGES=OFF`: FLAC's install step needs Pandoc.
- `-DBUILD_DOC=OFF`: Doxygen is optional; the Doxygen post-build step only runs when Doxygen is found.

Do not commit `build/` or `out/` (both are gitignored).

Pre-built third-party libs are checked in under `external/` (`glew-2.1.0`, `assimp`, `glm`) and linked by absolute path in `CMakeLists.txt`. They are x64 MSVC builds; don't replace them without also updating the paths.

There is no automated test suite. Verify changes by building and running `main.exe`.

## Layout

- `CMakeLists.txt`: builds the static library `CGEngineCore` from every file under `src/`, adds `src/Bindings` (pybind11 bindings for the engine types), and links `main` (from `src/main.cpp`) against it.
- `src/main.cpp`: builds a `TilemapScene` and calls `beginWorld()`.
- `src/Core/`: the engine.
  - `Engine/`: global objects (`renderer`, `world`, `time`, `input`, `assets`, `sceneList`, event name constants) declared in `Engine.h`. `renderer` is `nullptr` until `World::startWorld()` creates it.
  - `Body/`, `Behavior/`, `Scripts/`: the scene model (see below).
  - `World/`: owns the Body list and drives update/render; `startWorld()` creates the `PyInterpreter`, the `Renderer`, the window, and initializes `assets`.
  - `Interpreter/`: `PyInterpreter.h` embeds Python (`py::scoped_interpreter`), adds `resources/scripts` and `cg_engine_python/Scripts` to `sys.path`, and creates PyScripts.
  - `Mesh/`, `Material/`, `Shader/`, `Light/`, `Camera/`, `Animation/`, `Skeleton/`, `Importer/`, `Timers/`, `Input/`, `Time/`, `Logging/`, `Types/`, `AssetManager/`.
- `src/Bindings/`: pybind11 modules exposing engine types to Python.
- `src/Standard/`: reusable building blocks (`Behaviors/`, `Drawables/` such as `Tilemap`, `Models/`, `Scripts/`).
- `src/TilemapScene.cpp`: the demo scene wired to `main`.
- `resources/`: models, textures, fonts, `shaders/`, tilemap data, and `scripts/` (Python), copied beside the exe.
- `external/pybind11`: git submodule (see `.gitmodules`).
- `docs/`: generated Doxygen HTML. Don't hand-edit.

## Engine model

- A **Body** wraps a renderable `Transformable` (SFML 2D or a Mesh with 3D transform). Bodies hold Scripts, Behaviors, and Timers. Models are hierarchies of Bodies imported via Assimp.
- A **Behavior** is attached to Bodies and has Scripts, but has no transform of its own. `AnimationBehavior` reads the owner's `Sprite` in its constructor, so the owner must hold a Sprite.
- A **Script** runs a `ScriptEvent` lambda with the calling Body and Script. Scripts take `ScArgs` (typed input/output args). PyScripts are Python-side scripts created through `PyInterpreter`.
- Scripts fire on lifecycle events (`onStartEvent`, `onUpdateEvent`, `onDeleteEvent`, `onLoadEvent`) and input events (`onKeyPressEvent`, `onMousePressEvent`, and so on).
- **UniqueDomain** stores uniquely identifiable pointers. The asset cache (textures, meshes, materials, models) is `AssetManager`.
- `AssetManager::load<T>` and `create<T>` return `optional<pair<id_t, T*>>`. `Model::instantiate` and `Behavior::getId` return `optional<size_t>` / `optional<id_t>`. Don't mix the two; use `.value().second` for the pointer and `.value().first` for the id.

## Conventions

- C++17 (`cxx_std_17`). Match the surrounding file's style, naming, and indentation. Existing files mix tabs and spaces; keep each file's own style.
- Keep changes focused. Don't reformat unrelated code or regenerate `docs/`.
- Don't modify files under `external/` or `docs/`. Don't commit `.vs`, `build/`, or `out/` content.
- Add new assets under `resources/`; they are copied at build time.

## Git workflow

- `master`: release line. Updated by PRs from `develop`.
- `develop`: integration branch. Feature PRs target it.
- Feature branches use the author prefix: `u-mlt-<Topic>` for user work, `f-mlt-<Topic>` for longer features. Merged to `develop` by PR (e.g. `Merge pull request #148 from MathewTomberlin/u-mlt-...`).
- `TilemapScene_Example`: older personal branch; its work is merged into `develop`. Merge `develop` into it before reusing it.
- Commit messages are short and imperative, one change per commit (e.g. "Correctly implement static domains in behaviors").

Before branching, run `git fetch` and base on the latest `develop`, not on `master`.

## Known state (verify before relying on it)

- `develop` runs: `build/bin/Debug/main.exe` renders the TilemapScene (verified on this machine).
- Fixes made on `develop` (uncommitted as of this writing):
  - `src/TilemapScene.cpp`: `AssetManager::load/create` return pairs (`.value().second`); player sprite passed by value to `create<Body>` (a `Sprite*` silently bound to `Body(bool isWorldRoot)`, leaving the player with no entity).
  - `src/Core/Behavior/Behavior.cpp`: `getId()` returned `behaviorId.value()`, which threw `bad_optional_access` for unregistered behaviors; now returns the optional.
  - `CMakeLists.txt`: Doxygen post-build step is guarded.
- Footgun: `Body(bool isWorldRoot)` accepts any pointer through implicit conversion. Passing a pointer to `create<Body>` compiles and produces an empty Body.
- Benign warning at startup: `AssetManager: Resource loading failed, returning default resource: lava_tile.png` (referenced by `resources/Caveman_Test2.fbx`).
- `develop` has 120 commits not pushed to `origin/develop`.
- `.github/copilot-instructions.md.txt` is not active (it has a `.txt` suffix).
