# Screenshots a level-of-detail seam of the voxel sample at a fixed world seed, so a change to the meshing can be compared before and
# after: the same seed gives the same terrain, and the player stands at the same place each time.
# Usage (from the repo root, after building):
#   pwsh tools/voxel_seam_shot.ps1 -Seed 1 [-Seam 3|12] [-Pitch -18] [-Yaw 90] [-Out path.png] [-ExeDir build/bin/Release]
# -Seam 3 is the seam between the near skin and the mid skin, -Seam 12 the seam where the far skins (stride 2) begin.
# The screenshot is written to -Out (default: the temp folder, named by seed and seam). Exits 0 when it is saved.

param(
    [Parameter(Mandatory = $true)][int]$Seed,
    [ValidateSet(3, 12)][int]$Seam = 3,
    [double]$Pitch = -18,
    [double]$Yaw = 90,
    [string]$Out = "",
    [string]$ExeDir = "build/bin/Release"
)

$ErrorActionPreference = "Stop"
# Unattended: no window is shown and no real keyboard, mouse or cursor input is read (see CGENGINE_UNATTENDED in src/Bindings/GameBindings.cpp).
$env:CGENGINE_UNATTENDED = "1"
. (Join-Path $PSScriptRoot "unattended.ps1")
$exeDir = New-TestCopy (Resolve-Path $ExeDir).Path "seam"
$control = Join-Path $exeDir "cg_control"
$inbox = Join-Path $control "inbox"
$outbox = Join-Path $control "outbox"
$stateFile = Join-Path $control "voxel_state.json"
if (-not $Out) { $Out = Join-Path $env:TEMP ("voxel_seam_{0}_ring{1}.png" -f $Seed, $Seam) }
$Out = [System.IO.Path]::GetFullPath($Out)

# The same world every run: the seed, noon, a fixed frame step, and no creatures or survival (the world check does the same).
$env:CGENGINE_SCENE = "scenes/voxels.json"
$env:VOXEL_SEED = "$Seed"
$env:VOXEL_FIXED_STEP = "0.0166667"
$env:VOXEL_TIME = "0.25"
$env:VOXEL_WALKERS = "0"; $env:VOXEL_CREATIVE = "1"
# The view for tools/voxel_seam_view.py, read when it runs inside the game (so it is set before the game starts).
$env:SEAM_CHUNKS = "$Seam"; $env:SEAM_PITCH = "$Pitch"; $env:SEAM_YAW = "$Yaw"
$app = Start-Unattended (Join-Path $exeDir "main.exe") $exeDir
Remove-Item Env:CGENGINE_SCENE, Env:VOXEL_SEED, Env:VOXEL_FIXED_STEP, Env:VOXEL_TIME, Env:VOXEL_WALKERS, Env:VOXEL_CREATIVE, Env:SEAM_CHUNKS, Env:SEAM_PITCH, Env:SEAM_YAW

function Read-State() {
    for ($i = 0; $i -lt 10; $i++) {
        try { return Get-Content $stateFile -Raw | ConvertFrom-Json } catch { Start-Sleep -Milliseconds 100 }
    }
    return $null
}

try {
    $deadline = (Get-Date).AddSeconds(60)
    while (-not (Test-Path $stateFile)) {
        if ((Get-Date) -gt $deadline) { throw "the game did not write $stateFile" }
        Start-Sleep -Milliseconds 200
    }
    # The spawn area loads and meshes first; the seam's chunks are in the view radius, so they are loaded by then.
    $deadline = (Get-Date).AddSeconds(120)
    do {
        Start-Sleep -Milliseconds 500
        $state = Read-State
    } until (($state -and $state.pending -eq 0 -and $state.meshing -eq 0 -and $state.loaded -ge 169) -or (Get-Date) -gt $deadline)
    if (-not $state -or $state.pending -ne 0) { throw "the chunks did not finish loading" }

    $script = (Resolve-Path (Join-Path $PSScriptRoot "voxel_seam_view.py")).Path -replace '\\', '/'
    $id = "01_seam"
    Set-Content -Path (Join-Path $inbox "$id.json.tmp") -Value ('{"command":"run_script","params":{"path":"' + $script + '"}}') -Encoding utf8
    Rename-Item (Join-Path $inbox "$id.json.tmp") "$id.json"
    $ran = $null
    $resultPath = Join-Path $outbox "$id.result.json"
    $deadline = (Get-Date).AddSeconds(60)
    while (-not (Test-Path $resultPath)) { if ((Get-Date) -gt $deadline) { throw "the seam script did not run" }; Start-Sleep -Milliseconds 200 }
    $ran = Get-Content $resultPath -Raw | ConvertFrom-Json
    if (-not $ran.ok) { throw "the seam script failed: $($ran.error)" }

    # Let the chunks around the new place load and mesh before the picture is taken.
    Start-Sleep -Seconds 2
    $deadline = (Get-Date).AddSeconds(120)
    do {
        Start-Sleep -Milliseconds 500
        $state = Read-State
    } until (($state -and $state.pending -eq 0 -and $state.meshing -eq 0 -and $state.dirty -eq 0) -or (Get-Date) -gt $deadline)

    if (Test-Path $Out) { Remove-Item $Out }
    $png = $Out.Replace('\', '/')
    Set-Content -Path (Join-Path $inbox "02_shot.json.tmp") -Value ('{"command":"screenshot","params":{"path":"' + $png + '"}}') -Encoding utf8
    Rename-Item (Join-Path $inbox "02_shot.json.tmp") "02_shot.json"
    $deadline = (Get-Date).AddSeconds(30)
    while (-not (Test-Path $Out)) { if ((Get-Date) -gt $deadline) { throw "no screenshot was saved" }; Start-Sleep -Milliseconds 300 }
    Start-Sleep -Milliseconds 300
    Write-Host "Saved $Out (seed $Seed, seam ring $Seam, pitch $Pitch, yaw $Yaw)"
} finally {
    if ($app -and -not $app.HasExited) { $app.Kill() }
}
