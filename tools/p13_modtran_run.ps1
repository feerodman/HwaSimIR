param(
    [ValidateSet('pilot','track50')]
    [string]$Mode='pilot',
    [string]$CaseRoot='',
    [string]$PcModBin='F:\Programs\PcModWin5\Bin',
    [switch]$Resume
)

$ErrorActionPreference='Stop'
$repoRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$allowedRoot=[IO.Path]::GetFullPath((Join-Path $repoRoot "logs\p13\atmosphere\$Mode"))
$resolvedRoot=[IO.Path]::GetFullPath($(if([string]::IsNullOrWhiteSpace($CaseRoot)){$allowedRoot}else{$CaseRoot}))
if($resolvedRoot -ne $allowedRoot){throw "Refusing unexpected CaseRoot: $resolvedRoot"}
$expectedBin=[IO.Path]::GetFullPath('F:\Programs\PcModWin5\Bin')
$resolvedBin=[IO.Path]::GetFullPath($PcModBin)
if($resolvedBin -ne $expectedBin){throw "Refusing unexpected PcModBin: $resolvedBin"}
$exe=Join-Path $resolvedBin 'Mod5.2.1.0.exe'
$manifest=Join-Path $resolvedRoot 'case_manifest.csv'
foreach($required in @($exe,$manifest)){
    if(!(Test-Path -LiteralPath $required -PathType Leaf)){throw "Required file missing: $required"}
}

$cases=@(Import-Csv -LiteralPath $manifest)
$expected=$(if($Mode -eq 'pilot'){10}else{720})
if($cases.Count -ne $expected){throw "Expected $expected component cases for $Mode, got $($cases.Count)"}
$caseIds=@{}
foreach($case in $cases){
    if($caseIds.ContainsKey($case.case_id)){throw "Duplicate case id: $($case.case_id)"}
    $caseIds[$case.case_id]=$true
    $input=[IO.Path]::GetFullPath($case.input_file)
    if(!$input.StartsWith($resolvedRoot,[StringComparison]::OrdinalIgnoreCase)){
        throw "Input outside isolated root: $input"
    }
    if((Get-FileHash -Algorithm SHA256 -LiteralPath $input).Hash.ToLowerInvariant() -ne $case.input_sha256){
        throw "Input hash mismatch: $input"
    }
}

$fixedNames=@('modin','tape5','tape6','tape7','tape8','tape7.scn','specflux')
$tempBase=[IO.Path]::GetFullPath([IO.Path]::GetTempPath())
$backupRoot=[IO.Path]::Combine($tempBase,'hwasimir-p13-modtran-'+[Guid]::NewGuid().ToString('N'))
if(!$backupRoot.StartsWith($tempBase,[StringComparison]::OrdinalIgnoreCase)){throw 'Unsafe temporary backup path'}
New-Item -ItemType Directory -Force -Path $backupRoot | Out-Null
$before=@{}
foreach($name in $fixedNames){
    $path=Join-Path $resolvedBin $name
    $exists=Test-Path -LiteralPath $path -PathType Leaf
    $before[$name]=[pscustomobject]@{
        exists=$exists
        size_bytes=$(if($exists){(Get-Item -LiteralPath $path).Length}else{0})
        sha256=$(if($exists){(Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash.ToLowerInvariant()}else{''})
        last_write_time_utc=$(if($exists){(Get-Item -LiteralPath $path).LastWriteTimeUtc.ToString('o')}else{''})
    }
    if($exists){Copy-Item -LiteralPath $path -Destination (Join-Path $backupRoot $name) -Force}
}

$engine=Get-Item -LiteralPath $exe
$engineHash=(Get-FileHash -Algorithm SHA256 -LiteralPath $exe).Hash.ToLowerInvariant()
$engineVersion=$engine.VersionInfo.FileVersion
if([string]::IsNullOrWhiteSpace($engineVersion)){$engineVersion='5.2.1.0 (executable filename)'}
$prior=@{}
$priorManifest=Join-Path $resolvedRoot 'run_manifest.csv'
if($Resume -and (Test-Path -LiteralPath $priorManifest -PathType Leaf)){
    foreach($row in @(Import-Csv -LiteralPath $priorManifest)){
        if($row.case_id -and $row.output_kind){$prior["$($row.case_id)|$($row.output_kind)"]=$row}
    }
}
$runs=New-Object System.Collections.Generic.List[object]
$completedCases=0
$resumedCases=0

function Add-OutputRows([object]$case,[string]$caseDir,[bool]$resumed){
    $mapping=[ordered]@{'tape6'='MODOUT1.txt';'tape7'='MODOUT2.txt';'tape8'='MODOUT3.txt';'tape7.scn'='tape7.scn';'specflux'='spectral_flux.flx'}
    foreach($sourceName in $mapping.Keys){
        $destination=Join-Path $caseDir $mapping[$sourceName]
        if(Test-Path -LiteralPath $destination -PathType Leaf){
            $runs.Add([pscustomobject]@{
                case_id=$case.case_id;mode=$case.mode;band=$case.band
                observer_alt_km=$case.observer_alt_km;target_alt_km=$case.target_alt_km
                range_km=$case.range_km;visibility_km=$case.visibility_km;solar_zenith_deg=$case.solar_zenith_deg
                generated_at_utc=[DateTime]::UtcNow.ToString('o');engine_path=$exe
                engine_file_version=$engineVersion;engine_sha256=$engineHash
                input_sha256=$case.input_sha256;output_kind=$sourceName
                output_file=[IO.Path]::GetFullPath($destination)
                output_sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $destination).Hash.ToLowerInvariant()
                output_fresh=$true;resumed_existing_verified=$resumed
            })
        }
    }
}

