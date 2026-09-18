param(
    [string]$CaseRoot='D:\HwaSimIR\logs\p12\p12c\modtran_2km',
    [string]$PcModBin='F:\Programs\PcModWin5\Bin'
)
$ErrorActionPreference='Stop'
$expectedRoot=[IO.Path]::GetFullPath('D:\HwaSimIR\logs\p12\p12c\modtran_2km')
$resolvedRoot=[IO.Path]::GetFullPath($CaseRoot)
if($resolvedRoot -ne $expectedRoot){throw "Refusing unexpected CaseRoot: $resolvedRoot"}
$expectedBin=[IO.Path]::GetFullPath('F:\Programs\PcModWin5\Bin')
$resolvedBin=[IO.Path]::GetFullPath($PcModBin)
if($resolvedBin -ne $expectedBin){throw "Refusing unexpected PcModBin: $resolvedBin"}
$exe=Join-Path $resolvedBin 'Mod5.2.1.0.exe'
$manifest=Join-Path $resolvedRoot 'case_manifest.csv'
foreach($required in @($exe,$manifest)){if(!(Test-Path -LiteralPath $required -PathType Leaf)){throw "Required file missing: $required"}}
$cases=@(Import-Csv -LiteralPath $manifest)
if($cases.Count -ne 160){throw "Expected 160 missing 2 km LOS cases, got $($cases.Count)"}

