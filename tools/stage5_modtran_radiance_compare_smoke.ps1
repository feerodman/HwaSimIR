param(
    [int]$DelayMs = 500
)

$ErrorActionPreference = "Stop"

$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
$rootPath = $root.Path
$hwaExe = Join-Path $rootPath "HwaSim_IR\Bin\HwaSim_IR.exe"
$hwaWorkDir = Join-Path $rootPath "HwaSim_IR\Bin"
$networkConfig = Join-Path $hwaWorkDir "Config\NetworkConfig.ini"
$runtimeConfig = Join-Path $hwaWorkDir "Config\HwaSimIRRuntime.ini"
$logDir = Join-Path $rootPath "logs\stage5"
$summaryPath = Join-Path $logDir "modtran_radiance_compare_summary.md"
$csvPath = Join-Path $logDir "modtran_radiance_compare_summary.csv"

function Assert-Path {
    param([string]$Path, [string]$Label)
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "$Label not found: $Path"
    }
}

function Normalize-ProcessPathEnvironment {
    $processPathValue = [Environment]::GetEnvironmentVariable("Path", "Process")
    if ([string]::IsNullOrEmpty($processPathValue)) {
        $processPathValue = [Environment]::GetEnvironmentVariable("PATH", "Process")
    }
    if (-not [string]::IsNullOrEmpty($processPathValue)) {
        [Environment]::SetEnvironmentVariable("PATH", $null, "Process")
        [Environment]::SetEnvironmentVariable("Path", $processPathValue, "Process")
    }
}

function New-InitPacket {
    param(
        [int]$Band,
        [double]$VisibilityM
    )

    $ms = New-Object System.IO.MemoryStream
    $bw = New-Object System.IO.BinaryWriter($ms)

    function WInt([int]$value) { $bw.Write($value) }
    function WBool([bool]$value) { $bw.Write([byte]$(if ($value) { 1 } else { 0 })) }
    function WDouble([double]$value) { $bw.Write($value) }
    function WSpatial([double]$lat, [double]$lon, [double]$alt, [double]$yaw, [double]$pitch, [double]$roll, [double]$speed) {
        WDouble $lat; WDouble $lon; WDouble $alt
        WDouble $yaw; WDouble $pitch; WDouble $roll
        WDouble $speed
    }
    function WPlat([int]$id, [int]$type) {
        WInt $id
        WInt $type
        WSpatial 0.0 0.0 10000.0 0.0 0.0 0.0 0.0
    }
    function WSensor([int]$sensorBand) {
        WInt 0
        WBool $true
        WBool $true
        WBool $false
        WDouble 0.1
        WDouble 0.1
        WBool $false
        WDouble 0.0
        WBool $false
        WBool $false
        WInt $sensorBand
        WInt 640
        WInt 512
        WInt 1
        WInt 50000
        WDouble 0.0
        for ($i = 0; $i -lt 32; $i++) {
            WDouble 0.0
        }
        WInt 0
        WDouble 0.0
    }

    WInt 0x36
    WInt 1
    WInt 1
    WInt 1
    WInt 1
    WPlat 1 0x11
    WPlat 2 0x11

    WBool $true
    WInt 0
    WInt 0
    WDouble 0.0
    WDouble 0.0
    WDouble 0.0
    WDouble 0.0
    WDouble 1.0
    WDouble 1.0
    WDouble 25.0
    WDouble 25.0
    WDouble 40.0
    WDouble $VisibilityM
    WDouble 0.0
    WDouble 0.0
    WInt 30
    WSensor $Band

    WInt 3
    WInt 2
    WInt 0

    $bytes = $ms.ToArray()
    $bw.Dispose()
    $ms.Dispose()
    return $bytes
}

function New-ControlStartPacket {
    $ms = New-Object System.IO.MemoryStream
    $bw = New-Object System.IO.BinaryWriter($ms)
    $bw.Write([int]0x41)
    $bw.Write([int]1)
    $bw.Write([int]1)
    $bw.Write([int]2)
    $bw.Write([int]1)
    $bw.Write([int]0)
    $bytes = $ms.ToArray()
    $bw.Dispose()
    $ms.Dispose()
    return $bytes
}

