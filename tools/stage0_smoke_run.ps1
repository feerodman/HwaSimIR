param(
    [int]$Seconds = 8,
    [switch]$KeepRunning
)

$ErrorActionPreference = "Stop"

$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
$rootPath = $root.Path

$hwaExe = Join-Path $rootPath "HwaSim_IR\Bin\HwaSim_IR.exe"
$hwaWorkDir = Join-Path $rootPath "HwaSim_IR\Bin"
$qtExe = Join-Path $rootPath "build-DataDrivenTestQT-codex-mingw73_64-Release\release\DataDrivenTestQT.exe"
$qtWorkDir = Split-Path -Parent $qtExe
$logDir = Join-Path $rootPath "logs\stage0"

function Assert-Path {
    param(
        [string]$Path,
        [string]$Label
    )
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "$Label not found: $Path"
    }
}

Assert-Path $hwaExe "HwaSimIR executable"
Assert-Path $qtExe "DataDrivenTestQT executable"
Assert-Path $hwaWorkDir "HwaSimIR working directory"
Assert-Path $qtWorkDir "DataDrivenTestQT working directory"

New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$stamp = Get-Date -Format "yyyyMMdd-HHmmss"

$hwaOut = Join-Path $logDir "HwaSimIR-$stamp.out.log"
$hwaErr = Join-Path $logDir "HwaSimIR-$stamp.err.log"
$qtOut = Join-Path $logDir "DataDrivenTestQT-$stamp.out.log"
$qtErr = Join-Path $logDir "DataDrivenTestQT-$stamp.err.log"

Write-Host "Stage 0 smoke run"
Write-Host "Duration: $Seconds seconds"
Write-Host "HwaSimIR: $hwaExe"
Write-Host "DataDrivenTestQT: $qtExe"
Write-Host ""

$processes = @()
try {
    $processes += Start-Process -FilePath $hwaExe -WorkingDirectory $hwaWorkDir -PassThru -WindowStyle Hidden -RedirectStandardOutput $hwaOut -RedirectStandardError $hwaErr
    Start-Sleep -Seconds 2
    $processes += Start-Process -FilePath $qtExe -WorkingDirectory $qtWorkDir -PassThru -WindowStyle Hidden -RedirectStandardOutput $qtOut -RedirectStandardError $qtErr

    Start-Sleep -Seconds $Seconds

    foreach ($proc in $processes) {
        $proc.Refresh()
        $state = if ($proc.HasExited) { "exited($($proc.ExitCode))" } else { "running" }
        Write-Host "$($proc.ProcessName) pid=$($proc.Id) state=$state"
    }
}
finally {
    if (-not $KeepRunning) {
        foreach ($proc in $processes) {
            try {
                $proc.Refresh()
                if (-not $proc.HasExited) {
                    Stop-Process -Id $proc.Id -Force
                    Write-Host "stopped pid=$($proc.Id)"
                }
            }
            catch {
                Write-Host "process cleanup warning: $($_.Exception.Message)"
            }
        }
    }
}

Write-Host ""
Write-Host "Logs:"
Write-Host $hwaOut
Write-Host $hwaErr
Write-Host $qtOut
Write-Host $qtErr
