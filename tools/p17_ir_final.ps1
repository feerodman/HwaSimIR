$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$last=Join-Path $root 'logs/p17/ir_retest/after/mwir_plume1_detail_ir_v7/case_result.json'
$deadline=[DateTime]::UtcNow.AddMinutes(20)
while(-not (Test-Path $last)) {
    if([DateTime]::UtcNow -gt $deadline){throw 'Baseline matrix did not complete'}
    Start-Sleep -Seconds 5
}
if((Get-Content -Raw $last | ConvertFrom-Json).result -ne 'PASS'){throw 'Baseline matrix failed'}
Start-Sleep -Seconds 3
if(Get-Process -Name DataDrivenTestQT,HwaSim_IR_VideoDisplay -ErrorAction SilentlyContinue){throw 'Baseline still owns renderer clients'}
python (Join-Path $PSScriptRoot 'p17_deploy_overlay.py') `
    --elf logs/p17/ir_retest/build/hdr_v8/HwaSim_IR.aarch64 --out logs/p17/ir_retest/deploy/hdr_v8 --include-cloud
if($LASTEXITCODE -ne 0){throw 'HDR v8 deployment failed'}
& (Join-Path $PSScriptRoot 'p17_ir_retest.ps1') -Suffix ir_v8 -Bands @('MWIR','SWIR')
& (Join-Path $PSScriptRoot 'p17_ir_cloud_matrix.ps1') -Suffix ir_v8
& (Join-Path $PSScriptRoot 'p17_ir_retest.ps1') -Suite Detail -Suffix ir_v8
