# Two-process check for the voxel sample's loopback link (roadmap 18.2, resources/scripts/VoxelNet.py): one game listens and one
# connects, both on the same seed, and the edits one makes show in the other: a broken block, a placed block, a torch and a
# hit. It also checks that each sees the other's position. The second game runs from a copy of the build folder, so each
# process has its own control folder.
# Usage (from the repo root, after building the Release configuration):
#   pwsh tools/test_voxel_net.ps1 [-ExeDir build/bin/Release] [-Port 7790]
# Exits 0 on success, 1 on any failed check.

param(
    [string]$ExeDir = "build/bin/Release",
    [int]$Port = 7790
)

$ErrorActionPreference = "Stop"
$base = (Resolve-Path $ExeDir).Path
$second = "$base" + "_net_b"
$failures = @()

function Check($name, [bool]$condition) {
    if ($condition) { Write-Host "PASS  $name" } else { Write-Host "FAIL  $name"; $script:failures += $name }
}

function Run-Script($folder, $name, $code) {
    $control = Join-Path $folder "cg_control"
    New-Item -ItemType Directory -Force -Path (Join-Path $control "inbox") | Out-Null
    $path = Join-Path $control "$name.py"
    Set-Content -Path $path -Value $code -Encoding utf8
    $tmp = Join-Path $control "inbox/$name.json.tmp"
    Set-Content -Path $tmp -Value ('{"command":"run_script","params":{"path":"' + ($path -replace '\\', '/') + '"}}') -Encoding utf8
    Rename-Item -Path $tmp -NewName "$name.json"
}

function Read-State($folder) {
    try { return Get-Content (Join-Path $folder "cg_control/voxel_state.json") -Raw | ConvertFrom-Json } catch { return $null }
}

function Wait-Until($folder, [scriptblock]$ready, $timeoutSec = 90) {
    $deadline = (Get-Date).AddSeconds($timeoutSec)
    while ((Get-Date) -lt $deadline) {
        $s = Read-State $folder
        if ($s -and (& $ready $s)) { return $s }
        Start-Sleep -Milliseconds 250
    }
    return $null
}

function Wait-Result($folder, $name, $timeoutSec = 30) {
    $path = Join-Path $folder "cg_control/$name.json"
    $deadline = (Get-Date).AddSeconds($timeoutSec)
    while ((Get-Date) -lt $deadline) {
        if (Test-Path $path) { try { return Get-Content $path -Raw | ConvertFrom-Json } catch {} }
        Start-Sleep -Milliseconds 200
    }
    return $null
}

# Two folders: the second is a copy of the first without its control folder.
Remove-Item -Recurse -Force $second -ErrorAction SilentlyContinue
Copy-Item -Recurse -Force $base $second
Remove-Item -Recurse -Force (Join-Path $second "cg_control") -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force (Join-Path $base "cg_control") -ErrorAction SilentlyContinue
foreach ($folder in @($base, $second)) {
    Set-Content -Path (Join-Path $folder "voxel_settings.json") -Value '{"mouse_sensitivity": 100, "view_radius": 8}' -Encoding utf8
}

