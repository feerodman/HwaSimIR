param()
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$cases=@(
    @{Name='legacy_current_rain192';Band=1;Weather=2;Budget='legacy_192.json';Camera='pair_rain_asset.json';Policy=''},
    @{Name='legacy_per_kind_rain192';Band=1;Weather=2;Budget='legacy_192_per_kind.json';Camera='pair_rain_asset.json';Policy=''},
    @{Name='snow192';Band=2;Weather=3;Budget='precip_192.json';Camera='pair_snow_asset.json'},
    @{Name='snow128';Band=2;Weather=3;Budget='precip_128.json';Camera='pair_snow_asset.json'},
    @{Name='snow96';Band=2;Weather=3;Budget='precip_96.json';Camera='pair_snow_asset.json'},
    @{Name='rain96';Band=1;Weather=2;Budget='precip_96.json';Camera='pair_rain_asset.json'},
    @{Name='no_precip';Band=1;Weather=2;Budget='precip_zero.json';Camera='pair_rain_asset.json'},
    @{Name='rain128_norecord';Band=1;Weather=2;Budget='precip_128.json';Camera='pair_rain_asset.json';Save=0}
)
foreach($c in $cases){
    $name='P8_A6_'+$c.Name
    $save=1;if($c.ContainsKey('Save')){$save=$c.Save}
    $policy='/userdata/HwaSimIR/logs/p8_supported_high.conf';if($c.ContainsKey('Policy')){$policy=$c.Policy}
    try {
        & (Join-Path $PSScriptRoot 'p7_run_case.ps1') -Name $name -LogGroup p8 -InputAudit -NoDuplicateH264 -Normal -ProductionDefaults -Band $c.Band -Weather $c.Weather -Rate 60 -Seconds 65 -CaptureSeconds 0 -SaveMp4 $save -CameraInput (Join-Path $root ('tools/p7_inputs/'+$c.Camera)) -DiagnosticJson (Join-Path $root ('tools/p8_inputs/'+$c.Budget)) -PerformancePolicy $policy -SenderExe (Join-Path $root 'releases/windows/P8_A6/sender/DataDrivenTestQT.exe') -ReceiverExe (Join-Path $root 'releases/windows/P8_A6/receiver/HwaSim_IR_VideoDisplay.exe') *> (Join-Path $root "logs/p8/${name}_runner.log")
    } catch {
        $_ | Out-String | Set-Content -Encoding UTF8 (Join-Path $root "logs/p8/${name}_failure.txt")
        if(-not $c.Name.StartsWith('legacy_')){throw}
    }
}
