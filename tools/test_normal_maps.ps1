# Checks the normal maps of the standard shader (roadmap 35.1): a material with a normal map changes how a lit wall is shaded, and
# the same material at strength 0 looks like the wall with no map. Three scenes (resources/scenes/normal_map_*.json) are each
# launched once, and the game takes a screenshot through the control channel; tools/normal_map_check.py compares them.
# Usage (from the repo root, after building the Release configuration):
#   pwsh tools/test_normal_maps.ps1 [-ExeDir build/bin/Release]
# Exits 0 on success, 1 on any failed check. Needs a GPU or software OpenGL, like the other engine tests.

param(
    [string]$ExeDir = "build/bin/Release"
)

$ErrorActionPreference = "Stop"
$env:CGENGINE_UNATTENDED = "1"
. (Join-Path $PSScriptRoot "unattended.ps1")
$base = New-TestCopy (Resolve-Path $ExeDir).Path "normal_maps"
$failures = @()

function Check($name, [bool]$condition) {
    if ($condition) { Write-Host "PASS  $name" } else { Write-Host "FAIL  $name"; $script:failures += $name }
}

# Launches the engine on one scene, takes a screenshot, and closes the engine. Returns the PNG's path, or $null.
function Take-Shot($variant) {
    $control = Join-Path $base "cg_control"
    $inbox = Join-Path $control "inbox"
    $outbox = Join-Path $control "outbox"
    Remove-Item -Recurse -Force $control -ErrorAction SilentlyContinue
    $png = Join-Path $base "normal_$variant.png"
    Remove-Item -Force $png -ErrorAction SilentlyContinue
    $env:CGENGINE_SCENE = "scenes/normal_map_$variant.json"
    $app = Start-Unattended (Join-Path $base "main.exe") $base
    Remove-Item Env:CGENGINE_SCENE
    try {
        $deadline = (Get-Date).AddSeconds(20)
        while (-not (Test-Path $inbox)) {
            if ((Get-Date) -gt $deadline) { return $null }
            Start-Sleep -Milliseconds 200
        }
        # Let the scene load and a few frames draw before the shot.
        Start-Sleep -Seconds 3
        $tmp = Join-Path $inbox "shot.json.tmp"
        ('{"command":"screenshot","params":{"path":"' + ($png -replace '\\', '/') + '"}}') | Set-Content -Path $tmp -Encoding utf8
        Rename-Item -Path $tmp -NewName "shot.json"
        $deadline = (Get-Date).AddSeconds(30)
        while (-not (Test-Path $png) -or (Get-Item $png).Length -eq 0) {
            if ((Get-Date) -gt $deadline) { return $null }
            Start-Sleep -Milliseconds 250
        }
        Start-Sleep -Milliseconds 300
        return $png
    } finally {
        Stop-Process -Id $app.Id -Force -ErrorAction SilentlyContinue
    }
}

$shots = @{}
foreach ($variant in @("on", "zero", "off")) {
    $shots[$variant] = Take-Shot $variant
    Check "the $variant scene was screenshot" ($null -ne $shots[$variant])
}

if ($failures.Count -eq 0) {
    $python = (Get-Command python).Source
    & $python -I (Join-Path $PSScriptRoot "normal_map_check.py") $shots["on"] $shots["zero"] $shots["off"]
    Check "the normal map check passes (tools/normal_map_check.py)" ($LASTEXITCODE -eq 0)
}

if ($failures.Count -gt 0) {
    Write-Host "`n$($failures.Count) check(s) failed"
    exit 1
}
Write-Host "`nAll normal map checks passed."
