param(
    [switch]$Quiet
)

$ErrorActionPreference = "Stop"
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
$relativePaths = @(
    "HwaSim_IR\HwaSim_IR\Common\CommonData.h",
    "DataDrivenTestQT\CommonData.h",
    "HwaSim_IR_VideoDisplay\HwaSim_IR_VideoDisplay\CommonData.h"
)

function Get-NormalizedHeader {
    param([string]$Path)

    $text = [IO.File]::ReadAllText($Path)
    return ($text -replace "`r`n", "`n" -replace "`r", "`n")
}

function Get-Sha256Text {
    param([string]$Text)

    $sha = [Security.Cryptography.SHA256]::Create()
    try {
        $bytes = [Text.UTF8Encoding]::new($false).GetBytes($Text)
        return ([BitConverter]::ToString($sha.ComputeHash($bytes))).Replace("-", "")
    }
    finally {
        $sha.Dispose()
    }
}

$headers = foreach ($relativePath in $relativePaths) {
    $path = Join-Path $root $relativePath
    if (-not (Test-Path -LiteralPath $path)) {
        throw "CommonData header not found: $path"
    }
    $text = Get-NormalizedHeader -Path $path
    [pscustomobject]@{
        RelativePath = $relativePath
        Hash = Get-Sha256Text -Text $text
        Text = $text
    }
}

$reference = $headers[0]
foreach ($header in $headers) {
    if ($header.Hash -ne $reference.Hash -or $header.Text -cne $reference.Text) {
        throw "CommonData mismatch: $($header.RelativePath) differs from $($reference.RelativePath)"
    }
}

$controlMatch = [regex]::Match(
    $reference.Text,
    'struct\s+ControlP2cX1ObjTrackingCmd\s*\{(?<body>.*?)\};',
    [Text.RegularExpressions.RegexOptions]::Singleline)
if (-not $controlMatch.Success) {
    throw "ControlP2cX1ObjTrackingCmd definition not found"
}
if ($controlMatch.Groups['body'].Value -match '\bsensorID\b') {
    throw "Forbidden protocol change: ControlP2cX1ObjTrackingCmd contains sensorID"
}

if (-not $Quiet) {
    foreach ($header in $headers) {
        Write-Output ("[CommonDataSync] path={0} normalizedSha256={1}" -f $header.RelativePath, $header.Hash)
    }
    Write-Output "[CommonDataSync] status=PASS headers=3 controlSensorID=absent"
}
