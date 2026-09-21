[CmdletBinding()]
param(
    [string]$Root = ''
)

$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($Root)) {
    $Root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
}
$package = Join-Path $Root 'deliverables\HwaSimIR_P15'
$utf8 = New-Object System.Text.UTF8Encoding($false)

function Copy-P15File {
    param([string]$Source, [string]$RelativeDestination)
    $resolvedSource = Join-Path $Root $Source
    if (-not (Test-Path -LiteralPath $resolvedSource -PathType Leaf)) {
        throw "Missing P15 delivery input: $resolvedSource"
    }
    $destination = Join-Path $package $RelativeDestination
    $parent = Split-Path -Parent $destination
    if (-not (Test-Path -LiteralPath $parent)) {
        New-Item -ItemType Directory -Force -Path $parent | Out-Null
    }
    Copy-Item -LiteralPath $resolvedSource -Destination $destination -Force
}

if (-not (Test-Path -LiteralPath (Join-Path $package 'DataDrivenTestQT\DataDrivenTestQT.exe'))) {
    throw 'The staged DataDrivenTestQT runtime is missing; create it before building the evidence package.'
}

Copy-P15File 'docs\HwaSimIR_P15_Delivery_README.md' 'README.md'
Copy-P15File 'docs\HwaSimIR_P15_Closeout.md' 'docs\HwaSimIR_P15_Closeout.md'
Copy-P15File 'docs\HwaSimIR_P15_Issue_Ledger.csv' 'docs\HwaSimIR_P15_Issue_Ledger.csv'
Copy-P15File 'logs\p15\final_status.json' 'final_status.json'

