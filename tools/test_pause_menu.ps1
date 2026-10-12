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
# A click at (x, y), once.
function Click-Script($x, $y) {
    return @"
import cg_engine_bindings as cge
_press = [0]
def _button(name):
    if name == "Left":
        _press[0] += 1
        return _press[0] == 1
    return False
cge.key_down = lambda name: False
cge.mouse_down = _button
cge.mouse_position = lambda: ($x, $y)
cge.mouse_delta = lambda: (0.0, 0.0)
"@
}

# The menu of the 1000 x 500 window: the pause page has Resume (y 260), Settings (y 305) and Quit (y 351). The settings
# page has the sensitivity arrows (the up arrow at x 665, y 241), the time-of-day track (y 323, from x 350 to 650) and Back
# (y 377).
$settingsScript = Click-Script 500 305
$sensUpScript = Click-Script 665 241
$viewUpScript = Click-Script 680 273
$trackScript = Click-Script 500 323
$backScript = Click-Script 500 377
$quitScript = Click-Script 500 351

Remove-Item -Recurse -Force $control -ErrorAction SilentlyContinue
$env:CGENGINE_SCENE = "scenes/voxels.json"
$env:VOXEL_SEED = "1"
# The settings go to a file of the test's own, so the player's saved settings are not changed by the test.
$env:VOXEL_SETTINGS = "cg_control/test_settings.json"
$app = Start-Process -FilePath (Join-Path $exeDir "main.exe") -WorkingDirectory $exeDir -PassThru
Remove-Item Env:CGENGINE_SCENE
Remove-Item Env:VOXEL_SEED
Remove-Item Env:VOXEL_SETTINGS

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

    # 3. Escape again, Settings opens its page, Escape goes back to the pause page (the game stays paused), then Quit.
    $ran = Run-Script "03_pause_again" $pauseScript
    $paused = Wait-State { param($s) $s.paused -eq $true } 10
    Check "Escape pauses the game again" ($paused -and $paused.paused -eq $true)
    $ran = Run-Script "04_settings" $settingsScript
    $page = Wait-State { param($s) $s.menu -eq "settings" } 10
    Check "clicking Settings opens the settings page" ($page -and $page.menu -eq "settings")
    $ran = Run-Script "05_escape_from_settings" $pauseScript
    $back = Wait-State { param($s) $s.menu -eq "pause" } 10
    Check "Escape on the settings page goes back to the pause page, still paused" ($back -and $back.menu -eq "pause" -and $back.paused -eq $true)
    $ran = Run-Script "06_settings_again" $settingsScript
    $page = Wait-State { param($s) $s.menu -eq "settings" } 10
    Check "Settings opens again" ($page -and $page.menu -eq "settings")
    $ran = Run-Script "06b_time_track" $trackScript
    $noon = Wait-State { param($s) [math]::Abs($s.time_of_day - 0.5) -lt 0.02 } 10
    Check "clicking the middle of the time-of-day track sets the time to the middle of the day ($([math]::Round($noon.time_of_day, 3)))" ($noon -and [math]::Abs($noon.time_of_day - 0.5) -lt 0.02)
    Start-Sleep -Milliseconds 600
    Check "the time-of-day choice is saved to the settings file" ((Get-Content (Join-Path $exeDir "cg_control/test_settings.json") -Raw) -match "time_of_day`": 0\.5")
    $ran = Run-Script "07_sensitivity_up" $sensUpScript
    $raised = Wait-State { param($s) $s.sensitivity -eq 125 } 10
    Check "the up arrow raises the mouse sensitivity from 100% to 125%" ($raised -and $raised.sensitivity -eq 125)
    Check "the sensitivity is saved to the settings file" ((Test-Path (Join-Path $exeDir "cg_control/test_settings.json")) -and ((Get-Content (Join-Path $exeDir "cg_control/test_settings.json") -Raw) -match "125"))
    # The view distance starts at the middle step (14 chunks); the right arrow takes it to 18, and the file keeps it.
    $ran = Run-Script "07c_view_up" $viewUpScript
    $viewed = Wait-State { param($s) $s.view_radius -eq 18 } 10
    Check "the right arrow raises the view distance from 14 to 18 chunks" ($viewed -and $viewed.view_radius -eq 18)
    Check "the view distance is saved to the settings file" ((Get-Content (Join-Path $exeDir "cg_control/test_settings.json") -Raw) -match '"view_radius":\s*18')
    $ran = Run-Script "08_back" $backScript
    $back = Wait-State { param($s) $s.menu -eq "pause" } 10
    Check "Back returns to the pause page" ($back -and $back.menu -eq "pause")
    $ran = Run-Script "09_quit" $quitScript
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
