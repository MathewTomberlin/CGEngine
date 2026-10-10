# Script hooks (Python, over the control channel)

Two control commands let an agent run and attach Python scripts to a running engine:
`run_script` (run a file once) and `attach_script` (run a module on a body's events).
Both are called through the file inbox; see `docs/ai/control-api.md`.

## run_script

Runs a Python file once, in the engine's Python session. Use it to create or change scene
content with the `cg_engine_bindings` module.

- params: `path` (required; absolute, or relative to the exe folder)
- result: `{ "ran": path }`
- errors: the Python exception text, for example a syntax error or an exception raised by the file

Python keeps its state between `run_script` calls.

## attach_script

Attaches a script module from the `scripts` folder next to the exe to a named body. The module must
define a `create_instance()` function that returns an object with a `__call__(self, args)` method.
Inherit from `PyScript` (in `scripts/PyScript.py`) to get the base class.

- params: `name` (body, required), `module` (module name without `.py`, required),
  `domain` (`"start"`, `"update"` (default) or `"delete"`)
- result: `{ "name", "domain", "scriptId" }`
- errors: unknown body, unknown domain, module that cannot be loaded (the engine console
  has the Python error)

When the domain runs, the engine calls the object with `args`:
- `args.caller`: the body the script is attached to (use `args.caller.get_name()`)
- `args.script`, `args.behavior`: the script itself and the behaviour that called it, if any

Domains: `start` runs once when the body starts, `update` runs every frame, `delete` runs when the
body is removed.

Python exceptions raised inside an attached script are printed to the engine console. They do not
appear in the result file, because the script runs later, during the frame.

Modules are imported once per process. After you edit a module, restart the engine to pick up changes.

## Example

`resources/scripts/ControlHitLogger.py` logs the name of its body the first time it runs:

```json
{ "command": "attach_script", "params": { "name": "caveman", "module": "ControlHitLogger", "domain": "update" } }
```

Then check `cg_control/script_hits.log`.

## Security

`run_script` runs any Python file it is given, with the engine's full permissions. The control
channel is for a developer's own machine (see `docs/ai/control-api.md`). Do not expose it to
untrusted writers.