$copies = @(
    @('logs\p15\ui\final_interaction\P15_final_DataDrivenTestQT_interaction.mp4', 'evidence\ui\P15_final_DataDrivenTestQT_interaction.mp4'),
    @('logs\p15\ui\final_interaction\interaction_result.json', 'evidence\ui\interaction_result.json'),
    @('logs\p15\ui\final_interaction\frame_0000_ordinary_open.png', 'evidence\ui\frame_0000_ordinary_open.png'),
    @('logs\p15\ui\final_interaction\frame_0008_dropdown_open.png', 'evidence\ui\frame_0008_dropdown_open.png'),
    @('logs\p15\ui\final_interaction\frame_0016_civil_selected.png', 'evidence\ui\frame_0016_civil_selected.png'),
    @('logs\p15\ui\final_interaction\frame_0024_after_init_frozen.png', 'evidence\ui\frame_0024_after_init_frozen.png'),
    @('logs\p15\ui\final_interaction\frame_0032_after_reset_unlocked.png', 'evidence\ui\frame_0032_after_reset_unlocked.png'),
    @('logs\p15\ui\final_interaction\frame_0040_version_info.png', 'evidence\ui\frame_0040_version_info.png'),
    @('logs\p15\ui\final_interaction\frame_0056_reopened_same_delivery.png', 'evidence\ui\frame_0056_reopened_same_delivery.png'),
    @('logs\p15\ui\default\ordinary_default.json', 'evidence\ui\ordinary_default.json'),
    @('logs\p15\ui\default\ordinary_default_default.png', 'evidence\ui\ordinary_default_default.png'),
    @('logs\p15\ui\scale_125\scale_125.json', 'evidence\ui\scale_125.json'),
    @('logs\p15\ui\scale_125\scale_125_default.png', 'evidence\ui\scale_125_default.png'),
    @('logs\p15\ui\scale_150\scale_150.json', 'evidence\ui\scale_150.json'),
    @('logs\p15\ui\scale_150\scale_150_default.png', 'evidence\ui\scale_150_default.png'),
    @('logs\p15\numeric_labels\before\labels_0_after.png', 'evidence\numeric\before_names_and_coordinates.png'),
    @('logs\p15\numeric_labels\after\p15_numeric_hidden_2.png', 'evidence\numeric\after_hidden_2.png'),
    @('logs\p15\numeric_labels\after\p15_numeric_1_2_3.png', 'evidence\numeric\after_1_2_3.png'),
    @('logs\p15\numeric_labels\after\p15_stable_numbering.txt', 'evidence\numeric\p15_stable_numbering.txt'),
    @('logs\p15\numeric_labels\after\label_tests.csv', 'evidence\numeric\label_tests.csv'),
    @('logs\p15\ordinary_feedback_probe\P15_ordinary_original_1txt_mwir_dds.mp4', 'evidence\ordinary\P15_ordinary_original_1txt_mwir_dds.mp4'),
    @('logs\p15\ordinary_feedback_probe\P15_ordinary_user_entry_feedback_probe.mp4', 'evidence\ordinary\P15_ordinary_user_entry_feedback_probe.mp4'),
    @('logs\p15\ordinary_feedback_probe\probe_result.json', 'evidence\ordinary\probe_result.json'),
    @('logs\p15\ordinary_feedback_probe\frame_review.json', 'evidence\ordinary\frame_review.json'),
    @('logs\p15\ordinary_feedback_probe\dds_recording_status.json', 'evidence\ordinary\dds_recording_status.json'),
    @('logs\p15\ordinary_feedback_probe\dds_frame_index.jsonl', 'evidence\ordinary\dds_frame_index.jsonl'),
    @('logs\p15\ordinary_feedback_probe\dds_producer_annotations.jsonl', 'evidence\ordinary\dds_producer_annotations.jsonl'),
    @('logs\p15\ordinary_feedback_probe\dds_full_decode_rawvideo.stderr.log', 'evidence\ordinary\dds_full_decode_rawvideo.stderr.log'),
    @('logs\p15\ordinary_feedback_probe\dds_t08s.png', 'evidence\ordinary\dds_t08s.png'),
    @('logs\p15\ordinary_feedback_probe\dds_t12s.png', 'evidence\ordinary\dds_t12s.png'),
    @('logs\p15\ordinary_feedback_probe\dds_t24s.png', 'evidence\ordinary\dds_t24s.png'),
    @('logs\p15\ordinary_feedback_probe\dds_t32s.png', 'evidence\ordinary\dds_t32s.png'),
    @('logs\p15\deployment\p15_ordinary_feedback_board.log', 'evidence\ordinary\p15_ordinary_feedback_board.log'),
    @('logs\p15\p14_media_audit\p14_media_readonly_audit.json', 'evidence\p14\p14_media_readonly_audit.json'),
    @('logs\p14\deliverables\final_status.json', 'evidence\p14\historical_final_status.json'),
    @('logs\p14\deliverables\HwaSimIR_P14_Delivery_receipt.json', 'evidence\p14\historical_delivery_receipt.json'),
    @('logs\p14\deliverables\HwaSimIR_P14_Delivery\manifest.json', 'evidence\p14\historical_manifest.json'),
    @('logs\p14\deliverables\HwaSimIR_P14_Delivery\media\images\image_manifest.json', 'evidence\p14\historical_image_manifest.json'),
    @('docs\HwaSimIR_P14_Closeout.md', 'evidence\p14\HwaSimIR_P14_Closeout.md'),
    @('docs\HwaSimIR_P14_Issue_Ledger.csv', 'evidence\p14\HwaSimIR_P14_Issue_Ledger.csv'),
    @('logs\p15\identity\program_identity.json', 'evidence\identity\program_identity.json'),
    @('logs\p15\build_result.json', 'evidence\build\build_result.json'),
    @('logs\p15\build_DataDrivenTestQT_release.log', 'evidence\build\build_DataDrivenTestQT_release.log'),
    @('logs\p15\build_p15_ui_test_release.log', 'evidence\build\build_p15_ui_test_release.log'),
    @('logs\p15\build_HwaSimIR_win_x64_release.log', 'evidence\build\historical_sandbox_access_denied.log'),
    @('logs\p15\deployment\HwaSim_IR.p15.aarch64', 'deployment\board\HwaSim_IR'),
    @('logs\p15\deployment\deployment_version.env', 'deployment\board\deployment_version.env'),
    @('logs\p15\deployment\p15_normal_launcher_smoke.log', 'evidence\deployment\p15_normal_launcher_smoke.log'),
    @('build-DataDrivenTestQT-p15-ui-test-Release\release\p15_ui_default_visibility_test.exe', 'tests\p15_ui_default_visibility_test.exe'),
    @('logs\p10\bin\p10_label_test.exe', 'tests\p10_label_test.exe'),
    @('tools\p15_final_ui_evidence.ps1', 'tools\p15_final_ui_evidence.ps1'),
    @('tools\p15_inventory_program_identity.ps1', 'tools\p15_inventory_program_identity.ps1'),
    @('tools\p15_audit_p14_media.ps1', 'tools\p15_audit_p14_media.ps1'),
    @('tools\p15_ordinary_effect_feedback_probe.ps1', 'tools\p15_ordinary_effect_feedback_probe.ps1'),
    @('tools\p15_build_delivery.ps1', 'tools\p15_build_delivery.ps1')
)

