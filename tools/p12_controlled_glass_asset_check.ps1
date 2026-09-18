[CmdletBinding()]
param([string]$PandaBin='F:\Programs\Panda3D-1.10.15-x64\bin')
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Split-Path -Parent $PSScriptRoot)).Path
$asset=Join-Path $repo 'HwaSim_IR\Bin\Config\TargetLib\p12\controlled_glass'
$out=Join-Path $repo 'logs\p12\p12c\controlled_glass\asset'
New-Item -ItemType Directory -Force -Path $out|Out-Null
& python.exe (Join-Path $PSScriptRoot 'p12_build_controlled_glass_rig.py')
if($LASTEXITCODE -ne 0){throw 'P12 glass source generation failed'}
$obj=Join-Path $asset 'p12_controlled_glass.obj';$egg=Join-Path $asset 'p12_controlled_glass.egg';$bam=Join-Path $asset 'p12_controlled_glass.bam'
& (Join-Path $PandaBin 'obj2egg.exe') -noabs -nv 42 -uo m -o $egg $obj
if($LASTEXITCODE -ne 0){throw 'obj2egg failed'}
& python.exe (Join-Path $PSScriptRoot 'p12_finalize_controlled_glass_egg.py') $egg
if($LASTEXITCODE -ne 0){throw 'glass EGG tagging failed'}
& (Join-Path $PandaBin 'egg-trans.exe') -noabs -c -o (Join-Path $out 'roundtrip.egg') $egg
if($LASTEXITCODE -ne 0){throw 'egg-trans failed'}
& (Join-Path $PandaBin 'egg2bam.exe') -ps rel -pd $asset -o $bam $egg
if($LASTEXITCODE -ne 0){throw 'egg2bam failed'}
$required=@('p12_controlled_glass.obj','p12_controlled_glass.egg','p12_controlled_glass.bam','p12_controlled_glass_visible.ppm','p12_controlled_glass_material_id.pgm','p12_controlled_glass_bright.xml','p12_controlled_glass_dark.xml','manifest.json','README.md')
$rows=@();foreach($name in $required){$p=Join-Path $asset $name;if(!(Test-Path $p)){throw "missing $name"};$rows+=[ordered]@{file=$name;bytes=(Get-Item $p).Length;sha256=(Get-FileHash $p -Algorithm SHA256).Hash.ToLowerInvariant()}}
$eggText=Get-Content $egg -Raw;foreach($id in 31,32){if($eggText -notmatch "<Tag>\s+p11_material_id\s+\{\s+$id\s+\}"){throw "missing material tag $id"}}
foreach($variant in 'bright','dark'){[xml]$x=Get-Content (Join-Path $asset "p12_controlled_glass_$variant.xml") -Raw;foreach($entry in @($x.Composite_Material_Table.Composite_Material)){$s=[double]$entry.SWIRReflectance+[double]$entry.SWIREmissivity+[double]$entry.SWIRTransmissivity;$m=[double]$entry.MWIRReflectance+[double]$entry.MWIREmissivity+[double]$entry.MWIRTransmissivity;if([Math]::Abs($s-1)-gt 1e-9 -or [Math]::Abs($m-1)-gt 1e-9){throw "energy balance $variant id=$($entry.index)"}}}
$result=[ordered]@{schema='hwasimir.p12.controlled-glass-asset-check.v1';result='PASS';originalP11AssetModified=$false;materials=@(31,32);backgrounds=@('Bright','Dark');model='single straight-through';calibration='NOT_VERIFIED_CALIBRATION';files=$rows}
[IO.File]::WriteAllText((Join-Path $out 'asset_check.json'),($result|ConvertTo-Json -Depth 6)+"`n",(New-Object Text.UTF8Encoding($false)))
$result|ConvertTo-Json -Compress -Depth 6
