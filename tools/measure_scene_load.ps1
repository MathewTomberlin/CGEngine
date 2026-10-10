# Measures scene load time through the control channel (the loadMs field of load_scene results).
# Each scenario runs in a fresh engine process, so its first load is cold and later loads reuse the asset cache.
# Usage (from the repo root, after building):
#   pwsh tools/measure_scene_load.ps1 [-ExeDir build/bin/Debug] [-Repeats 3]
# Prints one line per load. Needs a GPU or software OpenGL, like test_control_channel.ps1.

param(
    [string]$ExeDir = "build/bin/Debug",
    [int]$Repeats = 3
)

$ErrorActionPreference = "Stop"
$exeDir = (Resolve-Path $ExeDir).Path
$control = Join-Path $exeDir "cg_control"
$inbox = Join-Path $control "inbox"
$outbox = Join-Path $control "outbox"

$scenarios = @(
    @{ name = "fbx model (animated)"; loads = @("scenes/animated.json", "scenes/animated.json", "scenes/animated.json") },
    @{ name = "obj model (example)";  loads = @("scenes/example.json", "scenes/example.json", "scenes/example.json") },
    @{ name = "primitives";           loads = @("scenes/primitives.json", "scenes/primitives.json", "scenes/primitives.json") },
    @{ name = "dungeon (textures)";   loads = @("scenes/dungeon.json", "scenes/dungeon.json", "scenes/dungeon.json") }
)

function Send-Command($id, $payload) {
    $tmp = Join-Path $inbox "$id.json.tmp"
    $payload | Set-Content -Path $tmp -Encoding utf8
    Rename-Item -Path $tmp -NewName "$id.json"
}

function Wait-Result($id, $timeoutSec = 60) {
    $path = Join-Path $outbox "$id.result.json"
    $deadline = (Get-Date).AddSeconds($timeoutSec)
    while (-not (Test-Path $path)) {
        if ((Get-Date) -gt $deadline) { throw "no result for $id" }
        Start-Sleep -Milliseconds 100
    }
    Start-Sleep -Milliseconds 50
    return Get-Content $path -Raw | ConvertFrom-Json
}

$rows = @()
foreach ($scenario in $scenarios) {
    for ($run = 1; $run -le $Repeats; $run++) {
        Remove-Item -Recurse -Force $control -ErrorAction SilentlyContinue
        $app = Start-Process -FilePath (Join-Path $exeDir "main.exe") -WorkingDirectory $exeDir -PassThru
        try {
            $deadline = (Get-Date).AddSeconds(30)
            while (-not (Test-Path $inbox)) {
                if ((Get-Date) -gt $deadline) { throw "engine did not create $inbox" }
                Start-Sleep -Milliseconds 100
            }
            $n = 0
            foreach ($path in $scenario.loads) {
                $n++
                $id = "{0:D2}_load" -f $n
                Send-Command $id ('{"command":"load_scene","params":{"path":"' + $path + '"}}')
                $r = Wait-Result $id
                if (-not $r.ok) { throw "load_scene $path failed: $($r.error)" }
                $rows += [pscustomobject]@{
                    scenario = $scenario.name
                    run      = $run
                    load     = if ($n -eq 1) { "cold" } else { "warm $($n - 1)" }
                    loadMs   = [math]::Round($r.result.loadMs, 2)
                    bodies   = $r.result.bodies
                }
            }
        }
        finally {
            Stop-Process -Id $app.Id -Force -ErrorAction SilentlyContinue
        }
    }
}

$rows | Format-Table -AutoSize
