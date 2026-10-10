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
$exeDir = (Resolve-Path $ExeDir).Path
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
$outLog = Join-Path $exeDir "dungeon_test_out.txt"
$errLog = Join-Path $exeDir "dungeon_test_err.txt"
Remove-Item -Force $outLog, $errLog -ErrorAction SilentlyContinue
$app = Start-Process -FilePath (Join-Path $exeDir "main.exe") -WorkingDirectory $exeDir -PassThru `
    -RedirectStandardOutput $outLog -RedirectStandardError $errLog
Remove-Item Env:CGENGINE_SCENE
Remove-Item Env:DUNGEON_SEED

try {
    $deadline = (Get-Date).AddSeconds(20)
    while (-not (Test-Path $stateFile)) {
        if ((Get-Date) -gt $deadline) { throw "the game did not write $stateFile" }
        Start-Sleep -Milliseconds 200
    }
    $start = Read-State
    Check "game starts on level 1 with full health" ($start.level -eq 1 -and $start.hp -eq 3)
    Check "player starts in the start room" ($start.room[0] -eq 0 -and $start.room[1] -eq 0)
    Check "the level has enemies outside the start room" ($start.enemies -ge 1)

    # replaceWorld removes the built-in demo, including bodies that had not started yet when the scene loaded.
    Send-Command "00_describe" '{"command":"describe_scene"}'
    $described = Wait-Result "00_describe"
    $foreign = @($described.result.bodies | Where-Object { $_.name -ne "Root" -and -not $_.name.StartsWith("dg_") })
    Check "replaceWorld leaves only the dungeon's bodies (found: $($foreign.name -join ', '))" ($described.ok -and $foreign.Count -eq 0)

    # Replace key_down with a scripted sequence, measured from when the script runs:
    #   0.0 to 1.0 s: D (walk right, about 4 tiles)
    #   2.5 to 6.0 s: A (walk left until the west wall stops the player)
    $keys = @'
import time
import cg_engine_bindings as cge
_t0 = time.perf_counter()
_phases = [(0.0, 1.0, "D"), (2.5, 6.0, "A")]
def _fake(name):
    now = time.perf_counter() - _t0
    return any(a <= now < b and name == k for a, b, k in _phases)
cge.key_down = _fake
'@
    $keyFile = Join-Path $control "fake_keys.py"
    Set-Content -Path $keyFile -Value $keys -Encoding utf8
    $runAt = Get-Date
    Send-Command "01_fake_keys" ('{"command":"run_script","params":{"path":"' + ($keyFile -replace '\\', '/') + '"}}')
    $ran = Wait-Result "01_fake_keys"
    Check "scripted keyboard installs" ($ran.ok)

    # Read about 1.5 s in: the player has walked right.
    $wait = 1.5 - ((Get-Date) - $runAt).TotalSeconds
    if ($wait -gt 0) { Start-Sleep -Milliseconds ([int]($wait * 1000)) }
    $right = Read-State
    Check "holding D walks the player right" ($right.player[0] -gt 8.5 -and $right.player[0] -lt 11.0)

    # Read about 6.5 s in: the player has been pushed against the west wall and stopped.
    $wait = 6.5 - ((Get-Date) - $runAt).TotalSeconds
    if ($wait -gt 0) { Start-Sleep -Milliseconds ([int]($wait * 1000)) }
    $left = Read-State
    Check "the west wall stops the player" ($left.player[0] -gt 0.6 -and $left.player[0] -lt 1.3)
    Check "player stays in the start room" ($left.room[0] -eq 0 -and $left.room[1] -eq 0)
    Check "no damage without contact" ($left.hp -eq 3)

    # Walk through the east door of the start room, which seed 7 links to room (1, 0). Holding D for six
    # seconds carries the player across the door and into that room. It stops at the far wall.
    $doorKeys = @'
import time
import cg_engine_bindings as cge
_t0 = time.perf_counter()
def _fake(name):
    return name == "D" and time.perf_counter() - _t0 < 6.0
cge.key_down = _fake
'@
    Set-Content -Path $keyFile -Value $doorKeys -Encoding utf8
    $doorRun = Get-Date
    Send-Command "02_door_keys" ('{"command":"run_script","params":{"path":"' + ($keyFile -replace '\\', '/') + '"}}')
    $doorRan = Wait-Result "02_door_keys"
    Check "scripted keyboard for the door walk installs" ($doorRan.ok)
    $wait = 6.5 - ((Get-Date) - $doorRun).TotalSeconds
    if ($wait -gt 0) { Start-Sleep -Milliseconds ([int]($wait * 1000)) }
    $through = Read-State
    Check "walking through the door enters room (1, 0)" ($through.room[0] -eq 1 -and $through.room[1] -eq 0)
    Check "the player stops at the far wall of the new room" ($through.player[0] -gt 23.0 -and $through.player[0] -lt 25.0)
    Check "the game keeps running after a room change" ($through.level -eq 1)

    # Walk back through the same door: the start room is loaded again, with the same body names.
    $backKeys = @'
import time
import cg_engine_bindings as cge
_t0 = time.perf_counter()
def _fake(name):
    return name == "A" and time.perf_counter() - _t0 < 6.0
cge.key_down = _fake
'@
    Set-Content -Path $keyFile -Value $backKeys -Encoding utf8
    Send-Command "03_back_keys" ('{"command":"run_script","params":{"path":"' + ($keyFile -replace '\\', '/') + '"}}')
    Wait-Result "03_back_keys" | Out-Null
    # The engine may run below real time in a Debug build, so wait until the player stops moving.
    $back = $null
    $previous = $null
    $settleDeadline = (Get-Date).AddSeconds(20)
    while ((Get-Date) -lt $settleDeadline) {
        Start-Sleep -Milliseconds 500
        $sample = Read-State
        if ($previous -and [math]::Abs($sample.player[0] - $previous.player[0]) -lt 0.001 -and $sample.player[0] -lt 2) { $back = $sample; break }
        $previous = $sample
        $back = $sample
    }
    Check "walking back returns to the start room" ($back.room[0] -eq 0 -and $back.room[1] -eq 0)
    Write-Host ("      state after the walk back: player x={0} hp={1} room=({2},{3})" -f $back.player[0], $back.hp, $back.room[0], $back.room[1])
    Check "the west wall stops the player again" ($back.player[0] -gt 0.6 -and $back.player[0] -lt 1.3)

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
    Send-Command "04_kill" ('{"command":"run_script","params":{"path":"' + ($killFile -replace '\\', '/') + '"}}')
    $killRan = Wait-Result "04_kill"
    Check "the kill script runs" ($killRan.ok)
    Start-Sleep -Milliseconds 1500
    $beforeKill = Read-State
    Check "the sword kills the one-health enemy (enemy count back to the level's own count)" ($beforeKill.enemies -eq $back.enemies)
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
'@
    $archerFile = Join-Path $control "archer.py"
    Set-Content -Path $archerFile -Value $archerScript -Encoding utf8
    $hpBefore = 3   # the archer script restores full health first, whatever the earlier steps cost
    Send-Command "05_archer" ('{"command":"run_script","params":{"path":"' + ($archerFile -replace '\\', '/') + '"}}')
    $archerRan = Wait-Result "05_archer"
    Check "the archer script runs" ($archerRan.ok)
    $shot = $null
    $shotDeadline = (Get-Date).AddSeconds(10)
    while ((Get-Date) -lt $shotDeadline) {
        Start-Sleep -Milliseconds 250
        $shot = Read-State
        if ($shot.hp -lt $hpBefore) { break }
    }
    Check "an archer's bolt costs the player a heart" ($shot.hp -eq $hpBefore - 1)

    $itemScript = @'
import gc
game = next(o for o in gc.get_objects() if type(o).__name__ == "DungeonGame")
archer = next(e for e in game.enemies[game.room] if e["id"] == 9998)
game.enemies[game.room].remove(archer)
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
