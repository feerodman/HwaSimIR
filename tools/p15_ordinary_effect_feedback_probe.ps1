param(
    [string]$DataDrivenExe = 'D:\HwaSimIR\deliverables\HwaSimIR_P15\DataDrivenTestQT\DataDrivenTestQT.exe',
    [string]$VideoDisplayExe = 'D:\HwaSimIR\HwaSim_IR_VideoDisplay\x64\Release\HwaSim_IR_VideoDisplay.exe',
    [string]$EvidenceDirectory = 'D:\HwaSimIR\logs\p15\ordinary_feedback_probe',
    [int]$CaptureSeconds = 24,
    [int]$CaptureFps = 5
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Text;
using System.Runtime.InteropServices;

public static class P15FeedbackWindow
{
    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left, Top, Right, Bottom; }
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumWindowsProc callback, IntPtr lParam);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint processId);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr hWnd, IntPtr hdc, uint flags);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetWindowText(IntPtr hWnd, StringBuilder text, int count);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetClassName(IntPtr hWnd, StringBuilder text, int count);
}
'@

function Get-P15ProcessWindows([int]$ProcessId) {
    $items = New-Object System.Collections.Generic.List[object]
    $callback = [P15FeedbackWindow+EnumWindowsProc]{
        param([IntPtr]$handle, [IntPtr]$target)
        [uint32]$owner = 0
        [P15FeedbackWindow]::GetWindowThreadProcessId($handle, [ref]$owner) | Out-Null
        if ($owner -eq [int]$target.ToInt64() -and [P15FeedbackWindow]::IsWindowVisible($handle)) {
            $title = New-Object Text.StringBuilder 1024
            $className = New-Object Text.StringBuilder 256
            [P15FeedbackWindow]::GetWindowText($handle, $title, $title.Capacity) | Out-Null
            [P15FeedbackWindow]::GetClassName($handle, $className, $className.Capacity) | Out-Null
            $rect = New-Object P15FeedbackWindow+RECT
            if ([P15FeedbackWindow]::GetWindowRect($handle, [ref]$rect)) {
                $items.Add([pscustomobject]@{
                    Handle = $handle
                    Title = $title.ToString()
                    Class = $className.ToString()
                    Left = $rect.Left
                    Top = $rect.Top
                    Right = $rect.Right
                    Bottom = $rect.Bottom
                    Area = ($rect.Right - $rect.Left) * ($rect.Bottom - $rect.Top)
                })
            }
        }
        return $true
    }
    [P15FeedbackWindow]::EnumWindows($callback, [IntPtr]$ProcessId) | Out-Null
    return @($items | ForEach-Object { $_ })
}

function Get-P15LargestQtWindow([int]$ProcessId) {
    $deadline = [DateTime]::UtcNow.AddSeconds(20)
    do {
        $window = Get-P15ProcessWindows $ProcessId |
            Where-Object { $_.Class -like 'Qt5QWindow*' -and $_.Class -notlike '*Popup*' } |
            Sort-Object Area -Descending |
            Select-Object -First 1
        if ($window) { return $window }
        Start-Sleep -Milliseconds 100
    } while ([DateTime]::UtcNow -lt $deadline)
    throw "Visible Qt window not found for PID $ProcessId"
}

