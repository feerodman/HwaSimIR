$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$build=Join-Path $repo 'build-p20m-stim\release'
$qt='D:\Qt\Qt5.12.12\5.12.12\mingw73_64'
$sdk='F:\Programs\ZRDDS\ZRDDS_MinGW7.3.0\ZRDDS-2.4.5'
$compiler='D:\Qt\Qt5.12.12\Tools\mingw730_64\bin\g++.exe'
$objects=@('DdsStimClient','DdsRuntimeManager','CommonDataDdsAdapter',
    'HwaSimIRProtocolV1','HwaSimIRProtocolV1TypeSupport') |
    ForEach-Object { Join-Path $build "$_.o" }
foreach($path in $objects) { if(!(Test-Path -LiteralPath $path)){throw "Build P20M stim first: $path"} }
$arguments=@('-std=c++11','-O2','-DQT_NO_DEBUG','-DQT_CORE_LIB','-DHWASIMIR_HAS_ZRDDS=1',
    "-I$repo\DataDrivenTestQT","-I$repo\DDS\Protocol","-I$repo\DDS\Runtime",
    "-I$repo\DDS\Generated\HwaSimIRProtocolV1","-I$qt\include","-I$qt\include\QtCore",
    "-I$sdk\include\CPlusPlusInterface","-I$sdk\include\ZRDDSCoreInterface",
    (Join-Path $repo 'tools\p20m_direct_dds.cpp')) + $objects +
    @("-L$sdk\lib",'-lZRDDSCpp',"$qt\lib\libQt5Core.a",'-lws2_32','-o',"$build\P20MDirectDds.exe")
& $compiler @arguments
if($LASTEXITCODE -ne 0){throw 'Independent DDS sender build failed'}
Get-FileHash -Algorithm SHA256 -LiteralPath "$build\P20MDirectDds.exe"
