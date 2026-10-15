# Integration test for the dungeon sample game (resources/scenes/dungeon.json, resources/scripts/DungeonGame.py).
# Starts main.exe on the dungeon scene, replaces the keyboard with a scripted sequence through the control
# channel's run_script, and checks the state the game writes to cg_control/dungeon_state.json.
# Usage (from the repo root, after building Debug):
#   pwsh tools/test_dungeon_game.ps1 [-ExeDir build/bin/Debug]
# Exits 0 on success, 1 on any failed check.

param(
    [string]$ExeDir = "build/bin/Debug"
)

$ErrorActionPreference = "Stop"
# Unattended: no window is shown and no real keyboard, mouse or cursor input is read (see CGENGINE_UNATTENDED in src/Bindings/GameBindings.cpp).
$env:CGENGINE_UNATTENDED = "1"
. (Join-Path $PSScriptRoot "unattended.ps1")
# The game runs in its own copy of the folder, so its control folder and settings never touch the folder the player runs.
$exeDir = New-TestCopy (Resolve-Path $ExeDir).Path "dungeon"
$control = Join-Path $exeDir "cg_control"
$inbox = Join-Path $control "inbox"
$outbox = Join-Path $control "outbox"
$stateFile = Join-Path $control "dungeon_state.json"
$failures = @()

function Check($name, [bool]$condition) {
    if ($condition) { Write-Host "PASS  $name" } else { Write-Host "FAIL  $name"; $script:failures += $name }
}

function Send-Command($id, $payload) {
    $tmp = Join-Path $inbox "$id.json.tmp"
    $payload | Set-Content -Path $tmp -Encoding utf8
    Rename-Item -Path $tmp -NewName "$id.json"
}

function Wait-Result($id, $timeoutSec = 20) {
    $path = Join-Path $outbox "$id.result.json"
    $deadline = (Get-Date).AddSeconds($timeoutSec)
    while (-not (Test-Path $path)) {
        if ((Get-Date) -gt $deadline) { return $null }
        Start-Sleep -Milliseconds 200
    }
    Start-Sleep -Milliseconds 100
    return Get-Content $path -Raw | ConvertFrom-Json
}

# Waits until the game has run at least this many frames (the state file reports the count), then returns that state.
function Wait-Frames($target, $timeoutSec = 90) {
    $deadline = (Get-Date).AddSeconds($timeoutSec)
    $state = Read-State
    # The engine runs at well over 1000 frames a second in Release, so poll often: a 100 ms poll would overshoot the target by hundreds of frames.
    while ((-not $state -or $state.frames -lt $target) -and (Get-Date) -lt $deadline) {
        Start-Sleep -Milliseconds 10
        $state = Read-State
    }
    return $state
}