function Save-P15Window([object]$Window, [string]$Path) {
    $width = $Window.Right - $Window.Left
    $height = $Window.Bottom - $Window.Top
    $bitmap = New-Object Drawing.Bitmap $width, $height, ([Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    $hdc = $graphics.GetHdc()
    try {
        [P15FeedbackWindow]::PrintWindow($Window.Handle, $hdc, 2) | Out-Null
    } finally {
        $graphics.ReleaseHdc($hdc)
        $graphics.Dispose()
    }
    try { $bitmap.Save($Path, [Drawing.Imaging.ImageFormat]::Png) }
    finally { $bitmap.Dispose() }
}

function Get-P15Root([int]$ProcessId, [string]$WindowName) {
    $pidCondition = New-Object System.Windows.Automation.PropertyCondition(
        [System.Windows.Automation.AutomationElement]::ProcessIdProperty, $ProcessId)
    $nameCondition = New-Object System.Windows.Automation.PropertyCondition(
        [System.Windows.Automation.AutomationElement]::NameProperty, $WindowName)
    $condition = New-Object System.Windows.Automation.AndCondition($pidCondition, $nameCondition)
    $deadline = [DateTime]::UtcNow.AddSeconds(20)
    do {
        $root = [System.Windows.Automation.AutomationElement]::RootElement.FindFirst(
            [System.Windows.Automation.TreeScope]::Children, $condition)
        if ($root) { return $root }
        Start-Sleep -Milliseconds 100
    } while ([DateTime]::UtcNow -lt $deadline)
    throw "UI Automation root not found for PID $ProcessId"
}

function Get-P15NamedElement([System.Windows.Automation.AutomationElement]$Root, [string]$Name) {
    $condition = New-Object System.Windows.Automation.PropertyCondition(
        [System.Windows.Automation.AutomationElement]::NameProperty, $Name)
    $element = $Root.FindFirst([System.Windows.Automation.TreeScope]::Descendants, $condition)
    if (-not $element) { throw "UI element not found: $Name" }
    return $element
}

function Invoke-P15([System.Windows.Automation.AutomationElement]$Root, [string]$Name) {
    $element = Get-P15NamedElement $Root $Name
    $pattern = [System.Windows.Automation.InvokePattern]$element.GetCurrentPattern(
        [System.Windows.Automation.InvokePattern]::Pattern)
    $pattern.Invoke()
}

function Close-P15([System.Diagnostics.Process]$Process, [System.Windows.Automation.AutomationElement]$Root, [int]$TimeoutMs) {
    try {
        $windowPattern = [System.Windows.Automation.WindowPattern]$Root.GetCurrentPattern(
            [System.Windows.Automation.WindowPattern]::Pattern)
        $windowPattern.Close()
    } catch { $Process.CloseMainWindow() | Out-Null }
    if (-not $Process.WaitForExit($TimeoutMs)) {
        throw "Process $($Process.Id) did not close normally within $TimeoutMs ms"
    }
}

$DataDrivenExe = (Resolve-Path -LiteralPath $DataDrivenExe).Path
$VideoDisplayExe = (Resolve-Path -LiteralPath $VideoDisplayExe).Path
New-Item -ItemType Directory -Force -Path $EvidenceDirectory | Out-Null
$videoProcess = Start-Process -FilePath $VideoDisplayExe -WorkingDirectory (Split-Path -Parent $VideoDisplayExe) -PassThru
$dataProcess = Start-Process -FilePath $DataDrivenExe -WorkingDirectory (Split-Path -Parent $DataDrivenExe) -PassThru
$videoWindow = Get-P15LargestQtWindow $videoProcess.Id
$dataRoot = Get-P15Root $dataProcess.Id '激励数据软件 - 红方仿真激励端'
$frame = 0
$timeline = New-Object System.Collections.Generic.List[object]

for ($i = 0; $i -lt (2 * $CaptureFps); ++$i) {
    $path = Join-Path $EvidenceDirectory ('frame_{0:D4}.png' -f $frame)
    Save-P15Window $videoWindow $path
    $timeline.Add([pscustomobject]@{ frame = $frame; phase = 'ordinary_open_waiting'; elapsedMs = $i * (1000 / $CaptureFps) })
    $frame += 1
    Start-Sleep -Milliseconds ([int](1000 / $CaptureFps))
}

$targetCombo = Get-P15NamedElement $dataRoot '目标类型（INIT 后冻结）:'
$targetValue = [System.Windows.Automation.ValuePattern]$targetCombo.GetCurrentPattern(
    [System.Windows.Automation.ValuePattern]::Pattern)
$defaultTarget = $targetValue.Current.Value
Invoke-P15 $dataRoot '○ 初始化 (0x36)'
Start-Sleep -Seconds 1
Invoke-P15 $dataRoot '▲▼ 开始仿真 (2)'
$clock = [Diagnostics.Stopwatch]::StartNew()
$captureCount = $CaptureSeconds * $CaptureFps
for ($i = 0; $i -lt $captureCount; ++$i) {
    $path = Join-Path $EvidenceDirectory ('frame_{0:D4}.png' -f $frame)
    Save-P15Window $videoWindow $path
    $timeline.Add([pscustomobject]@{ frame = $frame; phase = 'original_1txt_running'; elapsedMs = $clock.ElapsedMilliseconds })
    $frame += 1
    Start-Sleep -Milliseconds ([int](1000 / $CaptureFps))
}

Invoke-P15 $dataRoot '■ 停止仿真 (3)'
Start-Sleep -Seconds 7
for ($i = 0; $i -lt (2 * $CaptureFps); ++$i) {
    $path = Join-Path $EvidenceDirectory ('frame_{0:D4}.png' -f $frame)
    Save-P15Window $videoWindow $path
    $timeline.Add([pscustomobject]@{ frame = $frame; phase = 'after_stop_drain'; elapsedMs = $clock.ElapsedMilliseconds })
    $frame += 1
    Start-Sleep -Milliseconds ([int](1000 / $CaptureFps))
}

Close-P15 $dataProcess $dataRoot 45000
$videoRoot = [System.Windows.Automation.AutomationElement]::FromHandle($videoWindow.Handle)
Close-P15 $videoProcess $videoRoot 20000

$result = [ordered]@{
    schema = 'hwasimir.p15.ordinary-effect-feedback-probe.v1'
    createdUtc = [DateTime]::UtcNow.ToString('o')
    method = 'ordinary zero-argument final DataDrivenTestQT plus ordinary zero-argument VideoDisplay; production files unmodified'
    dataDriven = [ordered]@{
        path = $DataDrivenExe
        sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $DataDrivenExe).Hash.ToLowerInvariant()
        workingDirectory = Split-Path -Parent $DataDrivenExe
        configPath = Join-Path (Split-Path -Parent $DataDrivenExe) 'NetworkConfig.ini'
        processId = $dataProcess.Id
        arguments = @()
        environmentOverrides = @()
        input = Join-Path (Split-Path -Parent $DataDrivenExe) '1.txt'
        defaultTarget = $defaultTarget
    }
    videoDisplay = [ordered]@{
        path = $VideoDisplayExe
        sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $VideoDisplayExe).Hash.ToLowerInvariant()
        workingDirectory = Split-Path -Parent $VideoDisplayExe
        configPath = Join-Path (Split-Path -Parent $VideoDisplayExe) 'NetworkConfig.ini'
        processId = $videoProcess.Id
        title = $videoWindow.Title
        arguments = @()
        environmentOverrides = @()
    }
    capture = [ordered]@{
        frames = $frame
        fps = $CaptureFps
        requestedRunSeconds = $CaptureSeconds
        timeline = @($timeline | ForEach-Object { $_ })
    }
    closure = [ordered]@{
        dataDrivenNormalExit = $true
        videoDisplayNormalExit = $true
    }
    interpretationStatus = 'PENDING_FRAME_REVIEW'
    acceptanceBoundary = 'This short ordinary-entry observation may reproduce feedback but cannot close either user-visible production issue without user confirmation.'
}
$resultPath = Join-Path $EvidenceDirectory 'probe_result.json'
$result | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $resultPath -Encoding UTF8
$result | ConvertTo-Json -Depth 6
