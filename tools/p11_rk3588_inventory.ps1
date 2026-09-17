[CmdletBinding()]
param(
    [string]$VmHost = '192.168.203.128',
    [string]$VmUser = 'linaro',
    [string]$BoardHost = '192.168.1.116',
    [string]$BoardUser = 'root',
    [string]$VmPassword = $env:P11_VM_PASSWORD,
    [string]$BoardPassword = $env:P11_BOARD_PASSWORD,
    [string]$SshKey = 'C:\Users\kahn1\.ssh\codex_hwasimir_ed25519',
    [string]$OutputDirectory = '',
    [string]$VmSourceRoot = '/home/linaro/userdata/HwaSimIR',
    [string]$VmBuildRoot = '/home/linaro/userdata/HwaSimIR/cmake-build-codex-rk3588',
    [string]$BoardRoot = '/userdata/HwaSimIR',
    [switch]$SkipVm,
    [switch]$SkipBoard
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2
$repoRoot = Split-Path -Parent $PSScriptRoot
$remoteProbe = Join-Path $PSScriptRoot 'p11_rk3588_remote_inventory.sh'
$askPass = Join-Path $PSScriptRoot 'p5_ssh_askpass.cmd'
foreach ($required in @($remoteProbe, $askPass)) {
    if (-not (Test-Path -LiteralPath $required)) { throw "Missing required file: $required" }
}
if ($BoardRoot -ne '/userdata/HwaSimIR') { throw 'Board inventory is restricted to /userdata/HwaSimIR' }
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $repoRoot ("logs\p11\rk3588\inventory-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
}
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$OutputDirectory = (Resolve-Path -LiteralPath $OutputDirectory).Path

function Invoke-RemoteProbe {
    param(
        [string]$HostName,
        [string]$UserName,
        [string]$Role,
        [string]$Password,
        [string[]]$RemoteArguments,
        [string]$LogPath
    )

    $common = @('-o', 'ConnectTimeout=10', '-o', 'ServerAliveInterval=10', '-o', 'StrictHostKeyChecking=accept-new')
    $remoteCommand = "sh -s -- $Role $($RemoteArguments -join ' ')"
    $probeText = [IO.File]::ReadAllText($remoteProbe)

    function Invoke-SshProbeDirect {
        param([string[]]$Arguments)
        $savedErrorActionPreference = $ErrorActionPreference
        $ErrorActionPreference = 'Continue'
        try {
            $output = $probeText | & ssh.exe @Arguments 2>&1
            $exitCode = $LASTEXITCODE
        }
        finally {
            $ErrorActionPreference = $savedErrorActionPreference
        }
        return [pscustomobject]@{ ExitCode = $exitCode; Output = @($output) }
    }

    $usingKey = $false
    if ($SshKey -and (Test-Path -LiteralPath $SshKey)) {
        $keyArgs = $common + @('-i', $SshKey, '-o', 'BatchMode=yes', "$UserName@$HostName", $remoteCommand)
        $keyResult = Invoke-SshProbeDirect -Arguments $keyArgs
        if ($keyResult.ExitCode -eq 0) {
            $usingKey = $true
            [IO.File]::WriteAllText($LogPath, "Transport=ssh-key`n" + ($keyResult.Output -join "`n") + "`n", (New-Object Text.UTF8Encoding($false)))
        }
    }
    if ($usingKey) { return }
    if (-not $Password) { throw "SSH key failed and no password was supplied for $UserName@$HostName" }
    $previousPassword = $env:HWASIMIR_SSH_PASSWORD
    $previousAskPass = $env:SSH_ASKPASS
    $previousAskPassRequire = $env:SSH_ASKPASS_REQUIRE
    $previousDisplay = $env:DISPLAY
    try {
        $env:HWASIMIR_SSH_PASSWORD = $Password
        $env:SSH_ASKPASS = $askPass
        $env:SSH_ASKPASS_REQUIRE = 'force'
        $env:DISPLAY = 'p11-rk3588-inventory'
        $passwordArgs = $common + @('-o', 'PubkeyAuthentication=no', '-o', 'PreferredAuthentications=password', "$UserName@$HostName", $remoteCommand)
        $passwordResult = Invoke-SshProbeDirect -Arguments $passwordArgs
        if ($passwordResult.ExitCode -ne 0) {
            throw "Inventory probe failed for $UserName@$HostName with exit $($passwordResult.ExitCode): $($passwordResult.Output -join ' ')"
        }
        [IO.File]::WriteAllText($LogPath, "Transport=ssh-password`n" + ($passwordResult.Output -join "`n") + "`n", (New-Object Text.UTF8Encoding($false)))
    }
    finally {
        $env:HWASIMIR_SSH_PASSWORD = $previousPassword
        $env:SSH_ASKPASS = $previousAskPass
        $env:SSH_ASKPASS_REQUIRE = $previousAskPassRequire
        $env:DISPLAY = $previousDisplay
    }
}

$vmLog = Join-Path $OutputDirectory 'vm_inventory.txt'
$boardLog = Join-Path $OutputDirectory 'board_inventory.txt'
if (-not $SkipVm) {
    Invoke-RemoteProbe -HostName $VmHost -UserName $VmUser -Role vm -Password $VmPassword `
        -RemoteArguments @('/unused', $VmSourceRoot, $VmBuildRoot) -LogPath $vmLog
}
if (-not $SkipBoard) {
    Invoke-RemoteProbe -HostName $BoardHost -UserName $BoardUser -Role board -Password $BoardPassword `
        -RemoteArguments @($BoardRoot, '/unused', '/unused') -LogPath $boardLog
}

$result = [ordered]@{
    schema = 'hwasimir.p11.rk3588.inventory.v1'
    created_utc = [DateTime]::UtcNow.ToString('o')
    vm_host = $VmHost
    board_host = $BoardHost
    vm_log = if (Test-Path -LiteralPath $vmLog) { $vmLog } else { $null }
    vm_log_sha256 = if (Test-Path -LiteralPath $vmLog) { (Get-FileHash -LiteralPath $vmLog -Algorithm SHA256).Hash.ToLowerInvariant() } else { $null }
    board_log = if (Test-Path -LiteralPath $boardLog) { $boardLog } else { $null }
    board_log_sha256 = if (Test-Path -LiteralPath $boardLog) { (Get-FileHash -LiteralPath $boardLog -Algorithm SHA256).Hash.ToLowerInvariant() } else { $null }
    probe_sha256 = (Get-FileHash -LiteralPath $remoteProbe -Algorithm SHA256).Hash.ToLowerInvariant()
}
$json = $result | ConvertTo-Json -Depth 4
$jsonPath = Join-Path $OutputDirectory 'inventory.json'
[IO.File]::WriteAllText($jsonPath, $json + "`n", (New-Object Text.UTF8Encoding($false)))
Write-Host "[P11 RK3588 Inventory] result=PASS output=$OutputDirectory"
