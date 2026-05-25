param(
    [ValidateSet("mingw73_64", "msvc2015_64")]
    [string]$QtKit = "mingw73_64",

    [ValidateSet("x64", "x86")]
    [string]$Arch = "x64",

    [switch]$Quiet
)

$ErrorActionPreference = "Stop"

function Resolve-FirstExistingPath {
    param(
        [string]$Name,
        [string[]]$Candidates
    )

    foreach ($candidate in $Candidates) {
        if ([string]::IsNullOrWhiteSpace($candidate)) {
            continue
        }
        if (Test-Path -LiteralPath $candidate) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    throw "Cannot find $Name. Checked: $($Candidates -join ', ')"
}

function Add-PathFront {
    param([string[]]$Paths)

    $merged = New-Object System.Collections.Generic.List[string]

    foreach ($path in ($Paths + ($env:PATH -split ";"))) {
        if ([string]::IsNullOrWhiteSpace($path)) {
            continue
        }

        $normalized = $path.Trim()
        $exists = $false
        foreach ($item in $merged) {
            if ($item -ieq $normalized) {
                $exists = $true
                break
            }
        }

        if (-not $exists) {
            $merged.Add($normalized)
        }
    }

    $env:PATH = $merged -join ";"
}

function Import-VsBatchEnvironment {
    param(
        [string]$BatchFile,
        [string]$TargetArch
    )

    $vcArg = if ($TargetArch -eq "x64") { "amd64" } else { "x86" }
    $cmd = "`"$BatchFile`" $vcArg > nul && set"
    $lines = & $env:ComSpec /s /c $cmd
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to import Visual Studio environment from $BatchFile"
    }

    foreach ($line in $lines) {
        if ($line -match "^(.*?)=(.*)$") {
            Set-Item -Path "Env:$($Matches[1])" -Value $Matches[2]
        }
    }
}

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
$qtBase = if ($env:QT_5_12_12_ROOT) {
    $env:QT_5_12_12_ROOT
} elseif ($env:QT512_ROOT) {
    $env:QT512_ROOT
} else {
    "D:\Qt\Qt5.12.12\5.12.12"
}

$qtBin = Resolve-FirstExistingPath "Qt 5.12.12 $QtKit bin" @(
    (Join-Path $qtBase "$QtKit\bin")
)
$qmake = Resolve-FirstExistingPath "Qt 5.12.12 qmake.exe" @(
    (Join-Path $qtBin "qmake.exe")
)

$msbuild = Resolve-FirstExistingPath "MSBuild.exe for VS2015" @(
    $env:MSBUILD_EXE,
    "C:\Program Files (x86)\MSBuild\14.0\Bin\MSBuild.exe",
    "C:\Program Files\MSBuild\14.0\Bin\MSBuild.exe"
)
$msbuildBin = Split-Path -Parent $msbuild

$vcvarsall = Resolve-FirstExistingPath "VS2015 vcvarsall.bat" @(
    "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat"
)
Import-VsBatchEnvironment -BatchFile $vcvarsall -TargetArch $Arch

$pathPrefix = @($qtBin, $msbuildBin)

if ($QtKit -eq "mingw73_64") {
    $mingwBin = Resolve-FirstExistingPath "Qt 5.12.12 MinGW bin" @(
        (Join-Path (Split-Path -Parent $qtBase) "Tools\mingw730_64\bin"),
        "D:\Qt\Qt5.12.12\Tools\mingw730_64\bin"
    )
    $pathPrefix += $mingwBin
}

Add-PathFront -Paths $pathPrefix

$env:HWSIMIR_ROOT = $repoRoot
$env:QT_BIN_DIR = $qtBin
$env:QMAKE = $qmake
$env:MSBUILD_EXE = $msbuild

if (-not $Quiet) {
    Write-Host "Configured HwaSimIR build environment"
    Write-Host "Repo      : $repoRoot"
    Write-Host "Qt kit    : $QtKit"
    Write-Host "qmake     : $qmake"
    Write-Host "MSBuild   : $msbuild"
    Write-Host "VS arch   : $Arch"
    Write-Host ""
    & $qmake -v
    & $msbuild /version /nologo
}
