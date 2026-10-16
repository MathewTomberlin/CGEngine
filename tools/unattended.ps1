# Starts an engine program without a console window. The engine is a console program, so Start-Process would open a terminal in
# front of the desk for every run. CreateNoWindow keeps the console and hides it. With output logs, cmd writes the program's
# output to the two files (the console is still hidden); the process returned is then cmd, which ends when the program does.
# Dot-sourced by the test scripts: . (Join-Path $PSScriptRoot "unattended.ps1")
function Start-Unattended([string]$exe, [string]$workingDirectory, [string]$outLog = $null, [string]$errLog = $null, [string]$arguments = "") {
    $info = New-Object System.Diagnostics.ProcessStartInfo
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.WorkingDirectory = $workingDirectory
    if ($outLog -and $errLog) {
        $info.FileName = "cmd.exe"
        $line = '"' + $exe + '" ' + $arguments + ' > "' + $outLog + '" 2> "' + $errLog + '"'
        $info.Arguments = '/d /c "' + $line + '"'
    } else {
        $info.FileName = $exe
        $info.Arguments = $arguments
    }
    return [System.Diagnostics.Process]::Start($info)
}

# Gives a test run its own copy of an engine folder, so the test's control folder, settings and state never touch the folder the
# player runs the game from (the tests used to write voxel_settings.json and delete cg_control there). The copy is made fresh in
# the temp folder under cgengine_tests, and its name says which test made it. Returns the copy's full path.
function New-TestCopy([string]$source, [string]$name) {
    $root = Join-Path $env:TEMP "cgengine_tests"
    $copy = Join-Path $root $name
    # A game left running from an earlier run holds the copy's files, so it is stopped first (only games run from this copy).
    Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.Path -and $_.Path.StartsWith($copy, [StringComparison]::OrdinalIgnoreCase) } |
        ForEach-Object { Stop-Process -Id $_.Id -Force -ErrorAction SilentlyContinue }
    if (Test-Path $copy) { Remove-Item -Recurse -Force $copy }
    New-Item -ItemType Directory -Force -Path $root | Out-Null
    Copy-Item -Recurse -Force $source $copy
    Remove-Item -Recurse -Force (Join-Path $copy "cg_control") -ErrorAction SilentlyContinue
    Remove-Item -Force (Join-Path $copy "voxel_settings.json") -ErrorAction SilentlyContinue
    return $copy
}
