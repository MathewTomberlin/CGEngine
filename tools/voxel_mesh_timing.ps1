# Meshing-cost check for the voxel sample (roadmap 26): starts the game headless at a fixed seed, waits until the loaded chunks are
# meshed and nothing is changing, then reads the cumulative meshing timings from voxel_state.json. The numbers are repeatable for a
# seed and a view, so a change to the mesher can be measured against the same world.
# Usage (from the repo root, after building the Release configuration):
#   pwsh tools/voxel_mesh_timing.ps1 [-ExeDir build/bin/Release] [-Seed 1] [-MaxSeconds 240]
# Prints the chunks, the quads and the timings. It does not fail.

param(
    [string]$ExeDir = "build/bin/Release",
    [int]$Seed = 1,
    [int]$MaxSeconds = 240
)

$ErrorActionPreference = "Stop"
$env:CGENGINE_UNATTENDED = "1"
$env:CGENGINE_HEADLESS = "1"
$env:CGENGINE_HEADLESS_FPS = "60"
. (Join-Path $PSScriptRoot "unattended.ps1")
$exeDir = New-TestCopy (Resolve-Path $ExeDir).Path "mesh_timing"
$control = Join-Path $exeDir "cg_control"
$state = Join-Path $control "voxel_state.json"

$env:VOXEL_SEED = "$Seed"
$env:VOXEL_CREATIVE = "1"
$app = Start-Unattended (Join-Path $exeDir "voxel_game.exe") $exeDir
Remove-Item Env:VOXEL_SEED, Env:VOXEL_CREATIVE, Env:CGENGINE_HEADLESS, Env:CGENGINE_HEADLESS_FPS

try {
    $deadline = (Get-Date).AddSeconds($MaxSeconds)
    $quiet = 0
    $data = $null
    while ((Get-Date) -lt $deadline) {
        Start-Sleep -Seconds 2
        if (-not (Test-Path $state)) { continue }
        try { $data = Get-Content $state -Raw | ConvertFrom-Json } catch { continue }
        if ($data.pending -eq 0 -and $data.meshing -eq 0 -and $data.dirty -eq 0 -and $data.loaded -gt 0 -and $data.frames -gt 60) {
            $quiet++
            if ($quiet -ge 5) { break }
        } else {
            $quiet = 0
        }
    }
    if ($null -eq $data) { throw "the game wrote no state file" }
    $t = $data.timings
    Write-Host ("seed {0}: {1} chunks loaded, {2} quads, {3} builds" -f $Seed, $data.loaded, $data.quads, $t.builds)
    Write-Host ("timings (cumulative, ms): mesh {0}, light {1}, skin {2}, surface {3}" -f $t.mesh, $t.light, $t.skin, $t.surface)
} finally {
    if ($app -and -not $app.HasExited) { $app.Kill() }
}