# Waits for the marker file a scripted walk writes at the end of its phase, and returns its position.
function Wait-Mark($name, $timeoutSec = 60) {
    $path = Join-Path $control "mark_$name.json"
    $deadline = (Get-Date).AddSeconds($timeoutSec)
    while (-not (Test-Path $path) -and (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 10 }
    for ($i = 0; $i -lt 20; $i++) {
        try { return Get-Content $path -Raw | ConvertFrom-Json } catch { Start-Sleep -Milliseconds 20 }
    }
    return $null
}

function Read-State() {
    # The game rewrites the file every quarter second. Retry if a read catches it mid-write.
    for ($i = 0; $i -lt 10; $i++) {
        try { return Get-Content $stateFile -Raw | ConvertFrom-Json } catch { Start-Sleep -Milliseconds 100 }
    }
    return $null
}

# Start clean and launch the engine on the dungeon scene with a fixed seed.
Remove-Item -Recurse -Force $control -ErrorAction SilentlyContinue
$env:CGENGINE_SCENE = "scenes/dungeon.json"
$env:DUNGEON_SEED = "7"
$env:DUNGEON_FIXED_STEP = "0.0166667"
$outLog = Join-Path $exeDir "dungeon_test_out.txt"
$errLog = Join-Path $exeDir "dungeon_test_err.txt"
Remove-Item -Force $outLog, $errLog -ErrorAction SilentlyContinue
$app = Start-Unattended (Join-Path $exeDir "main.exe") $exeDir $outLog $errLog
Remove-Item Env:CGENGINE_SCENE
Remove-Item Env:DUNGEON_SEED
Remove-Item Env:DUNGEON_FIXED_STEP

try {
    $deadline = (Get-Date).AddSeconds(20)
    while (-not (Test-Path $stateFile)) {
        if ((Get-Date) -gt $deadline) { throw "the game did not write $stateFile" }
        Start-Sleep -Milliseconds 200
    }
    $start = Read-State
    Check "game starts on level 1 with full health" ($start.level -eq 1 -and $start.hp -eq 3)
    if (-not ($start.level -eq 1 -and $start.hp -eq 3)) { Write-Host ("info  state at the start: level {0}, hp {1}, room {2}" -f $start.level, $start.hp, ($start.room -join ",")) }
    Check "player starts in the start room" ($start.room[0] -eq 0 -and $start.room[1] -eq 0)
    Check "the level has enemies outside the start room" ($start.enemies -ge 1)

    # replaceWorld removes the built-in demo, including bodies that had not started yet when the scene loaded.
    Send-Command "00_describe" '{"command":"describe_scene"}'
    $described = Wait-Result "00_describe"
    $foreign = @($described.result.bodies | Where-Object { $_.name -ne "Root" -and -not $_.name.StartsWith("dg_") })
    Check "replaceWorld leaves only the dungeon's bodies (found: $($foreign.name -join ', '))" ($described.ok -and $foreign.Count -eq 0)
    # The player is the rigged knight: its clips must be loaded for the game to animate it.
    Send-Command "00_anim" '{"command":"list_animations","params":{"name":"dg_player"}}'
    $anims = Wait-Result "00_anim"
    $names = @($anims.result.animations | ForEach-Object { $_.name })
    Check "the player's model has the knight clips (idle, walk, attack)" ($anims.ok -and ("knight_idle" -in $names) -and ("knight_walk" -in $names) -and ("knight_attack" -in $names))

    # Scripted walks. Each phase writes a marker file from inside the game, at the frame where it ends, with the player's
    # position then. The test reads the marker, so the check does not depend on how long a run_script takes to install.
    $controlFwd = $control -replace '\\', '/'
    $markerPrelude = @'
import gc
import json
import cg_engine_bindings as cge
_game = next(o for o in gc.get_objects() if type(o).__name__ == "DungeonGame")
_frame = [0]
def _write_mark(name):
    with open("CONTROL/mark_" + name + ".json", "w") as f:
        json.dump({"x": _game.px, "z": _game.pz, "room": list(_game.room), "hp": _game.hp}, f)
'@ -replace 'CONTROL', $controlFwd

    # Walk right for 60 frames, then left for 210 frames, from the centre of the start room.
    $keys = $markerPrelude + "`n" + @'
_phases = [(1, 61, "D"), (151, 361, "A")]
_marks = {61: "right", 361: "left"}
def _fake(name):
    if name == "D":
        _frame[0] += 1
        if _frame[0] in _marks:
            _write_mark(_marks[_frame[0]])
    return any(a <= _frame[0] < b and name == k for a, b, k in _phases)
cge.key_down = _fake
'@
    $keyFile = Join-Path $control "fake_keys.py"
    Set-Content -Path $keyFile -Value $keys -Encoding utf8
    Send-Command "01_fake_keys" ('{"command":"run_script","params":{"path":"' + ($keyFile -replace '\\', '/') + '"}}')
    $ran = Wait-Result "01_fake_keys"
    Check "scripted keyboard installs" ($ran.ok)

    $right = Wait-Mark "right"
    Check "holding D walks the player right" ($right -and $right.x -gt 8.5 -and $right.x -lt 11.0)
    $left = Wait-Mark "left"
    Check "the west wall stops the player" ($left -and $left.x -gt 0.6 -and $left.x -lt 1.3)
    Check "player stays in the start room" ($left -and $left.room[0] -eq 0 -and $left.room[1] -eq 0)
    Check "no damage without contact" ($left -and $left.hp -eq 3)

    # Walk east through the door of the start room, which seed 7 links to room (1, 0): D for 240 frames.
    $doorKeys = $markerPrelude + "`n" + @'
def _fake(name):
    if name == "D":
        _frame[0] += 1
        if _frame[0] == 241:
            _write_mark("door")
    return name == "D" and _frame[0] <= 240
cge.key_down = _fake
'@
    Set-Content -Path $keyFile -Value $doorKeys -Encoding utf8
    Send-Command "02_door_keys" ('{"command":"run_script","params":{"path":"' + ($keyFile -replace '\\', '/') + '"}}')
    $doorRan = Wait-Result "02_door_keys"
    Check "scripted keyboard for the door walk installs" ($doorRan.ok)
    $through = Wait-Mark "door"
    Check "walking through the door enters room (1, 0)" ($through -and $through.room[0] -eq 1 -and $through.room[1] -eq 0)
    Check "the player walks on into room (1, 0), past the door" ($through -and $through.x -gt 13.5 -and $through.x -lt 17.5)
    Check "the game keeps running after a room change" ($through -and $through.room.Count -eq 2)

    # Walk back through the same door (A for 300 frames): the start room is loaded again, with the same body names.
    $backKeys = $markerPrelude + "`n" + @'
def _fake(name):
    if name == "D":
        _frame[0] += 1
        if _frame[0] == 301:
            _write_mark("back")
    return name == "A" and _frame[0] <= 300
cge.key_down = _fake
'@
    Set-Content -Path $keyFile -Value $backKeys -Encoding utf8
    Send-Command "03_back_keys" ('{"command":"run_script","params":{"path":"' + ($keyFile -replace '\\', '/') + '"}}')
    Wait-Result "03_back_keys" | Out-Null
    $back = Wait-Mark "back"
    Check "walking back returns to the start room" ($back -and $back.room[0] -eq 0 -and $back.room[1] -eq 0)
    Write-Host ("      state after the walk back: player x={0} hp={1} room=({2},{3})" -f $back.x, $back.hp, $back.room[0], $back.room[1])
    Check "the west wall stops the player again" ($back -and $back.x -gt 0.6 -and $back.x -lt 1.3)

    # Kill an enemy with the sword while the demo scene's movement scripts are still alive. This is the path that
    # crashed before: an enemy removed mid-frame. The script finds the game object, puts a one-health enemy just
    # east of the player, faces east, and presses J on the first frame. The enemy closes in at ENEMY_SPEED, so a
    # swing held back by wall-clock time can land after it reaches the player and miss. Pressing on the first frame
    # keeps the swing ahead of the enemy whatever the frame rate.
    $killScript = @'
import gc
import cg_engine_bindings as cge
game = next(o for o in gc.get_objects() if type(o).__name__ == "DungeonGame")
room = game.room
game.facing = (1.0, 0.0)
game.enemies.setdefault(room, []).append({"id": 9999, "kind": "slime", "x": game.px + 0.6, "z": game.pz, "hp": 1, "shot": 0.0})
game.load_room(room)
_presses = 0
def _fake(name):
    global _presses
    if name != "J":
        return False
    _presses += 1
    return _presses == 1
cge.key_down = _fake
'@
    $killFile = Join-Path $control "kill_enemy.py"
    Set-Content -Path $killFile -Value $killScript -Encoding utf8
    $enemiesBeforeKill = (Read-State).enemies
    Send-Command "04_kill" ('{"command":"run_script","params":{"path":"' + ($killFile -replace '\\', '/') + '"}}')
    $killRan = Wait-Result "04_kill"
    Check "the kill script runs" ($killRan.ok)
    Start-Sleep -Milliseconds 1500
    $beforeKill = Read-State
    Check "the sword kills the one-health enemy (enemy count back to the level's own count)" ($beforeKill.enemies -eq $enemiesBeforeKill)
    Check "engine still running after the kill" (-not $app.HasExited)
    Check "the state lists enemy kinds" ($null -ne $beforeKill.enemy_kinds -and $beforeKill.enemy_kinds.slime -ge 1)

    # An archer shoots: restore full health and put an archer four tiles east of the standing player with its shot ready. The bolt flies through
    # the open centre row, hits the player and costs one heart. Then a heart item at the player's feet restores it,
    # and a coin is counted. The archer is removed again so it does not keep shooting.
    $archerScript = @'
import gc
import cg_engine_bindings as cge
game = next(o for o in gc.get_objects() if type(o).__name__ == "DungeonGame")
cge.key_down = lambda name: False
game.hp = 3
game.invulnerable = 0.0
game.enemies.setdefault(game.room, []).append({"id": 9998, "kind": "archer", "x": game.px + 4.0, "z": game.pz, "hp": 1, "shot": 0.0})
game.load_room(game.room)
import json
_orig_hurt = type(game).hurt
_first_hit = []
def _hurt_hook(self):
    hp_before = self.hp
    result = _orig_hurt(self)
    if not _first_hit:
        _first_hit.append(hp_before)
        with open("CONTROL/mark_hurt.json", "w") as f:
            json.dump({"hp": self.hp, "hp_before": hp_before, "restarted": result}, f)
    return result
type(game).hurt = _hurt_hook
'@
    $archerScript = $archerScript.Replace("CONTROL", $control.Replace([string][char]92, [string][char]47))
    $archerFile = Join-Path $control "archer.py"
    Set-Content -Path $archerFile -Value $archerScript -Encoding utf8
    $hpBefore = 3   # the archer script restores full health first, whatever the earlier steps cost
    Send-Command "05_archer" ('{"command":"run_script","params":{"path":"' + ($archerFile -replace '\\', '/') + '"}}')
    $archerRan = Wait-Result "05_archer"
    Check "the archer script runs" ($archerRan.ok)
    # The game writes a marker when the player is hurt, with the health left, so the check reads the hit itself.
    $shot = Wait-Mark "hurt"
    Check "an archer's bolt costs the player a heart" ($shot -and $shot.hp -eq $hpBefore - 1)

    $itemScript = @'
import gc
game = next(o for o in gc.get_objects() if type(o).__name__ == "DungeonGame")
# The archer may already be gone (a later shot can restart the level), so it is removed only if it is still here.
game.enemies[game.room] = [e for e in game.enemies.get(game.room, []) if e["id"] != 9998]
game.items.setdefault(game.room, []).append({"id": 99001, "kind": "heart", "x": game.px, "z": game.pz})
game.items[game.room].append({"id": 99002, "kind": "coin", "x": game.px, "z": game.pz})
game.load_room(game.room)
'@
    $itemFile = Join-Path $control "items.py"
    Set-Content -Path $itemFile -Value $itemScript -Encoding utf8
    $coinsBefore = (Read-State).coins
    Send-Command "06_items" ('{"command":"run_script","params":{"path":"' + ($itemFile -replace '\\', '/') + '"}}')
    $itemsRan = Wait-Result "06_items"
    Check "the item script runs" ($itemsRan.ok)
    Start-Sleep -Milliseconds 1000
    $picked = Read-State
    Check "a heart item restores a heart" ($picked.hp -eq $hpBefore)
    Check "a coin item is counted" ($picked.coins -eq $coinsBefore + 1)
    Check "engine still running after bolts and items" (-not $app.HasExited)

    Check "engine still running after the scripted run" (-not $app.HasExited)
    $errText = if (Test-Path $errLog) { Get-Content $errLog -Raw } else { "" }
    Check "no Python traceback in the engine's error output" (-not ($errText -match "Traceback"))
}
finally {
    if (-not $app.HasExited) { Stop-Process -Id $app.Id -Force }
}

if ($failures.Count -gt 0) {
    Write-Host "`n$($failures.Count) check(s) failed:" -ForegroundColor Red
    $failures | ForEach-Object { Write-Host "  - $_" }
    exit 1
}

Write-Host "`nAll dungeon game checks passed." -ForegroundColor Green
exit 0