function New-DisplayPacket {
    param(
        [double]$TimeMs,
        [double]$RangeKm,
        [double]$TargetAltKm
    )

    $ms = New-Object System.IO.MemoryStream
    $bw = New-Object System.IO.BinaryWriter($ms)
    $lonOffsetDeg = $RangeKm / 111.32

    function WInt([int]$value) { $bw.Write($value) }
    function WBool([bool]$value) { $bw.Write([byte]$(if ($value) { 1 } else { 0 })) }
    function WDouble([double]$value) { $bw.Write($value) }
    function WSpatial([double]$lat, [double]$lon, [double]$alt, [double]$yaw, [double]$pitch, [double]$roll, [double]$speed) {
        WDouble $lat; WDouble $lon; WDouble $alt
        WDouble $yaw; WDouble $pitch; WDouble $roll
        WDouble $speed
    }
    function WWeaponState {
        WInt 0x22
        WInt 1
        WInt 0
        WDouble 0.0
        WDouble 0.0
        WBool $false
        WBool $false
        WDouble 0.0
        WDouble 0.0
        WBool $true
        WInt 0
        WBool $false
        WInt 0
    }
    function WTargetState([int]$TargetType, [int]$TargetId, [bool]$Visible, [double]$LonOffset, [double]$AltM) {
        WInt $TargetType
        WInt 1
        WInt $TargetId
        WBool $false
        WBool $Visible
        WSpatial 0.0 $LonOffset $AltM 0.0 0.0 0.0 0.0
        WInt 0x01
    }

    WInt 0x38
    WInt 1
    WInt 1
    WDouble $TimeMs
    WSpatial 0.0 0.0 10000.0 0.0 0.0 0.0 0.0
    WWeaponState
    WInt 1
    WTargetState 0x22 0 $true $lonOffsetDeg ($TargetAltKm * 1000.0)
    WTargetState 0x22 1 $false 0.015 3000.0
    WTargetState 0x22 2 $false 0.020 3000.0
    WTargetState 0x33 3 $false 0.025 3000.0
    WTargetState 0x33 4 $false 0.030 3000.0

    $bytes = $ms.ToArray()
    $bw.Dispose()
    $ms.Dispose()
    return $bytes
}

function Set-Stage5ModtranEnv {
    [Environment]::SetEnvironmentVariable("EnableIRPhysicalPipeline", "1", "Process")
    [Environment]::SetEnvironmentVariable("Stage5RadianceDebugView", "Off", "Process")
    [Environment]::SetEnvironmentVariable("Stage5RadianceLogComponents", "0", "Process")
    [Environment]::SetEnvironmentVariable("EnableStage5ModtranRadianceDebug", "1", "Process")
    [Environment]::SetEnvironmentVariable("UseModtranPathRuntime", "0", "Process")
    [Environment]::SetEnvironmentVariable("UseModtranSkyRuntime", "0", "Process")
    [Environment]::SetEnvironmentVariable("UseModtranSolarRuntime", "0", "Process")
    [Environment]::SetEnvironmentVariable("Stage5ModtranRadianceLogEveryFrames", "1", "Process")
    [Environment]::SetEnvironmentVariable("Stage5ModtranRadianceCompareLegacy", "1", "Process")
    [Environment]::SetEnvironmentVariable("EnableIRVerboseLog", "0", "Process")
    [Environment]::SetEnvironmentVariable("LegacyEngineBodyHeating", "0", "Process")
}

