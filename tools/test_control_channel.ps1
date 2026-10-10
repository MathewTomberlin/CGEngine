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
    Send-Command "07_primitives" '{"command":"load_scene","params":{"path":"scenes/primitives.json"}}'
    Send-Command "08_describe_primitives" '{"command":"describe_scene"}'
    $invalid = @{
        "09_syntax"   = @{ file = "scenes/invalid/syntax_error.json";      error = "invalid JSON" }
        "10_material" = @{ file = "scenes/invalid/unknown_material.json";  error = "unknown material" }
        "11_primitive"= @{ file = "scenes/invalid/unknown_primitive.json"; error = "unknown primitive" }
        "12_model"    = @{ file = "scenes/invalid/missing_model.json";     error = "failed to load model" }
        "13_version"  = @{ file = "scenes/invalid/bad_version.json";       error = "version" }
        "14_vector"   = @{ file = "scenes/invalid/bad_vector.json";        error = "3 numbers" }
        "22_anim"     = @{ file = "scenes/invalid/unknown_animation.json";  error = "unknown animation" }
    }
    foreach ($id in ($invalid.Keys | Sort-Object)) {
        Send-Command $id ('{"command":"load_scene","params":{"path":"' + $invalid[$id].file + '"}}')
    }
    Send-Command "15_set_material" '{"command":"set_material","params":{"name":"block","material":"brick"}}'
    Send-Command "16_set_material_bad" '{"command":"set_material","params":{"name":"block","material":"nope"}}'
    Send-Command "17_remove_body" '{"command":"remove_body","params":{"name":"block_big"}}'
    Send-Command "18_remove_root" '{"command":"remove_body","params":{"name":"Root"}}'
    Send-Command "19_remove_missing" '{"command":"remove_body","params":{"name":"no_such_body"}}'
    Send-Command "20_describe_after_remove" '{"command":"describe_scene"}'
    Send-Command "21_stats_after_remove" '{"command":"get_stats"}'
    Send-Command "23_load_animated" '{"command":"load_scene","params":{"path":"scenes/animated.json"}}'
    Send-Command "24_list_animations" '{"command":"list_animations","params":{"name":"caveman"}}'
    Send-Command "25_pause" '{"command":"pause_animation","params":{"name":"caveman","paused":true}}'
    Send-Command "26_play_bad_clip" '{"command":"play_animation","params":{"name":"caveman","animation":"nope"}}'
    Send-Command "27_play" '{"command":"play_animation","params":{"name":"caveman","animation":"Scene","speed":2.0,"looping":false}}'
    Send-Command "28_list_after" '{"command":"list_animations","params":{"name":"caveman"}}'
    Send-Command "29_load_clip" '{"command":"load_clip","params":{"path":"clips/wave.json"}}'
    Send-Command "30_load_clip_again" '{"command":"load_clip","params":{"path":"clips/wave.json"}}'
    Send-Command "31_list_with_clip" '{"command":"list_animations","params":{"name":"caveman"}}'
    Send-Command "32_play_wave" '{"command":"play_animation","params":{"name":"caveman","animation":"wave"}}'
    Send-Command "33_clip_bad_bone" '{"command":"load_clip","params":{"path":"clips/invalid/unknown_bone.json"}}'
    Send-Command "34_clip_bad_order" '{"command":"load_clip","params":{"path":"clips/invalid/unordered_keys.json"}}'
    Send-Command "35_clip_duplicate" '{"command":"load_clip","params":{"path":"clips/invalid/duplicate_name.json"}}'

    # Script hooks. The engine runs Python files given by path, and attaches scripts from the scripts folder.
    $scriptOk = Join-Path $control "run_ok.txt"
    $okPy = Join-Path $control "control_test_ok.py"
    Set-Content -Path $okPy -Value ("open(r'" + ($scriptOk -replace '\\', '/') + "', 'w').write('ok')") -Encoding utf8
    $badPy = Join-Path $control "control_test_bad.py"
    Set-Content -Path $badPy -Value "this is not python (" -Encoding utf8
    Send-Command "40_run_ok" ('{"command":"run_script","params":{"path":"' + ($okPy -replace '\\', '/') + '"}}')
    Send-Command "41_run_bad" ('{"command":"run_script","params":{"path":"' + ($badPy -replace '\\', '/') + '"}}')
    Send-Command "42_attach_missing_body" '{"command":"attach_script","params":{"name":"no_such_body","module":"ControlHitLogger"}}'
    Send-Command "43_attach_bad_domain" '{"command":"attach_script","params":{"name":"caveman","module":"ControlHitLogger","domain":"frame"}}'
    Send-Command "44_attach_missing_module" '{"command":"attach_script","params":{"name":"caveman","module":"no_such_module"}}'
    Send-Command "45_attach" '{"command":"attach_script","params":{"name":"caveman","module":"ControlHitLogger","domain":"update"}}'

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

    $prims = Wait-Result "07_primitives"
    Check "primitive scene loads (2 bodies)" ($prims.ok -and $prims.result.bodies -eq 2)
    $described = Wait-Result "08_describe_primitives"
    $pnames = @($described.result.bodies | ForEach-Object { $_.name })
    Check "primitives appear by name in describe_scene" (($pnames -contains "block") -and ($pnames -contains "block_big"))

    $setMat = Wait-Result "15_set_material"
    Check "set_material assigns a material to a mesh" ($setMat.ok -and $setMat.result.meshesUpdated -ge 1)
    $setBad = Wait-Result "16_set_material_bad"
    Check "set_material rejects an unknown material" ((-not $setBad.ok) -and ($setBad.error -match "no material"))
    $removed = Wait-Result "17_remove_body"
    Check "remove_body reports removal" ($removed.ok -and $removed.result.removed -eq "block_big")
    $rootRemoval = Wait-Result "18_remove_root"
    Check "remove_body refuses the world root" ((-not $rootRemoval.ok) -and ($rootRemoval.error -match "root"))
    $missingRemoval = Wait-Result "19_remove_missing"
    Check "remove_body reports a missing body" ((-not $missingRemoval.ok) -and ($missingRemoval.error -match "no body"))
    $afterRemove = Wait-Result "20_describe_after_remove"
    $remaining = @($afterRemove.result.bodies | ForEach-Object { $_.name })
    Check "removed body is gone from describe_scene" (-not ($remaining -contains "block_big"))
    Check "other bodies survive a removal" ($remaining -contains "block")
    $loadAnim = Wait-Result "23_load_animated"
    Check "animated scene loads" ($loadAnim.ok -and $loadAnim.result.bodies -eq 1)
    $list = Wait-Result "24_list_animations"
    Check "list_animations reports the clip and its duration" ($list.ok -and $list.result.current -eq "Scene" -and $list.result.animations.Count -ge 1 -and $list.result.durationSeconds -gt 0)
    $pause = Wait-Result "25_pause"
    Check "pause_animation pauses" ($pause.ok -and $pause.result.paused -eq $true)
    $badPlay = Wait-Result "26_play_bad_clip"
    Check "play_animation rejects an unknown clip" ((-not $badPlay.ok) -and ($badPlay.error -match "no animation named"))
    $play = Wait-Result "27_play"
    Check "play_animation sets speed and looping" ($play.ok -and $play.result.speed -eq 2.0 -and $play.result.looping -eq $false)
    $listAfter = Wait-Result "28_list_after"
    Check "play_animation resumes playback" ($listAfter.ok -and $listAfter.result.paused -eq $false -and $listAfter.result.speed -eq 2.0)

    $clip = Wait-Result "29_load_clip"
    Check "load_clip registers a clip from a file" ($clip.ok -and $clip.result.name -eq "wave")
    $clipAgain = Wait-Result "30_load_clip_again"
    Check "load_clip refuses a name that already exists" ((-not $clipAgain.ok) -and ($clipAgain.error -match "already exists"))
    $withClip = Wait-Result "31_list_with_clip"
    $clipNames = @($withClip.result.animations | ForEach-Object { $_.name })
    Check "list_animations shows the loaded clip and the skeleton's bones" ($clipNames -contains "wave" -and $withClip.result.bones.Count -gt 0 -and ($withClip.result.bones -contains "UpperArm.R"))
    $playWave = Wait-Result "32_play_wave"
    Check "the loaded clip can be played" ($playWave.ok -and $playWave.result.animation -eq "wave")
    $badBone = Wait-Result "33_clip_bad_bone"
    Check "clip with an unknown bone is rejected" ((-not $badBone.ok) -and ($badBone.error -match "not a bone"))
    $badOrder = Wait-Result "34_clip_bad_order"
    Check "clip with unordered keys is rejected" ((-not $badOrder.ok) -and ($badOrder.error -match "strictly increase"))
    $dupClip = Wait-Result "35_clip_duplicate"
    Check "clip reusing an existing name is rejected" ((-not $dupClip.ok) -and ($dupClip.error -match "already exists"))

    $statsAfter = Wait-Result "21_stats_after_remove"
    Check "get_stats still answers after removal" ($statsAfter.ok -and $statsAfter.result.bodies -gt 0)

    foreach ($id in ($invalid.Keys | Sort-Object)) {
        $r = Wait-Result $id
        $expect = $invalid[$id].error
        Check "invalid scene rejected: $($invalid[$id].file)" ((-not $r.ok) -and ($r.error -match [regex]::Escape($expect)))
    }

    $runOk = Wait-Result "40_run_ok"
    Check "run_script runs a Python file" ($runOk.ok -and (Test-Path $scriptOk))
    $runBad = Wait-Result "41_run_bad"
    Check "run_script reports a Python syntax error" ((-not $runBad.ok) -and ($runBad.error -match "SyntaxError|syntax"))
    $attachMissing = Wait-Result "42_attach_missing_body"
    Check "attach_script rejects an unknown body" ((-not $attachMissing.ok) -and ($attachMissing.error -match "no body"))
    $attachDomain = Wait-Result "43_attach_bad_domain"
    Check "attach_script rejects an unknown domain" ((-not $attachDomain.ok) -and ($attachDomain.error -match "domain"))
    $attachModule = Wait-Result "44_attach_missing_module"
    Check "attach_script reports a module it cannot load" ((-not $attachModule.ok) -and ($attachModule.error -match "could not create"))
    $attach = Wait-Result "45_attach"
    Check "attach_script returns a script id" ($attach.ok -and $attach.result.domain -eq "update")
    $hitLog = Join-Path $control "script_hits.log"
    $hitDeadline = (Get-Date).AddSeconds(10)
    while (-not (Test-Path $hitLog) -and (Get-Date) -lt $hitDeadline) { Start-Sleep -Milliseconds 200 }
    Check "attached script runs each update and sees its body" ((Test-Path $hitLog) -and ((Get-Content $hitLog -Raw) -match "caveman"))
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
