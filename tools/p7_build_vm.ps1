param([string]$Name='current')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
if(!$env:HWASIMIR_VM_SSH_PASSWORD){throw 'Set HWASIMIR_VM_SSH_PASSWORD for the authorized build VM.'}
$originalAskpassPassword=$env:HWASIMIR_SSH_PASSWORD
try {
$env:HWASIMIR_SSH_PASSWORD=$env:HWASIMIR_VM_SSH_PASSWORD
$env:SSH_ASKPASS=Join-Path $PSScriptRoot 'p5_ssh_askpass.cmd'
$env:SSH_ASKPASS_REQUIRE='force';$env:DISPLAY='p7'
$remote='/home/linaro/userdata/HwaSimIR'
$ErrorActionPreference="Continue"
$files=@(& git -c core.safecrlf=false -C $root diff --name-only 2>$null)+@(& git -C $root ls-files --others --exclude-standard 2>$null)
if($LASTEXITCODE -ne 0){throw "Git source inventory failed"}
$ErrorActionPreference="Stop"
foreach($file in $files | Sort-Object -Unique){
    if($file -match '^HwaSim_IR/HwaSim_IR/(.+\.(cpp|h|inl))$'){$target=$Matches[1]}
    elseif($file -match '^DDS/.+\.(h|cpp)$'){$target=$file}
    else{continue}
    & scp -q -o StrictHostKeyChecking=yes (Join-Path $root $file) "linaro@192.168.203.128:$remote/$target"
    if($LASTEXITCODE -ne 0){throw "Upload failed: $file"}
}
& scp -q -o StrictHostKeyChecking=yes (Join-Path $root 'tools/p7_nv12_test.cpp') (Join-Path $root 'tools/p7_auto_mapping_test.cpp') "linaro@192.168.203.128:$remote/"
if($LASTEXITCODE -ne 0){throw 'Test upload failed'}
& ssh -o StrictHostKeyChecking=yes linaro@192.168.203.128 "cmake --build $remote/cmake-build-codex-rk3588 -j4" *> (Join-Path $root "logs/p7/build_vm_$Name.log")
if($LASTEXITCODE -ne 0){throw 'aarch64 build failed'}
& scp -q -o StrictHostKeyChecking=yes "linaro@192.168.203.128:$remote/cmake-build-codex-rk3588/HwaSim_IR" (Join-Path $root "logs/p7/HwaSim_IR_$Name")
if($LASTEXITCODE -ne 0){throw 'ELF copy failed'}
Get-FileHash (Join-Path $root "logs/p7/HwaSim_IR_$Name")
} finally {
    $env:HWASIMIR_SSH_PASSWORD=$originalAskpassPassword
}
