# Integration test for the pause menu: Escape opens it (the mouse is freed), clicking Resume locks the mouse again, and
# clicking Quit closes the app. The keys and the mouse are scripted through run_script, so real input does not interfere.
# Usage (from the repo root, after building):
#   pwsh tools/test_pause_menu.ps1 [-ExeDir build/bin/Debug]
# The window must be the default 1000 x 500 (do not set CGENGINE_FULLSCREEN). Exits 0 on success, 1 on any failed check.

param(
    [string]$ExeDir = "build/bin/Debug"
)

$ErrorActionPreference = "Stop"
$exeDir = (Resolve-Path $ExeDir).Path
$control = Join-Path $exeDir "cg_control"
$inbox = Join-Path $control "inbox"
$outbox = Join-Path $control "outbox"
$stateFile = Join-Path $control "voxel_state.json"
$failures = @()

function Check($name, [bool]$condition) {
    if ($condition) { Write-Host "PASS  $name" } else { Write-Host "FAIL  $name"; $script:failures += $name }
}

function Send-Command($id, $payload) {
    $tmp = Join-Path $inbox "$id.json.tmp"
    Set-Content -Path $tmp -Value $payload -Encoding utf8
    Rename-Item -Path $tmp -NewName "$id.json"
}

function Wait-Result($id, $timeoutSec = 20) {
    $path = Join-Path $outbox "$id.result.json"
    $deadline = (Get-Date).AddSeconds($timeoutSec)
    while (-not (Test-Path $path)) {
        if ((Get-Date) -gt $deadline) { return $null }
        Start-Sleep -Milliseconds 100
    }
    Start-Sleep -Milliseconds 100
    return Get-Content $path -Raw | ConvertFrom-Json
}

function Read-State() {
    for ($i = 0; $i -lt 10; $i++) {
        try { return Get-Content $stateFile -Raw | ConvertFrom-Json } catch { Start-Sleep -Milliseconds 100 }
    }
    return $null
}

function Wait-State($condition, $timeoutSec) {
    $deadline = (Get-Date).AddSeconds($timeoutSec)
    $state = Read-State
    while (-not ($state -and (& $condition $state)) -and (Get-Date) -lt $deadline) {
        Start-Sleep -Milliseconds 200
        $state = Read-State
    }
    return $state
}

# Scripts that replace the keyboard and mouse functions the game reads. Each one is a single pulse: Escape is down on the
# first call only, and the mouse button on its first call only, so each action happens once.
$pauseScript = @'
import cg_engine_bindings as cge
_escape = [0]
def _key(name):
    if name == "Escape":
        _escape[0] += 1
        return _escape[0] == 1
    return False
cge.key_down = _key
cge.mouse_down = lambda name: False
cge.mouse_delta = lambda: (0.0, 0.0)
'@
# Clicking at (500, 260): the Resume item in the centre of the 1000 x 500 window.
$resumeScript = @'
import cg_engine_bindings as cge
_press = [0]
def _button(name):
    if name == "Left":
        _press[0] += 1
        return _press[0] == 1
    return False
cge.key_down = lambda name: False
cge.mouse_down = _button
cge.mouse_position = lambda: (500, 260)
cge.mouse_delta = lambda: (0.0, 0.0)
'@
# Clicking at (500, 305): the Quit item.
$quitScript = @'
import cg_engine_bindings as cge
_press = [0]
def _button(name):
    if name == "Left":
        _press[0] += 1
        return _press[0] == 1
    return False
cge.key_down = lambda name: False
cge.mouse_down = _button
cge.mouse_position = lambda: (500, 305)
cge.mouse_delta = lambda: (0.0, 0.0)
'@

Remove-Item -Recurse -Force $control -ErrorAction SilentlyContinue
$env:CGENGINE_SCENE = "scenes/voxels.json"
$env:VOXEL_SEED = "1"
$app = Start-Process -FilePath (Join-Path $exeDir "main.exe") -WorkingDirectory $exeDir -PassThru
Remove-Item Env:CGENGINE_SCENE
Remove-Item Env:VOXEL_SEED

function Run-Script($id, $text) {
    $path = Join-Path $control "$id.py"
    Set-Content -Path $path -Value $text -Encoding utf8
    Send-Command $id ('{"command":"run_script","params":{"path":"' + ($path.Replace('\', '/')) + '"}}')
    return Wait-Result $id
}

try {
    $deadline = (Get-Date).AddSeconds(60)
    while (-not (Test-Path $stateFile)) {
        if ((Get-Date) -gt $deadline) { throw "the game did not write $stateFile" }
        Start-Sleep -Milliseconds 200
    }
    $loaded = Wait-State { param($s) $s.loaded -ge 169 -and $s.pending -eq 0 } 90
    Check "the world loads before the menu is tested" ($loaded -and $loaded.loaded -ge 169)

    # 1. Escape opens the menu: the game pauses and the mouse is freed.
    $ran = Run-Script "01_pause" $pauseScript
    Check "the pause script installs" ($ran -and $ran.ok)
    $paused = Wait-State { param($s) $s.paused -eq $true } 10
    Check "Escape pauses the game" ($paused -and $paused.paused -eq $true)
    Check "pausing frees the mouse" ($paused -and $paused.mouse_captured -eq $false)
    $frozen = Read-State
    Start-Sleep -Milliseconds 800
    $later = Read-State
    Check "the world stands still while paused (the time of day does not move)" ($frozen.time_of_day -eq $later.time_of_day)

    # 2. Clicking Resume closes the menu and locks the mouse again.
    $ran = Run-Script "02_resume" $resumeScript
    Check "the resume script installs" ($ran -and $ran.ok)
    $resumed = Wait-State { param($s) $s.paused -eq $false } 10
    Check "clicking Resume unpauses the game" ($resumed -and $resumed.paused -eq $false)
    Check "resuming locks the mouse again" ($resumed -and $resumed.mouse_captured -eq $true)

    # 3. Escape again, then clicking Quit closes the app.
    $ran = Run-Script "03_pause_again" $pauseScript
    $paused = Wait-State { param($s) $s.paused -eq $true } 10
    Check "Escape pauses the game again" ($paused -and $paused.paused -eq $true)
    $ran = Run-Script "04_quit" $quitScript
    Check "the quit script installs" ($ran -and $ran.ok)
    $deadline = (Get-Date).AddSeconds(15)
    while (-not $app.HasExited -and (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 200 }
    Check "clicking Quit closes the app" $app.HasExited
}
finally {
    if (-not $app.HasExited) { Stop-Process -Id $app.Id -Force }
}

if ($failures.Count -gt 0) {
    Write-Host "`n$($failures.Count) check(s) failed"
    exit 1
}
Write-Host "`nAll pause menu checks passed."
exit 0
