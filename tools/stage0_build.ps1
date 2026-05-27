param(
    [string]$Configuration = "Release",
    [string]$Platform = "x64",
    [switch]$SkipQt,
    [switch]$SkipVs
)

$ErrorActionPreference = "Stop"

$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
$rootPath = $root.Path

$msbuild = "C:\Program Files (x86)\MSBuild\14.0\Bin\MSBuild.exe"
$qtRoot = "D:\Qt\Qt5.12.12"
$qmake = Join-Path $qtRoot "5.12.12\mingw73_64\bin\qmake.exe"
$mingwMake = Join-Path $qtRoot "Tools\mingw730_64\bin\mingw32-make.exe"
$qtBin = Join-Path $qtRoot "5.12.12\mingw73_64\bin"
$mingwBin = Join-Path $qtRoot "Tools\mingw730_64\bin"
$windeployqt = Join-Path $qtBin "windeployqt.exe"

$vsSolution = Join-Path $rootPath "ConsoleApplication1_LLA\ConsoleApplication1.sln"
$qtProject = Join-Path $rootPath "DataDrivenTestQT\DataDrivenTestQT.pro"
$qtBuildDir = Join-Path $rootPath "build-DataDrivenTestQT-codex-mingw73_64-$Configuration"
$qtExe = Join-Path $qtBuildDir "$($Configuration.ToLower())\DataDrivenTestQT.exe"
$qtDataFile = Join-Path $rootPath "DataDrivenTestQT\1.txt"
$vsExe = Join-Path $rootPath "ConsoleApplication1_LLA\Bin\ConsoleApplication1.exe"

function Assert-Path {
    param(
        [string]$Path,
        [string]$Label
    )
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "$Label not found: $Path"
    }
}

Assert-Path $msbuild "VS2015 MSBuild"
Assert-Path $qmake "Qt 5.12.12 qmake"
Assert-Path $mingwMake "Qt MinGW make"
Assert-Path $windeployqt "Qt windeployqt"
Assert-Path $vsSolution "HwaSimIR solution"
Assert-Path $qtProject "DataDrivenTestQT project"

$env:PATH = "$qtBin;$mingwBin;$env:PATH"

Write-Host "Stage 0 build"
Write-Host "Workspace: $rootPath"
Write-Host "Configuration: $Configuration"
Write-Host "Platform: $Platform"
Write-Host "MSBuild: $msbuild"
Write-Host "qmake: $qmake"
Write-Host ""

if (-not $SkipVs) {
    Write-Host "Building HwaSimIR VS project..."
    & $msbuild $vsSolution "/m" "/t:Build" "/p:Configuration=$Configuration" "/p:Platform=$Platform" "/verbosity:minimal"
    if ($LASTEXITCODE -ne 0) {
        throw "MSBuild failed with exit code $LASTEXITCODE"
    }
    Assert-Path $vsExe "HwaSimIR executable"
}

if (-not $SkipQt) {
    Write-Host "Building DataDrivenTestQT Qt project..."
    New-Item -ItemType Directory -Force -Path $qtBuildDir | Out-Null
    Push-Location $qtBuildDir
    try {
        if ($Configuration -ieq "Release") {
            $qmakeArgs = @($qtProject, "-spec", "win32-g++", "CONFIG+=release", "CONFIG-=debug")
        }
        else {
            $qmakeArgs = @($qtProject, "-spec", "win32-g++", "CONFIG+=debug", "CONFIG-=release")
        }
        & $qmake @qmakeArgs
        if ($LASTEXITCODE -ne 0) {
            throw "qmake failed with exit code $LASTEXITCODE"
        }

        $jobs = [Math]::Max(1, [Environment]::ProcessorCount)
        & $mingwMake "-j$jobs"
        if ($LASTEXITCODE -ne 0) {
            throw "mingw32-make failed with exit code $LASTEXITCODE"
        }
    }
    finally {
        Pop-Location
    }
    Assert-Path $qtExe "DataDrivenTestQT executable"

    Write-Host "Deploying DataDrivenTestQT Qt runtime..."
    $deployMode = if ($Configuration -ieq "Release") { "--release" } else { "--debug" }
    & $windeployqt $deployMode "--compiler-runtime" "--force" $qtExe
    if ($LASTEXITCODE -ne 0) {
        throw "windeployqt failed with exit code $LASTEXITCODE"
    }

    if (Test-Path -LiteralPath $qtDataFile) {
        Copy-Item -LiteralPath $qtDataFile -Destination (Join-Path (Split-Path -Parent $qtExe) "1.txt") -Force
    }
}

Write-Host ""
Write-Host "Stage 0 build completed."
Write-Host "HwaSimIR: $vsExe"
Write-Host "DataDrivenTestQT: $qtExe"
