param(
    [int[]]$Bands = @(1, 2, 3),
    [int]$DelayMs = 500
)

$ErrorActionPreference = "Stop"

$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
$rootPath = $root.Path
$hwaExe = Join-Path $rootPath "HwaSim_IR\Bin\HwaSim_IR.exe"
$hwaWorkDir = Join-Path $rootPath "HwaSim_IR\Bin"
$logDir = Join-Path $rootPath "logs\stage4"
$networkConfig = Join-Path $hwaWorkDir "Config\NetworkConfig.ini"
$runtimeConfig = Join-Path $hwaWorkDir "Config\HwaSimIRRuntime.ini"

if ($Bands.Count -eq 1 -and $Bands[0] -eq 1) {
    $Bands = @(1, 2, 3)
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
        [bool]$EngineState,
        [bool]$StrikeFlag,
        [int]$StrikePart
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
        WBool $StrikeFlag
        WInt $StrikePart
    }
    function WTargetState([int]$TargetType, [int]$TargetId, [bool]$TargetEngineState, [double]$LonOffset) {
        WInt $TargetType
        WInt 1
        WInt $TargetId
        WBool $TargetEngineState
        WBool $true
        WSpatial 0.0 $LonOffset 1000.0 0.0 0.0 0.0 0.0
        WInt 0x01
    }

    WInt 0x38
    WInt 1
    WInt 1
    WDouble $TimeMs
    WSpatial 0.0 0.0 1000.0 0.0 0.0 0.0 0.0
    WWeaponState
    WInt 1
    WTargetState 0x22 0 $EngineState 0.010
    WTargetState 0x22 1 $false 0.015
    WTargetState 0x22 2 $false 0.020
    WTargetState 0x33 3 $false 0.025
    WTargetState 0x33 4 $false 0.030

    $bytes = $ms.ToArray()
    $bw.Dispose()
    $ms.Dispose()
    return $bytes
}

function Add-Check {
    param([string]$Name, [bool]$Ok, [string]$Detail)
    [PSCustomObject]@{
        Check  = $Name
        Status = $(if ($Ok) { "OK" } else { "FAIL" })
        Detail = $Detail
    }
}

function Get-Stage4HeatSourceRows {
    param([string]$Text)

    $pattern = "(?m)^\[Stage4 HeatSourceDiag\].*?\btargetID=(?<targetID>-?\d+).*?\bengineState=(?<engine>[01]).*?\blegacyEngineBodyHeating=(?<legacy>[01]).*?\bbodyTempK=(?<bodyTemp>[-+0-9eE.]+).*?\bbodyRadiance=(?<bodyRadiance>[-+0-9eE.]+).*?\brearHotspotEnabled=(?<rear>[01]).*?\brearHotspotTempK=(?<rearTemp>[-+0-9eE.]+).*?\brearHotspotIntensity=(?<rearIntensity>[-+0-9eE.]+).*?\bplumeEnabled=(?<plume>[01]).*?\bstrikeFlag=(?<strike>[01]).*?\bstrikePart=(?<strikePart>-?\d+).*?\bbrightspotPart=(?<brightPart>[A-Za-z]+).*?\bbrightspotEnabled=(?<bright>[01])"
    return @([regex]::Matches($Text, $pattern) | ForEach-Object {
        [PSCustomObject]@{
            targetID     = [int]$_.Groups["targetID"].Value
            engine       = [int]$_.Groups["engine"].Value
            legacy       = [int]$_.Groups["legacy"].Value
            bodyTemp     = [double]$_.Groups["bodyTemp"].Value
            bodyRadiance = [double]$_.Groups["bodyRadiance"].Value
            rear         = [int]$_.Groups["rear"].Value
            rearTemp     = [double]$_.Groups["rearTemp"].Value
            rearIntensity = [double]$_.Groups["rearIntensity"].Value
            plume        = [int]$_.Groups["plume"].Value
            strike       = [int]$_.Groups["strike"].Value
            strikePart   = [int]$_.Groups["strikePart"].Value
            brightPart   = $_.Groups["brightPart"].Value
            bright       = [int]$_.Groups["bright"].Value
        }
    })
}

function First-HeatRow {
    param(
        [object[]]$Rows,
        [int]$Engine,
        [int]$Strike,
        [int]$StrikePart
    )

    return @($Rows | Where-Object {
        $_.targetID -eq 0 -and
        $_.engine -eq $Engine -and
        $_.strike -eq $Strike -and
        $_.strikePart -eq $StrikePart
    } | Select-Object -First 1)[0]
}

