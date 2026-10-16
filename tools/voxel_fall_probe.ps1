# Runs tools/voxel_fall_probe.py in the voxel sample (roadmap 39.1): breaks the pillar under a slab and checks that the slab falls one
# cell and lands. Usage (from the repo root, after building voxel_game):
#   pwsh tools/voxel_fall_probe.ps1 [-Seed 1] [-ExeDir build/bin/Release]
# Prints the blocks before and after the fall. Exits 1 when the slab did not land as expected.

param(
    [int]$Seed = 1,
    [string]$ExeDir = "build/bin/Release"
)

$ErrorActionPreference = "Stop"
$env:CGENGINE_UNATTENDED = "1"
. (Join-Path $PSScriptRoot "unattended.ps1")
$exeDir = New-TestCopy (Resolve-Path $ExeDir).Path "fall_probe"
$control = Join-Path $exeDir "cg_control"
$inbox = Join-Path $control "inbox"
$outbox = Join-Path $control "outbox"

$env:CGENGINE_SCENE = "scenes/voxels.json"
$env:VOXEL_SEED = "$Seed"
$env:VOXEL_TIME = "0.25"
$env:VOXEL_WALKERS = "0"
$env:VOXEL_CREATIVE = "1"
$app = Start-Unattended (Join-Path $exeDir "voxel_game.exe") $exeDir
Remove-Item Env:CGENGINE_SCENE, Env:VOXEL_SEED, Env:VOXEL_TIME, Env:VOXEL_WALKERS, Env:VOXEL_CREATIVE

function Invoke-Phase([string]$phase, [string]$id) {
    Set-Content -Path (Join-Path $control "fall_phase.txt") -Value $phase -Encoding utf8
    $script = (Resolve-Path (Join-Path $PSScriptRoot "voxel_fall_probe.py")).Path.Replace('\', '/')
    Set-Content -Path (Join-Path $inbox "$id.json.tmp") -Value ('{"command":"run_script","params":{"path":"' + $script + '"}}') -Encoding utf8
    Rename-Item (Join-Path $inbox "$id.json.tmp") "$id.json"
    $resultPath = Join-Path $outbox "$id.result.json"
    $deadline = (Get-Date).AddSeconds(120)
    while (-not (Test-Path $resultPath)) { if ((Get-Date) -gt $deadline) { throw "the probe script did not run ($phase)" }; Start-Sleep -Milliseconds 300 }
    $ran = Get-Content $resultPath -Raw | ConvertFrom-Json
    if (-not $ran.ok) { throw "the probe script failed ($phase): $($ran.error)" }
}

try {
    Start-Sleep -Seconds 25
    Invoke-Phase "make" "02_fall_make"
    Start-Sleep -Seconds 3
    Invoke-Phase "read" "03_fall_read"
    $data = Get-Content (Join-Path $control "fall_probe.json") -Raw | ConvertFrom-Json
    Write-Host ("pillar column at {0}, {1}; pieces after the break: {2} ({3} blocks)" -f $data.column[0], $data.column[2], $data.pieces_after_break, $data.cells_after_break)
    Write-Host ("after the fall: y54 {0}, y55 {1}, y56 {2}; pieces {3}" -f $data.after.y54, $data.after.y55, $data.after.y56, $data.after.pieces)
    $stone = 3   # VoxelGen.STONE (AIR, GRASS, DIRT, STONE: range(9))
    if ($data.after.y54 -eq $stone -and $data.after.y55 -eq $stone -and $data.after.y56 -eq 0 -and $data.after.pieces -eq 0) {
        Write-Host "PASS: the slab fell one cell and landed on the pillar"
    } else {
        Write-Host "FAIL: the slab did not land as expected"
        exit 1
    }
} finally {
    if ($app -and -not $app.HasExited) { $app.Kill() }
}
