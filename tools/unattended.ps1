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