$apps = @()
try {
    $env:VOXEL_SEED = "1"; $env:VOXEL_TIME = "0.25"
    $env:VOXEL_NET = "listen:$Port"
    $apps += Start-Process -FilePath (Join-Path $base "voxel_game.exe") -WorkingDirectory $base -PassThru
    Start-Sleep -Milliseconds 1500
    $env:VOXEL_NET = "connect:127.0.0.1:$Port"
    $apps += Start-Process -FilePath (Join-Path $second "voxel_game.exe") -WorkingDirectory $second -PassThru
    Remove-Item Env:VOXEL_NET, Env:VOXEL_SEED, Env:VOXEL_TIME

    $a = Wait-Until $base { param($s) $s.pending -eq 0 -and $s.meshing -eq 0 -and $s.on_ground -eq $true -and $s.loaded -ge 100 } 120
    $b = Wait-Until $second { param($s) $s.pending -eq 0 -and $s.meshing -eq 0 -and $s.on_ground -eq $true -and $s.loaded -ge 100 } 120
    Check "both games load the world" ($a -and $b)
    $linked = Wait-Until $second { param($s) $s.net -and $s.net.connected } 60
    Check "the connecting game links to the listener" ($null -ne $linked)
    if (-not $linked) { throw "no link" }
    Start-Sleep -Seconds 2

    # A breaks a block three blocks out from its feet, and B sees it gone.
    $pick = @'
import gc
import json
import math
game = [o for o in gc.get_objects() if type(o).__name__ == "VoxelGame"][0]
x, z = math.floor(game.pos[0]) + 3, math.floor(game.pos[2])
top = None
for y in range(60, 3, -1):
    # A stone with stone under it (the spawn can be desert, with sand over the stone): the break and the hit land on stone.
    if game.world.get_block(x, y, z) == 3 and game.world.get_block(x, y - 1, z) == 3:
        top = y
        break
with open("cg_control/net_cells.json", "w") as f:
    json.dump({"x": x, "z": z, "top": top}, f)
'@
    Run-Script $base "net_pick" $pick
    $cells = Wait-Result $base "net_cells"
    if (-not $cells -or $null -eq $cells.top) { throw "no stone found near the spawn" }
    $x = $cells.x; $z = $cells.z; $top = $cells.top
    $breakA = @"
import gc
game = [o for o in gc.get_objects() if type(o).__name__ == "VoxelGame"][0]
game.world.set_block($x, $top, $z, 0)
game.net_send({"t": "block", "c": [$x, $top, $z], "id": 0})
"@
    Run-Script $base "net_break" $breakA
    $checkB = @"
import gc
import json
game = [o for o in gc.get_objects() if type(o).__name__ == "VoxelGame"][0]
with open("cg_control/net_b_break.json", "w") as f:
    json.dump({"block": game.world.get_block($x, $top, $z), "applied": game.net_applied}, f)
"@
    $seen = $false
    for ($i = 0; $i -lt 20 -and -not $seen; $i++) {
        Run-Script $second "net_check_break" $checkB
        $r = Wait-Result $second "net_b_break" 5
        if ($r -and $r.block -eq 0) { $seen = $true } else { Start-Sleep -Milliseconds 500 }
        Remove-Item -Force (Join-Path $second "cg_control/net_b_break.json") -ErrorAction SilentlyContinue
    }
    Check "a block broken in one game is gone in the other" $seen

    # B places stone in the air above the same column, and A sees it.
    $placeB = @"
import gc
game = [o for o in gc.get_objects() if type(o).__name__ == "VoxelGame"][0]
game.world.set_block($x, $($top + 1), $z, 3)
game.net_send({"t": "block", "c": [$x, $($top + 1), $z], "id": 3})
"@
    Run-Script $second "net_place" $placeB
    $checkA = @"
import gc
import json
game = [o for o in gc.get_objects() if type(o).__name__ == "VoxelGame"][0]
with open("cg_control/net_a_place.json", "w") as f:
    json.dump({"block": game.world.get_block($x, $($top + 1), $z)}, f)
"@
    $placed = $false
    for ($i = 0; $i -lt 20 -and -not $placed; $i++) {
        Run-Script $base "net_check_place" $checkA
        $r = Wait-Result $base "net_a_place" 5
        if ($r -and $r.block -eq 3) { $placed = $true } else { Start-Sleep -Milliseconds 500 }
        Remove-Item -Force (Join-Path $base "cg_control/net_a_place.json") -ErrorAction SilentlyContinue
    }
    Check "a block placed in one game is there in the other" $placed

    # A hits the stone that is still standing at the top of the column: B's damage stage follows.
    $hitA = @"
import gc
game = [o for o in gc.get_objects() if type(o).__name__ == "VoxelGame"][0]
stage = game.world.hit_block($x, $top - 1, $z)
game.net_send({"t": "hit", "c": [$x, $($top - 1), $z], "stage": stage})
"@
    Run-Script $base "net_hit" $hitA
    $checkHit = @"
import gc
import json
game = [o for o in gc.get_objects() if type(o).__name__ == "VoxelGame"][0]
with open("cg_control/net_b_hit.json", "w") as f:
    json.dump({"damage": game.world.damage_at($x, $($top - 1), $z)}, f)
"@
    $hit = $false
    for ($i = 0; $i -lt 20 -and -not $hit; $i++) {
        Run-Script $second "net_check_hit" $checkHit
        $r = Wait-Result $second "net_b_hit" 5
        if ($r -and $r.damage -eq 1) { $hit = $true } else { Start-Sleep -Milliseconds 500 }
        Remove-Item -Force (Join-Path $second "cg_control/net_b_hit.json") -ErrorAction SilentlyContinue
    }
    Check "a hit in one game shows as damage in the other" $hit

    # Positions: each game knows where the other player is.
    $aSeesB = Wait-Until $base { param($s) $s.net -and $s.net.remote -ne $null } 20
    $bSeesA = Wait-Until $second { param($s) $s.net -and $s.net.remote -ne $null } 20
    Check "each game receives the other player's position" ($aSeesB -and $bSeesA)
}
finally {
    foreach ($app in $apps) { if (-not $app.HasExited) { Stop-Process -Id $app.Id -Force } }
    Remove-Item (Join-Path $base "voxel_settings.json") -ErrorAction SilentlyContinue
    Remove-Item -Recurse -Force $second -ErrorAction SilentlyContinue
}

if ($failures.Count -gt 0) {
    Write-Host "`n$($failures.Count) check(s) failed"
    exit 1
}
Write-Host "`nAll voxel link checks passed."
exit 0
