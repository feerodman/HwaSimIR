@echo off
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" amd64
if errorlevel 1 exit /b 1
cd /d D:\HwaSimIR
if not exist samples\P16IndependentGraphics\bin mkdir samples\P16IndependentGraphics\bin
cl /nologo /W4 /EHsc /MD /O2 /DUNICODE /D_UNICODE samples\P16IndependentGraphics\launcher\main.cpp /Fe:samples\P16IndependentGraphics\bin\P16IndependentGraphics.exe /link /SUBSYSTEM:CONSOLE
exit /b %errorlevel%

