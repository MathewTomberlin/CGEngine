# Integration test for the voxel sample: runs the engine on resources/scenes/voxels.json, walks and turns the player
# with scripted keys, and checks that the chunks load, the camera turns without tilting, and no frame is too long.
# Usage (from the repo root, after building):
#   pwsh tools/test_voxels.ps1 [-ExeDir build/bin/Release] [-MaxFrameMs 60]
# The worst-frame check is for the Release configuration: a Debug build of the 841-chunk world runs too slowly for it.
# Exits 0 on success, 1 on any failed check.

param(
    [string]$ExeDir = "build/bin/Release",
    [double]$MaxFrameMs = 60.0
)

$ErrorActionPreference = "Stop"
# Unattended: no window is shown and no real keyboard, mouse or cursor input is read (see CGENGINE_UNATTENDED in src/Bindings/GameBindings.cpp).
$env:CGENGINE_UNATTENDED = "1"
. (Join-Path $PSScriptRoot "unattended.ps1")
$exeDir = (Resolve-Path $ExeDir).Path
$control = Join-Path $exeDir "cg_control"
$inbox = Join-Path $control "inbox"
$outbox = Join-Path $control "outbox"
$stateFile = Join-Path $control "voxel_state.json"
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
    for ($i = 0; $i -lt 10; $i++) {
        try { return Get-Content $stateFile -Raw | ConvertFrom-Json } catch { Start-Sleep -Milliseconds 100 }
    }
    return $null
}

function Wait-State($condition, $timeoutSec) {
    $deadline = (Get-Date).AddSeconds($timeoutSec)
    $state = Read-State
    while (-not ($state -and (& $condition $state)) -and (Get-Date) -lt $deadline) {
        Start-Sleep -Milliseconds 250
        $state = Read-State
    }
    return $state
}

# Start clean and launch the engine on the voxel scene with a fixed seed.
Remove-Item -Recurse -Force $control -ErrorAction SilentlyContinue
$env:CGENGINE_SCENE = "scenes/voxels.json"
$env:VOXEL_SEED = "1"
# Every frame advances 1/60 s, so the keys below last the same game time at any frame rate.
$env:VOXEL_FIXED_STEP = "0.0166667"
# The start is noon, whatever time the player saved: the sun checks below depend on it.
$env:VOXEL_TIME = "0.25"
# No ambient creatures and survival mode: a bite sends the player back to the spawn and moves the walk and the checks.
$env:VOXEL_WALKERS = "0"; $env:VOXEL_CREATIVE = "0"
# The game starts from its saved settings, so the test writes a known baseline first: noon, normal time speed, windowed.
Set-Content -Path (Join-Path $exeDir "voxel_settings.json") -Value '{"mouse_sensitivity": 500, "time_of_day": 0.25, "view_radius": 14, "time_speed": 1.0, "fullscreen": false}' -Encoding utf8
$app = Start-Unattended (Join-Path $exeDir "main.exe") $exeDir
Remove-Item Env:VOXEL_TIME, Env:VOXEL_WALKERS, Env:VOXEL_CREATIVE
Remove-Item Env:CGENGINE_SCENE
Remove-Item Env:VOXEL_SEED
Remove-Item Env:VOXEL_FIXED_STEP

