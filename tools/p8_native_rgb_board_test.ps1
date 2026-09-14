$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
if(!$env:HWASIMIR_SSH_PASSWORD -or !$env:HWASIMIR_VM_SSH_PASSWORD){throw 'Set authorized board and VM credentials in environment.'}
$boardPassword=$env:HWASIMIR_SSH_PASSWORD
$env:SSH_ASKPASS=Join-Path $PSScriptRoot 'p5_ssh_askpass.cmd';$env:SSH_ASKPASS_REQUIRE='force';$env:DISPLAY='p8'
function Checked([string]$program,[string[]]$arguments){& $program @arguments;if($LASTEXITCODE -ne 0){throw "Native RGB verification failed: $program exit=$LASTEXITCODE"}}
try {
    $env:HWASIMIR_SSH_PASSWORD=$env:HWASIMIR_VM_SSH_PASSWORD
    $remote='/home/linaro/userdata/HwaSimIR'
    Checked 'scp' @('-q','-o','StrictHostKeyChecking=yes',"$root/tools/p8_native_rgb_test.cpp", "linaro@192.168.203.128:$remote/p8_native_rgb_test.cpp")
    Checked 'ssh' @('-o','StrictHostKeyChecking=yes','linaro@192.168.203.128',"aarch64-linux-gnu-g++ -O3 -std=c++11 -I$remote/IR $remote/p8_native_rgb_test.cpp -o $remote/p8_native_rgb_test")
    Checked 'scp' @('-q','-o','StrictHostKeyChecking=yes',"linaro@192.168.203.128:$remote/p8_native_rgb_test", "$root/logs/p8/native_rgb_test_aarch64")
    $env:HWASIMIR_SSH_PASSWORD=$boardPassword
    Checked 'scp' @('-q','-o','StrictHostKeyChecking=yes',"$root/logs/p8/native_rgb_test_aarch64", "$root/logs/p8/native_rgb_panda_golden.bin",'root@192.168.1.116:/userdata/HwaSimIR/logs/')
    Checked 'ssh' @('-o','StrictHostKeyChecking=yes','root@192.168.1.116','chmod +x /userdata/HwaSimIR/logs/native_rgb_test_aarch64 && /userdata/HwaSimIR/logs/native_rgb_test_aarch64 /userdata/HwaSimIR/logs/native_rgb_panda_golden.bin')
} finally {$env:HWASIMIR_SSH_PASSWORD=$boardPassword}