function Invoke-HwaRun {
    $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $stdout = Join-Path $logDir "HwaSimIR-stage4-hotspot-smoke-$stamp.out.log"
    $stderr = Join-Path $logDir "HwaSimIR-stage4-hotspot-smoke-$stamp.err.log"
    $process = $null
    $udp = $null
    $networkConfigBackup = [System.IO.File]::ReadAllBytes($networkConfig)
    $runtimeConfigBackup = [System.IO.File]::ReadAllBytes($runtimeConfig)

    try {
        $utf8NoBom = New-Object System.Text.UTF8Encoding($false)
        $networkText = [System.IO.File]::ReadAllText($networkConfig, $utf8NoBom)
        $networkText = [regex]::Replace($networkText, "(?m)^localIp=.*$", "localIp=127.0.0.1")
        $networkText = [regex]::Replace($networkText, "(?m)^serverIp=.*$", "serverIp=127.0.0.1")
        [System.IO.File]::WriteAllText($networkConfig, $networkText, $utf8NoBom)

        $runtimeText = [System.IO.File]::ReadAllText($runtimeConfig, $utf8NoBom)
        $runtimeText = [regex]::Replace($runtimeText, "(?m)^EnableIRVerboseLog=.*$", "EnableIRVerboseLog=1")
        $runtimeText = [regex]::Replace($runtimeText, "(?m)^LegacyEngineBodyHeating=.*$", "LegacyEngineBodyHeating=false")
        [System.IO.File]::WriteAllText($runtimeConfig, $runtimeText, $utf8NoBom)

        Normalize-ProcessPathEnvironment
        $process = Start-Process -FilePath $hwaExe -WorkingDirectory $hwaWorkDir -PassThru -WindowStyle Hidden -RedirectStandardOutput $stdout -RedirectStandardError $stderr
        Start-Sleep -Seconds 3

        $udp = New-Object System.Net.Sockets.UdpClient
        foreach ($band in $Bands) {
            $packet = New-InitPacket -Band $band
            $sent = $udp.Send($packet, $packet.Length, "127.0.0.1", 8888)
            Write-Host "sent init band=$band bytes=$sent"
            Start-Sleep -Milliseconds $DelayMs

            $control = New-ControlStartPacket
            $sentControl = $udp.Send($control, $control.Length, "127.0.0.1", 8888)
            Write-Host "sent control-start bytes=$sentControl"
            Start-Sleep -Milliseconds $DelayMs

            $cases = @(
                @{ Label = "engine0_strike0"; Engine = $false; Strike = $false; Part = 0 },
                @{ Label = "engine1_strike0"; Engine = $true; Strike = $false; Part = 0 },
                @{ Label = "engine0_head"; Engine = $false; Strike = $true; Part = 1 },
                @{ Label = "engine0_mid"; Engine = $false; Strike = $true; Part = 2 }
            )

            $frame = 0
            foreach ($case in $cases) {
                ++$frame
                $display = New-DisplayPacket -TimeMs ([double](($band * 1000) + ($frame * 33))) -EngineState $case.Engine -StrikeFlag $case.Strike -StrikePart $case.Part
                $sentDisplay = $udp.Send($display, $display.Length, "127.0.0.1", 8888)
                Write-Host "sent $($case.Label) band=$band bytes=$sentDisplay"
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
                Write-Host "stopped pid=$($process.Id)"
            }
        }
        [System.IO.File]::WriteAllBytes($networkConfig, $networkConfigBackup)
        [System.IO.File]::WriteAllBytes($runtimeConfig, $runtimeConfigBackup)
    }

    return [PSCustomObject]@{
        Stdout = $stdout
        Stderr = $stderr
        Text   = $(if (Test-Path -LiteralPath $stdout) { Get-Content -LiteralPath $stdout -Raw -Encoding UTF8 } else { "" })
    }
}

Assert-Path $hwaExe "HwaSimIR executable"
Assert-Path $hwaWorkDir "HwaSimIR working directory"
Assert-Path $networkConfig "NetworkConfig.ini"
Assert-Path $runtimeConfig "HwaSimIRRuntime.ini"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null

Write-Host "Stage 4 hotspot/brightspot smoke"
Write-Host "Bands: $($Bands -join ', ')"
Write-Host "HwaSimIR: $hwaExe"
Write-Host ""

$run = Invoke-HwaRun
$text = $run.Text
$heatRows = Get-Stage4HeatSourceRows -Text $text
$engineOffRow = First-HeatRow -Rows $heatRows -Engine 0 -Strike 0 -StrikePart 0
$engineOnRow = First-HeatRow -Rows $heatRows -Engine 1 -Strike 0 -StrikePart 0
$headStrikeRow = First-HeatRow -Rows $heatRows -Engine 0 -Strike 1 -StrikePart 1
$midStrikeRow = First-HeatRow -Rows $heatRows -Engine 0 -Strike 1 -StrikePart 2
$bodyTempStable = $false
$bodyRadianceStable = $false
if ($engineOffRow -and $engineOnRow) {
    $bodyTempStable = [Math]::Abs($engineOnRow.bodyTemp - $engineOffRow.bodyTemp) -lt 0.01
    $bodyRadianceStable = $engineOnRow.bodyRadiance -le ($engineOffRow.bodyRadiance + 0.0001)
}
$checks = New-Object System.Collections.Generic.List[object]

