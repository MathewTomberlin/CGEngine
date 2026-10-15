# Reproduces the Debug crash of "timespeed 0.5" in the voxel sample (roadmap 23.4): runs the game unattended, types the command
# through the console (voxel_timespeed_repro.py), and reports whether the game survived a few seconds more. The game's output goes
# to logs in the test copy, so a crash's message is kept. Usage (from the repo root): pwsh tools/voxel_timespeed_repro.ps1 [-ExeDir build/bin/Debug]
param([string]$ExeDir = "build/bin/Debug", [string]$Command = "timespeed 0.5")
$ErrorActionPreference = "Stop"
$env:CGENGINE_UNATTENDED = "1"
. (Join-Path $PSScriptRoot "unattended.ps1")
$exeDir = New-TestCopy (Resolve-Path $ExeDir).Path "timespeed"
# The player's own settings (fullscreen, sensitivity, auto jump) are carried over, since a crash can depend on them.
if (Test-Path (Join-Path (Resolve-Path $ExeDir).Path "voxel_settings.json")) { Copy-Item -Force (Join-Path (Resolve-Path $ExeDir).Path "voxel_settings.json") $exeDir }
$control = Join-Path $exeDir "cg_control"
$inbox = Join-Path $control "inbox"
$env:CGENGINE_SCENE = "scenes/voxels.json"
$env:VOXEL_SEED = "1"
$env:VOXEL_REPRO_COMMAND = $Command
$outLog = Join-Path $exeDir "repro_out.txt"
$errLog = Join-Path $exeDir "repro_err.txt"
$app = Start-Unattended (Join-Path $exeDir "voxel_game.exe") $exeDir $outLog $errLog
Remove-Item Env:CGENGINE_SCENE, Env:VOXEL_SEED, Env:VOXEL_REPRO_COMMAND -ErrorAction SilentlyContinue
try {
    Start-Sleep -Seconds 25
    $script = (Resolve-Path (Join-Path $PSScriptRoot "voxel_timespeed_repro.py")).Path.Replace('\', '/')
    $id = "01_repro"
    Set-Content -Path (Join-Path $inbox "$id.json.tmp") -Value ('{"command":"run_script","params":{"path":"' + $script + '"}}') -Encoding utf8
    Rename-Item (Join-Path $inbox "$id.json.tmp") "$id.json"
    $resultPath = Join-Path (Join-Path $control "outbox") "$id.result.json"
    $deadline = (Get-Date).AddSeconds(60)
    while (-not (Test-Path $resultPath)) { if ((Get-Date) -gt $deadline) { throw "the repro script did not run" }; Start-Sleep -Milliseconds 200 }
    Start-Sleep -Seconds 40
    $repro = Get-Content (Join-Path $control "timespeed_repro.json") -Raw | ConvertFrom-Json
    Write-Host ("console said: " + ($repro.output -join " | "))
    if ($app.HasExited) { Write-Host ("CRASHED: the game exited with code " + $app.ExitCode) } else { Write-Host "the game is still running after the command" }
} finally {
    # The app is the cmd wrapper when logs are kept, so the game is ended with its whole tree.
    if ($app -and -not $app.HasExited) { & taskkill /T /F /PID $app.Id | Out-Null }
    Get-Process voxel_game -ErrorAction SilentlyContinue | Where-Object { $_.Path -like "*timespeed*" } | Stop-Process -Force
    if (Test-Path $errLog) { Get-Content $errLog -Tail 20 | ForEach-Object { Write-Host "stderr: $_" } }
}
