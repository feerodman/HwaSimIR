param(
    [string]$Manifest = "",
    [string]$PcModWinRoot = "F:\Programs\PcModWin5",
    [Alias("Executable")]
    [string]$ModtranExe = "",
    [string]$Python = "python",
    [switch]$SingleCase,
    [string]$CaseId = "",
    [ValidateRange(0, 3000)]
    [int]$CaseLimit = 0,
    [switch]$NoDeleteRaw,
    [switch]$ValidationSix,
    [switch]$Pilot72,
    [switch]$VisibilitySmoke18,
    [switch]$AerosolOverrideSmoke,
    [switch]$ProductionNirMwir,
    [ValidateSet("", "NIR_Transmittance", "MWIR_Transmittance", "MWIR_ThermalRadiance", "Solar_NIR_MWIR", "NIR_RadianceWithScattering")]
    [string]$BatchName = "",
    [switch]$Resume,
    [switch]$Pilot
)

$ErrorActionPreference = "Stop"

$runner = Join-Path $PSScriptRoot "modtran\run_modtran_cases.ps1"
& $runner `
    -Manifest $Manifest `
    -PcModWinRoot $PcModWinRoot `
    -ModtranExe $ModtranExe `
    -Python $Python `
    -SingleCase:$SingleCase `
    -CaseId $CaseId `
    -CaseLimit $CaseLimit `
    -NoDeleteRaw:$NoDeleteRaw `
    -ValidationSix:$ValidationSix `
    -Pilot72:$Pilot72 `
    -VisibilitySmoke18:$VisibilitySmoke18 `
    -AerosolOverrideSmoke:$AerosolOverrideSmoke `
    -ProductionNirMwir:$ProductionNirMwir `
    -BatchName $BatchName `
    -Resume:$Resume `
    -Pilot:$Pilot
exit $LASTEXITCODE