try {
    foreach($case in $cases){
        $input=[IO.Path]::GetFullPath($case.input_file)
        $caseDir=Split-Path -Parent $input
        $requiredName=$(if($case.mode -eq 'SpectralFlux'){'spectral_flux.flx'}else{'MODOUT2.txt'})
        $required=Join-Path $caseDir $requiredName
        $canResume=$false
        if($Resume -and (Test-Path -LiteralPath $required -PathType Leaf)){
            $kind=$(if($case.mode -eq 'SpectralFlux'){'specflux'}else{'tape7'})
            $key="$($case.case_id)|$kind"
            if($prior.ContainsKey($key)){
                $actual=(Get-FileHash -Algorithm SHA256 -LiteralPath $required).Hash.ToLowerInvariant()
                $canResume=($prior[$key].engine_sha256 -eq $engineHash -and
                    $prior[$key].input_sha256 -eq $case.input_sha256 -and
                    $prior[$key].output_sha256 -eq $actual)
            }
        }
        if($canResume){
            Add-OutputRows $case $caseDir $true
            $resumedCases++
            Write-Output "Resumed verified $($case.case_id) ($($resumedCases + $completedCases)/$($cases.Count))"
            continue
        }

        Copy-Item -LiteralPath $input -Destination (Join-Path $resolvedBin 'modin') -Force
        Copy-Item -LiteralPath $input -Destination (Join-Path $resolvedBin 'tape5') -Force
        foreach($name in @('tape6','tape7','tape8','tape7.scn','specflux')){
            $path=Join-Path $resolvedBin $name
            if(Test-Path -LiteralPath $path){Remove-Item -LiteralPath $path -Force}
        }
        $stdout=Join-Path $caseDir 'engine_stdout.txt'
        $stderr=Join-Path $caseDir 'engine_stderr.txt'
        $started=[DateTime]::UtcNow
        $process=Start-Process -FilePath $exe -WorkingDirectory $resolvedBin -WindowStyle Hidden -Wait -PassThru `
            -RedirectStandardOutput $stdout -RedirectStandardError $stderr
        if($process.ExitCode -ne 0){throw "MODTRAN failed: $($case.case_id), exit=$($process.ExitCode)"}
        $mapping=[ordered]@{'tape6'='MODOUT1.txt';'tape7'='MODOUT2.txt';'tape8'='MODOUT3.txt';'tape7.scn'='tape7.scn';'specflux'='spectral_flux.flx'}
        foreach($sourceName in $mapping.Keys){
            $source=Join-Path $resolvedBin $sourceName
            if(Test-Path -LiteralPath $source -PathType Leaf){
                $destination=Join-Path $caseDir $mapping[$sourceName]
                Copy-Item -LiteralPath $source -Destination $destination -Force
                $runs.Add([pscustomobject]@{
                    case_id=$case.case_id;mode=$case.mode;band=$case.band
                    observer_alt_km=$case.observer_alt_km;target_alt_km=$case.target_alt_km
                    range_km=$case.range_km;visibility_km=$case.visibility_km;solar_zenith_deg=$case.solar_zenith_deg
                    generated_at_utc=[DateTime]::UtcNow.ToString('o');engine_path=$exe
                    engine_file_version=$engineVersion;engine_sha256=$engineHash
                    input_sha256=$case.input_sha256;output_kind=$sourceName
                    output_file=[IO.Path]::GetFullPath($destination)
                    output_sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $destination).Hash.ToLowerInvariant()
                    output_fresh=((Get-Item -LiteralPath $destination).LastWriteTimeUtc -ge $started.AddSeconds(-2))
                    resumed_existing_verified=$false
                })
            }
        }
        if(!(Test-Path -LiteralPath $required -PathType Leaf)){throw "Required MODTRAN product absent: $required"}
        if((Get-Item -LiteralPath $stderr).Length -ne 0){throw "MODTRAN stderr is not empty: $($case.case_id)"}
        $completedCases++
        Write-Output "Completed $($case.case_id) ($($resumedCases + $completedCases)/$($cases.Count))"
    }
    $runs | Export-Csv -LiteralPath $priorManifest -NoTypeInformation -Encoding UTF8
    [ordered]@{
        schema='HwaSimIR.P13.RealModtranRun.1';mode=$Mode
        checked_at_utc=[DateTime]::UtcNow.ToString('o');executable=$exe
        file_version=$engineVersion;sha256=$engineHash
        run_status='SUCCESS_REAL_MODTRAN_OUTPUT';requested_component_runs=$cases.Count
        executed_this_invocation=$completedCases;resumed_verified=$resumedCases
        required_products_fresh_or_hash_verified=(@($runs|Where-Object {!$_.output_fresh}).Count -eq 0)
        license_evidence="All $($cases.Count) component cases exited zero or were resumed only after engine/input/output SHA-256 verification."
    } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $resolvedRoot 'engine_and_license_evidence.json') -Encoding UTF8
}
finally {
    foreach($name in $fixedNames){
        $path=Join-Path $resolvedBin $name
        $backup=Join-Path $backupRoot $name
        if($before[$name].exists){Copy-Item -LiteralPath $backup -Destination $path -Force}
        elseif(Test-Path -LiteralPath $path){Remove-Item -LiteralPath $path -Force}
    }
    $resolvedBackup=[IO.Path]::GetFullPath($backupRoot)
    if(!$resolvedBackup.StartsWith($tempBase,[StringComparison]::OrdinalIgnoreCase)){throw "Unsafe cleanup path: $resolvedBackup"}
    if(Test-Path -LiteralPath $resolvedBackup){Remove-Item -LiteralPath $resolvedBackup -Recurse -Force}
    $restore=@()
    foreach($name in $fixedNames){
        $path=Join-Path $resolvedBin $name
        $exists=Test-Path -LiteralPath $path -PathType Leaf
        $afterHash=$(if($exists){(Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash.ToLowerInvariant()}else{''})
        $restore+=[pscustomobject]@{
            fixed_name=$name;before_exists=$before[$name].exists
            before_size_bytes=$before[$name].size_bytes;before_sha256=$before[$name].sha256
            before_last_write_time_utc=$before[$name].last_write_time_utc
            after_exists=$exists;after_size_bytes=$(if($exists){(Get-Item -LiteralPath $path).Length}else{0})
            after_sha256=$afterHash
            restored_exactly=(($before[$name].exists -eq $exists) -and ($before[$name].sha256 -eq $afterHash))
        }
    }
    $restore | Export-Csv -LiteralPath (Join-Path $resolvedRoot 'installation_restore_evidence.csv') -NoTypeInformation -Encoding UTF8
}
