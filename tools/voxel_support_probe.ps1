# Runs tools/voxel_support_probe.py in the voxel sample (roadmap 39.1): times the connection check on a block broken beside the spawn and
# on a detached 2048-block slab in the air. Usage (from the repo root, after building voxel_game):
#   pwsh tools/voxel_support_probe.ps1 [-Seed 1] [-ExeDir build/bin/Release]
# Prints the verdicts, the blocks each fill took in, and the milliseconds per call.

param(
    [int]$Seed = 1,
    [string]$ExeDir = "build/bin/Release"
)

$ErrorActionPreference = "Stop"
$env:CGENGINE_UNATTENDED = "1"
. (Join-Path $PSScriptRoot "unattended.ps1")
$exeDir = New-TestCopy (Resolve-Path $ExeDir).Path "support_probe"
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

try {
    Start-Sleep -Seconds 25
    $script = (Resolve-Path (Join-Path $PSScriptRoot "voxel_support_probe.py")).Path.Replace('\', '/')
    $id = "02_support_probe"
    Set-Content -Path (Join-Path $inbox "$id.json.tmp") -Value ('{"command":"run_script","params":{"path":"' + $script + '"}}') -Encoding utf8
    Rename-Item (Join-Path $inbox "$id.json.tmp") "$id.json"
    $resultPath = Join-Path $outbox "$id.result.json"
    $deadline = (Get-Date).AddSeconds(180)
    while (-not (Test-Path $resultPath)) { if ((Get-Date) -gt $deadline) { throw "the probe script did not run" }; Start-Sleep -Milliseconds 300 }
    $ran = Get-Content $resultPath -Raw | ConvertFrom-Json
    if (-not $ran.ok) { throw "the probe script failed: $($ran.error)" }
    $data = Get-Content (Join-Path $control "support_probe.json") -Raw | ConvertFrom-Json
    foreach ($name in @("ground_block", "slab_2048", "slab_capped_500")) {
        $r = $data.$name
        Write-Host ("{0}: status {1}, visited {2} blocks, {3:N3} ms" -f $name, $r.status, $r.visited, $r.ms)
    }
} finally {
    if ($app -and -not $app.HasExited) { $app.Kill() }
}