function Clear-Stage5ModtranEnv {
    foreach ($name in @(
        "EnableIRPhysicalPipeline",
        "Stage5RadianceDebugView",
        "Stage5RadianceLogComponents",
        "EnableStage5ModtranRadianceDebug",
        "UseModtranPathRuntime",
        "UseModtranSkyRuntime",
        "UseModtranSolarRuntime",
        "Stage5ModtranRadianceLogEveryFrames",
        "Stage5ModtranRadianceCompareLegacy",
        "EnableIRVerboseLog",
        "LegacyEngineBodyHeating")) {
        [Environment]::SetEnvironmentVariable($name, $null, "Process")
    }
}

function Invoke-HwaStage5ModtranRun {
    param(
        [string]$Label,
        [int]$Band,
        [double]$VisibilityKm,
        [object[]]$DisplayCases
    )

    $safeName = ($Label -replace '[^A-Za-z0-9_-]', '_')
    $stamp = Get-Date -Format "yyyyMMdd-HHmmss-fff"
    $stdout = Join-Path $logDir "HwaSimIR-stage5-modtran-$safeName-$stamp.out.log"
    $stderr = Join-Path $logDir "HwaSimIR-stage5-modtran-$safeName-$stamp.err.log"
    $process = $null
    $udp = $null
    $networkConfigBackup = [System.IO.File]::ReadAllBytes($networkConfig)
    $runtimeConfigBackup = [System.IO.File]::ReadAllBytes($runtimeConfig)

    try {
        Normalize-ProcessPathEnvironment
        Set-Stage5ModtranEnv
        $utf8NoBom = New-Object System.Text.UTF8Encoding($false)
        $networkText = [System.IO.File]::ReadAllText($networkConfig, $utf8NoBom)
        $networkText = [regex]::Replace($networkText, "(?m)^localIp=.*$", "localIp=127.0.0.1")
        $networkText = [regex]::Replace($networkText, "(?m)^remoteIp=.*$", "remoteIp=127.0.0.1")
        $networkText = [regex]::Replace($networkText, "(?m)^serverIp=.*$", "serverIp=127.0.0.1")
        [System.IO.File]::WriteAllText($networkConfig, $networkText, $utf8NoBom)

        $process = Start-Process -FilePath $hwaExe -WorkingDirectory $hwaWorkDir -PassThru -WindowStyle Hidden -RedirectStandardOutput $stdout -RedirectStandardError $stderr
        Start-Sleep -Seconds 3

        $udp = New-Object System.Net.Sockets.UdpClient
        $packet = New-InitPacket -Band $Band -VisibilityM ($VisibilityKm * 1000.0)
        [void]$udp.Send($packet, $packet.Length, "127.0.0.1", 8888)
        Start-Sleep -Milliseconds $DelayMs

        $control = New-ControlStartPacket
        [void]$udp.Send($control, $control.Length, "127.0.0.1", 8888)
        Start-Sleep -Milliseconds ([Math]::Max($DelayMs, 2000))

        $seq = 1
        foreach ($case in $DisplayCases) {
            $display = New-DisplayPacket -TimeMs ([double](($Band * 100000) + $seq * 33)) -RangeKm ([double]$case.RangeKm) -TargetAltKm ([double]$case.TargetAltKm)
            [void]$udp.Send($display, $display.Length, "127.0.0.1", 8888)
            Start-Sleep -Milliseconds $DelayMs
            $seq++
        }
        Start-Sleep -Seconds 4
    }
    finally {
        if ($udp) {
            $udp.Close()
        }
        if ($process) {
            $process.Refresh()
            if (-not $process.HasExited) {
                Stop-Process -Id $process.Id -Force
            }
        }
        [System.IO.File]::WriteAllBytes($networkConfig, $networkConfigBackup)
        [System.IO.File]::WriteAllBytes($runtimeConfig, $runtimeConfigBackup)
        Clear-Stage5ModtranEnv
    }

    $text = ""
    if (Test-Path -LiteralPath $stdout) {
        $text = Get-Content -LiteralPath $stdout -Raw -Encoding UTF8
    }
    return [PSCustomObject]@{
        Label = $Label
        Band = $Band
        VisibilityKm = $VisibilityKm
        Stdout = $stdout
        Stderr = $stderr
        Text = $text
    }
}