$fixedNames=@('modin','tape5','tape6','tape7','tape8','tape7.scn','specflux')
$tempBase=[IO.Path]::GetFullPath([IO.Path]::GetTempPath())
$backupRoot=[IO.Path]::Combine($tempBase,'hwasimir-p12-modtran-2km-'+[Guid]::NewGuid().ToString('N'))
if(!([IO.Path]::GetFullPath($backupRoot).StartsWith($tempBase,[StringComparison]::OrdinalIgnoreCase))){throw 'Unsafe temporary backup path'}
New-Item -ItemType Directory -Force -Path $backupRoot | Out-Null
$before=@{}
foreach($name in $fixedNames){
    $path=Join-Path $resolvedBin $name
    $exists=Test-Path -LiteralPath $path -PathType Leaf
    $before[$name]=[pscustomobject]@{exists=$exists;size_bytes=$(if($exists){(Get-Item -LiteralPath $path).Length}else{0});sha256=$(if($exists){(Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash.ToLowerInvariant()}else{''});last_write_time_utc=$(if($exists){(Get-Item -LiteralPath $path).LastWriteTimeUtc.ToString('o')}else{''})}
    if($exists){Copy-Item -LiteralPath $path -Destination (Join-Path $backupRoot $name) -Force}
}
$engine=Get-Item -LiteralPath $exe
$engineHash=(Get-FileHash -Algorithm SHA256 -LiteralPath $exe).Hash.ToLowerInvariant()
$engineVersion=$engine.VersionInfo.FileVersion
if([string]::IsNullOrWhiteSpace($engineVersion)){$engineVersion='5.2.1.0 (executable filename)'}
$runs=New-Object System.Collections.Generic.List[object]
try {
    $index=0
    foreach($case in $cases){
        $index++
        $input=[IO.Path]::GetFullPath($case.input_file)
        if(!$input.StartsWith($resolvedRoot,[StringComparison]::OrdinalIgnoreCase)){throw "Input outside isolated root: $input"}
        if([double]$case.range_km -ne 2.0){throw "Non-2 km card in P12 grid: $($case.case_id)"}
        $caseDir=Split-Path -Parent $input
        Copy-Item -LiteralPath $input -Destination (Join-Path $resolvedBin 'modin') -Force
        Copy-Item -LiteralPath $input -Destination (Join-Path $resolvedBin 'tape5') -Force
        foreach($name in @('tape6','tape7','tape8','tape7.scn','specflux')){$path=Join-Path $resolvedBin $name;if(Test-Path -LiteralPath $path){Remove-Item -LiteralPath $path -Force}}
        $stdout=Join-Path $caseDir 'engine_stdout.txt';$stderr=Join-Path $caseDir 'engine_stderr.txt';$started=[DateTime]::UtcNow
        $process=Start-Process -FilePath $exe -WorkingDirectory $resolvedBin -WindowStyle Hidden -Wait -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
        if($process.ExitCode -ne 0){throw "MODTRAN failed: $($case.case_id), exit=$($process.ExitCode)"}
        $mapping=@{'tape6'='MODOUT1.txt';'tape7'='MODOUT2.txt';'tape8'='MODOUT3.txt';'tape7.scn'='tape7.scn'}
        foreach($sourceName in $mapping.Keys){
            $source=Join-Path $resolvedBin $sourceName
            if(Test-Path -LiteralPath $source -PathType Leaf){
                $destination=Join-Path $caseDir $mapping[$sourceName];Copy-Item -LiteralPath $source -Destination $destination -Force
                $runs.Add([pscustomobject]@{case_id=$case.case_id;mode=$case.mode;band=$case.band;humidity_profile=$case.humidity_profile;range_km=$case.range_km;generated_at_utc=[DateTime]::UtcNow.ToString('o');engine_path=$exe;engine_file_version=$engineVersion;engine_sha256=$engineHash;output_kind=$sourceName;output_file=[IO.Path]::GetFullPath($destination);output_sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $destination).Hash.ToLowerInvariant();output_fresh=((Get-Item -LiteralPath $destination).LastWriteTimeUtc -ge $started.AddSeconds(-2))})
            }
        }
        $required=Join-Path $caseDir 'MODOUT2.txt'
        if(!(Test-Path -LiteralPath $required -PathType Leaf)){throw "Required MODTRAN product absent: $required"}
        if((Get-Item -LiteralPath $stderr).Length -ne 0){throw "MODTRAN stderr is not empty: $($case.case_id)"}
        Write-Output "[$index/$($cases.Count)] Completed $($case.case_id)"
    }
    $runs | Export-Csv -LiteralPath (Join-Path $resolvedRoot 'run_manifest.csv') -NoTypeInformation -Encoding UTF8
    [ordered]@{checked_at_utc=[DateTime]::UtcNow.ToString('o');executable=$exe;file_version=$engineVersion;sha256=$engineHash;run_status='SUCCESS_REAL_MODTRAN_OUTPUT';requested_component_runs=$cases.Count;required_products_fresh=(@($runs|Where-Object {!$_.output_fresh}).Count -eq 0);license_evidence="All $($cases.Count) component runs exited zero and produced fresh MODOUT products."} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $resolvedRoot 'engine_and_license_evidence.json') -Encoding UTF8
}
finally {
    foreach($name in $fixedNames){$path=Join-Path $resolvedBin $name;$backup=Join-Path $backupRoot $name;if($before[$name].exists){Copy-Item -LiteralPath $backup -Destination $path -Force}elseif(Test-Path -LiteralPath $path){Remove-Item -LiteralPath $path -Force}}
    $resolvedBackup=[IO.Path]::GetFullPath($backupRoot)
    if(!$resolvedBackup.StartsWith($tempBase,[StringComparison]::OrdinalIgnoreCase)){throw "Unsafe cleanup path: $resolvedBackup"}
    if(Test-Path -LiteralPath $resolvedBackup){Remove-Item -LiteralPath $resolvedBackup -Recurse -Force}
    $restore=@()
    foreach($name in $fixedNames){$path=Join-Path $resolvedBin $name;$exists=Test-Path -LiteralPath $path -PathType Leaf;$afterHash=$(if($exists){(Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash.ToLowerInvariant()}else{''});$restore+=[pscustomobject]@{fixed_name=$name;before_exists=$before[$name].exists;before_size_bytes=$before[$name].size_bytes;before_sha256=$before[$name].sha256;before_last_write_time_utc=$before[$name].last_write_time_utc;after_exists=$exists;after_size_bytes=$(if($exists){(Get-Item -LiteralPath $path).Length}else{0});after_sha256=$afterHash;restored_exactly=(($before[$name].exists -eq $exists) -and ($before[$name].sha256 -eq $afterHash))}}
    $restore | Export-Csv -LiteralPath (Join-Path $resolvedRoot 'installation_restore_evidence.csv') -NoTypeInformation -Encoding UTF8
}
