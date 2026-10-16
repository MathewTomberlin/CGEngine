# In-game check for creature contact (resources/scripts/VoxelPhysics.py, CONTACT): a biter is put three blocks from a player who
# stands still. It must come to the player's side and bite, and never get closer than the contact distance (the boxes would
# overlap). The check reads the nearest creature's distance from the state file each time the game writes it.
# Usage (from the repo root, after building the Debug configuration):
#   pwsh tools/test_creature_contact.ps1 [-ExeDir build/bin/Debug]
# Exits 0 on success, 1 on any failed check.

param(
    [string]$ExeDir = "build/bin/Debug"
)

$ErrorActionPreference = "Stop"
# Unattended: no window is shown and no real keyboard, mouse or cursor input is read (see CGENGINE_UNATTENDED in src/Bindings/GameBindings.cpp).
$env:CGENGINE_UNATTENDED = "1"
. (Join-Path $PSScriptRoot "unattended.ps1")
$exeDir = (Resolve-Path $ExeDir).Path
$control = Join-Path $exeDir "cg_control"
$stateFile = Join-Path $control "voxel_state.json"
$failures = @()
$CONTACT = 0.8485   # (0.3 + 0.3) * sqrt(2): the centre distance two creature/player boxes can come to without overlapping

function Check($name, [bool]$condition) {
    if ($condition) { Write-Host "PASS  $name" } else { Write-Host "FAIL  $name"; $script:failures += $name }
}

function Run-Script($name, $code) {
    $inbox = Join-Path $control "inbox"
    New-Item -ItemType Directory -Force -Path $inbox | Out-Null
    $path = Join-Path $control "$name.py"
    Set-Content -Path $path -Value $code -Encoding utf8
    $tmp = Join-Path $inbox "$name.json.tmp"
    Set-Content -Path $tmp -Value ('{"command":"run_script","params":{"path":"' + ($path -replace '\\', '/') + '"}}') -Encoding utf8
    Rename-Item -Path $tmp -NewName "$name.json"
}

function Read-State {
    try { return Get-Content $stateFile -Raw | ConvertFrom-Json } catch { return $null }
}

function Wait-Until([scriptblock]$ready, $timeoutSec = 90) {
    $deadline = (Get-Date).AddSeconds($timeoutSec)
    while ((Get-Date) -lt $deadline) {
        $s = Read-State
        if ($s -and (& $ready $s)) { return $s }
        Start-Sleep -Milliseconds 250
    }
    return $null
}

# The scene, the seed and the settings are the test's own; no ambient creatures and survival mode, so only this biter is there.
New-Item -ItemType Directory -Force -Path $control | Out-Null
Set-Content -Path (Join-Path $control "test_contact_settings.json") -Value '{"mouse_sensitivity": 100, "view_radius": 8}' -Encoding utf8
$env:CGENGINE_SCENE = "scenes/voxels.json"; $env:VOXEL_SEED = "1"; $env:VOXEL_TIME = "0.25"
$env:VOXEL_WALKERS = "0"; $env:VOXEL_CREATIVE = "0"; $env:VOXEL_SETTINGS = "cg_control/test_contact_settings.json"
$app = $null
try {
    $app = Start-Unattended (Join-Path $exeDir "main.exe") $exeDir
} finally {
    Remove-Item Env:CGENGINE_SCENE, Env:VOXEL_SEED, Env:VOXEL_TIME, Env:VOXEL_WALKERS, Env:VOXEL_CREATIVE, Env:VOXEL_SETTINGS
}

try {
    $ready = Wait-Until { param($s) $s.pending -eq 0 -and $s.meshing -eq 0 -and $s.on_ground -eq $true -and $s.loaded -ge 100 } 120
    Check "the game loads the world and the player stands on the ground" ($null -ne $ready)
    if (-not $ready) { throw "the world did not load" }
    Start-Sleep -Seconds 1

    # The biter is placed on the ground three blocks east of the player, with the same mesh and walker a creature gets.
    Run-Script "contact_spawn" @'
import gc
import math
import random
import VoxelWalkers
game = [o for o in gc.get_objects() if type(o).__name__ == "VoxelGame"][0]
x, z = math.floor(game.pos[0]) + 3, math.floor(game.pos[2])
top = None
for y in range(60, 3, -1):
    if game.world.is_solid(x, y, z):
        top = y
        break
biter = VoxelWalkers.Walker(game.world, "contact_biter", (x + 0.5, top + 1.0, z + 0.5), (0.4, 0.1, 0.1), random.Random(5), biter=True)
game.walkers.walkers.append(biter)
'@

    # Watch the nearest creature for six seconds, while the player stands still.
    $closest = @()
    $deadline = (Get-Date).AddSeconds(6)
    while ((Get-Date) -lt $deadline) {
        $s = Read-State
        if ($s -and $null -ne $s.closest_creature) { $closest += [double]$s.closest_creature }
        Start-Sleep -Milliseconds 100
    }
    $end = Read-State
    $nearest = if ($closest.Count) { ($closest | Measure-Object -Minimum).Minimum } else { $null }
    Write-Host "info  nearest creature over six seconds: $nearest blocks (contact distance $CONTACT); health $($end.health); deaths $($end.deaths)"

    Check "the biter comes to the player's side" (($null -ne $nearest) -and ($nearest -le 1.1))
    Check "the biter never gets closer than the contact distance" (($null -ne $nearest) -and ($nearest -ge $CONTACT - 0.01))
    Check "the biter bites from the side" ($end.health -lt 10.0)
}
finally {
    if ($app -and -not $app.HasExited) { Stop-Process -Id $app.Id -Force }
}

if ($failures.Count -gt 0) {
    Write-Host "`n$($failures.Count) check(s) failed"
    exit 1
}
Write-Host "`nAll creature contact checks passed."
exit 0
