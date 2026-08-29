[CmdletBinding()]
param(
    [string]$Vs2015Root = $env:ZRDDS_VS2015_ROOT,
    [string]$MinGw730Root = $env:ZRDDS_MINGW730_ROOT,
    [switch]$KeepComparisonDirectories
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$idlPath = Join-Path $repoRoot 'DDS\IDL\HwaSimIRProtocolV1.idl'
$canonicalDir = Join-Path $repoRoot 'DDS\Generated\HwaSimIRProtocolV1'

if ([string]::IsNullOrWhiteSpace($Vs2015Root)) {
    $Vs2015Root = 'F:\Programs\ZRDDS\ZRDDS_VS2015\ZRDDS-2.4.5'
}
if ([string]::IsNullOrWhiteSpace($MinGw730Root)) {
    $MinGw730Root = 'F:\Programs\ZRDDS\ZRDDS_MinGW7.3.0\ZRDDS-2.4.5'
}

$vsGenerator = Join-Path $Vs2015Root 'bin\ZRDDSGen\zrddsgen.exe'
$mingwGenerator = Join-Path $MinGw730Root 'bin\ZRDDSGen\zrddsgen.exe'
foreach ($required in @($idlPath, $vsGenerator, $mingwGenerator)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Required file not found: $required"
    }
}

$scratchRoot = Join-Path ([System.IO.Path]::GetTempPath()) ('hwasimir-zrddsgen-' + [Guid]::NewGuid().ToString('N'))
$vsDir = Join-Path $scratchRoot 'vs2015'
$mingwDir = Join-Path $scratchRoot 'mingw730'
New-Item -ItemType Directory -Force -Path $vsDir, $mingwDir | Out-Null

function Invoke-ZrddsGen([string]$Generator, [string]$OutputDirectory) {
    & $Generator -input_idl $idlPath -output_dir $OutputDirectory -language 'C++'
    if ($LASTEXITCODE -ne 0) {
        throw "zrddsgen failed with exit code ${LASTEXITCODE}: $Generator"
    }
}

try {
    Invoke-ZrddsGen $vsGenerator $vsDir
    Invoke-ZrddsGen $mingwGenerator $mingwDir

    $vsFiles = Get-ChildItem -File -Recurse -LiteralPath $vsDir | ForEach-Object {
        $_.FullName.Substring($vsDir.Length + 1)
    }
    $mingwFiles = Get-ChildItem -File -Recurse -LiteralPath $mingwDir | ForEach-Object {
        $_.FullName.Substring($mingwDir.Length + 1)
    }
    $fileNameDiff = Compare-Object $vsFiles $mingwFiles
    if ($fileNameDiff) {
        $fileNameDiff | Format-Table | Out-String | Write-Error
        throw 'VS2015 and MinGW zrddsgen produced different file sets.'
    }

    $contentDifferences = @()
    foreach ($relativePath in $vsFiles) {
        $vsHash = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $vsDir $relativePath)).Hash
        $mingwHash = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $mingwDir $relativePath)).Hash
        if ($vsHash -ne $mingwHash) {
            $contentDifferences += $relativePath
        }
    }
    if ($contentDifferences.Count -ne 0) {
        throw ('VS2015 and MinGW generated content differs: ' + ($contentDifferences -join ', '))
    }

    New-Item -ItemType Directory -Force -Path $canonicalDir | Out-Null
    Get-ChildItem -File -LiteralPath $canonicalDir -ErrorAction SilentlyContinue | Remove-Item -Force
    Copy-Item -Path (Join-Path $vsDir '*') -Destination $canonicalDir -Recurse -Force

    Write-Output "Canonical generated source: $canonicalDir"
    Write-Output "Compared files: $($vsFiles.Count)"
    Write-Output 'VS2015/MinGW generated source: IDENTICAL (SHA256)'
}
finally {
    if ($KeepComparisonDirectories) {
        Write-Output "Comparison directories retained: $scratchRoot"
    }
    elseif (Test-Path -LiteralPath $scratchRoot) {
        Remove-Item -LiteralPath $scratchRoot -Recurse -Force
    }
}
