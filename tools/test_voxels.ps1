# Integration test for the voxel sample: runs the engine on resources/scenes/voxels.json, walks and turns the player
# with scripted keys, and checks that the chunks load, the camera turns without tilting, and no frame is too long.
# Usage (from the repo root, after building):
#   pwsh tools/test_voxels.ps1 [-ExeDir build/bin/Debug] [-MaxFrameMs 60]
# Build the Release configuration for real frame-time numbers. Exits 0 on success, 1 on any failed check.

param(
    [string]$ExeDir = "build/bin/Debug",
    [double]$MaxFrameMs = 60.0
)

$ErrorActionPreference = "Stop"
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
$app = Start-Process -FilePath (Join-Path $exeDir "main.exe") -WorkingDirectory $exeDir -PassThru
Remove-Item Env:CGENGINE_SCENE
Remove-Item Env:VOXEL_SEED

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

    # Walk forward for 4 seconds, then turn right for 2 seconds: 90 degrees a second is 180 degrees of turn.
    # Mouse input is scripted to nothing so that the real mouse cannot turn the view during the test.
    $keys = @'
import time
import cg_engine_bindings as cge
_t0 = time.perf_counter()
def _fake(name):
    t = time.perf_counter() - _t0
    if 1.0 <= t < 5.0 and name == "W":
        return True
    if 6.0 <= t < 8.0 and name == "Right":
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
    $end = (Get-Date).AddSeconds(11)
    while ((Get-Date) -lt $end) {
        Start-Sleep -Milliseconds 250
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
    $turned = [math]::Abs($turn - 180) -lt 40
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
