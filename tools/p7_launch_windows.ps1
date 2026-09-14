param([ValidateSet('receiver','sender')][string]$Application='receiver',[string]$Version='')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$releases=Join-Path $root 'releases/windows'
if(!$Version){$Version=(Get-Content -LiteralPath (Join-Path $releases 'current.json') -Raw | ConvertFrom-Json).version}
if($Version -notmatch '^[A-Za-z0-9_-]+$'){throw 'Invalid release version'}
$release=Join-Path $releases $Version
$manifest=Get-Content -LiteralPath (Join-Path $release 'manifest.json') -Raw | ConvertFrom-Json
# Only this finite release manifest is checked; never scan model/resource trees.
foreach($entry in $manifest.files.PSObject.Properties){
    if((Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $release $entry.Name)).Hash -ne $entry.Value){throw "Release hash mismatch: $($entry.Name)"}
}
$directory=Join-Path $release $Application
$exe=if($Application -eq 'receiver'){'HwaSim_IR_VideoDisplay.exe'}else{'DataDrivenTestQT.exe'}
Start-Process -FilePath (Join-Path $directory $exe) -WorkingDirectory $directory
