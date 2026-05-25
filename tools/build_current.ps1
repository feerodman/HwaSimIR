param(
    [ValidateSet("Auto", "DataDrivenTestQT", "ConsoleApplication1", "MaterialTest", "All")]
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

function Resolve-AutoTarget {
    $cwd = (Get-Location).Path
    $dataDrivenRoot = Join-Path $repoRoot "DataDrivenTestQT"
    $consoleRoot = Join-Path $repoRoot "ConsoleApplication1_LLA"
    $materialRoot = Join-Path $repoRoot "MaterialTest"

    if ($cwd.StartsWith($dataDrivenRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        return "DataDrivenTestQT"
    }
    if ($cwd.StartsWith($consoleRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        return "ConsoleApplication1"
    }
    if ($cwd.StartsWith($materialRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        return "MaterialTest"
    }

    return "DataDrivenTestQT"
}

function Build-QtProject {
    $project = Join-Path $repoRoot "DataDrivenTestQT\DataDrivenTestQT.pro"
    $buildDir = Join-Path $repoRoot "build-DataDrivenTestQT-codex-$QtKit-$Configuration"
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
    "ConsoleApplication1" {
        Build-Solution -Name "ConsoleApplication1" -RelativePath "ConsoleApplication1_LLA\ConsoleApplication1.sln"
    }
    "MaterialTest" {
        Build-Solution -Name "MaterialTest" -RelativePath "MaterialTest\MaterialTest.sln"
    }
    "All" {
        Build-QtProject
        Build-Solution -Name "ConsoleApplication1" -RelativePath "ConsoleApplication1_LLA\ConsoleApplication1.sln"
        Build-Solution -Name "MaterialTest" -RelativePath "MaterialTest\MaterialTest.sln"
    }
}
