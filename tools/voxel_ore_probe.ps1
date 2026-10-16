# Runs tools/voxel_ore_probe.py in the voxel sample (roadmap 28.4): counts the iron ore around the spawn, how much of it a player can
# see (touching air) and how much lies within 8 blocks of the ground. Usage (from the repo root, after building voxel_game):
#   pwsh tools/voxel_ore_probe.ps1 [-Seed 1] [-ExeDir build/bin/Release]
# Writes the counts to the console. It does not fail unless the probe cannot run.

param(
    [int]$Seed = 1,
    [string]$ExeDir = "build/bin/Release"
)

$ErrorActionPreference = "Stop"
$env:CGENGINE_UNATTENDED = "1"
. (Join-Path $PSScriptRoot "unattended.ps1")
$exeDir = New-TestCopy (Resolve-Path $ExeDir).Path "ore_probe"
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
    # The spawn area loads and meshes in the first seconds; the probe reads the blocks, which are generated with the chunks.
    Start-Sleep -Seconds 25
    $script = (Resolve-Path (Join-Path $PSScriptRoot "voxel_ore_probe.py")).Path.Replace('\', '/')
    $id = "02_ore_probe"
    Set-Content -Path (Join-Path $inbox "$id.json.tmp") -Value ('{"command":"run_script","params":{"path":"' + $script + '"}}') -Encoding utf8
    Rename-Item (Join-Path $inbox "$id.json.tmp") "$id.json"
    $resultPath = Join-Path $outbox "$id.result.json"
    $deadline = (Get-Date).AddSeconds(180)
    while (-not (Test-Path $resultPath)) { if ((Get-Date) -gt $deadline) { throw "the probe script did not run" }; Start-Sleep -Milliseconds 300 }
    $ran = Get-Content $resultPath -Raw | ConvertFrom-Json
    if (-not $ran.ok) { throw "the probe script failed: $($ran.error)" }
    $data = Get-Content (Join-Path $control "ore_probe.json") -Raw | ConvertFrom-Json
    Write-Host ("seed {0}: {1} columns in a {2} block box; {3} iron ore; {4} touch air; {5} within {6} blocks of the ground ({7} columns)" -f `
        $Seed, $data.columns, (2 * $data.radius + 1), $data.iron, $data.exposed, $data.near_top, $data.near, $data.columns_with_near_top)
} finally {
    if ($app -and -not $app.HasExited) { $app.Kill() }
}
