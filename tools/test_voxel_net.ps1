# Two-process check for the voxel sample's loopback link (roadmap 18.2 and 18.3, resources/scripts/VoxelNet.py). One game listens
# and is the host: it owns the world. The other connects and asks the host for its edits, as a click does: a local edit, then a
# request. The checks: a client's hit, break, placement and torch show on the host and back on the client; a placement the host
# refuses is undone on the client; a hit on the host shows on the client; a client that joins later is sent every edit.
# The second game runs from a copy of the build folder, so each process has its own control folder.
# Usage (from the repo root, after building the Release configuration):
#   pwsh tools/test_voxel_net.ps1 [-ExeDir build/bin/Release] [-Port 7790] [-HeadlessHost]
# -HeadlessHost starts the listening game with CGENGINE_HEADLESS=1 (roadmap 31.2): no window, no draws, the same checks.
# Exits 0 on success, 1 on any failed check.

param(
    [string]$ExeDir = "build/bin/Release",
    [int]$Port = 7790,
    [switch]$HeadlessHost
)

$ErrorActionPreference = "Stop"
# Unattended: no window is shown and no real keyboard, mouse or cursor input is read (see CGENGINE_UNATTENDED in src/Bindings/GameBindings.cpp).
$env:CGENGINE_UNATTENDED = "1"
. (Join-Path $PSScriptRoot "unattended.ps1")
$base = New-TestCopy (Resolve-Path $ExeDir).Path "voxel_net"
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

