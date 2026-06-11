param(
    [int[]]$Bands = @(1, 2),
    [int]$DelayMs = 500
)

$ErrorActionPreference = "Stop"

$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
$rootPath = $root.Path
$hwaExe = Join-Path $rootPath "HwaSim_IR\Bin\HwaSim_IR.exe"
$hwaWorkDir = Join-Path $rootPath "HwaSim_IR\Bin"
$logDir = Join-Path $rootPath "logs\stage3_modtran_tau_ab"
$metricsCsv = Join-Path $logDir "ab_metrics.csv"
$summaryMd = Join-Path $logDir "ab_summary.md"
$loaderSource = Join-Path $rootPath "HwaSim_IR\HwaSim_IR\IR\IRModtranTauLut.cpp"
$atmosphereSource = Join-Path $rootPath "HwaSim_IR\HwaSim_IR\IRSimulation.cpp"
$appSource = Join-Path $rootPath "HwaSim_IR\HwaSim_IR\HwaSimIR.cpp"

if ($Bands.Count -eq 1 -and $Bands[0] -eq 1) {
    $Bands = @(1, 2)
}

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
    param([int]$Band)

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
        WSpatial 0.0 0.0 1000.0 0.0 0.0 0.0 0.0
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
    WDouble 1.0
    WDouble 25.0
    WDouble 40.0
    WDouble 23000.0
    WDouble 0.0
    WDouble 0.0
    WInt 30
    WSensor $Band

    WInt 5
    WInt 5
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
    param([double]$TimeMs)

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
    function WTargetState([int]$targetType, [int]$targetId, [double]$lonOffset, [double]$altMeters) {
        WInt $targetType
        WInt 1
        WInt $targetId
        WBool $false
        WBool $true
        WSpatial 0.0 $lonOffset $altMeters 0.0 0.0 0.0 0.0
        WInt 0x01
    }

    WInt 0x38
    WInt 1
    WInt 1
    WDouble $TimeMs
    WSpatial 0.0 0.0 1000.0 0.0 0.0 0.0 0.0
    WWeaponState
    WInt 5
    WTargetState 0x22 0 0.010 1000.0
    WTargetState 0x22 1 0.015 1000.0
    WTargetState 0x22 2 0.020 1000.0
    WTargetState 0x33 3 0.025 1000.0
    WTargetState 0x33 4 0.030 1000.0

    $bytes = $ms.ToArray()
    $bw.Dispose()
    $ms.Dispose()
    return $bytes
}

function Invoke-HwaRun {
    param([string]$Label, [string]$UseValue)

    $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $stdout = Join-Path $logDir "HwaSimIR-modtran-tau-ab-$Label-$stamp.out.log"
    $stderr = Join-Path $logDir "HwaSimIR-modtran-tau-ab-$Label-$stamp.err.log"
    $oldDebug = [Environment]::GetEnvironmentVariable("EnableModtranTauDebug", "Process")
    $oldUse = [Environment]::GetEnvironmentVariable("UseModtranTauForAtmosphere", "Process")

    $process = $null
    $udp = $null
    try {
        [Environment]::SetEnvironmentVariable("EnableModtranTauDebug", "1", "Process")
        [Environment]::SetEnvironmentVariable("UseModtranTauForAtmosphere", $UseValue, "Process")
        Normalize-ProcessPathEnvironment
        $process = Start-Process -FilePath $hwaExe -WorkingDirectory $hwaWorkDir -PassThru -WindowStyle Hidden -RedirectStandardOutput $stdout -RedirectStandardError $stderr
        Start-Sleep -Seconds 3

        $udp = New-Object System.Net.Sockets.UdpClient
        foreach ($band in $Bands) {
            $packet = New-InitPacket -Band $band
            $sent = $udp.Send($packet, $packet.Length, "127.0.0.1", 8888)
            Write-Host "sent $Label init band=$band bytes=$sent"
            Start-Sleep -Milliseconds $DelayMs

            $control = New-ControlStartPacket
            $sentControl = $udp.Send($control, $control.Length, "127.0.0.1", 8888)
            Write-Host "sent $Label control-start bytes=$sentControl"
            Start-Sleep -Milliseconds $DelayMs

            for ($frame = 0; $frame -lt 3; ++$frame) {
                $display = New-DisplayPacket -TimeMs ([double](($frame + 1) * 33))
                $sentDisplay = $udp.Send($display, $display.Length, "127.0.0.1", 8888)
                Write-Host "sent $Label display frame=$($frame + 1) bytes=$sentDisplay"
                Start-Sleep -Milliseconds $DelayMs
            }
        }
        Start-Sleep -Seconds 5
    }
    finally {
        if ($udp) {
            $udp.Close()
        }
        if ($process) {
            $process.Refresh()
            if (-not $process.HasExited) {
                Stop-Process -Id $process.Id -Force
                Write-Host "stopped $Label pid=$($process.Id)"
            }
        }
        [Environment]::SetEnvironmentVariable("EnableModtranTauDebug", $oldDebug, "Process")
        [Environment]::SetEnvironmentVariable("UseModtranTauForAtmosphere", $oldUse, "Process")
    }

    return [PSCustomObject]@{
        Label  = $Label
        Active = $UseValue
        Stdout = $stdout
        Stderr = $stderr
        Text   = $(if (Test-Path -LiteralPath $stdout) { Get-Content -LiteralPath $stdout -Raw -Encoding UTF8 } else { "" })
    }
}

