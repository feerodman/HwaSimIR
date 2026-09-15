param([Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$version=(Get-Content -Raw -LiteralPath "$root/releases/windows/current.json" | ConvertFrom-Json).version
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$OutputDirectory=(Resolve-Path -LiteralPath $OutputDirectory).Path
foreach($name in @('P6Scene','P6ExistingTargets','WeatherCameraInput','WeatherStartSec','SensorDisplayPreset')){
    if([Environment]::GetEnvironmentVariable($name)){throw "Unexpected inherited business override: $name"}
}
$env:QT_FORCE_STDERR_LOGGING='1'
$env:P7RecordingRoot=Join-Path $OutputDirectory 'recording'
$env:P9ControlCsv=Join-Path $OutputDirectory 'control_timing.csv'
$env:HwaInputAuditDirectory=$OutputDirectory
$env:P6ReceiverUiDump=Join-Path $OutputDirectory 'receiver_ui.png'
$env:P6ReceiverUiResponsiveCapture='1'
$env:P7SenderUiDump=Join-Path $OutputDirectory 'sender_ui.png'
$receiverDirectory="$root/releases/windows/$version/receiver"
$senderDirectory="$root/releases/windows/$version/sender"
$receiver=Start-Process -FilePath "$receiverDirectory/HwaSim_IR_VideoDisplay.exe" -WorkingDirectory $receiverDirectory -PassThru -RedirectStandardOutput "$OutputDirectory/receiver.out.log" -RedirectStandardError "$OutputDirectory/receiver.err.log"
# Deliberately different CWD: normal startup must resolve 1.txt beside its EXE.
$sender=Start-Process -FilePath "$senderDirectory/DataDrivenTestQT.exe" -WorkingDirectory $root -PassThru -RedirectStandardOutput "$OutputDirectory/sender.out.log" -RedirectStandardError "$OutputDirectory/sender.err.log"
@{receiver=$receiver.Id;sender=$sender.Id;args=@();senderWorkingDirectory=$root;release=$version;businessOverrides=@();diagnostics=@('recording path','input audit','Control CSV','actual widget capture')} | ConvertTo-Json | Set-Content -Encoding UTF8 "$OutputDirectory/processes.json"