function Get-ModtranCompareRows {
    param(
        [object]$Run
    )

    $pattern = "(?m)^\[Stage5 ModtranRadianceCompare\].*?\bsourceSeq=(?<seq>\d+).*?\btargetID=(?<targetID>-?\d+).*?\bband=(?<band>\w+).*?\brangeKm=(?<range>[-+0-9eE.]+).*?\bobserverAltKm=(?<obs>[-+0-9eE.]+).*?\btargetAltKm=(?<tgt>[-+0-9eE.]+).*?\bvisibilityKm=(?<vis>[-+0-9eE.]+).*?\blegacyPath=(?<legacy>[-+0-9eE.]+).*?\bmodtranPath=(?<path>[-+0-9eE.]+).*?\bmodtranSky=(?<sky>[-+0-9eE.]+).*?\bmodtranSolar=(?<solar>[-+0-9eE.]+).*?\btauUp=(?<tau>[-+0-9eE.]+).*?\bvalid=(?<valid>[01]).*?\bfallbackReason=(?<fallback>\S+).*?\binterpolationMode=(?<interp>\S+).*?\bpathRadianceSource=(?<source>\S+).*?\bratio=(?<ratio>[-+0-9eE.]+).*?\bdiff=(?<diff>[-+0-9eE.]+)"
    $matches = @([regex]::Matches($Run.Text, $pattern) | Where-Object { [int]$_.Groups["targetID"].Value -eq 0 })
    foreach ($m in $matches) {
        [PSCustomObject]@{
            run = $Run.Label
            sourceSeq = [int]$m.Groups["seq"].Value
            band = $m.Groups["band"].Value
            rangeKm = [double]$m.Groups["range"].Value
            obsAltKm = [double]$m.Groups["obs"].Value
            tgtAltKm = [double]$m.Groups["tgt"].Value
            visibilityKm = [double]$m.Groups["vis"].Value
            legacyPath = [double]$m.Groups["legacy"].Value
            modtranPath = [double]$m.Groups["path"].Value
            modtranSky = [double]$m.Groups["sky"].Value
            modtranSolar = [double]$m.Groups["solar"].Value
            tauUp = [double]$m.Groups["tau"].Value
            valid = $m.Groups["valid"].Value
            fallbackReason = $m.Groups["fallback"].Value
            interpolationMode = $m.Groups["interp"].Value
            pathRadianceSource = $m.Groups["source"].Value
            ratio = [double]$m.Groups["ratio"].Value
            diff = [double]$m.Groups["diff"].Value
            stdout = $Run.Stdout
            stderr = $Run.Stderr
        }
    }
}

Assert-Path $hwaExe "HwaSimIR executable"
Assert-Path $hwaWorkDir "HwaSimIR working directory"
Assert-Path $networkConfig "HwaSimIR network config"
Assert-Path $runtimeConfig "HwaSimIR runtime config"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null

$mwirCases = @()
foreach ($range in @(5.0, 20.0, 50.0)) {
    foreach ($alt in @(3.0, 10.0, 20.0)) {
        $mwirCases += [PSCustomObject]@{ RangeKm = $range; TargetAltKm = $alt }
    }
}
$singleValidCase = @([PSCustomObject]@{ RangeKm = 20.0; TargetAltKm = 10.0 })
$outOfRangeCase = @([PSCustomObject]@{ RangeKm = 80.0; TargetAltKm = 10.0 })

Write-Host "Stage5 MODTRAN radiance compare smoke"
Write-Host "HwaSimIR: $hwaExe"