function Convert-DebugLineToRecord {
    param(
        [string]$Line,
        [string]$RunLabel,
        [string]$Active,
        [string]$LogFile
    )

    $values = @{}
    foreach ($match in [regex]::Matches($Line, "([A-Za-z_]+)=([^ ]+)")) {
        $values[$match.Groups[1].Value] = $match.Groups[2].Value.Trim()
    }

    function NumberOrBlank([string]$Name) {
        if (-not $values.ContainsKey($Name)) {
            return ""
        }
        $parsed = 0.0
        if ([double]::TryParse($values[$Name], [System.Globalization.NumberStyles]::Float, [System.Globalization.CultureInfo]::InvariantCulture, [ref]$parsed)) {
            return $parsed
        }
        return ""
    }

    $oldTau = NumberOrBlank "old_tau"
    $newTau = NumberOrBlank "new_tau"
    $diff = NumberOrBlank "diff"
    $ratio = ""
    if ($oldTau -is [double] -and $newTau -is [double] -and [math]::Abs($oldTau) -gt 1.0e-12) {
        $ratio = $newTau / $oldTau
    }

    $warnings = New-Object System.Collections.Generic.List[string]
    if ($diff -is [double] -and [math]::Abs($diff) -gt 0.5) {
        $warnings.Add("WARNING_LARGE_DIFF") | Out-Null
    }
    if ($ratio -is [double] -and [math]::Abs($ratio) -gt 10.0) {
        $warnings.Add("WARNING_LARGE_RATIO") | Out-Null
    }
    if ($values.ContainsKey("warning") -and -not [string]::IsNullOrWhiteSpace($values["warning"])) {
        $warnings.Add($values["warning"]) | Out-Null
    }

    [PSCustomObject]@{
        run_label            = $RunLabel
        active               = $Active
        band                 = $(if ($values.ContainsKey("band")) { $values["band"] } else { "" })
        old_tau              = $oldTau
        new_tau              = $newTau
        diff                 = $diff
        ratio                = $ratio
        return_source        = $(if ($values.ContainsKey("return_source")) { $values["return_source"] } else { "" })
        fallback_state       = $(if ($values.ContainsKey("fallback_state")) { $values["fallback_state"] } else { "" })
        fallback             = $(if ($values.ContainsKey("fallback")) { $values["fallback"] } else { "" })
        fallback_input       = $(if ($values.ContainsKey("fallback_input")) { $values["fallback_input"] } else { "" })
        source               = $(if ($values.ContainsKey("source")) { $values["source"] } else { "" })
        range_km             = $(if ($values.ContainsKey("range_km")) { $values["range_km"] } else { "" })
        visibility_km        = $(if ($values.ContainsKey("visibility_km")) { $values["visibility_km"] } else { "" })
        solar_zenith_deg     = $(if ($values.ContainsKey("solar_zenith_deg")) { $values["solar_zenith_deg"] } else { "" })
        warning              = ($warnings -join ";")
        frame_mean           = ""
        frame_min            = ""
        frame_max            = ""
        frame_std            = ""
        frame_metrics_status = "unavailable"
        log_file             = $LogFile
        line                 = $Line
    }
}

