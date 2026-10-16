# Runs tools/voxel_ground_probe.py in the voxel sample: does placing water on a grass top move the ground's mesh? Prints the
# result. Usage (from the repo root, after building the Release configuration):
#   pwsh tools/voxel_ground_probe.ps1 [-Seed 1] [-ExeDir build/bin/Release]
# Exits 0 when the probe ran and the ground did not move, 1 when water moved it or the probe did not run.

param(
    [int]$Seed = 1,
    [string]$ExeDir = "build/bin/Release"
)

$ErrorActionPreference = "Stop"
$env:CGENGINE_UNATTENDED = "1"
. (Join-Path $PSScriptRoot "unattended.ps1")
$exeDir = New-TestCopy (Resolve-Path $ExeDir).Path "ground_probe"
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
    # The spawn area loads and meshes in the first seconds.
    Start-Sleep -Seconds 25
    $script = (Resolve-Path (Join-Path $PSScriptRoot "voxel_ground_probe.py")).Path.Replace('\', '/')
    $id = "01_probe"
    Set-Content -Path (Join-Path $inbox "$id.json.tmp") -Value ('{"command":"run_script","params":{"path":"' + $script + '"}}') -Encoding utf8
    Rename-Item (Join-Path $inbox "$id.json.tmp") "$id.json"
    $resultPath = Join-Path (Join-Path $control "outbox") "$id.result.json"
    $deadline = (Get-Date).AddSeconds(120)
    while (-not (Test-Path $resultPath)) { if ((Get-Date) -gt $deadline) { throw "the probe script did not run" }; Start-Sleep -Milliseconds 300 }
    $ran = Get-Content $resultPath -Raw | ConvertFrom-Json
    if (-not $ran.ok) { throw "the probe script failed: $($ran.error)" }

    $probe = Join-Path $control "ground_probe.json"
    if (-not (Test-Path $probe)) { throw "the probe wrote no result" }
    $data = Get-Content $probe -Raw | ConvertFrom-Json
    if (-not $data.ok) { Write-Host "no grass top with air above near the spawn (seed $Seed)"; exit 1 }
    Write-Host "grass top at $($data.spot -join ', '); $($data.vertices_near) ground vertices near it"
    $moved = 0
    foreach ($name in @("spring", "volume")) {
        $r = $data.results.$name
        Write-Host ("{0}: {1} of {2} ground vertices moved, the largest rise {3}" -f $name, $r.moved, $data.vertices_near, $r.max_rise)
        Write-Host ("   the water's lowest surface is {0} (the grass top is {1}): it dips {2} into the grass" -f $r.water_lowest_y, $r.grass_top_y, $r.water_dips_into_grass)
        foreach ($e in $r.examples) { Write-Host ("   at {0}: rise {1}, colour {2} -> {3}" -f ($e.at -join ', '), $e.dy, ($e.colour_before -join ' '), ($e.colour_after -join ' ')) }
        $moved += $r.moved
    }
    if ($moved -gt 0) { exit 1 }
} finally {
    if ($app -and -not $app.HasExited) { $app.Kill() }
}
