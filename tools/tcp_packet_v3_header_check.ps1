param(
    [string]$Compiler = "D:\Qt\Qt5.12.12\Tools\mingw730_64\bin\g++.exe"
)

$ErrorActionPreference = "Stop"
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
$source = Join-Path $root "tools\tcp_packet_v3_header_check.cpp"
$output = Join-Path $root "logs\tcp_packet_v3_header_check.exe"

foreach ($required in @($Compiler, $source, (Join-Path $root "HwaSim_IR\HwaSim_IR\Common\TcpVideoPacketV3.h"))) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "Required path not found: $required"
    }
}
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $output) | Out-Null
& $Compiler -std=c++14 -O2 -Wall -Wextra -Werror $source -o $output
if ($LASTEXITCODE -ne 0) {
    throw "Packet v3 header compile failed: $LASTEXITCODE"
}
& $output
if ($LASTEXITCODE -ne 0) {
    throw "Packet v3 header test failed: $LASTEXITCODE"
}
