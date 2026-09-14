"""Actual temporary display-mode test; restore the original mode in finally.

Qt scale is an explicit application test setting. It does not claim to change
the Windows desktop DPI setting. Unsupported physical modes are not emulated.
"""
import argparse
import ctypes as c
import json
import os
import struct
import subprocess
from pathlib import Path


def mode(index):
    buffer=c.create_string_buffer(220)
    struct.pack_into('<H',buffer,68,220)
    if not c.windll.user32.EnumDisplaySettingsW(None,index,buffer):
        return None
    return buffer


def dimensions(buffer):
    return struct.unpack_from('<II',buffer,172)


if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('--width',type=int,default=1920)
    parser.add_argument('--height',type=int,default=1080)
    parser.add_argument('--qt-scale',type=float,required=True)
    parser.add_argument('--name',required=True)
    parser.add_argument('--fullscreen',action='store_true')
    args=parser.parse_args()
    c.windll.shcore.SetProcessDpiAwareness(2)
    original=mode(-1)
    candidate=None
    for index in range(1000):
        value=mode(index)
        if value is None:break
        if dimensions(value)==(args.width,args.height):
            candidate=value;break
    if candidate is None:
        raise SystemExit('Physical display mode is unsupported; no simulated 4K claim.')
    report=dict(originalPhysical=dimensions(original),requestedPhysical=[args.width,args.height],
        qtScale=args.qt_scale,windowsDpi=c.windll.user32.GetDpiForSystem(),
        scaleSource='explicit QT_SCREEN_SCALE_FACTORS for this application test; Windows desktop DPI unchanged')
    env=os.environ.copy();env['QT_SCREEN_SCALE_FACTORS']=str(args.qt_scale)
    if args.fullscreen:env['P7ReceiverFullScreen']='1'
    report['requestedFullscreen']=args.fullscreen
    root=Path(__file__).resolve().parents[1]
    try:
        result=c.windll.user32.ChangeDisplaySettingsExW(None,candidate,None,0,None)
        if result!=0:raise RuntimeError(f'Display change failed: {result}')
        report['actualPhysical']=dimensions(mode(-1))
        assert tuple(report['actualPhysical'])==(args.width,args.height)
        subprocess.run(['powershell','-NoProfile','-ExecutionPolicy','Bypass','-File',str(root/'tools/p7_run_case.ps1'),
            '-Name',args.name,'-ProductionDefaults','-Normal','-Band','2','-Rate','30','-Seconds','10',
            '-CaptureSeconds','10','-SaveMp4','1','-OutputFps','60',
            '-CameraInput',str(root/'tools/p7_inputs/five_telemetry.json')],cwd=root,env=env,check=True)
    finally:
        report['restoreResult']=c.windll.user32.ChangeDisplaySettingsExW(None,original,None,0,None)
        report['restoredPhysical']=dimensions(mode(-1))
        output=root/'logs/p7'/args.name;output.mkdir(parents=True,exist_ok=True)
        (output/'physical_display_test.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
        print(json.dumps(report))
