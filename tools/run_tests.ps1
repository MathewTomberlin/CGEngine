# Runs the project's test suites and prints one summary line per suite. Exits 1 if any suite fails, so it can gate a commit or a CI step.
# The Python suites need no GPU and run by default. The engine suites start the game unattended (see tools/unattended.ps1), so
# they need the built executables (main and voxel_game, Release or Debug): pass -Engine to run them too. Each suite keeps its own
# script; this only runs them and counts.
# Usage (from the repo root):
#   pwsh tools/run_tests.ps1                       # the Python suites
#   pwsh tools/run_tests.ps1 -Engine               # the Python suites, then the engine suites (Release build)
#   pwsh tools/run_tests.ps1 -Engine -ExeDir build/bin/Debug

param(
    [switch]$Engine,
    [string]$ExeDir = "build/bin/Release"
)

$ErrorActionPreference = "Continue"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
Push-Location $root

# Each suite: its name, the command, and the arguments.
$python = @(
    "tools/test_dungeon_gen.py",
    "tools/test_scene_format.py",
    "tools/test_voxel_physics.py",
    "tools/test_voxel_recipes.py",
    "tools/test_voxel_inventory.py",
    "tools/test_voxel_drops.py",
    "tools/test_voxel_night.py",
    "tools/test_voxel_sounds.py",
    "tools/test_voxel_rules.py",
    "tools/test_voxel_tiles.py",
    "tools/test_voxel_moon.py",
    "tools/test_voxel_creatures.py",
    "tools/test_voxel_paths.py",
    "tools/test_voxel_hands.py",
    "tools/test_held_items.py",
    "tools/test_voxel_sheet.py",
    "tools/test_voxel_walkers.py",
    "tools/test_voxel_tools.py",
    "tools/test_voxel_auto_jump.py"
)
$engineSuites = @(
    "tools/test_voxels.ps1",
    "tools/test_creature_contact.ps1",
    "tools/test_swimming.ps1",
    "tools/test_pause_menu.ps1",
    "tools/test_voxel_net.ps1",
    "tools/test_control_channel.ps1",
    "tools/test_dungeon_game.ps1"
)

$results = @()
function Run-Suite($script, $command, $arguments) {
    $watch = [System.Diagnostics.Stopwatch]::StartNew()
    & $command @arguments > $null 2>&1
    $code = $LASTEXITCODE
    $watch.Stop()
    $script:results += [pscustomobject]@{ Suite = $script; Result = $(if ($code -eq 0) { "PASS" } else { "FAIL" }); Seconds = [math]::Round($watch.Elapsed.TotalSeconds, 1); Exit = $code }
    Write-Host ("{0,-4}  {1,-40} {2,6:N1} s" -f $results[-1].Result, $script, $results[-1].Seconds)
}

foreach ($suite in $python) { Run-Suite $suite "python" @("-I", $suite) }
if ($Engine) {
    foreach ($suite in $engineSuites) { Run-Suite $suite "pwsh" @("-NoProfile", "-File", $suite, "-ExeDir", $ExeDir) }
}

Pop-Location
$failed = @($results | Where-Object { $_.Result -ne "PASS" })
Write-Host ""
Write-Host ("{0} of {1} suites passed" -f ($results.Count - $failed.Count), $results.Count)
if ($failed.Count -gt 0) {
    Write-Host ("failed: " + (($failed | ForEach-Object { $_.Suite }) -join ", "))
    exit 1
}
exit 0
