# Frame-rate check for the voxel sample: loads the world, then measures the frame rate standing still, turning and walking,
# with the HUD and outlines on, and prints the average and the worst frame for each. Keys are scripted through run_script
# and the mouse is silenced, so the numbers are repeatable.
# Usage (from the repo root, after building the Release configuration):
#   pwsh tools/voxel_perf.ps1 [-ExeDir build/bin/Release] [-Seconds 4]

param(
    [string]$ExeDir = "build/bin/Release",
    [double]$Seconds = 4.0
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

function Read-State {
    try { return Get-Content $stateFile -Raw | ConvertFrom-Json } catch { return $null }
}

function Send-Command($id, $payload) {
    $tmp = Join-Path $inbox "$id.json.tmp"
    $payload | Set-Content -Path $tmp -Encoding utf8
    Rename-Item -Path $tmp -NewName "$id.json"
}

# Holds one key (or none) for the given time windows: a list of @(name, startSec, endSec).
function Set-Script($id, $windows) {
    $list = ($windows | ForEach-Object { '("{0}", {1}, {2})' -f $_[0], $_[1], $_[2] }) -join ", "
    $script = @"
import time
import cg_engine_bindings as cge
_t0 = time.perf_counter()
_windows = [$list]
def _fake(name):
    t = time.perf_counter() - _t0
    return any(name == key and start <= t < end for key, start, end in _windows)
cge.key_down = _fake
cge.mouse_down = lambda name: False
cge.mouse_delta = lambda: (0.0, 0.0)
"@
    $path = Join-Path $control "$id.py"
    Set-Content -Path $path -Value $script -Encoding utf8
    Send-Command $id ('{"command":"run_script","params":{"path":"' + ($path -replace '\\', '/') + '"}}')
    $deadline = (Get-Date).AddSeconds(20)
    while (-not (Test-Path (Join-Path $outbox "$id.result.json"))) {
        if ((Get-Date) -gt $deadline) { throw "no result for $id" }
        Start-Sleep -Milliseconds 100
    }
}

# Samples the state for the given number of seconds: returns the frames per second and the worst frame.
function Sample-Frames($label, $seconds) {
    Start-Sleep -Milliseconds 300
    $a = Read-State
    $t0 = Get-Date
    $worst = 0.0
    while (((Get-Date) - $t0).TotalSeconds -lt $seconds) {
        Start-Sleep -Milliseconds 200
        $s = Read-State
        if ($s) { $worst = [math]::Max($worst, [double]$s.max_frame_ms) }
    }
    $b = Read-State
    $elapsed = ((Get-Date) - $t0).TotalSeconds
    $fps = ($b.frames - $a.frames) / $elapsed
    Write-Host ("{0,-10} {1,6:N0} fps   worst frame {2,6:N1} ms" -f $label, $fps, $worst)
}

Remove-Item -Recurse -Force $control -ErrorAction SilentlyContinue
$env:CGENGINE_SCENE = "scenes/voxels.json"; $env:VOXEL_SEED = "1"
$launched = Get-Date
$app = Start-Unattended (Join-Path $exeDir "main.exe") $exeDir
Remove-Item Env:CGENGINE_SCENE; Remove-Item Env:VOXEL_SEED
try {
    while (-not (Test-Path $stateFile)) { Start-Sleep -Milliseconds 200 }
    do { Start-Sleep -Milliseconds 250; $s = Read-State } while (-not $s -or $s.pending -ne 0 -or $s.meshing -ne 0 -or $s.loaded -lt $s.chunks -or -not $s.on_ground)
    Write-Host ("loaded {0} chunks ({1} quads) in {2:N1} s from the start" -f $s.loaded, $s.quads, ((Get-Date) - $launched).TotalSeconds)

    $still = [double]$Seconds
    Set-Script "perf_still" @()
    Sample-Frames "standing" $still

    Set-Script "perf_turn" @(,@("Right", 0.0, 1000.0))
    Sample-Frames "turning" $still

    Set-Script "perf_walk" @(,@("W", 0.0, 1000.0))
    Sample-Frames "walking" $still
} finally {
    if (-not $app.HasExited) { Stop-Process -Id $app.Id -Force }
}
