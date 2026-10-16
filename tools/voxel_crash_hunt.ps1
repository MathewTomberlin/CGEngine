# Hunts the crash of "timespeed 0.5" in the voxel sample (roadmap 23.4): runs the game under the Windows debugger (cdb) for each
# world seed, types the command through the console (voxel_timespeed_repro.py), and keeps the debugger's stack if the game crashes.
# The Release build with debug information (build_sym) is the one to use, so the stack has function names.
# Usage (from the repo root): pwsh tools/voxel_crash_hunt.ps1 -ExeDir build_sym/bin/Release -Seeds 1,2,3 -Settings build/bin/Release/voxel_settings.json
param(
    [string]$ExeDir = "build_sym/bin/Release",
    [string]$Seeds = "1,2,3",
    [string]$Settings = "",
    [string]$Cdb = "C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\cdb.exe"
)
$ErrorActionPreference = "Stop"
if (-not $Visible) { $env:CGENGINE_UNATTENDED = "1" }
. (Join-Path $PSScriptRoot "unattended.ps1")
$source = (Resolve-Path $ExeDir).Path
$results = @()
foreach ($seed in ($Seeds -split "," | ForEach-Object { [int]$_ })) {
    $exeDir = New-TestCopy $source "crash_$seed"
    if ($Settings -and (Test-Path $Settings)) { Copy-Item -Force $Settings (Join-Path $exeDir "voxel_settings.json") }
    $control = Join-Path $exeDir "cg_control"
    $inbox = Join-Path $control "inbox"
    $log = Join-Path $exeDir "cdb_seed_$seed.txt"
    $env:CGENGINE_SCENE = "scenes/voxels.json"
    $env:VOXEL_SEED = "$seed"
    # cdb breaks on access violations and prints the stack, then quits. The game is its child.
    $app = Start-Unattended $Cdb $exeDir $null $null ("-logo `"$log`" -c `"sxe av; g; kb 40; q`" `"" + (Join-Path $exeDir "voxel_game.exe") + "`"")
    Remove-Item Env:CGENGINE_SCENE, Env:VOXEL_SEED -ErrorAction SilentlyContinue
    try {
        Start-Sleep -Seconds 25
        $script = (Resolve-Path (Join-Path $PSScriptRoot "voxel_timespeed_repro.py")).Path.Replace('\', '/')
        $id = "01_repro"
        Set-Content -Path (Join-Path $inbox "$id.json.tmp") -Value ('{"command":"run_script","params":{"path":"' + $script + '"}}') -Encoding utf8
        Rename-Item (Join-Path $inbox "$id.json.tmp") "$id.json"
        $deadline = (Get-Date).AddSeconds(60)
        $resultPath = Join-Path (Join-Path $control "outbox") "$id.result.json"
        while (-not (Test-Path $resultPath)) { if ((Get-Date) -gt $deadline) { break }; Start-Sleep -Milliseconds 200 }
        Start-Sleep -Seconds 40
    } finally {
        $crashed = (Test-Path $log) -and (Select-String -Path $log -Pattern "Access violation" -Quiet)
        if (-not $app.HasExited) { & taskkill /T /F /PID $app.Id | Out-Null }
        Get-Process voxel_game -ErrorAction SilentlyContinue | Where-Object { $_.Path -like "*crash_*" } | Stop-Process -Force
    }
    $results += [pscustomobject]@{ Seed = $seed; Crashed = $crashed; Log = $log }
    Write-Host ("seed {0}: {1}" -f $seed, $(if ($crashed) { "CRASHED, stack in $log" } else { "survived" }))
}
