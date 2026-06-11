param(
    [ValidateSet("Auto", "DataDrivenTestQT", "HwaSim_IR", "MaterialTest", "All")]
    [string]$Target = "Auto",

    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",

    [ValidateSet("x64", "x86")]
    [string]$Platform = "x64",

    [ValidateSet("mingw73_64", "msvc2015_64")]
    [string]$QtKit = "mingw73_64",

    [switch]$Clean
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
$vsArch = if ($Platform -eq "x64") { "x64" } else { "x86" }

. "$PSScriptRoot\build_env.ps1" -QtKit $QtKit -Arch $vsArch -Quiet

function Invoke-CheckedCommand {
    param(
        [string]$FilePath,
        [string[]]$Arguments,
        [string]$FailureMessage
    )

    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$FailureMessage (exit code $LASTEXITCODE)"
    }
}

function Deploy-QtProject {
    param(
        [string]$Executable,
        [string]$Configuration
    )

    if (-not (Test-Path -LiteralPath $Executable)) {
        throw "DataDrivenTestQT executable not found: $Executable"
    }

    $windeployqt = Join-Path $env:QT_BIN_DIR "windeployqt.exe"
    if (-not (Test-Path -LiteralPath $windeployqt)) {
        throw "windeployqt.exe not found: $windeployqt"
    }

    $deployMode = if ($Configuration -eq "Debug") { "--debug" } else { "--release" }

    Write-Host "Deploying DataDrivenTestQT runtime"
    Write-Host "windeployqt: $windeployqt"
    Write-Host "exe       : $Executable"

    Invoke-CheckedCommand -FilePath $windeployqt -Arguments @(
        $deployMode,
        "--compiler-runtime",
        "--force",
        $Executable
    ) -FailureMessage "windeployqt failed"

    $dataSource = Join-Path $repoRoot "DataDrivenTestQT\1.txt"
    if (Test-Path -LiteralPath $dataSource) {
        $targetDir = Split-Path -Parent $Executable
        Copy-Item -LiteralPath $dataSource -Destination (Join-Path $targetDir "1.txt") -Force
        Write-Host "runtime data: copied 1.txt"
    }
}

function Resolve-AutoTarget {
    $cwd = (Get-Location).Path
    $dataDrivenRoot = Join-Path $repoRoot "DataDrivenTestQT"
    $consoleRoot = Join-Path $repoRoot "HwaSim_IR"
    $materialRoot = Join-Path $repoRoot "MaterialTest"

    if ($cwd.StartsWith($dataDrivenRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        return "DataDrivenTestQT"
    }
    if ($cwd.StartsWith($consoleRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        return "HwaSim_IR"
    }
    if ($cwd.StartsWith($materialRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        return "MaterialTest"
    }

    return "DataDrivenTestQT"
}

function Build-QtProject {
    $project = Join-Path $repoRoot "DataDrivenTestQT\DataDrivenTestQT.pro"
    $buildDir = Join-Path $repoRoot "build-DataDrivenTestQT-codex-$QtKit-$Configuration"
    $outputSubdir = if ($Configuration -eq "Debug") { "debug" } else { "release" }
    $qtExe = Join-Path $buildDir "$outputSubdir\DataDrivenTestQT.exe"
    $makeName = if ($QtKit -eq "msvc2015_64") { "nmake.exe" } else { "mingw32-make.exe" }
    $spec = if ($QtKit -eq "msvc2015_64") { "win32-msvc" } else { "win32-g++" }
    $configAdd = if ($Configuration -eq "Debug") { "CONFIG+=debug" } else { "CONFIG+=release" }
    $configRemove = if ($Configuration -eq "Debug") { "CONFIG-=release" } else { "CONFIG-=debug" }

    $make = (Get-Command $makeName -ErrorAction Stop).Source
    New-Item -ItemType Directory -Path $buildDir -Force | Out-Null

    Write-Host "Building DataDrivenTestQT"
    Write-Host "qmake : $env:QMAKE"
    Write-Host "make  : $make"
    Write-Host "dir   : $buildDir"

    Push-Location $buildDir
    try {
        Invoke-CheckedCommand -FilePath $env:QMAKE -Arguments @($project, "-spec", $spec, $configAdd, $configRemove) -FailureMessage "qmake failed"
        if ($Clean) {
            Invoke-CheckedCommand -FilePath $make -Arguments @("clean") -FailureMessage "$makeName clean failed"
        }
        Invoke-CheckedCommand -FilePath $make -Arguments @() -FailureMessage "$makeName build failed"
    } finally {
        Pop-Location
    }

    Deploy-QtProject -Executable $qtExe -Configuration $Configuration
}

function Build-Solution {
    param(
        [string]$Name,
        [string]$RelativePath
    )

    $solution = Join-Path $repoRoot $RelativePath
    $target = if ($Clean) { "Rebuild" } else { "Build" }

    Write-Host "Building $Name"
    Write-Host "MSBuild : $env:MSBUILD_EXE"
    Write-Host "Solution: $solution"

    Invoke-CheckedCommand -FilePath $env:MSBUILD_EXE -Arguments @(
        $solution,
        "/m",
        "/t:$target",
        "/p:Configuration=$Configuration",
        "/p:Platform=$Platform",
        "/v:minimal",
        "/nologo"
    ) -FailureMessage "MSBuild failed for $Name"
}

$resolvedTarget = if ($Target -eq "Auto") { Resolve-AutoTarget } else { $Target }

Write-Host "HwaSimIR build"
Write-Host "Target       : $resolvedTarget"
Write-Host "Configuration: $Configuration"
Write-Host "Platform     : $Platform"
Write-Host "Qt kit       : $QtKit"
Write-Host ""

switch ($resolvedTarget) {
    "DataDrivenTestQT" {
        Build-QtProject
    }
    "HwaSim_IR" {
        Build-Solution -Name "HwaSim_IR" -RelativePath "HwaSim_IR\HwaSim_IR.sln"
    }
    "MaterialTest" {
        Build-Solution -Name "MaterialTest" -RelativePath "MaterialTest\MaterialTest.sln"
    }
    "All" {
        Build-QtProject
        Build-Solution -Name "HwaSim_IR" -RelativePath "HwaSim_IR\HwaSim_IR.sln"
        Build-Solution -Name "MaterialTest" -RelativePath "MaterialTest\MaterialTest.sln"
    }
}
