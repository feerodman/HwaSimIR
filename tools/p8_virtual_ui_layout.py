"""Resize the actual Qt test window to a virtual 3840x2160 client area.

The desktop stays at its physical mode. QWidget::grab and its layout report
are produced by the tested application. This is NOT a physical 4K screen test.
Only a receiver child of this test's PowerShell process is manipulated.
"""
import argparse,ctypes as c,json,os,subprocess,time
from ctypes import wintypes as W
from pathlib import Path
import psutil

ROOT=Path(__file__).resolve().parents[1]
NAME='P8_virtual_4k_layout'

def main():
    c.windll.shcore.SetProcessDpiAwareness(2)
    user=c.windll.user32
    user.GetWindowThreadProcessId.argtypes=[W.HWND,c.POINTER(W.DWORD)]
    user.GetClientRect.argtypes=[W.HWND,c.POINTER(W.RECT)]
    user.GetWindowRect.argtypes=[W.HWND,c.POINTER(W.RECT)]
    user.SetWindowPos.argtypes=[W.HWND,W.HWND,c.c_int,c.c_int,c.c_int,c.c_int,W.UINT]
    CALLBACK=c.WINFUNCTYPE(W.BOOL,W.HWND,W.LPARAM)
    user.EnumWindows.argtypes=[CALLBACK,W.LPARAM]
    folder=ROOT/'logs/p8'/NAME
    if (folder/'request.json').exists():raise RuntimeError('Refusing to overwrite evidence')
    selected=json.loads((ROOT/'releases/windows/current.json').read_text())['version']
    args=['powershell','-NoProfile','-ExecutionPolicy','Bypass','-File',str(ROOT/'tools/p7_run_case.ps1'),'-Name',NAME,'-LogGroup','p8','-InputAudit','-NoDuplicateH264','-Normal','-ProductionDefaults','-Band','2','-Weather','1','-Rate','30','-Seconds','10','-CaptureSeconds','0','-CameraInput',str(ROOT/'tools/p8_inputs/ordinary5.json'),'-DiagnosticJson',str(ROOT/'tools/p8_inputs/ordinary5_diagnostic.json'),'-SenderExe',str(ROOT/f'releases/windows/{selected}/sender/DataDrivenTestQT.exe'),'-ReceiverExe',str(ROOT/f'releases/windows/{selected}/receiver/HwaSim_IR_VideoDisplay.exe')]
    record=dict(kind='VIRTUAL_LAYOUT_NOT_PHYSICAL_4K',requestedClientPhysical=[3840,2160],desktopModeChanged=False)
    with (ROOT/f'logs/p8/{NAME}_runner.log').open('w',encoding='utf-8') as log:
        process=subprocess.Popen(args,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT)
        owner=psutil.Process(process.pid);applied=False;deadline=time.monotonic()+100
        while process.poll() is None:
            if not applied and time.monotonic()<deadline:
                children={p.pid for p in owner.children(recursive=True) if p.name().lower()=='hwasim_ir_videodisplay.exe'}
                handles=[]
                @CALLBACK
                def visit(hwnd,_):
                    pid=W.DWORD();user.GetWindowThreadProcessId(hwnd,c.byref(pid))
                    if pid.value in children and user.IsWindowVisible(hwnd):handles.append((hwnd,pid.value))
                    return True
                user.EnumWindows(visit,0)
                if handles:
                    hwnd,pid=handles[0];user.ShowWindow(hwnd,9)
                    client=W.RECT();outer=W.RECT();user.GetClientRect(hwnd,c.byref(client));user.GetWindowRect(hwnd,c.byref(outer))
                    bw=outer.right-outer.left-client.right;bh=outer.bottom-outer.top-client.bottom
                    # Suppress the default WM_WINDOWPOSCHANGING size clamp to
                    # this physical monitor; WM_SIZE still updates the actual
                    # Qt client and its layout. No rendered image is rescaled.
                    result=user.SetWindowPos(hwnd,None,0,0,3840+bw,2160+bh,0x0414)
                    if not result:raise c.WinError()
                    user.GetClientRect(hwnd,c.byref(client));record.update(receiverPid=pid,actualClientPhysical=[client.right,client.bottom]);applied=True
            time.sleep(.2)
        record['runnerExitCode']=process.returncode;record['resizeApplied']=applied
    layout=folder/'receiver_ui.png.layout.json'
    if layout.exists():
        report=json.loads(layout.read_text());record['actualQtLayout']=report
        record['result']='PASS' if applied and record['actualClientPhysical']==[3840,2160] and report['leftWidth']==report['rightWidth'] and report['videoPhysicalWidth']<=1024 and all(report[k]['allCellTextFits'] and report[k]['entireTableVisible'] and report[k]['horizontalRange']==report[k]['verticalRange']==0 for k in ['tableWidget_platData','tableWidget_targetData']) else 'FAIL'
    else:record['result']='NOT_MEASURED'
    (folder/'virtual_layout_validation.json').write_text(json.dumps(record,indent=2),encoding='utf-8');print(json.dumps(record))

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--name',default=NAME);NAME=parser.parse_args().name;main()
