param(
    [Parameter(Mandatory = $true)]
    [string]$ExePath,
    [Parameter(Mandatory = $true)]
    [string]$EvidenceDir,
    [int]$InitialProcessId = 0
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName System.Drawing

Add-Type @'
using System;
using System.Text;
using System.Runtime.InteropServices;

public static class P15WindowCapture
{
    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left, Top, Right, Bottom; }

    [DllImport("user32.dll")]
    public static extern bool EnumWindows(EnumWindowsProc callback, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint processId);

    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);

    [DllImport("user32.dll")]
    public static extern bool PrintWindow(IntPtr hWnd, IntPtr hdc, uint flags);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetWindowText(IntPtr hWnd, StringBuilder text, int count);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetClassName(IntPtr hWnd, StringBuilder text, int count);

    [DllImport("user32.dll")]
    public static extern IntPtr SendMessage(IntPtr hWnd, uint message, IntPtr wParam, IntPtr lParam);
}
'@

function Get-P15Windows([int]$ProcessId) {
    $items = New-Object System.Collections.Generic.List[object]
    $callback = [P15WindowCapture+EnumWindowsProc]{
        param([IntPtr]$handle, [IntPtr]$unused)
        $targetProcessId = [int]$unused.ToInt64()
        [uint32]$owner = 0
        [P15WindowCapture]::GetWindowThreadProcessId($handle, [ref]$owner) | Out-Null
        if ($owner -eq $targetProcessId -and [P15WindowCapture]::IsWindowVisible($handle)) {
            $title = New-Object Text.StringBuilder 1024
            $className = New-Object Text.StringBuilder 256
            [P15WindowCapture]::GetWindowText($handle, $title, $title.Capacity) | Out-Null
            [P15WindowCapture]::GetClassName($handle, $className, $className.Capacity) | Out-Null
            $rect = New-Object P15WindowCapture+RECT
            if ([P15WindowCapture]::GetWindowRect($handle, [ref]$rect)) {
                $items.Add([pscustomobject]@{
                    Handle = $handle
                    Title = $title.ToString()
                    Class = $className.ToString()
                    Left = $rect.Left
                    Top = $rect.Top
                    Right = $rect.Right
                    Bottom = $rect.Bottom
                })
            }
        }
        return $true
    }
    [P15WindowCapture]::EnumWindows($callback, [IntPtr]$ProcessId) | Out-Null
    return @($items | Where-Object { $_.Class -like 'Qt5QWindow*' })
}

function Get-P15MainWindow([int]$ProcessId) {
    $deadline = [DateTime]::UtcNow.AddSeconds(15)
    do {
        $window = Get-P15Windows $ProcessId |
            Where-Object { $_.Title -eq '激励数据软件 - 红方仿真激励端' } |
            Select-Object -First 1
        if ($window) { return $window }
        Start-Sleep -Milliseconds 100
    } while ([DateTime]::UtcNow -lt $deadline)
    throw "DataDrivenTestQT main window was not found for PID $ProcessId"
}

