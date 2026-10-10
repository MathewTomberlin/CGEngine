# Integration test for the AI control channel and scene loader.
# Runs main.exe with a scene, drives it through cg_control/inbox, and checks the results.
# Usage (from the repo root, after building Debug):
#   pwsh tools/test_control_channel.ps1 [-ExeDir build/bin/Debug]
# Exits 0 on success, 1 on any failed check. Requires a GPU or software OpenGL (see CLAUDE.md).

param(
    [string]$ExeDir = "build/bin/Debug"
)

$ErrorActionPreference = "Stop"
$exeDir = (Resolve-Path $ExeDir).Path
$control = Join-Path $exeDir "cg_control"
$inbox = Join-Path $control "inbox"
$outbox = Join-Path $control "outbox"
$shot = Join-Path $exeDir "control_test_shot.png"
$failures = @()

function Check($name, [bool]$condition) {
    if ($condition) { Write-Host "PASS  $name" } else { Write-Host "FAIL  $name"; $script:failures += $name }
}

function Send-Command($id, $payload) {
    $tmp = Join-Path $inbox "$id.json.tmp"
    $payload | Set-Content -Path $tmp -Encoding utf8
    Rename-Item -Path $tmp -NewName "$id.json"
}

function Wait-Result($id, $timeoutSec = 20) {
    $path = Join-Path $outbox "$id.result.json"
    $deadline = (Get-Date).AddSeconds($timeoutSec)
    while (-not (Test-Path $path)) {
        if ((Get-Date) -gt $deadline) { return $null }
        Start-Sleep -Milliseconds 200
    }
    Start-Sleep -Milliseconds 100
    return Get-Content $path -Raw | ConvertFrom-Json
}

# Start clean and launch the engine with the example scene.
Remove-Item -Recurse -Force $control -ErrorAction SilentlyContinue
Remove-Item -Force $shot -ErrorAction SilentlyContinue
$env:CGENGINE_SCENE = "scenes/example.json"
$app = Start-Process -FilePath (Join-Path $exeDir "main.exe") -WorkingDirectory $exeDir -PassThru
Remove-Item Env:CGENGINE_SCENE

try {
    # Wait for the engine to create the inbox.
    $deadline = (Get-Date).AddSeconds(20)
    while (-not (Test-Path $inbox)) {
        if ((Get-Date) -gt $deadline) { throw "engine did not create $inbox" }
        Start-Sleep -Milliseconds 200
    }

    Send-Command "01_describe" '{"command":"describe_scene"}'
    Send-Command "02_set" '{"command":"set_transform","params":{"name":"cube_right","position":[5,1,-6],"rotation":[0,90,0]}}'
    Send-Command "03_stats" '{"command":"get_stats"}'
    Send-Command "04_bad" '{"command":"no_such_command"}'
    Send-Command "05_shot" ('{"command":"screenshot","params":{"path":"' + ($shot -replace '\\', '/') + '"}}')
    Send-Command "06_missing_scene" '{"command":"load_scene","params":{"path":"scenes/missing.json"}}'

    $describe = Wait-Result "01_describe"
    Check "describe_scene returns ok" ($describe -and $describe.ok)
    $names = @($describe.result.bodies | ForEach-Object { $_.name })
    Check "describe_scene lists 'cube' and 'cube_right'" (($names -contains "cube") -and ($names -contains "cube_right"))
    Check "describe_scene omits unnamed model parts" (-not ($names -contains ""))

    $set = Wait-Result "02_set"
    Check "set_transform applies position" ($set.ok -and [math]::Abs($set.result.position[0] - 5) -lt 0.001 -and [math]::Abs($set.result.position[2] + 6) -lt 0.001)

    $stats = Wait-Result "03_stats"
    Check "get_stats reports frames and bodies" ($stats.ok -and $stats.result.frames -gt 0 -and $stats.result.bodies -gt 0)

    $bad = Wait-Result "04_bad"
    Check "unknown command reports ok=false with error" ((-not $bad.ok) -and ($bad.error -match "unknown command"))

    $shotResult = Wait-Result "05_shot"
    Check "screenshot is queued" ($shotResult.ok -and $shotResult.result.queued)
    $shotDeadline = (Get-Date).AddSeconds(10)
    while (-not (Test-Path $shot) -and (Get-Date) -lt $shotDeadline) { Start-Sleep -Milliseconds 200 }
    Check "screenshot file is written" (Test-Path $shot)
    if (Test-Path $shot) {
        Add-Type -AssemblyName System.Drawing
        $img = [System.Drawing.Bitmap]::FromFile($shot)
        $sized = ($img.Width -gt 0 -and $img.Height -gt 0)
        $opaque = ($img.GetPixel(0, 0).A -eq 255)
        $img.Dispose()
        Check "screenshot is a valid image" $sized
        Check "screenshot pixels are opaque" $opaque
    }

    $missing = Wait-Result "06_missing_scene"
    Check "load_scene reports a missing file" ((-not $missing.ok) -and ($missing.error -match "cannot open"))

    Check "engine still running after commands" (-not $app.HasExited)
}
finally {
    if (-not $app.HasExited) { Stop-Process -Id $app.Id -Force }
}

if ($failures.Count -gt 0) {
    Write-Host "`n$($failures.Count) check(s) failed:" -ForegroundColor Red
    $failures | ForEach-Object { Write-Host "  - $_" }
    exit 1
}
Write-Host "`nAll control channel checks passed." -ForegroundColor Green
exit 0
