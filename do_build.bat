@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
cd /d C:\OpenCiv4\build
"C:\Program Files\CMake\bin\cmake.exe" -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release .. 2>&1
if errorlevel 1 exit /b 1
nmake 2>&1
if errorlevel 1 exit /b 1
echo BUILD_SUCCESS