function Parse-DebugRecords {
    param([object]$Run)

    $records = @()
    $lines = $Run.Text -split "`r?`n"
    foreach ($line in $lines) {
        if ($line -match "MODTRAN Tau Debug") {
            $records += (Convert-DebugLineToRecord -Line $line -RunLabel $Run.Label -Active $Run.Active -LogFile $Run.Stdout)
        }
    }
    return $records
}

function Add-Check {
    param([string]$Name, [bool]$Ok, [string]$Detail)
    [PSCustomObject]@{
        Check  = $Name
        Status = $(if ($Ok) { "OK" } else { "FAIL" })
        Detail = $Detail
    }
}

function Test-SourceHasNoBandRadianceHook {
    $forbiddenPattern = "path_radiance_band|sky_radiance_band|path_scattering_radiance_band|solar_irradiance_band"
    foreach ($file in @($loaderSource, $atmosphereSource, $appSource)) {
        if (-not (Test-Path -LiteralPath $file -PathType Leaf)) {
            return $false
        }
        $text = Get-Content -LiteralPath $file -Raw -Encoding UTF8
        if ($text -match $forbiddenPattern) {
            return $false
        }
    }
    return $true
}

function Format-CountMap {
    param([object[]]$Rows, [string]$Field)
    $groups = @($Rows | Group-Object -Property $Field | Sort-Object Name)
    if ($groups.Count -eq 0) {
        return "none"
    }
    return (($groups | ForEach-Object { "$($_.Name)=$($_.Count)" }) -join ", ")
}

Assert-Path $hwaExe "HwaSimIR executable"
Assert-Path $hwaWorkDir "HwaSimIR working directory"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null

Write-Host "Stage 3 MODTRAN tau A/B smoke"
Write-Host "Bands: $($Bands -join ', ')"
Write-Host "HwaSimIR: $hwaExe"
Write-Host ""

$inactiveRun = Invoke-HwaRun -Label "active0" -UseValue "0"
$activeRun = Invoke-HwaRun -Label "active1" -UseValue "1"
$records = @()
$records += Parse-DebugRecords -Run $inactiveRun
$records += Parse-DebugRecords -Run $activeRun

if ($records.Count -gt 0) {
    $records | Export-Csv -LiteralPath $metricsCsv -NoTypeInformation -Encoding UTF8
}

$inactiveRecords = @($records | Where-Object { $_.active -eq "0" })
$activeRecords = @($records | Where-Object { $_.active -eq "1" })
$activeSupported = @($activeRecords | Where-Object { $_.band -in @("NIR", "MWIR") })
$checks = New-Object System.Collections.Generic.List[object]