$checks.Add((Add-Check "ThermalHotspot logs present" ($text -match "\[Stage4 ThermalHotspot\]") $run.Stdout)) | Out-Null
$checks.Add((Add-Check "BrightSpot logs present" ($text -match "\[Stage4 BrightSpot\]") $run.Stdout)) | Out-Null
$checks.Add((Add-Check "HeatSourceDiag logs present" ($heatRows.Count -gt 0) "$($heatRows.Count) rows")) | Out-Null
$checks.Add((Add-Check "logs remain separated" ($text -notmatch "\[Stage4 ThermalHotspot\].*\[Stage4 BrightSpot\]|\[Stage4 BrightSpot\].*\[Stage4 ThermalHotspot\]") "separate log prefixes")) | Out-Null
$checks.Add((Add-Check "engineState=false strikeFlag=false keeps both off" (($text -match "\[Stage4 ThermalHotspot\].*engineState=0.*enabled=0") -and ($text -match "\[Stage4 BrightSpot\].*strikeFlag=0.*enabled=0")) "engine0/strike0")) | Out-Null
$checks.Add((Add-Check "engineState=true enables rear thermal hotspot only" (($text -match "\[Stage4 ThermalHotspot\].*engineState=1.*enabled=1") -and ($text -match "\[Stage4 BrightSpot\].*strikeFlag=0.*enabled=0")) "engine1/strike0")) | Out-Null
$checks.Add((Add-Check "strikePart=1 enables head brightspot only" (($text -match "\[Stage4 ThermalHotspot\].*engineState=0.*enabled=0") -and ($text -match "\[Stage4 BrightSpot\].*strikeFlag=1.*strikePart=1.*part=Head.*enabled=1")) "strike head")) | Out-Null
$checks.Add((Add-Check "strikePart=2 enables mid brightspot only" (($text -match "\[Stage4 ThermalHotspot\].*engineState=0.*enabled=0") -and ($text -match "\[Stage4 BrightSpot\].*strikeFlag=1.*strikePart=2.*part=MidBody.*enabled=1")) "strike mid")) | Out-Null
$checks.Add((Add-Check "engineState does not heat whole body temperature" $bodyTempStable "engine0=$($engineOffRow.bodyTemp) engine1=$($engineOnRow.bodyTemp)")) | Out-Null
$checks.Add((Add-Check "engineState does not increase body radiance" $bodyRadianceStable "engine0=$($engineOffRow.bodyRadiance) engine1=$($engineOnRow.bodyRadiance)")) | Out-Null
$checks.Add((Add-Check "HeatSourceDiag engineState controls rear/plume only" (($engineOnRow -and $engineOnRow.rear -eq 1 -and $engineOnRow.plume -eq 1 -and $engineOnRow.bright -eq 0) -and ($engineOffRow -and $engineOffRow.rear -eq 0 -and $engineOffRow.plume -eq 0 -and $engineOffRow.bright -eq 0)) "engine0 rear/plume/bright=$($engineOffRow.rear)/$($engineOffRow.plume)/$($engineOffRow.bright); engine1=$($engineOnRow.rear)/$($engineOnRow.plume)/$($engineOnRow.bright)")) | Out-Null
$checks.Add((Add-Check "HeatSourceDiag strikePart=1 maps to Head only" ($headStrikeRow -and $headStrikeRow.rear -eq 0 -and $headStrikeRow.plume -eq 0 -and $headStrikeRow.bright -eq 1 -and $headStrikeRow.brightPart -eq "Head") "part=$($headStrikeRow.brightPart) rear=$($headStrikeRow.rear) plume=$($headStrikeRow.plume)")) | Out-Null
$checks.Add((Add-Check "HeatSourceDiag strikePart=2 maps to MidBody only" ($midStrikeRow -and $midStrikeRow.rear -eq 0 -and $midStrikeRow.plume -eq 0 -and $midStrikeRow.bright -eq 1 -and $midStrikeRow.brightPart -eq "MidBody") "part=$($midStrikeRow.brightPart) rear=$($midStrikeRow.rear) plume=$($midStrikeRow.plume)")) | Out-Null
$checks.Add((Add-Check "strikeFlag does not enable rear hotspot" ($text -notmatch "\[Stage4 ThermalHotspot\].*engineState=0.*enabled=1") "no engine0 rear enabled")) | Out-Null
$checks.Add((Add-Check "engineState does not enable brightspot" ($text -notmatch "\[Stage4 BrightSpot\].*strikeFlag=0.*enabled=1") "no strike0 brightspot enabled")) | Out-Null

$checks | Format-Table -AutoSize

Write-Host ""
Write-Host "Logs:"
Write-Host $run.Stdout
Write-Host $run.Stderr

$failed = @($checks | Where-Object { $_.Status -ne "OK" })
if ($failed.Count -gt 0) {
    Write-Host ""
    Write-Host "Stage4 hotspot/brightspot smoke failed:" -ForegroundColor Red
    $failed | Format-List
    exit 1
}

Write-Host ""
Write-Host "Stage4 hotspot/brightspot smoke passed." -ForegroundColor Green