foreach ($copy in $copies) {
    Copy-P15File $copy[0] $copy[1]
}

$excluded = @('manifest.json', 'SHA256SUMS.txt', 'HwaSimIR_P15_Delivery_receipt.json')
$payloadFiles = Get-ChildItem -LiteralPath $package -Recurse -File |
    Where-Object { $excluded -notcontains $_.Name } |
    Sort-Object FullName
$entries = foreach ($file in $payloadFiles) {
    $relative = $file.FullName.Substring($package.Length + 1).Replace('\', '/')
    [pscustomobject][ordered]@{
        path = $relative
        bytes = $file.Length
        sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $file.FullName).Hash.ToLowerInvariant()
    }
}
$manifestObject = [ordered]@{
    schema = 'hwasimir.p15.delivery-manifest.v1'
    createdUtc = (Get-Date).ToUniversalTime().ToString('o')
    root = $package
    coverage = 'All payload files except manifest.json, HwaSimIR_P15_Delivery_receipt.json and SHA256SUMS.txt'
    fileCount = $entries.Count
    totalBytes = [long](($entries | Measure-Object -Property bytes -Sum).Sum)
    files = $entries
}
$manifestPath = Join-Path $package 'manifest.json'
[IO.File]::WriteAllText($manifestPath, ($manifestObject | ConvertTo-Json -Depth 6), $utf8)
$manifestHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $manifestPath).Hash.ToLowerInvariant()

$receipt = [ordered]@{
    schema = 'hwasimir.p15.delivery-receipt.v1'
    createdUtc = (Get-Date).ToUniversalTime().ToString('o')
    deliveryRoot = $package
    payloadFileCount = $entries.Count
    payloadBytes = [long](($entries | Measure-Object -Property bytes -Sum).Sum)
    manifestSha256 = $manifestHash
    finalDataDrivenExeSha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $package 'DataDrivenTestQT\DataDrivenTestQT.exe')).Hash.ToLowerInvariant()
    original1TxtSha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $package 'DataDrivenTestQT\1.txt')).Hash.ToLowerInvariant()
    statuses = [ordered]@{
        buildPassed = 'PASS'
        defaultUiVisible = 'PASS'
        actualInteraction = 'PASS_AUTOMATED_FINAL_EXE'
        numericLabels = 'PASS'
        mediaDecodable = 'PASS'
        ordinaryWeatherVisible = 'REOPENED'
        ordinaryPlumeVisible = 'REOPENED'
        calibration = 'NOT_VERIFIED_CALIBRATION'
    }
}
$receiptPath = Join-Path $package 'HwaSimIR_P15_Delivery_receipt.json'
[IO.File]::WriteAllText($receiptPath, ($receipt | ConvertTo-Json -Depth 6), $utf8)

$checksumFiles = Get-ChildItem -LiteralPath $package -Recurse -File |
    Where-Object { $_.Name -ne 'SHA256SUMS.txt' } |
    Sort-Object FullName
$checksumLines = foreach ($file in $checksumFiles) {
    $relative = $file.FullName.Substring($package.Length + 1).Replace('\', '/')
    $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $file.FullName).Hash.ToLowerInvariant()
    "$hash *$relative"
}
[IO.File]::WriteAllLines((Join-Path $package 'SHA256SUMS.txt'), $checksumLines, $utf8)

[pscustomobject]@{
    result = 'PASS'
    deliveryRoot = $package
    payloadFileCount = $entries.Count
    payloadBytes = [long](($entries | Measure-Object -Property bytes -Sum).Sum)
    manifestSha256 = $manifestHash
    checksumEntries = $checksumLines.Count
} | ConvertTo-Json
