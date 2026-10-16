[CmdletBinding()]
param([string]$BuildDirectory='build-p18-customer-receiver-v4')
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$qt='D:\Qt\Qt5.12.12\5.12.12\mingw73_64\bin'
$mingw='D:\Qt\Qt5.12.12\Tools\mingw730_64\bin'
$sdk='F:\Programs\ZRDDS\ZRDDS_MinGW7.3.0\ZRDDS-2.4.5'
$ff=Join-Path $repo '.deps\ffmpeg-n8.1-win64-gpl-shared\ffmpeg-n8.1-latest-win64-gpl-shared-8.1'
$env:PATH="$qt;$mingw;$env:PATH"
$env:ZRDDS_MINGW730_ROOT=$sdk
$env:FFMPEG_ROOT=$ff
$build=Join-Path $repo $BuildDirectory
New-Item -ItemType Directory -Force -Path $build | Out-Null
Push-Location $build
try {
    & "$qt\qmake.exe" (Join-Path $repo 'DDS\HwaSimIRCustomerTxVideoReceiverDemo\HwaSimIRCustomerTxVideoReceiverDemo.pro') -spec win32-g++ CONFIG+=release CONFIG-=debug
    if($LASTEXITCODE){throw 'qmake failed'}
    & "$mingw\mingw32-make.exe" -j4
    if($LASTEXITCODE){throw 'customer receiver build failed'}
    $exe=Join-Path $build 'release\HwaSimIRCustomerTxVideoReceiverDemo.exe'
    & "$qt\windeployqt.exe" --release --compiler-runtime $exe
    if($LASTEXITCODE){throw 'Qt deployment failed'}
    $dest=Split-Path $exe
    Copy-Item "$sdk\lib\ZRDDSCpp.dll" $dest -Force
    Copy-Item 'F:\Programs\ZRDDS\ZRDDS_VS2015\ZRDDS-2.4.5\zrddslicence.lic' $dest -Force
    Copy-Item "$ff\bin\*.dll" $dest -Force
    Copy-Item "$ff\bin\ffmpeg.exe" $dest -Force
    Copy-Item (Join-Path $repo 'HwaSim_IR\Bin\Config\DDS\ZRDDS_PROTOCOL_QOS.xml') $dest -Force
    Write-Host "Built: $exe"
} finally {Pop-Location}