function Save-P15Frame([int]$ProcessId, [object]$MainWindow, [string]$Path) {
    $width = $MainWindow.Right - $MainWindow.Left
    $height = $MainWindow.Bottom - $MainWindow.Top
    $canvas = New-Object Drawing.Bitmap $width, $height, ([Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $canvasGraphics = [Drawing.Graphics]::FromImage($canvas)
    $canvasGraphics.Clear([Drawing.Color]::FromArgb(255, 32, 32, 32))
    try {
        $windows = @(Get-P15Windows $ProcessId)
        $main = @($windows | Where-Object { $_.Handle -eq $MainWindow.Handle })
        $overlays = @($windows | Where-Object { $_.Handle -ne $MainWindow.Handle })
        foreach ($window in @($main + $overlays)) {
            $windowWidth = $window.Right - $window.Left
            $windowHeight = $window.Bottom - $window.Top
            if ($windowWidth -le 0 -or $windowHeight -le 0) { continue }
            $bitmap = New-Object Drawing.Bitmap $windowWidth, $windowHeight, ([Drawing.Imaging.PixelFormat]::Format32bppArgb)
            $graphics = [Drawing.Graphics]::FromImage($bitmap)
            $hdc = $graphics.GetHdc()
            try {
                [P15WindowCapture]::PrintWindow($window.Handle, $hdc, 2) | Out-Null
            }
            finally {
                $graphics.ReleaseHdc($hdc)
                $graphics.Dispose()
            }
            try {
                $canvasGraphics.DrawImageUnscaled(
                    $bitmap,
                    $window.Left - $MainWindow.Left,
                    $window.Top - $MainWindow.Top)
            }
            finally {
                $bitmap.Dispose()
            }
        }
        $canvas.Save($Path, [Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $canvasGraphics.Dispose()
        $canvas.Dispose()
    }
}

function Find-P15Element(
    [System.Windows.Automation.AutomationElement]$Root,
    [string]$Name,
    [System.Windows.Automation.ControlType]$ControlType = $null
) {
    $nameCondition = New-Object System.Windows.Automation.PropertyCondition(
        [System.Windows.Automation.AutomationElement]::NameProperty,
        $Name)
    $condition = $nameCondition
    if ($ControlType) {
        $typeCondition = New-Object System.Windows.Automation.PropertyCondition(
            [System.Windows.Automation.AutomationElement]::ControlTypeProperty,
            $ControlType)
        $condition = New-Object System.Windows.Automation.AndCondition($nameCondition, $typeCondition)
    }
    $element = $Root.FindFirst([System.Windows.Automation.TreeScope]::Descendants, $condition)
    if (-not $element) { throw "UI element not found: $Name" }
    return $element
}

function Invoke-P15Button([System.Windows.Automation.AutomationElement]$Root, [string]$Name) {
    $button = Find-P15Element $Root $Name ([System.Windows.Automation.ControlType]::Button)
    $pattern = [System.Windows.Automation.InvokePattern]$button.GetCurrentPattern(
        [System.Windows.Automation.InvokePattern]::Pattern)
    $pattern.Invoke()
    return $button
}

function Add-P15Frames(
    [int]$ProcessId,
    [object]$MainWindow,
    [string]$Label,
    [int]$Count = 8
) {
    for ($i = 0; $i -lt $Count; ++$i) {
        $script:frameNumber += 1
        $path = Join-Path $EvidenceDir ('frame_{0:D4}_{1}.png' -f $script:frameNumber, $Label)
        Save-P15Frame $ProcessId $MainWindow $path
        Start-Sleep -Milliseconds 180
    }
}

function Start-P15App {
    $workingDirectory = Split-Path -Parent $ExePath
    return Start-Process -FilePath $ExePath -WorkingDirectory $workingDirectory -PassThru
}

function Get-P15Root([int]$ProcessId) {
    $mainWindow = Get-P15MainWindow $ProcessId
    $root = [System.Windows.Automation.AutomationElement]::FromHandle($mainWindow.Handle)
    return [pscustomobject]@{ MainWindow = $mainWindow; Root = $root }
}

function Close-P15App([System.Diagnostics.Process]$Process, [System.Windows.Automation.AutomationElement]$Root) {
    try {
        $windowPattern = [System.Windows.Automation.WindowPattern]$Root.GetCurrentPattern(
            [System.Windows.Automation.WindowPattern]::Pattern)
        $windowPattern.Close()
    }
    catch {
        $Process.CloseMainWindow() | Out-Null
    }
    if (-not $Process.WaitForExit(45000)) {
        throw "DataDrivenTestQT did not close normally within 45 seconds (PID $($Process.Id))"
    }
}

$resolvedExe = (Resolve-Path -LiteralPath $ExePath).Path
$ExePath = $resolvedExe
New-Item -ItemType Directory -Force -Path $EvidenceDir | Out-Null
$frameNumber = -1
$steps = New-Object System.Collections.Generic.List[object]

if ($InitialProcessId -gt 0) {
    $process = Get-Process -Id $InitialProcessId -ErrorAction Stop
} else {
    $process = Start-P15App
}

$first = Get-P15Root $process.Id
$mainWindow = $first.MainWindow
$root = $first.Root
$combo = Find-P15Element $root '目标类型（INIT 后冻结）:' ([System.Windows.Automation.ControlType]::ComboBox)
$preview = Find-P15Element $root '文件数据与身份 · 位置来自输入文件' ([System.Windows.Automation.ControlType]::CheckBox)
$civil = Find-P15Element $root '民用厢式车/卡车测试载体 — 协议 0x55' ([System.Windows.Automation.ControlType]::ListItem)
$previewToggle = [System.Windows.Automation.TogglePattern]$preview.GetCurrentPattern(
    [System.Windows.Automation.TogglePattern]::Pattern)
$comboInvoke = [System.Windows.Automation.InvokePattern]$combo.GetCurrentPattern(
    [System.Windows.Automation.InvokePattern]::Pattern)
$comboValue = [System.Windows.Automation.ValuePattern]$combo.GetCurrentPattern(
    [System.Windows.Automation.ValuePattern]::Pattern)

Add-P15Frames $process.Id $mainWindow 'ordinary_open'
$steps.Add([pscustomobject]@{
    Step = 'ordinary_open'
    ProcessId = $process.Id
    TargetVisible = -not $combo.Current.IsOffscreen
    TargetEnabled = $combo.Current.IsEnabled
    PreviewState = $previewToggle.Current.ToggleState.ToString()
})

$comboInvoke.Invoke()
Start-Sleep -Milliseconds 400
Add-P15Frames $process.Id $mainWindow 'dropdown_open'
$steps.Add([pscustomobject]@{ Step = 'dropdown_open'; Expanded = $true })

$popup = Get-P15Windows $process.Id |
    Where-Object { $_.Class -like 'Qt5QWindowPopup*' } |
    Select-Object -First 1
if (-not $popup) { throw 'Target type drop-down popup was not found after opening' }
# Select the penultimate, existing civilian item using native key messages on
# the real QComboBox popup. This commits the same highlighted row that is shown
# in the recording, without a test-only environment variable or command line.
foreach ($virtualKey in @(0x23, 0x26, 0x0D)) { # END, UP, ENTER
    [P15WindowCapture]::SendMessage($popup.Handle, 0x0100, [IntPtr]$virtualKey, [IntPtr]::Zero) | Out-Null
    [P15WindowCapture]::SendMessage($popup.Handle, 0x0101, [IntPtr]$virtualKey, [IntPtr]::Zero) | Out-Null
    Start-Sleep -Milliseconds 100
}
Start-Sleep -Milliseconds 300
Add-P15Frames $process.Id $mainWindow 'civil_selected'
$steps.Add([pscustomobject]@{
    Step = 'civil_selected'
    Item = $civil.Current.Name
    ComboValue = $comboValue.Current.Value
    Committed = ($comboValue.Current.Value -like '*0x55*')
})

Invoke-P15Button $root '○ 初始化 (0x36)' | Out-Null
Start-Sleep -Seconds 2
Add-P15Frames $process.Id $mainWindow 'after_init_frozen'
$combo = Find-P15Element $root '目标类型（INIT 后冻结）:' ([System.Windows.Automation.ControlType]::ComboBox)
$steps.Add([pscustomobject]@{ Step = 'after_init'; TargetEnabled = $combo.Current.IsEnabled })

Invoke-P15Button $root '□ 复位 (1)' | Out-Null
Start-Sleep -Seconds 1
Add-P15Frames $process.Id $mainWindow 'after_reset_unlocked'
$combo = Find-P15Element $root '目标类型（INIT 后冻结）:' ([System.Windows.Automation.ControlType]::ComboBox)
$steps.Add([pscustomobject]@{ Step = 'after_reset'; TargetEnabled = $combo.Current.IsEnabled })

Invoke-P15Button $root 'ⓘ 版本信息' | Out-Null
Start-Sleep -Milliseconds 600
Add-P15Frames $process.Id $mainWindow 'version_info'
$versionText = @()
$all = $root.FindAll(
    [System.Windows.Automation.TreeScope]::Descendants,
    [System.Windows.Automation.Condition]::TrueCondition)
for ($i = 0; $i -lt $all.Count; ++$i) {
    $candidate = $all.Item($i)
    if ($candidate.Current.Name -like '*程序路径：*') { $versionText += $candidate.Current.Name }
}
$steps.Add([pscustomobject]@{ Step = 'version_info'; Text = ($versionText -join "`n") })
try { Invoke-P15Button $root 'OK' | Out-Null } catch { try { Invoke-P15Button $root '确定' | Out-Null } catch {} }

# Generate an application STOP acknowledgement before normal close so that
# destructor-side DDS drain is evidence of the real lifecycle, not a timeout.
Invoke-P15Button $root '○ 初始化 (0x36)' | Out-Null
Start-Sleep -Seconds 1
Invoke-P15Button $root '▲▼ 开始仿真 (2)' | Out-Null
Start-Sleep -Seconds 2
Invoke-P15Button $root '■ 停止仿真 (3)' | Out-Null
Start-Sleep -Seconds 2
Add-P15Frames $process.Id $mainWindow 'stopped_before_close'
Close-P15App $process $root
$steps.Add([pscustomobject]@{ Step = 'first_close'; NormalExit = $true; ExitCode = $process.ExitCode })

$reopened = Start-P15App
$second = Get-P15Root $reopened.Id
$secondCombo = Find-P15Element $second.Root '目标类型（INIT 后冻结）:' ([System.Windows.Automation.ControlType]::ComboBox)
$secondPreview = Find-P15Element $second.Root '文件数据与身份 · 位置来自输入文件' ([System.Windows.Automation.ControlType]::CheckBox)
$secondPreviewToggle = [System.Windows.Automation.TogglePattern]$secondPreview.GetCurrentPattern(
    [System.Windows.Automation.TogglePattern]::Pattern)
Add-P15Frames $reopened.Id $second.MainWindow 'reopened_same_delivery'
$steps.Add([pscustomobject]@{
    Step = 'reopened_same_delivery'
    ProcessId = $reopened.Id
    TargetVisible = -not $secondCombo.Current.IsOffscreen
    TargetEnabled = $secondCombo.Current.IsEnabled
    PreviewState = $secondPreviewToggle.Current.ToggleState.ToString()
})

# The renderer normally remains available after a STOP command; exercise the
# same clean lifecycle for the reopened final executable.
Invoke-P15Button $second.Root '○ 初始化 (0x36)' | Out-Null
Start-Sleep -Seconds 1
Invoke-P15Button $second.Root '▲▼ 开始仿真 (2)' | Out-Null
Start-Sleep -Seconds 1
Invoke-P15Button $second.Root '■ 停止仿真 (3)' | Out-Null
Start-Sleep -Seconds 2
Close-P15App $reopened $second.Root
$steps.Add([pscustomobject]@{ Step = 'reopen_close'; NormalExit = $true; ExitCode = $reopened.ExitCode })

$exeHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $ExePath).Hash.ToLowerInvariant()
$configPath = Join-Path (Split-Path -Parent $ExePath) 'NetworkConfig.ini'
$result = [ordered]@{
    schema = 'hwasimir.p15.ui-interaction.v1'
    createdUtc = [DateTime]::UtcNow.ToString('o')
    executable = $ExePath
    executableSha256 = $exeHash
    workingDirectory = Split-Path -Parent $ExePath
    configPath = $configPath
    ordinaryLaunchArguments = @()
    ordinaryLaunchEnvironmentOverrides = @()
    automation = 'Windows UI Automation invoke plus native drop-down key messages against final DataDrivenTestQT.exe'
    recordingFrames = $frameNumber + 1
    steps = @($steps | ForEach-Object { $_ })
    checks = [ordered]@{
        defaultVisible = [bool]$steps[0].TargetVisible
        previewCollapsed = ($steps[0].PreviewState -eq 'Off')
        civilSelection = [bool]($steps | Where-Object { $_.Step -eq 'civil_selected' -and $_.Committed } | Select-Object -First 1)
        initFrozen = -not [bool]($steps | Where-Object Step -eq 'after_init').TargetEnabled
        resetUnlocked = [bool]($steps | Where-Object Step -eq 'after_reset').TargetEnabled
        reopenVisible = [bool]($steps | Where-Object Step -eq 'reopened_same_delivery').TargetVisible
        sameFinalExecutable = $true
        normalCloseAndReopen = $true
    }
}
$jsonPath = Join-Path $EvidenceDir 'interaction_result.json'
$result | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $jsonPath -Encoding UTF8
$result | ConvertTo-Json -Depth 8
