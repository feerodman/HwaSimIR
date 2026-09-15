@echo off
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" amd64
cd /d D:\HwaSimIR
set VSLANG=1033
cl /nologo /c /w /EHsc /MD /O2 /I F:\Programs\Panda3D-1.10.15-x64\include tools\p10_label_test.cpp /Fologs\p10\bin\p10_label_test.obj
if errorlevel 1 exit /b 1
cl /nologo /c /w /EHsc /MD /O2 /I F:\Programs\Panda3D-1.10.15-x64\include HwaSim_IR\HwaSim_IR\Annotation\AnnotationOverlay.cpp /Fologs\p10\bin\AnnotationOverlay.obj
if errorlevel 1 exit /b 1
"C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\bin\amd64\link.exe" /nologo /out:logs\p10\bin\p10_label_test.exe logs\p10\bin\p10_label_test.obj logs\p10\bin\AnnotationOverlay.obj /LIBPATH:F:\Programs\Panda3D-1.10.15-x64\lib libp3framework.lib libpanda.lib libpandaexpress.lib libp3dtool.lib libp3dtoolconfig.lib
exit /b %errorlevel%