$runs = @()
foreach ($visibility in @(5.0, 15.0, 30.0)) {
    Write-Host "running MWIR visibility=$visibility km"
    $runs += Invoke-HwaStage5ModtranRun -Label "mwir_vis$visibility" -Band 2 -VisibilityKm $visibility -DisplayCases $mwirCases
}
Write-Host "running NIR solar query"
$runs += Invoke-HwaStage5ModtranRun -Label "nir_solar" -Band 1 -VisibilityKm 15.0 -DisplayCases $singleValidCase
Write-Host "running SWIR missing-band fallback"
$runs += Invoke-HwaStage5ModtranRun -Label "swir_missing" -Band 0 -VisibilityKm 15.0 -DisplayCases $singleValidCase
Write-Host "running MWIR out-of-range fallback"
$runs += Invoke-HwaStage5ModtranRun -Label "mwir_out_of_range" -Band 2 -VisibilityKm 15.0 -DisplayCases $outOfRangeCase

$rows = @()
foreach ($run in $runs) {
    if ($run.Text -match "pathRadianceSource=modtran_runtime") {
        throw "Unexpected modtran_runtime path source with UseModtranPathRuntime=false in $($run.Stdout)"
    }
    $rows += @(Get-ModtranCompareRows -Run $run)
}

if ($rows.Count -eq 0) {
    throw "No [Stage5 ModtranRadianceCompare] rows captured"
}

$rows | Export-Csv -NoTypeInformation -Encoding UTF8 -LiteralPath $csvPath

$mwirValid = @($rows | Where-Object { $_.band -eq "MWIR" -and $_.valid -eq "1" })
$nirRows = @($rows | Where-Object { $_.band -eq "NIR" })
$swirRows = @($rows | Where-Object { $_.run -eq "swir_missing" })
$fallbackRows = @($rows | Where-Object { $_.valid -eq "0" })

if ($mwirValid.Count -eq 0) {
    throw "No valid MWIR MODTRAN compare rows captured"
}
if (@($nirRows | Where-Object { $_.modtranSolar -gt 0 }).Count -eq 0) {
    throw "NIR solar irradiance was not found"
}
if (@($swirRows | Where-Object { $_.valid -eq "0" -and $_.fallbackReason -eq "missing_band" }).Count -eq 0) {
    throw "SWIR missing-band fallback was not observed"
}
if (@($fallbackRows | Where-Object { $_.fallbackReason -in @("invalid_geometry", "out_of_lut_range", "missing_band") }).Count -eq 0) {
    throw "Expected invalid_geometry/out_of_lut_range/missing_band fallback was not observed"
}

$md = New-Object System.Collections.Generic.List[string]
$md.Add("# Stage5 MODTRAN Radiance Compare Smoke Summary") | Out-Null
$md.Add("") | Out-Null
$md.Add("CSV: $csvPath") | Out-Null
$md.Add("") | Out-Null
$md.Add("| Run | Band | Valid | Range km | Obs alt km | Target alt km | Visibility km | Legacy path | MODTRAN path | MODTRAN sky | MODTRAN solar | Ratio | Diff | Fallback | Interp | Source |") | Out-Null
$md.Add("|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|---|---|") | Out-Null
foreach ($row in $rows) {
    $md.Add("| $($row.run) | $($row.band) | $($row.valid) | $($row.rangeKm) | $($row.obsAltKm) | $($row.tgtAltKm) | $($row.visibilityKm) | $($row.legacyPath) | $($row.modtranPath) | $($row.modtranSky) | $($row.modtranSolar) | $($row.ratio) | $($row.diff) | $($row.fallbackReason) | $($row.interpolationMode) | $($row.pathRadianceSource) |") | Out-Null
}
$md.Add("") | Out-Null
$md.Add("Checks: UseModtranPathRuntime=false keeps pathRadianceSource away from modtran_runtime; valid MWIR rows exist; NIR solar is readable; SWIR missing band and out-of-range/invalid geometry fallback are explicit.") | Out-Null
$md | Set-Content -LiteralPath $summaryPath -Encoding UTF8

$rows | Format-Table -AutoSize

Write-Host ""
Write-Host "Summary:"
Write-Host $summaryPath
Write-Host $csvPath
Write-Host ""
Write-Host "Stage5 MODTRAN radiance compare smoke passed." -ForegroundColor Green
