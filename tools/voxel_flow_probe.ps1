# Runs tools/voxel_flow_probe.py in the voxel sample: places one unit of water at the top of a slope, waits for it to flow, then reports how
# far down the slope the water reached. Usage (from the repo root, after building voxel_game):
#   pwsh tools/voxel_flow_probe.ps1 [-Seed 1] [-ExeDir build/bin/Debug] [-Wait 20]
# Exits 0 when the probe ran; prints the reach and the units of water left on the slope.

param(
    [int]$Seed = 1,
    [string]$ExeDir = "build/bin/Debug",
    [int]$Wait = 20,
    [ValidateSet("place", "place8", "bucket")][string]$Place = "place"
)

$ErrorActionPreference = "Stop"
$env:CGENGINE_UNATTENDED = "1"
. (Join-Path $PSScriptRoot "unattended.ps1")
$exeDir = New-TestCopy (Resolve-Path $ExeDir).Path "flow_probe"
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
    Set-Content -Path (Join-Path $control "flow_phase.txt") -Value $phase -Encoding utf8
    $script = (Resolve-Path (Join-Path $PSScriptRoot "voxel_flow_probe.py")).Path.Replace('\', '/')
    Set-Content -Path (Join-Path $inbox "$id.json.tmp") -Value ('{"command":"run_script","params":{"path":"' + $script + '"}}') -Encoding utf8
    Rename-Item (Join-Path $inbox "$id.json.tmp") "$id.json"
    $resultPath = Join-Path $outbox "$id.result.json"
    $deadline = (Get-Date).AddSeconds(120)
    while (-not (Test-Path $resultPath)) { if ((Get-Date) -gt $deadline) { throw "the probe script did not run ($phase)" }; Start-Sleep -Milliseconds 300 }
    $ran = Get-Content $resultPath -Raw | ConvertFrom-Json
    if (-not $ran.ok) { throw "the probe script failed ($phase): $($ran.error)" }
}

try {
    # The spawn area loads and meshes in the first seconds.
    Start-Sleep -Seconds 25
    if ($Place -eq "bucket") {
        Invoke-Phase "bucket" "02_bucket"
        $b = Get-Content (Join-Path $control "flow_probe.json") -Raw | ConvertFrom-Json
        if (-not $b.ok) { Write-Host "no slope near the spawn (seed $Seed)"; exit 1 }
        $b | ConvertTo-Json -Depth 4
        exit 0
    }
    Invoke-Phase $Place "02_flow_place"
    $placed = Get-Content (Join-Path $control "flow_probe.json") -Raw | ConvertFrom-Json
    if (-not $placed.ok) { Write-Host "no slope of 3 blocks over 6 cells near the spawn (seed $Seed)"; exit 1 }
    Write-Host "placed $($placed.placed_units) unit(s) at $($placed.spot -join ', ') sloping toward $($placed.direction -join ', '); tops $($placed.tops -join ' ')"
    Start-Sleep -Seconds $Wait
    Invoke-Phase "read" "03_flow_read"
    $data = Get-Content (Join-Path $control "flow_probe_result.json") -Raw | ConvertFrom-Json
    Write-Host "after $Wait s: $($data.units) of $($data.placed_units) units of water on the slope, furthest downhill $($data.furthest_downhill) blocks"
    foreach ($c in $data.cells) { Write-Host ("   {0}, {1}, {2}: {3} units" -f $c[0], $c[1], $c[2], $c[3]) }
    Write-Host "terrain tops around the spot (rows z, columns x, 13 by 13 from the spot):"
    foreach ($row in $data.grid_tops) { Write-Host ("   " + (($row | ForEach-Object { "{0,3}" -f $_ }) -join "")) }
} finally {
    if ($app -and -not $app.HasExited) { $app.Kill() }
}
