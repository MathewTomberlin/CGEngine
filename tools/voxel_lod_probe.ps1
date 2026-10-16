# Runs tools/voxel_lod_probe.py in the voxel sample: is water drawn in a near chunk as in a far one? Prints the water and ground
# vertices over a water volume placed in each. Usage (from the repo root, after building the Release configuration):
#   pwsh tools/voxel_lod_probe.ps1 [-Seed 1] [-ExeDir build/bin/Release]
param(
    [int]$Seed = 1,
    [string]$ExeDir = "build/bin/Release"
)

$ErrorActionPreference = "Stop"
$env:CGENGINE_UNATTENDED = "1"
. (Join-Path $PSScriptRoot "unattended.ps1")
$exeDir = New-TestCopy (Resolve-Path $ExeDir).Path "lod_probe"
$control = Join-Path $exeDir "cg_control"
$inbox = Join-Path $control "inbox"

$env:CGENGINE_SCENE = "scenes/voxels.json"
$env:VOXEL_SEED = "$Seed"
$env:VOXEL_TIME = "0.25"
$env:VOXEL_WALKERS = "0"
$env:VOXEL_CREATIVE = "1"
$app = Start-Unattended (Join-Path $exeDir "voxel_game.exe") $exeDir
Remove-Item Env:CGENGINE_SCENE, Env:VOXEL_SEED, Env:VOXEL_TIME, Env:VOXEL_WALKERS, Env:VOXEL_CREATIVE

try {
    Start-Sleep -Seconds 30
    $script = (Resolve-Path (Join-Path $PSScriptRoot "voxel_lod_probe.py")).Path.Replace('\', '/')
    $id = "01_lod"
    Set-Content -Path (Join-Path $inbox "$id.json.tmp") -Value ('{"command":"run_script","params":{"path":"' + $script + '"}}') -Encoding utf8
    Rename-Item (Join-Path $inbox "$id.json.tmp") "$id.json"
    $resultPath = Join-Path (Join-Path $control "outbox") "$id.result.json"
    $deadline = (Get-Date).AddSeconds(120)
    while (-not (Test-Path $resultPath)) { if ((Get-Date) -gt $deadline) { throw "the LOD probe did not run" }; Start-Sleep -Milliseconds 300 }
    $ran = Get-Content $resultPath -Raw | ConvertFrom-Json
    if (-not $ran.ok) { throw "the LOD probe failed: $($ran.error)" }
    Start-Sleep -Seconds 2
    $data = Get-Content (Join-Path $control "lod_probe.json") -Raw | ConvertFrom-Json
    foreach ($name in @("near", "far")) {
        $r = $data.$name
        Write-Host ("{0} chunk ({1}, {2}): water vertices {3}, ground vertices {4}, water from y {5} to {6}" -f $name, $r.chunk[0], $r.chunk[1], $r.water_vertices_near, $r.ground_vertices_near, $r.water_lowest_y, $r.water_top_y)
    }
} finally {
    if ($app -and -not $app.HasExited) { $app.Kill() }
}

