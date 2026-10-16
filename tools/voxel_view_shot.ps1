# Screenshots the voxel sample at a fixed world seed: the first-person hands holding an item (-View hands), or the nearest water (-View water).
# Usage (from the repo root, after building):
#   pwsh tools/voxel_view_shot.ps1 [-View hands|water|volume|shore] [-Item pickaxe|iron_pickaxe|torch|none] [-Seed 1] [-Out path.png] [-ExeDir build/bin/Release]
# The screenshot is written to -Out (default: the temp folder). Exits 0 when it is saved.

param(
    [ValidateSet("hands", "water", "volume", "shore", "equipment")][string]$View = "hands",
    [ValidateSet("pickaxe", "iron_pickaxe", "sword", "shovel", "bucket", "water_bucket", "lava_bucket", "bow", "torch", "none")][string]$Item = "pickaxe",
    [int]$Seed = 1,
    [ValidateSet("", "carried", "raised")][string]$Shield = "",
    [string]$Out = "",
    [string]$ExeDir = "build/bin/Release"
)

$ErrorActionPreference = "Stop"
# Unattended: no window is shown and no real keyboard, mouse or cursor input is read (see CGENGINE_UNATTENDED in src/Bindings/GameBindings.cpp).
$env:CGENGINE_UNATTENDED = "1"
. (Join-Path $PSScriptRoot "unattended.ps1")
$exeDir = New-TestCopy (Resolve-Path $ExeDir).Path "view"
$control = Join-Path $exeDir "cg_control"
$inbox = Join-Path $control "inbox"
if (-not $Out) { $Out = Join-Path $env:TEMP ("voxel_{0}_{1}.png" -f $View, $Item) }
$Out = [System.IO.Path]::GetFullPath($Out)

# The same world every run, at noon, with no creatures and a fixed frame step.
$env:CGENGINE_SCENE = "scenes/voxels.json"
$env:VOXEL_SEED = "$Seed"
$env:VOXEL_FIXED_STEP = "0.0166667"
$env:VOXEL_TIME = "0.25"
$env:VOXEL_WALKERS = "0"; $env:VOXEL_CREATIVE = "1"
$env:HANDS_ITEM = $Item
if ($Shield) { $env:HANDS_SHIELD = $Shield }
$app = Start-Unattended (Join-Path $exeDir "main.exe") $exeDir
Remove-Item Env:CGENGINE_SCENE, Env:VOXEL_SEED, Env:VOXEL_FIXED_STEP, Env:VOXEL_TIME, Env:VOXEL_WALKERS, Env:VOXEL_CREATIVE, Env:HANDS_ITEM

try {
    # The spawn area loads and meshes in the first seconds; the hands are drawn from the start.
    Start-Sleep -Seconds 25
    $viewFile = switch ($View) { "water" { "voxel_water_view.py" } "volume" { "voxel_volume_view.py" } "shore" { "voxel_shore_view.py" } "equipment" { "voxel_equipment_view.py" } default { "voxel_hands_view.py" } }
    $script = (Resolve-Path (Join-Path $PSScriptRoot $viewFile)).Path.Replace('\', '/')
    $id = "01_hands"
    Set-Content -Path (Join-Path $inbox "$id.json.tmp") -Value ('{"command":"run_script","params":{"path":"' + $script + '"}}') -Encoding utf8
    Rename-Item (Join-Path $inbox "$id.json.tmp") "$id.json"
    $resultPath = Join-Path (Join-Path $control "outbox") "$id.result.json"
    $deadline = (Get-Date).AddSeconds(60)
    while (-not (Test-Path $resultPath)) { if ((Get-Date) -gt $deadline) { throw "the hands script did not run" }; Start-Sleep -Milliseconds 200 }
    $ran = Get-Content $resultPath -Raw | ConvertFrom-Json
    if (-not $ran.ok) { throw "the hands script failed: $($ran.error)" }

    # A volume spreads over a few seconds of game time after it is placed, so its shot waits longer.
    Start-Sleep -Seconds $(if ($View -eq "volume" -or $View -eq "shore") { 8 } else { 3 })
    if (Test-Path $Out) { Remove-Item $Out }
    $png = $Out.Replace('\', '/')
    Set-Content -Path (Join-Path $inbox "02_shot.json.tmp") -Value ('{"command":"screenshot","params":{"path":"' + $png + '"}}') -Encoding utf8
    Rename-Item (Join-Path $inbox "02_shot.json.tmp") "02_shot.json"
    $deadline = (Get-Date).AddSeconds(30)
    while (-not (Test-Path $Out)) { if ((Get-Date) -gt $deadline) { throw "no screenshot was saved" }; Start-Sleep -Milliseconds 300 }
    Start-Sleep -Milliseconds 300
    Write-Host "Saved $Out (view $View, item $Item, seed $Seed)"
} finally {
    if ($app -and -not $app.HasExited) { $app.Kill() }
}
