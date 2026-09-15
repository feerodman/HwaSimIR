@echo off
setlocal
set VSLANG=1033
call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" amd64
if errorlevel 1 exit /b 1
cd /d D:\HwaSimIR
if not exist logs\p10\bin mkdir logs\p10\bin
set VSLANG=1033
cl /nologo /c /EHsc /O2 /MD /I HwaSim_IR\HwaSim_IR\opencv2-440 tools\p10_profile_check.cpp /Fologs\p10\bin\p10_profile_check.obj
if errorlevel 1 exit /b 1
cl /nologo /c /EHsc /O2 /MD /I HwaSim_IR\HwaSim_IR\opencv2-440 HwaSim_IR\HwaSim_IR\IR\IRConfig.cpp /Fologs\p10\bin\IRConfig.obj
if errorlevel 1 exit /b 1
cl /nologo /c /EHsc /O2 /MD HwaSim_IR\HwaSim_IR\IR\IRTypes.cpp /Fologs\p10\bin\IRTypes.obj
if errorlevel 1 exit /b 1
"C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\bin\amd64\link.exe" /nologo /out:logs\p10\bin\p10_profile_check.exe logs\p10\bin\p10_profile_check.obj logs\p10\bin\IRConfig.obj logs\p10\bin\IRTypes.obj /LIBPATH:HwaSim_IR\HwaSim_IR\opencv2-440\opencv_x64\vc14\lib opencv_world440.lib
exit /b %errorlevel%
