@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" amd64
if errorlevel 1 exit /b 1
cl /nologo /EHsc /O2 tools\p8_windows_clock_reference.cpp /Fo"logs\p8\clock_msvc.obj" /Fe"logs\p8\clock_msvc.exe"
if errorlevel 1 exit /b 1
echo compiler=MSVC14
logs\p8\clock_msvc.exe
if errorlevel 1 exit /b 1
set "PATH=D:\Qt\Qt5.12.12\Tools\mingw730_64\bin;%PATH%"
g++ -O2 -std=c++11 tools\p8_windows_clock_reference.cpp -o logs\p8\clock_mingw.exe
if errorlevel 1 exit /b 1
echo compiler=MinGW7.3
logs\p8\clock_mingw.exe
exit /b %ERRORLEVEL%