$checks.Add((Add-Check "active=0 returns legacy" (($inactiveRecords.Count -gt 0) -and (@($inactiveRecords | Where-Object { $_.return_source -ne "legacy" }).Count -eq 0)) "active=0 return_source=legacy")) | Out-Null
$checks.Add((Add-Check "active=1 has NIR/MWIR records" (($activeSupported | Select-Object -ExpandProperty band -Unique).Count -ge 2) "NIR/MWIR debug records")) | Out-Null
$checks.Add((Add-Check "active=1 returns MODTRAN tau or explicit fallback" (($activeSupported.Count -gt 0) -and (@($activeSupported | Where-Object { $_.return_source -ne "modtran_tau" -and $_.fallback -ne "legacy" }).Count -eq 0)) "return_source=modtran_tau or fallback=legacy")) | Out-Null
$checks.Add((Add-Check "old/new/diff captured" (($records.Count -gt 0) -and (@($records | Where-Object { $_.old_tau -eq "" -or $_.new_tau -eq "" -or $_.diff -eq "" }).Count -eq 0)) "old_tau/new_tau/diff")) | Out-Null
$checks.Add((Add-Check "VIS/SWIR/LWIR stay legacy if present" (@($activeRecords | Where-Object { $_.band -in @("VIS", "SWIR", "LWIR") -and $_.return_source -eq "modtran_tau" }).Count -eq 0) "no VIS/SWIR/LWIR return_source=modtran_tau")) | Out-Null
$checks.Add((Add-Check "radiance fields disabled" (Test-SourceHasNoBandRadianceHook) "checked loader, atmosphere, app source")) | Out-Null

$summary = New-Object System.Collections.Generic.List[string]
$summary.Add("# Stage 3 MODTRAN Tau A/B Smoke") | Out-Null
$summary.Add("") | Out-Null
$summary.Add("Date: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')") | Out-Null
$summary.Add("") | Out-Null
$summary.Add("Bands requested: $($Bands -join ', ')") | Out-Null
$summary.Add("") | Out-Null
$summary.Add("Frame metrics: unavailable. The current UDP smoke path drives HwaSimIR and captures process logs, but it does not consume a rendered frame or JPEG stream for pixel statistics.") | Out-Null
$summary.Add("") | Out-Null
$summary.Add("## Metrics") | Out-Null
$summary.Add("") | Out-Null
$summary.Add("- CSV: $metricsCsv") | Out-Null
$summary.Add("- active=0 records: $($inactiveRecords.Count)") | Out-Null
$summary.Add("- active=1 records: $($activeRecords.Count)") | Out-Null
$summary.Add("- return_source counts: $(Format-CountMap -Rows $records -Field 'return_source')") | Out-Null
$summary.Add("- fallback_state counts: $(Format-CountMap -Rows $records -Field 'fallback_state')") | Out-Null
$summary.Add("- warning records: $(@($records | Where-Object { $_.warning -ne '' }).Count)") | Out-Null
$summary.Add("") | Out-Null
$summary.Add("## Checks") | Out-Null
$summary.Add("") | Out-Null
$summary.Add("| check | status | detail |") | Out-Null
$summary.Add("| --- | --- | --- |") | Out-Null
foreach ($check in $checks) {
    $summary.Add("| $($check.Check) | $($check.Status) | $($check.Detail) |") | Out-Null
}
$summary.Add("") | Out-Null
$summary.Add("## Logs") | Out-Null
$summary.Add("") | Out-Null
$summary.Add("- active=0 stdout: $($inactiveRun.Stdout)") | Out-Null
$summary.Add("- active=0 stderr: $($inactiveRun.Stderr)") | Out-Null
$summary.Add("- active=1 stdout: $($activeRun.Stdout)") | Out-Null
$summary.Add("- active=1 stderr: $($activeRun.Stderr)") | Out-Null
Set-Content -LiteralPath $summaryMd -Value $summary -Encoding UTF8

$checks | Format-Table -AutoSize

Write-Host ""
Write-Host "Outputs:"
Write-Host $metricsCsv
Write-Host $summaryMd
Write-Host ""
Write-Host "Logs:"
Write-Host $inactiveRun.Stdout
Write-Host $inactiveRun.Stderr
Write-Host $activeRun.Stdout
Write-Host $activeRun.Stderr

$failed = @($checks | Where-Object { $_.Status -ne "OK" })
if ($failed.Count -gt 0) {
    Write-Host ""
    Write-Host "Stage3 MODTRAN tau A/B smoke failed:" -ForegroundColor Red
    $failed | Format-List
    exit 1
}

Write-Host ""
Write-Host "Stage3 MODTRAN tau A/B smoke passed." -ForegroundColor Green
