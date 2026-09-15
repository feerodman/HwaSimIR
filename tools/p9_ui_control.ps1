param([int]$ProcessId,[ValidateSet('inspect','reset','init','start','stop','close')][string]$Action='inspect')
$ErrorActionPreference='Stop'
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
$condition=New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::ProcessIdProperty,$ProcessId)
$window=[System.Windows.Automation.AutomationElement]::RootElement.FindFirst([System.Windows.Automation.TreeScope]::Children,$condition)
if(!$window){throw "No visible UI for owned test PID $ProcessId"}
if($Action -eq 'close'){
    $pattern=$window.GetCurrentPattern([System.Windows.Automation.WindowPattern]::Pattern)
    $pattern.Close();return
}
$elements=$window.FindAll([System.Windows.Automation.TreeScope]::Descendants,[System.Windows.Automation.Condition]::TrueCondition)
if($Action -eq 'inspect'){
    $elements | ForEach-Object {if($_.Current.ControlType.ProgrammaticName -match 'Button|ComboBox|CheckBox|Edit|Text'){
        [pscustomobject]@{name=$_.Current.Name;type=$_.Current.ControlType.ProgrammaticName;rect=$_.Current.BoundingRectangle.ToString()}
    }} | ConvertTo-Json;return
}
$match=@{reset='\(1\)$';init='\(0x36\)$';start='\(2\)$';stop='\(3\)$'}[$Action]
$buttons=@($elements | Where-Object {$_.Current.ControlType.ProgrammaticName -eq 'ControlType.Button' -and $_.Current.Name -match $match})
if($buttons.Count -ne 1){throw "Expected one $Action button; got $($buttons.Count)"}
$label=$buttons[0].Current.Name
$pattern=$buttons[0].GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern)
$pattern.Invoke()
[pscustomobject]@{processId=$ProcessId;action=$Action;button=$label;utc=[DateTime]::UtcNow.ToString('o');source='actual_Qt_UI_InvokePattern'} | ConvertTo-Json -Compress
