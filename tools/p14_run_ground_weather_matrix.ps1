[CmdletBinding()]
param(
    [string]$BoardHost = '192.168.1.116',
    [string]$BoardUser = 'root',
    [string]$BoardPassword = $env:HWASIMIR_SSH_PASSWORD,
    [string]$ExpectedElf = '80267b01054d9fe8e8ef2c3fd8f3e726003794dafc6cc1086958e9713f18841c',
    [string]$ExpectedConfig = '9b344f42964dbf4e05d7502ba815bea6d4c6e37276bebd3aa63a8f702ec600c9',
    [string]$NamePrefix = 'p14_final4_ground'
)

$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$runner=Join-Path $PSScriptRoot 'p14_original_dds_case.ps1'
$expectedElf=$ExpectedElf.ToLowerInvariant()
$expectedConfig=$ExpectedConfig.ToLowerInvariant()
$cases=@()
foreach($band in @('SWIR','MWIR')) {
    $cases += [ordered]@{Name="${NamePrefix}_$($band.ToLower())_clear";Band=$band;Weather='Clear';Visibility=23.0;Humidity=30.0;UtcHour=5.0;Clouds=0}
    $cases += [ordered]@{Name="${NamePrefix}_$($band.ToLower())_cloud_visibility";Band=$band;Weather='Cloudy';Visibility=12.0;Humidity=60.0;UtcHour=3.5;Clouds=2}
    $cases += [ordered]@{Name="${NamePrefix}_$($band.ToLower())_rain";Band=$band;Weather='Rain';Visibility=6.0;Humidity=85.0;UtcHour=1.0;Clouds=2}
    $cases += [ordered]@{Name="${NamePrefix}_$($band.ToLower())_snow";Band=$band;Weather='Snow';Visibility=6.0;Humidity=85.0;UtcHour=3.0;Clouds=2}
}
foreach($case in $cases) {
    $caseDir=Join-Path $root "logs\p14\runs\$($case.Name)"
    $resultPath=Join-Path $caseDir 'case_result.json'
    if(Test-Path -LiteralPath $resultPath) {
        $result=Get-Content -LiteralPath $resultPath -Raw|ConvertFrom-Json
        $preflight=@($result.boardPreflight)-join "`n"
        if($result.result -eq 'PASS' -and $preflight -match "ElfSha256=$expectedElf" -and
           $preflight -match "ConfigManifestSha256=$expectedConfig") {
            Write-Output "[P14 GroundMatrix] reuse=PASS case=$($case.Name) identity=final_same_elf_config"
            continue
        }
        throw "Existing case is not reusable final evidence: $caseDir"
    }
    & $runner -Band $case.Band -Weather $case.Weather -InputMode P14GroundWeather `
        -TargetTypeCode 0x55 -VisibilityKm $case.Visibility `
        -RelativeHumidityPercent $case.Humidity -UtcHour $case.UtcHour `
        -CloudMaxVisibleVolumes $case.Clouds -LinearDiagnosticSeqs 900 `
        -EnableAgcDiagnostic -DurationGuardSec 55 -Name $case.Name `
        -BoardHost $BoardHost -BoardUser $BoardUser -BoardPassword $BoardPassword
    if($LASTEXITCODE -ne 0){throw "P14 ground matrix case failed: $($case.Name)"}
}
Write-Output "[P14 GroundMatrix] result=PASS cases=8 bands=SWIR,MWIR weather=Clear,Cloudy,Rain,Snow input=P14GroundWeather targetType=0x55 elfSha256=$expectedElf configManifestSha256=$expectedConfig"
