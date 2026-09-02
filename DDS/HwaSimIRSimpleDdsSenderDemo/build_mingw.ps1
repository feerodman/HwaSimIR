param(
    [string]$BuildDirectory = 'D:\HwaSimIR\build-HwaSimIRSimpleDdsSenderDemo-mingw73_64-Release',
    [string]$MinGwRoot = 'D:\Qt\Qt5.12.12\Tools\mingw730_64',
    [string]$ZrddsRoot = 'F:\Programs\ZRDDS\ZRDDS_MinGW7.3.0\ZRDDS-2.4.5',
    [string]$CMakeExe = '',
    [string]$LicencePath = ''
)

$ErrorActionPreference = 'Stop'
$sourceDirectory = $PSScriptRoot
$repoRoot = (Resolve-Path (Join-Path $sourceDirectory '..\..')).Path
$gxx = Join-Path $MinGwRoot 'bin\g++.exe'
$make = Join-Path $MinGwRoot 'bin\mingw32-make.exe'
$zrddsLibrary = Join-Path $ZrddsRoot 'lib\ZRDDSCpp.lib'

foreach ($required in @(
    $gxx,
    $make,
    (Join-Path $ZrddsRoot 'include\CPlusPlusInterface\ZRDDSCppSimpleInterface.h'),
    $zrddsLibrary,
    (Join-Path $ZrddsRoot 'lib\ZRDDSCpp.dll'),
    (Join-Path $ZrddsRoot 'lib\ZRDDSC.dll')))
{
    if (-not (Test-Path -LiteralPath $required)) {
        throw "Required MinGW/ZRDDS file not found: $required"
    }
}
if ($ZrddsRoot -match 'VS2015') {
    throw "VS2015 ZRDDS SDK cannot be used by this MinGW demo: $ZrddsRoot"
}

Write-Host 'Compiler:'
& $gxx --version
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Write-Host "ZRDDS MinGW root: $ZrddsRoot"
Write-Host "ZRDDS MinGW library: $zrddsLibrary"

$oldPath = $env:PATH
$oldZrddsHome = $env:ZRDDS_HOME
try {
    $env:PATH = (Join-Path $MinGwRoot 'bin') + ';' + (Join-Path $ZrddsRoot 'lib') + ';' + $oldPath
    $env:ZRDDS_HOME = $ZrddsRoot
    if (-not $CMakeExe) {
        $cmakeCommand = Get-Command cmake.exe -ErrorAction SilentlyContinue
        if ($cmakeCommand) { $CMakeExe = $cmakeCommand.Source }
    }
    if ($CMakeExe -and (Test-Path -LiteralPath $CMakeExe)) {
        & $CMakeExe -S $sourceDirectory -B $BuildDirectory -G 'MinGW Makefiles' `
            "-DCMAKE_BUILD_TYPE=Release" `
            "-DCMAKE_C_COMPILER=$(Join-Path $MinGwRoot 'bin\gcc.exe')" `
            "-DCMAKE_CXX_COMPILER=$gxx" `
            "-DCMAKE_MAKE_PROGRAM=$make" `
            "-DZRDDS_ROOT=$ZrddsRoot"
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        & $CMakeExe --build $BuildDirectory --config Release -- -j4
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }
    else {
        Write-Warning 'cmake.exe was not found; using the equivalent direct MinGW Release compile.'
        New-Item -ItemType Directory -Force -Path $BuildDirectory | Out-Null
        $generated = Join-Path $repoRoot 'DDS\Generated\HwaSimIRProtocolV1'
        $includes = @(
            '-std=c++11', '-O2', '-DNDEBUG', '-D_ZRDDSCPPINTERFACE', '-D_ZRDDSDLLIMPORT',
            "-I$sourceDirectory", "-I$generated",
            "-I$(Join-Path $ZrddsRoot 'include\CPlusPlusInterface')",
            "-I$(Join-Path $ZrddsRoot 'include\ZRDDSCoreInterface')")
        $sources = @(
            (Join-Path $sourceDirectory 'main.cpp'),
            (Join-Path $sourceDirectory 'HwaSimIRSimpleDdsClient.cpp'),
            (Join-Path $generated 'HwaSimIRProtocolV1.cpp'),
            (Join-Path $generated 'HwaSimIRProtocolV1TypeSupport.cpp'))
        $objects = @()
        foreach ($source in $sources) {
            $object = Join-Path $BuildDirectory ((Split-Path $source -Leaf) + '.o')
            & $gxx @includes -c $source -o $object
            if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
            $objects += $object
        }
        & $gxx @objects $zrddsLibrary -o (Join-Path $BuildDirectory 'HwaSimIRSimpleDdsSenderDemo.exe')
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }

    $exe = Join-Path $BuildDirectory 'HwaSimIRSimpleDdsSenderDemo.exe'
    if (-not (Test-Path -LiteralPath $exe)) { throw "Build output missing: $exe" }

    $configDirectory = Join-Path $BuildDirectory 'Config'
    New-Item -ItemType Directory -Force -Path $configDirectory | Out-Null
    Copy-Item -LiteralPath (Join-Path $sourceDirectory 'Config\ZRDDS_PROTOCOL_QOS.xml') `
        -Destination (Join-Path $configDirectory 'ZRDDS_PROTOCOL_QOS.xml') -Force
    Copy-Item -LiteralPath (Join-Path $ZrddsRoot 'lib\ZRDDSCpp.dll') -Destination $BuildDirectory -Force
    Copy-Item -LiteralPath (Join-Path $ZrddsRoot 'lib\ZRDDSC.dll') -Destination $BuildDirectory -Force
    foreach ($runtimeDll in @('libgcc_s_seh-1.dll', 'libstdc++-6.dll', 'libwinpthread-1.dll')) {
        Copy-Item -LiteralPath (Join-Path $MinGwRoot "bin\$runtimeDll") -Destination $BuildDirectory -Force
    }

    if (-not $LicencePath) {
        $licenceCandidates = @(
            (Join-Path $ZrddsRoot 'zrddslicence.lic'),
            'F:\Programs\ZRDDS\ZRDDS_VS2015\ZRDDS-2.4.5\zrddslicence.lic',
            (Join-Path $repoRoot 'DDS\ZRDDSSetupX64VS2015-2.4.5-CAEP-Trial-cc-dg\ZRDDS-2.4.5\zrddslicence.lic'))
        $LicencePath = $licenceCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
    }
    if ($LicencePath -and (Test-Path -LiteralPath $LicencePath)) {
        Copy-Item -LiteralPath $LicencePath -Destination (Join-Path $BuildDirectory 'zrddslicence.lic') -Force
        Write-Host 'Writable runtime licence copy prepared (content not printed).'
    }
    Write-Host "Build PASS: $exe"
}
finally {
    $env:PATH = $oldPath
    $env:ZRDDS_HOME = $oldZrddsHome
}
