# Runs tools/voxel_pool_probe.py in the voxel sample: lists the pools near the swimming test's spot (seed 1, column 0,-31), so a
# test that places the player in one can be kept in step with the terrain (roadmap 43.1 moved the terrain there).
# Usage (from the repo root, after building voxel_game): pwsh tools/voxel_pool_probe.ps1 [-ExeDir build/bin/Release]

param(
    [string]$ExeDir = "build/bin/Release"
)

$ErrorActionPreference = "Stop"
$env:CGENGINE_UNATTENDED = "1"
. (Join-Path $PSScriptRoot "unattended.ps1")
$exeDir = New-TestCopy (Resolve-Path $ExeDir).Path "pool_probe"
$control = Join-Path $exeDir "cg_control"
$inbox = Join-Path $control "inbox"
$outbox = Join-Path $control "outbox"

$env:CGENGINE_SCENE = "scenes/voxels.json"
$env:VOXEL_SEED = "1"
$env:VOXEL_TIME = "0.25"
$env:VOXEL_WALKERS = "0"
$env:VOXEL_CREATIVE = "1"
$app = Start-Unattended (Join-Path $exeDir "voxel_game.exe") $exeDir
Remove-Item Env:CGENGINE_SCENE, Env:VOXEL_SEED, Env:VOXEL_TIME, Env:VOXEL_WALKERS, Env:VOXEL_CREATIVE

try {
    Start-Sleep -Seconds 25
    $script = (Resolve-Path (Join-Path $PSScriptRoot "voxel_pool_probe.py")).Path.Replace('\', '/')
    $id = "02_pool_probe"
    Set-Content -Path (Join-Path $inbox "$id.json.tmp") -Value ('{"command":"run_script","params":{"path":"' + $script + '"}}') -Encoding utf8
    Rename-Item (Join-Path $inbox "$id.json.tmp") "$id.json"
    $resultPath = Join-Path $outbox "$id.result.json"
    $deadline = (Get-Date).AddSeconds(180)
    while (-not (Test-Path $resultPath)) { if ((Get-Date) -gt $deadline) { throw "the probe script did not run" }; Start-Sleep -Milliseconds 300 }
    $ran = Get-Content $resultPath -Raw | ConvertFrom-Json
    if (-not $ran.ok) { throw "the probe script failed: $($ran.error)" }
    $data = Get-Content (Join-Path $control "pool_probe.json") -Raw | ConvertFrom-Json
    Write-Host "columns: $($data.columns.Count), pools (2+ water over solid): $($data.pools.Count)"
    $data.pools | Sort-Object { [math]::Abs($_.x) + [math]::Abs($_.z + 31) } | Select-Object -First 12 |
        ForEach-Object { Write-Host ("  x {0} z {1}: floor top y {2} (block {3}), water {4}, biome {5}" -f $_.x, $_.z, $_.floor, $_.block, $_.water, $_.biome) }
}
finally {
    if (-not $app.HasExited) { Stop-Process -Id $app.Id -Force }
}