# Runs Python in the game: the body sees `game`, the sample's VoxelGame.
function Send-Code($folder, $name, $body) {
    Run-Script $folder $name ("import gc`ngame = [o for o in gc.get_objects() if type(o).__name__ == 'VoxelGame'][0]`n" + $body)
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

# The block, damage stage and torch of a cell in one game, as the game sees them now.
function Read-Cell($folder, $cell) {
    $tag = "cell_" + [guid]::NewGuid().ToString("N").Substring(0, 10)
    $c = "($($cell[0]), $($cell[1]), $($cell[2]))"
    Send-Code $folder $tag ("import json`nc = $c`nwith open('cg_control/$tag.json', 'w') as f:`n" +
        "    json.dump({'block': game.world.get_block(*c), 'damage': game.world.damage_at(*c), 'prop': game.world.prop_at(*c)}, f)`n")
    $r = Wait-Result $folder $tag 10
    Remove-Item -Force (Join-Path $folder "cg_control/$tag.json") -ErrorAction SilentlyContinue
    return $r
}

# Sends a control command to a game's inbox, as an agent does, and returns its result file (or $null).
function Send-Command($folder, $id, $command, $params) {
    $inbox = Join-Path $folder "cg_control/inbox"
    New-Item -ItemType Directory -Force -Path $inbox | Out-Null
    $payload = @{ command = $command; params = $params } | ConvertTo-Json -Depth 5 -Compress
    $tmp = Join-Path $inbox "$id.json.tmp"
    Set-Content -Path $tmp -Value $payload -Encoding utf8
    Rename-Item -Path $tmp -NewName "$id.json"
    $result = Join-Path $folder "cg_control/outbox/$id.result.json"
    $deadline = (Get-Date).AddSeconds(20)
    while ((Get-Date) -lt $deadline) {
        if (Test-Path $result) { try { return Get-Content $result -Raw | ConvertFrom-Json } catch {} }
        Start-Sleep -Milliseconds 200
    }
    return $null
}

# Polls a cell in one game until the condition holds on it, and returns its state (or $null).
function Wait-Cell($folder, $cell, [scriptblock]$ready, $timeoutSec = 20) {
    $deadline = (Get-Date).AddSeconds($timeoutSec)
    while ((Get-Date) -lt $deadline) {
        $r = Read-Cell $folder $cell
        if ($r -and (& $ready $r)) { return $r }
        Start-Sleep -Milliseconds 300
    }
    return $null
}

# A click on a block in a client: the local hit, then the request the host applies.
function Client-Hit($folder, $cell, $name) {
    $c = "[$($cell[0]), $($cell[1]), $($cell[2])]"
    Send-Code $folder $name ("stage = game.world.hit_block($($cell[0]), $($cell[1]), $($cell[2]))`n" +
        "game.send_request({'op': 'hit', 'c': $c, 'swings': 1})`n")
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
    # No creatures and survival mode: a biter's bite sends the player back to the spawn, which moves the cells the checks use.
    $env:VOXEL_SEED = "1"; $env:VOXEL_TIME = "0.25"; $env:VOXEL_WALKERS = "0"; $env:VOXEL_CREATIVE = "0"
    $env:VOXEL_NET = "listen:$Port"
    if ($HeadlessHost) { $env:CGENGINE_HEADLESS = "1" }
    $apps += Start-Unattended (Join-Path $base "voxel_game.exe") $base
    Remove-Item Env:CGENGINE_HEADLESS -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 1500
    $env:VOXEL_NET = "connect:127.0.0.1:$Port"
    $apps += Start-Unattended (Join-Path $second "voxel_game.exe") $second
    Remove-Item Env:VOXEL_NET, Env:VOXEL_SEED, Env:VOXEL_TIME, Env:VOXEL_WALKERS, Env:VOXEL_CREATIVE

    $a = Wait-Until $base { param($s) $s.pending -eq 0 -and $s.meshing -eq 0 -and $s.on_ground -eq $true -and $s.loaded -ge 100 } 120
    $b = Wait-Until $second { param($s) $s.pending -eq 0 -and $s.meshing -eq 0 -and $s.on_ground -eq $true -and $s.loaded -ge 100 } 120
    Check "both games load the world" ($a -and $b)
    Check "the listening game runs as asked (headless: $([bool]$HeadlessHost))" ($a -and ([bool]$a.headless -eq [bool]$HeadlessHost))
    $linked = Wait-Until $second { param($s) $s.net -and $s.net.connected } 60
    Check "the connecting game links to the listener" ($null -ne $linked)
    if (-not $linked) { throw "no link" }
    Start-Sleep -Seconds 2

    # Stone at the top of a column three blocks out from the host's feet, with stone under it: the cells the checks use.
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
    json.dump({"x": x, "z": z, "top": top, "pos": list(game.pos)}, f)
'@
    Run-Script $base "net_pick" $pick
    $cells = Wait-Result $base "net_cells"
    if (-not $cells -or $null -eq $cells.top) { throw "no stone found near the spawn" }
    $x = $cells.x; $z = $cells.z; $top = $cells.top
    $stone = @($x, $top, $z)
    $below = @($x, ($top - 1), $z)
    $above = @($x, ($top + 1), $z)

    # The client breaks the stone. Stone takes three hits: the first leaves damage 1 on both games.
    Client-Hit $second $stone "net_b_hit1"
    $a1 = Wait-Cell $base $stone { param($r) $r.damage -eq 1 }
    $b1 = Wait-Cell $second $stone { param($r) $r.damage -eq 1 }
    Check "a client's hit shows as damage on the host and on the client" ($a1 -and $b1)

    Client-Hit $second $stone "net_b_hit2"
    $null = Wait-Cell $base $stone { param($r) $r.damage -eq 2 }
    Client-Hit $second $stone "net_b_hit3"
    $a2 = Wait-Cell $base $stone { param($r) $r.block -eq 0 -and $r.damage -eq 0 }
    $b2 = Wait-Cell $second $stone { param($r) $r.block -eq 0 -and $r.damage -eq 0 }
    Check "a client's third hit breaks the block on the host and on the client" ($a2 -and $b2)

    # The host hits the stone under the hole: the client shows the damage.
    Send-Code $base "net_a_hit" ("game.world.hit_block($($below[0]), $($below[1]), $($below[2]))`n")
    $b3 = Wait-Cell $second $below { param($r) $r.block -eq 3 -and $r.damage -eq 1 }
    Check "a hit on the host shows as damage on the client" ($null -ne $b3)

    # The client places stone on the damaged stone, as a click would (a local edit, then the request). The host refuses it, so
    # the client must go back to the host's stone with its damage.
    Send-Code $second "net_b_refused" ("game.world.set_block($($below[0]), $($below[1]), $($below[2]), 4)`n" +
        "game.send_request({'op': 'place', 'c': [$($below[0]), $($below[1]), $($below[2])], 'id': 4})`n")
    $b4 = Wait-Cell $second $below { param($r) $r.block -eq 3 -and $r.damage -eq 1 }
    $a4 = Read-Cell $base $below
    Check "a placement the host refuses is undone on the client" (($null -ne $b4) -and $a4 -and $a4.block -eq 3)

    # The client places stone in the air above the hole: the host and the client both have it.
    # The cell above the hole is dirt on this seed, and a placement must not replace dirt, so the host clears it first (a host
    # edit, sent to the client). Both games must read it as air before the client places.
    Send-Code $base "net_a_clear" ("game.world.set_block($($above[0]), $($above[1]), $($above[2]), 0)`n")
    $preA = Wait-Cell $base $above { param($r) $r.block -eq 0 }
    $preB = Wait-Cell $second $above { param($r) $r.block -eq 0 }
    Check "the cell above the hole is cleared on the host and on the client" ($preA -and $preB)
    Send-Code $second "net_b_place" ("game.world.set_block($($above[0]), $($above[1]), $($above[2]), 3)`n" +
        "game.send_request({'op': 'place', 'c': [$($above[0]), $($above[1]), $($above[2])], 'id': 3})`n")
    $a5 = Wait-Cell $base $above { param($r) $r.block -eq 3 }
    $b5 = Wait-Cell $second $above { param($r) $r.block -eq 3 }
    if (-not ($a5 -and $b5)) {
        $hostNow = Read-Cell $base $above
        $clientNow = Read-Cell $second $above
        Write-Host "info  after the placement: the host's cell above is block $($hostNow.block), the client's is $($clientNow.block)"
    }
    Check "a block placed by a client is on the host and on the client" ($a5 -and $b5)

    # The client leans a torch on the stone below the hole, from the hole's cell (the cell is air now).
    $down = 3   # the prop direction toward -y (VoxelGen.DIRECTIONS)
    Send-Code $second "net_b_torch" ("game.world.set_prop($($stone[0]), $($stone[1]), $($stone[2]), $down)`n" +
        "game.send_request({'op': 'prop', 'c': [$($stone[0]), $($stone[1]), $($stone[2])], 's': [$($below[0]), $($below[1]), $($below[2])], 'dir': $down})`n")
    $a6 = Wait-Cell $base $stone { param($r) $r.prop -eq $down }
    $b6 = Wait-Cell $second $stone { param($r) $r.prop -eq $down }
    Check "a torch placed by a client is on the host and on the client" ($a6 -and $b6)

    # A client that joins later is sent every edit. The client is restarted and must see the edits made while it was away.
    Stop-Process -Id $apps[1].Id -Force
    Start-Sleep -Milliseconds 1000
    Remove-Item -Force (Join-Path $second "cg_control/voxel_state.json") -ErrorAction SilentlyContinue
    $env:VOXEL_SEED = "1"; $env:VOXEL_TIME = "0.25"; $env:VOXEL_WALKERS = "0"; $env:VOXEL_CREATIVE = "0"
    $env:VOXEL_NET = "connect:127.0.0.1:$Port"
    $apps[1] = Start-Unattended (Join-Path $second "voxel_game.exe") $second
    Remove-Item Env:VOXEL_NET, Env:VOXEL_SEED, Env:VOXEL_TIME, Env:VOXEL_WALKERS, Env:VOXEL_CREATIVE
    $b = Wait-Until $second { param($s) $s.pending -eq 0 -and $s.meshing -eq 0 -and $s.on_ground -eq $true -and $s.loaded -ge 100 } 120
    $linked = Wait-Until $second { param($s) $s.net -and $s.net.connected } 60
    $late = $null
    if ($b -and $linked) { $late = Wait-Cell $second $stone { param($r) $r.prop -eq $down } 40 }
    $lateBelow = if ($late) { Wait-Cell $second $below { param($r) $r.block -eq 3 -and $r.damage -eq 1 } 20 } else { $null }
    $lateAbove = if ($late) { Wait-Cell $second $above { param($r) $r.block -eq 3 } 20 } else { $null }
    Check "a client that joins later is sent every edit" ($late -and $lateBelow -and $lateAbove)

    # The late client takes its torch back, and the host removes it.
    Send-Code $second "net_b_unprop" ("game.world.remove_prop($($stone[0]), $($stone[1]), $($stone[2]))`n" +
        "game.send_request({'op': 'unprop', 'c': [$($stone[0]), $($stone[1]), $($stone[2])]})`n")
    $a7 = Wait-Cell $base $stone { param($r) $r.prop -eq -1 }
    Check "a torch taken back by a client is removed on the host" ($null -ne $a7)

    # The same edit as a control command (voxel_edit, registered by the game): a hit from the client's inbox reaches the host,
    # and a bad op is reported as an error with the game still running.
    $cmd = Send-Command $second "net_voxel_edit" "voxel_edit" @{ op = "hit"; c = $above; swings = 1 }
    $a8 = Wait-Cell $base $above { param($r) $r.damage -eq 1 }
    Check "a voxel_edit control command from a client reaches the host" (($null -ne $cmd) -and ($cmd.ok -eq $true) -and ($null -ne $a8))
    $bad = Send-Command $second "net_voxel_bad" "voxel_edit" @{ op = "dig"; c = $above }
    Check "a bad voxel_edit is reported as an error" (($null -ne $bad) -and ($bad.ok -eq $false) -and ($bad.error -match "op must be"))

    # Positions: each game knows where the other player is.
    $aSeesB = Wait-Until $base { param($s) $s.net -and $s.net.remote -ne $null } 20
    $bSeesA = Wait-Until $second { param($s) $s.net -and $s.net.remote -ne $null } 20
    Check "each game receives the other player's position" ($aSeesB -and $bSeesA)

    # Neither player died: a death sends the player back to the spawn, which moves the cells the checks use.
    $aEnd = Read-State $base
    $bEnd = Read-State $second
    Check "neither player died during the test" (($null -ne $aEnd) -and ($null -ne $bEnd) -and ($aEnd.deaths -eq 0) -and ($bEnd.deaths -eq 0))
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
