[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][ValidateSet('SWIR','MWIR')][string]$Band,
    [ValidateSet('Clear','Cloudy','Rain','Snow')][string]$Weather = 'Clear',
    [ValidateSet('Original','P13Performance300s','P14GroundWeather')][string]$InputMode = 'Original',
    [ValidateSet('','materials','nozzle','cloud')][string]$P5Scene = '',
    [ValidateSet('oblique','end','side','near','far','below','above','behind','occluded')][string]$P5View = 'end',
    [ValidateSet('A','B')][string]$P5MaterialCase = 'A',
    [ValidateSet('On','Off')][string]$P5SyntheticHeatSource = 'On',
    [ValidateSet('0x22','0x55','0x66')][string]$TargetTypeCode = '0x22',
    [ValidateSet(0,1,2)][int]$CloudMaxVisibleVolumes = 0,
    [switch]$RenderPerfProbe,
    [string]$LinearDiagnosticSeqs = '',
    [switch]$EnableAgcDiagnostic,
    [ValidateRange(0.1,100.0)][double]$VisibilityKm = 6.0,
    [ValidateRange(0.0,100.0)][double]$RelativeHumidityPercent = 85.0,
    [ValidateRange(0.0,23.999999)][double]$UtcHour = 3.5,
    [int]$DurationGuardSec = 100,
    [string]$Name = '',
    [string]$OutputRoot = 'logs\p14\runs',
    [string]$BoardHost = '192.168.1.116',
    [string]$BoardUser = 'root',
    [string]$BoardPassword = $env:HWASIMIR_SSH_PASSWORD
)

$arguments = @{
    Band=$Band; Weather=$Weather; InputMode=$InputMode; P5View=$P5View
    P5MaterialCase=$P5MaterialCase; P5SyntheticHeatSource=$P5SyntheticHeatSource
    TargetTypeCode=$TargetTypeCode
    VisibilityKm=$VisibilityKm; RelativeHumidityPercent=$RelativeHumidityPercent; UtcHour=$UtcHour
    CloudMaxVisibleVolumes=$CloudMaxVisibleVolumes; LinearDiagnosticSeqs=$LinearDiagnosticSeqs
    DurationGuardSec=$DurationGuardSec; Name=$Name; OutputRoot=$OutputRoot
    BoardHost=$BoardHost; BoardUser=$BoardUser; BoardPassword=$BoardPassword
}
if($P5Scene){$arguments.P5Scene=$P5Scene}
if($RenderPerfProbe){$arguments.RenderPerfProbe=$true}
if($EnableAgcDiagnostic){$arguments.EnableAgcDiagnostic=$true}
& (Join-Path $PSScriptRoot 'p13_original_dds_case.ps1') @arguments
exit $LASTEXITCODE
