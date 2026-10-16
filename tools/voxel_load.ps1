# Load-time check for the voxel sample: starts the world at a view distance, waits for it to settle, then moves the player
# a number of blocks and reports how long the ground takes to arrive. "Ready" counts the chunks in a square around the
# player whose mesh is uploaded (voxel_state.json, ready.r2, r6 and view), so the time to a near square is the time to the
# ground ahead. Keys are scripted through run_script, so the numbers are repeatable.
# Usage (from the repo root, after building the Release configuration):
#   pwsh tools/voxel_load.ps1 [-ExeDir build/bin/Release] [-View 40] [-Jump 60]
# Prints the seconds from the move until each square is ready, and until the world is settled. It does not fail.

param(
    [string]$ExeDir = "build/bin/Release",
    [int]$View = 40,
    [double]$Jump = 60.0
)

$ErrorActionPreference = "Stop"
# Unattended: no window is shown and no real keyboard, mouse or cursor input is read (see CGENGINE_UNATTENDED in src/Bindings/GameBindings.cpp).
$env:CGENGINE_UNATTENDED = "1"
. (Join-Path $PSScriptRoot "unattended.ps1")
$exeDir = New-TestCopy (Resolve-Path $ExeDir).Path "voxel_load"
$control = Join-Path $exeDir "cg_control"
$inbox = Join-Path $control "inbox"
New-Item -ItemType Directory -Force -Path $inbox | Out-Null
$stateFile = Join-Path $control "voxel_state.json"
Set-Content -Path (Join-Path $control "load_settings.json") -Value ('{"mouse_sensitivity": 100, "view_radius": ' + $View + '}') -Encoding utf8
$env:CGENGINE_SCENE = "scenes/voxels.json"; $env:VOXEL_SEED = "1"; $env:VOXEL_WALKERS = "0"; $env:VOXEL_CREATIVE = "1"
$env:VOXEL_SETTINGS = "cg_control/load_settings.json"
$launch = [System.Diagnostics.Stopwatch]::StartNew()
$app = Start-Unattended (Join-Path $exeDir "main.exe") $exeDir
$env:CGENGINE_SCENE = $null; $env:VOXEL_SEED = $null; $env:VOXEL_WALKERS = $null; $env:VOXEL_CREATIVE = $null; $env:VOXEL_SETTINGS = $null

function Read-State { try { return Get-Content $stateFile -Raw | ConvertFrom-Json } catch { return $null } }
function Run-Script($name, $code) {
    $path = Join-Path $control "$name.py"
    Set-Content -Path $path -Value $code -Encoding utf8
    $tmp = Join-Path $inbox "$name.json.tmp"
    Set-Content -Path $tmp -Value ('{"command":"run_script","params":{"path":"' + ($path -replace '\\', '/') + '"}}') -Encoding utf8
    Rename-Item -Path $tmp -NewName "$name.json"
}
function Is-Settled($s) { return $s -and $s.pending -eq 0 -and $s.meshing -eq 0 -and $s.dirty -eq 0 }

try {
    # The world is settled when the loaded count stops growing and nothing is pending, meshing or dirty for a few seconds.
    $last = -1
    $steady = 0
    $deadline = (Get-Date).AddSeconds(600)
    while ($steady -lt 12 -and (Get-Date) -lt $deadline) {
        $s = Read-State
        if ($s -and $s.loaded -eq $last -and (Is-Settled $s)) { $steady++ } else { $steady = 0 }
        if ($s) { $last = $s.loaded }
        Start-Sleep -Milliseconds 500
    }
    $s = Read-State
    Write-Host ("launch to settled: {0:N1} s" -f $launch.Elapsed.TotalSeconds)
    $side = 2 * $s.view_radius + 1
    $full = @{ r2 = 25; r6 = 169; view = $side * $side }
    Write-Host ("start: {0} chunks loaded, view {1}, ready near {2} of 25, view {3} of {4}" -f $s.loaded, $s.view_radius, $s.ready.r2, $s.ready.view, $full.view)

    $before = Read-State
    Run-Script "load_jump" ("import gc`ngame = [o for o in gc.get_objects() if type(o).__name__ == 'VoxelGame'][0]`ngame.creative = True`ngame.flying = True`ngame.pos = [game.pos[0] + " + $Jump + ", game.pos[1] + 30.0, game.pos[2]]`n")
    $target = $before.player[0] + $Jump
    $watch = [System.Diagnostics.Stopwatch]::StartNew()
    $arrived = $null
    $reached = @{}
    $maxFrame = 0.0
    $seenBusy = $false
    $settledAt = $null
    $deadline = (Get-Date).AddSeconds(600)
    while ((Get-Date) -lt $deadline -and $watch.Elapsed.TotalSeconds -lt 300) {
        $s = Read-State
        # The state is written every quarter second, so the move counts as made once the reported position is at the target.
        if ($s -and $null -eq $arrived -and [math]::Abs($s.player[0] - $target) -lt 2.0) { $arrived = $watch.Elapsed.TotalSeconds }
        if ($s -and $null -ne $arrived -and $s.frames -gt $before.frames + 5) {
            if ($s.max_frame_ms -gt $maxFrame) { $maxFrame = $s.max_frame_ms }
            foreach ($key in @("r2", "r6", "view")) {
                if (-not $reached.ContainsKey($key) -and $s.ready.$key -ge $full[$key]) {
                    $reached[$key] = $watch.Elapsed.TotalSeconds
                }
            }
            if (-not (Is-Settled $s)) { $seenBusy = $true }
            if ($seenBusy -and (Is-Settled $s) -and $null -eq $settledAt) { $settledAt = $watch.Elapsed.TotalSeconds }
            if ($null -ne $settledAt -and $reached.ContainsKey("view")) { break }
        }
        Start-Sleep -Milliseconds 100
    }
    $end = Read-State
    foreach ($key in @("r2", "r6", "view")) {
        if ($reached.ContainsKey($key)) {
            Write-Host ("square {0} ({1} chunks) ready after {2:N2} s" -f $key, $full[$key], $reached[$key])
        } else {
            Write-Host ("square {0} ({1} chunks) not ready: {2} at the end" -f $key, $full[$key], $end.ready.$key)
        }
    }
    if ($null -ne $arrived) { Write-Host ("player arrived after {0:N2} s" -f $arrived) } else { Write-Host "player did not arrive" }
    if ($null -ne $settledAt) { Write-Host ("settled after {0:N2} s" -f $settledAt) } else { Write-Host "not settled before the end" }
    Write-Host ("worst frame {0:N1} ms; loaded {1}" -f $maxFrame, $end.loaded)
    # Where the time went, from the move to the end: the worker's terrain and meshes, and the main thread's install and mesh starts.
    Write-Host ("chunks meshed {0:N0}" -f ($end.timings.builds - $before.timings.builds))
    Write-Host ("time from the move (ms): terrain {0:N0}; mesh {1:N0} = light {2:N0} + skin {3:N0} + surface {4:N0}; install {5:N0}; start meshes {6:N0}" -f ($end.timings.terrain - $before.timings.terrain), ($end.timings.mesh - $before.timings.mesh), ($end.timings.light - $before.timings.light), ($end.timings.skin - $before.timings.skin), ($end.timings.surface - $before.timings.surface), ($end.timings.install - $before.timings.install), ($end.timings.start - $before.timings.start))
}
finally {
    Get-CimInstance Win32_Process -Filter "ParentProcessId = $($app.Id)" -ErrorAction SilentlyContinue | ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
    if (-not $app.HasExited) { Stop-Process -Id $app.Id -Force -ErrorAction SilentlyContinue }
}
