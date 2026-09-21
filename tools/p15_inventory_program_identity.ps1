param(
    [string]$RepositoryRoot = 'D:\HwaSimIR',
    [string]$OutputPath = 'D:\HwaSimIR\logs\p15\identity\program_identity.json'
)

$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path -LiteralPath $RepositoryRoot).Path
$commonExe = Join-Path $repo 'build-DataDrivenTestQT-codex-mingw73_64-Release\release\DataDrivenTestQT.exe'
$deliveryExe = Join-Path $repo 'deliverables\HwaSimIR_P15\DataDrivenTestQT\DataDrivenTestQT.exe'
$uiTestExe = Join-Path $repo 'build-DataDrivenTestQT-p15-ui-test-Release\release\p15_ui_default_visibility_test.exe'
$oldBackupExe = Join-Path $repo 'logs\p15\backups\pre_p15_release_20260921\DataDrivenTestQT.exe'

function Get-FileIdentity([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return [ordered]@{ path = $Path; exists = $false }
    }
    $item = Get-Item -LiteralPath $Path
    return [ordered]@{
        path = $item.FullName
        exists = $true
        bytes = $item.Length
        lastWriteUtc = $item.LastWriteTimeUtc.ToString('o')
        sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $item.FullName).Hash.ToLowerInvariant()
    }
}

$shortcutRoots = @(
    [Environment]::GetFolderPath('Desktop'),
    [Environment]::GetFolderPath('CommonDesktopDirectory'),
    [Environment]::GetFolderPath('StartMenu'),
    [Environment]::GetFolderPath('CommonStartMenu')
) | Where-Object { $_ -and (Test-Path -LiteralPath $_) } | Select-Object -Unique
$shell = New-Object -ComObject WScript.Shell
$shortcuts = New-Object System.Collections.Generic.List[object]
foreach ($root in $shortcutRoots) {
    foreach ($file in Get-ChildItem -LiteralPath $root -Filter '*.lnk' -File -Recurse -ErrorAction SilentlyContinue) {
        try {
            $shortcut = $shell.CreateShortcut($file.FullName)
            if ($file.Name -like '*DataDriven*' -or $shortcut.TargetPath -like '*DataDrivenTestQT*') {
                $shortcuts.Add([pscustomobject]@{
                    shortcut = $file.FullName
                    target = $shortcut.TargetPath
                    arguments = $shortcut.Arguments
                    workingDirectory = $shortcut.WorkingDirectory
                })
            }
        } catch {}
    }
}

$running = @(Get-Process DataDrivenTestQT -ErrorAction SilentlyContinue | ForEach-Object {
    [ordered]@{
        processId = $_.Id
        executable = $_.Path
        started = $_.StartTime.ToString('o')
    }
})

$buildOutputs = @(Get-ChildItem -LiteralPath $repo -Filter 'DataDrivenTestQT.exe' -File -Recurse -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -notlike "$repo\.git\*" } |
    Sort-Object FullName |
    ForEach-Object { Get-FileIdentity $_.FullName })

$head = (& git -C $repo rev-parse HEAD).Trim()
$branch = (& git -C $repo branch --show-current).Trim()
$status = @(& git -C $repo status --short --untracked-files=all)
$nameStatus = @(& git -C $repo diff --name-status)
$uiEvidence = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $repo 'logs\p15\ui\final_interaction\interaction_result.json') | ConvertFrom-Json

$result = [ordered]@{
    schema = 'hwasimir.p15.program-identity.v1'
    createdUtc = [DateTime]::UtcNow.ToString('o')
    referenceCommit = '633048417108dfc20958324b683b467d7a54cc40'
    actualHead = $head
    branch = $branch
    referenceMatchesHead = ($head -eq '633048417108dfc20958324b683b467d7a54cc40')
    workingTreeStatus = $status
    trackedDiffNameStatus = $nameStatus
    runningDataDrivenProcessesAtFinalInventory = $running
    shortcutSearchRoots = $shortcutRoots
    matchingShortcuts = @($shortcuts | ForEach-Object { $_ })
    commonBuild = Get-FileIdentity $commonExe
    finalDelivery = Get-FileIdentity $deliveryExe
    uiTestProgram = Get-FileIdentity $uiTestExe
    preP15Backup = Get-FileIdentity $oldBackupExe
    commonBuildConfig = Get-FileIdentity (Join-Path (Split-Path -Parent $commonExe) 'NetworkConfig.ini')
    finalDeliveryConfig = Get-FileIdentity (Join-Path (Split-Path -Parent $deliveryExe) 'NetworkConfig.ini')
    sourceOriginal1 = Get-FileIdentity (Join-Path $repo 'DataDrivenTestQT\1.txt')
    deliveryOriginal1 = Get-FileIdentity (Join-Path (Split-Path -Parent $deliveryExe) '1.txt')
    finalOrdinaryLaunch = [ordered]@{
        executable = $uiEvidence.executable
        sha256 = $uiEvidence.executableSha256
        workingDirectory = $uiEvidence.workingDirectory
        configPath = $uiEvidence.configPath
        arguments = $uiEvidence.ordinaryLaunchArguments
        environmentOverrides = $uiEvidence.ordinaryLaunchEnvironmentOverrides
        firstProcessId = $uiEvidence.steps[0].ProcessId
        reopenProcessId = ($uiEvidence.steps | Where-Object Step -eq 'reopened_same_delivery').ProcessId
        checks = $uiEvidence.checks
    }
    buildOutputs = $buildOutputs
}

New-Item -ItemType Directory -Force -Path (Split-Path -Parent $OutputPath) | Out-Null
$result | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $OutputPath -Encoding UTF8
$result | ConvertTo-Json -Depth 8
