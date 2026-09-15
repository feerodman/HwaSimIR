param([int]$ProcessId,[string]$OutputPath)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
Add-Type @'
using System;
using System.Runtime.InteropServices;
public class P9WindowCapture {
 [DllImport("user32.dll")]public static extern bool PrintWindow(IntPtr hwnd,IntPtr dc,uint flags);
 [DllImport("user32.dll")]public static extern bool GetWindowRect(IntPtr hwnd,out RECT rectangle);
 [StructLayout(LayoutKind.Sequential)]public struct RECT {public int left,top,right,bottom;}
}
'@
$condition=New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::ProcessIdProperty,$ProcessId)
$w=[System.Windows.Automation.AutomationElement]::RootElement.FindFirst([System.Windows.Automation.TreeScope]::Children,$condition)
if(!$w){throw 'No owned test window'}
$handle=[IntPtr]$w.Current.NativeWindowHandle
$rect=New-Object P9WindowCapture+RECT
if(![P9WindowCapture]::GetWindowRect($handle,[ref]$rect)){throw 'GetWindowRect failed'}
$bitmap=New-Object System.Drawing.Bitmap(($rect.right-$rect.left),($rect.bottom-$rect.top))
$graphics=[System.Drawing.Graphics]::FromImage($bitmap);$dc=$graphics.GetHdc()
try {if(![P9WindowCapture]::PrintWindow($handle,$dc,2)){throw 'PrintWindow failed'}} finally {$graphics.ReleaseHdc($dc);$graphics.Dispose()}
$bitmap.Save($OutputPath,[System.Drawing.Imaging.ImageFormat]::Png)
[pscustomobject]@{kind='actual_window_PrintWindow';width=$bitmap.Width;height=$bitmap.Height;processId=$ProcessId;path=$OutputPath} | ConvertTo-Json
$bitmap.Dispose()