try {
    $deadline = (Get-Date).AddSeconds(20)
    while (-not (Test-Path $stateFile)) {
        if ((Get-Date) -gt $deadline) { throw "the game did not write $stateFile" }
        Start-Sleep -Milliseconds 200
    }

    # The chunks around the spawn point load in the background. The world is loaded when none are pending or meshing.
    $loaded = Wait-State { param($s) $s.pending -eq 0 -and $s.meshing -eq 0 -and $s.loaded -ge 169 } 60
    Check "the view's chunks load (169 or more, none pending)" ($loaded -and $loaded.loaded -ge 169)
    Check "the loaded chunks are meshed (quads, nothing dirty)" ($loaded -and $loaded.quads -gt 0 -and $loaded.dirty -eq 0)

    # The generator's caves and the block edits, checked inside the engine by tools/voxel_world_check.py.
    $checkFile = (Resolve-Path "tools/voxel_world_check.py").Path -replace '\\', '/'
    Send-Command "00_world_check" ('{"command":"run_script","params":{"path":"' + $checkFile + '"}}')
    $ranCheck = Wait-Result "00_world_check" 120
    Check "the world check script runs" ($ranCheck -and $ranCheck.ok)
    $worldCheck = Get-Content (Join-Path $control "voxel_check.json") -Raw | ConvertFrom-Json
    $worldCheck.check.PSObject.Properties | ForEach-Object { Check ("world: " + $_.Name) ([bool]$_.Value) }
    Write-Host ("      caves: {0} of {1} underground blocks are air" -f $worldCheck.caveAir, $worldCheck.underground)
    if ($worldCheck.measure) { $worldCheck.measure.PSObject.Properties | ForEach-Object { Write-Host ("      remesh ms, {0}: {1}" -f $_.Name, $_.Value) } }
    Write-Host ("      light: {0} open-ground samples, {1} underground samples with a mean of {2} (255 is full sky)" -f $worldCheck.light.surfaceSamples, $worldCheck.light.deepSamples, $worldCheck.light.deepMean)

    # The sample game's inventory and block use, checked inside the running game by tools/voxel_game_check.py.
    $gameCheck = (Resolve-Path "tools/voxel_game_check.py").Path -replace '\\', '/'
    Send-Command "00b_game_check" ('{"command":"run_script","params":{"path":"' + $gameCheck + '"}}')
    $ranGame = Wait-Result "00b_game_check" 120
    Check "the game check script runs" ($ranGame -and $ranGame.ok)
    $gameResult = Get-Content (Join-Path $control "voxel_game_check.json") -Raw | ConvertFrom-Json
    $gameResult.check.PSObject.Properties | ForEach-Object { Check ("game: " + $_.Name) ([bool]$_.Value) }
    $gameResult.errors | ForEach-Object { Check ("game check error: " + $_) $false }

    # The developer console's commands, run inside the game by tools/voxel_console_check.py.
    $consoleCheck = (Resolve-Path "tools/voxel_console_check.py").Path -replace '\\', '/'
    Send-Command "00c_console_check" ('{"command":"run_script","params":{"path":"' + $consoleCheck + '"}}')
    $ranConsole = Wait-Result "00c_console_check" 120
    Check "the console check script runs" ($ranConsole -and $ranConsole.ok)
    $consoleResult = Get-Content (Join-Path $control "voxel_console_check.json") -Raw | ConvertFrom-Json
    $consoleResult.check.PSObject.Properties | ForEach-Object { Check ("console: " + $_.Name) ([bool]$_.Value) }
    $consoleResult.errors | ForEach-Object { Check ("console check error: " + $_) $false }

    # Walk forward for 240 frames (4 seconds), then turn right for 120 frames (2 seconds): 90 degrees a second is 180
    # degrees of turn. The frames are counted by the Enter key, which the game asks for once a frame, before the walk.
    # Mouse input is scripted to nothing so that the real mouse cannot turn the view during the test.
    $keys = @'
import cg_engine_bindings as cge
_frames = [0]
def _fake(name):
    if name == "Enter":
        _frames[0] += 1
    f = _frames[0]
    if 1 <= f <= 240 and name == "W":
        return True
    if 300 <= f <= 420 and name == "Right":
        return True
    return False
cge.key_down = _fake
cge.mouse_down = lambda name: False
cge.mouse_delta = lambda: (0.0, 0.0)
'@
    $keyFile = Join-Path $control "voxel_keys.py"
    Set-Content -Path $keyFile -Value $keys -Encoding utf8
    Send-Command "01_keys" ('{"command":"run_script","params":{"path":"' + ($keyFile -replace '\\', '/') + '"}}')
    $ran = Wait-Result "01_keys"
    Check "scripted keys install" ($ran -and $ran.ok)
    $before = Read-State

    $worst = 0.0
    $samples = 0
    $tilt = 0.0
    $aim = 0.0
    # Sample until the scripted keys have run their frames (the state reports the frame count).
    $target = $before.frames + 430
    $end = (Get-Date).AddSeconds(120)
    $s = $before
    while ($s.frames -lt $target -and (Get-Date) -lt $end) {
        Start-Sleep -Milliseconds 100
        $s = Read-State
        if ($s) {
            $worst = [math]::Max($worst, [double]$s.max_frame_ms); $samples++
            # The right vector must stay level (y = 0) however the view turns, and the camera must look where the player does.
            $tilt = [math]::Max($tilt, [math]::Abs([double]$s.camera_right[1]))
            $f = $s.camera_forward; $l = $s.look
            $aim = [math]::Max($aim, 1.0 - ([double]$f[0] * $l[0] + [double]$f[1] * $l[1] + [double]$f[2] * $l[2]))
        }
    }
    $after = Read-State

    $turn = ((($after.yaw - $before.yaw) % 360) + 360) % 360
    $turned = [math]::Abs($turn - 180) -lt 10
    Check "the walk and turn ran (yaw changed by about 180 degrees)" $turned
    Write-Host "      yaw $($before.yaw) -> $($after.yaw), pitch $($before.pitch) -> $($after.pitch), mouse captured: $($after.mouse_captured)"
    Check "turning keeps the pitch fixed (the camera does not tilt)" ([math]::Abs($after.pitch - $before.pitch) -lt 0.5)
    Check "the view stays level while turning (the right vector has no height; worst $([math]::Round($tilt, 4)))" ($samples -gt 0 -and $tilt -lt 0.005)
    Check "the camera looks where the player's yaw and pitch point (worst error $([math]::Round($aim, 6)))" ($samples -gt 0 -and $aim -lt 1e-4)
    Check "the day runs: the time of day moves while the game runs" ($after.time_of_day -ne $before.time_of_day)
    Check "the sun is up at the start (its light travels downwards)" ($before.sun[1] -lt -0.5)
    Check "the fog is on and ends past the loaded chunks (fog $($after.fog[0]) to $($after.fog[1]))" ($after.fog[0] -gt 0 -and $after.fog[1] -gt $after.fog[0] -and $after.fog[1] -ge 100)
    Check "the player moved forward" (($after.player[2] - $before.player[2]) -ne 0 -or ($after.player[0] - $before.player[0]) -ne 0)
    Check "the worst frame while walking and turning is under $MaxFrameMs ms (was $([math]::Round($worst, 1)) ms)" ($samples -gt 0 -and $worst -lt $MaxFrameMs)
    Check "the chunks stay loaded while walking" ($after.loaded -ge 169)

    Check "the engine is still running" (-not $app.HasExited)
}
finally {
    if (-not $app.HasExited) { Stop-Process -Id $app.Id -Force }
}

if ($failures.Count -gt 0) {
    Write-Host "`n$($failures.Count) check(s) failed"
    exit 1
}
Write-Host "`nAll voxel sample checks passed."
exit 0
