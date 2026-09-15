param([int]$ProcessId,[string]$OutputDirectory)
$ErrorActionPreference='Stop'
$actions=Join-Path $OutputDirectory 'ui_actions.jsonl'
& "$PSScriptRoot/p9_ui_control.ps1" -ProcessId $ProcessId -Action start | Add-Content -Encoding UTF8 $actions
$clock=[Diagnostics.Stopwatch]::StartNew()
for($section=1;$section -le 3;$section++){
    while($clock.Elapsed.TotalSeconds -lt $section*20){Start-Sleep -Milliseconds 100}
    Write-Output "P9 normal UI recording elapsedSeconds=$($clock.Elapsed.TotalSeconds)"
}
& "$PSScriptRoot/p9_ui_control.ps1" -ProcessId $ProcessId -Action stop | Add-Content -Encoding UTF8 $actions
Write-Output 'P9 STOP button invoked; receiver stays open for independent Control diagnostics.'
