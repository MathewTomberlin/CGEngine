# Integration test for swimming in the voxel sample: the player starts over a pool (seed 1, column 0,-31), drops into it, sinks
# to the floor, swims up while Space is held, and walks in the water. The keys are counted in frames (the Enter key is asked
# for once a frame), and the game runs a fixed step, so the checks hold at any frame rate.
# Usage (from the repo root, after building):
#   pwsh tools/test_swimming.ps1 [-ExeDir build/bin/Release]
# Exits 0 on success, 1 on any failed check.

param(
    [string]$ExeDir = "build/bin/Release"
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

function Read-State {
    for ($i = 0; $i -lt 10; $i++) {
        try { return Get-Content $stateFile -Raw | ConvertFrom-Json } catch { Start-Sleep -Milliseconds 100 }
    }
    return $null
}

function Wait-State($condition, $timeoutSec) {
    $deadline = (Get-Date).AddSeconds($timeoutSec)
    $state = Read-State
    while (-not ($state -and (& $condition $state)) -and (Get-Date) -lt $deadline) {
        Start-Sleep -Milliseconds 200
        $state = Read-State
    }
    return $state
}

# Holds the given keys for the given numbers of frames, from the frame the script is installed. Each entry is @(key, frames).
function Install-Keys($id, $holds) {
    $table = ($holds | ForEach-Object { '("{0}", {1})' -f $_[0], $_[1] }) -join ", "
    $script = @"
import cg_engine_bindings as cge
_frames = [0]
_holds = [$table]
def _fake(name):
    if name == "Enter":
        _frames[0] += 1
    return any(name == key and _frames[0] <= limit for key, limit in _holds)
cge.key_down = _fake
cge.mouse_down = lambda name: False
cge.mouse_delta = lambda: (0.0, 0.0)
"@
    $path = Join-Path $control "$id.py"
    Set-Content -Path $path -Value $script -Encoding utf8
    $tmp = Join-Path $inbox "$id.json.tmp"
    ('{"command":"run_script","params":{"path":"' + ($path -replace '\\', '/') + '"}}') | Set-Content -Path $tmp -Encoding utf8
    Rename-Item -Path $tmp -NewName "$id.json"
    $deadline = (Get-Date).AddSeconds(20)
    while (-not (Test-Path (Join-Path $outbox "$id.result.json"))) {
        if ((Get-Date) -gt $deadline) { throw "no result for $id" }
        Start-Sleep -Milliseconds 100
    }
}

Remove-Item -Recurse -Force $control -ErrorAction SilentlyContinue
$env:CGENGINE_SCENE = "scenes/voxels.json"
$env:VOXEL_SEED = "1"
$env:VOXEL_SPAWN = "0.5,-30.5"
$env:VOXEL_FIXED_STEP = "0.0166667"
$app = Start-Process -FilePath (Join-Path $exeDir "main.exe") -WorkingDirectory $exeDir -PassThru
Remove-Item Env:CGENGINE_SCENE; Remove-Item Env:VOXEL_SEED; Remove-Item Env:VOXEL_SPAWN; Remove-Item Env:VOXEL_FIXED_STEP

try {
    # The pool's floor is the solid block at y = 11, so the player's feet rest at 12 on the bottom of the water.
    $landed = Wait-State { param($s) $s.on_ground -eq $true -and $s.pending -eq 0 -and $s.player[1] -lt 20 } 90
    Check "the player drops into the pool and lands on its floor" ([bool]$landed)
    Check "the player is swimming in the water" ([bool]($landed -and $landed.swimming))
    # The floor's top is at 12 and the skin 0.2 above it; a narrow pool's walls push the body's corners up a little more.
    Check "the player rests on the floor of the pool (feet at 12 to 12.6, now $([math]::Round($landed.player[1], 2)))" ([bool]($landed -and $landed.player[1] -gt 11.9 -and $landed.player[1] -lt 12.6))

    # Space is held for 180 frames (3 s): the swim speed is up, so the player rises in the water.
    $before = Read-State
    Install-Keys "01_space" @(,@("Space", 180))
    $risen = Wait-State { param($s) $s.player[1] -gt 13.5 } 20
    Check "holding Space swims the player up to the surface (y $([math]::Round($risen.player[1], 2)))" ([bool]($risen -and $risen.player[1] -gt 13.5))

    # Nothing held: the player sinks back to the floor.
    Install-Keys "02_none" @()
    $sunk = Wait-State { param($s) $s.player[1] -lt 12.8 } 20
    Check "with no keys held the player sinks back down (y $([math]::Round($sunk.player[1], 2)))" ([bool]($sunk -and $sunk.player[1] -lt 12.8))

    # Walking in the water: W for 120 frames (2 s). The pool is small, so the walls may stop the player short of 6 blocks.
    $start = Read-State
    Install-Keys "03_walk" @(,@("W", 120))
    $base = $start.frames
    $walked = Wait-State { param($s) $s.frames -ge $base + 140 } 30
    $distance = [math]::Sqrt([math]::Pow($walked.player[0] - $start.player[0], 2) + [math]::Pow($walked.player[2] - $start.player[2], 2))
    Check "walking in the water moves the player (moved $([math]::Round($distance, 2)) blocks in 2 s)" ($distance -gt 0.5 -and $distance -lt 6.5)

    Check "the engine is still running" (-not $app.HasExited)
}
finally {
    if (-not $app.HasExited) { Stop-Process -Id $app.Id -Force }
}

if ($failures.Count -gt 0) {
    Write-Host "`n$($failures.Count) check(s) failed"
    exit 1
}
Write-Host "`nAll swimming checks passed."
exit 0
