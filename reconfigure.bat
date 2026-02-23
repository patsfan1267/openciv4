@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
cd /d C:\OpenCiv4\build
"C:\Program Files\CMake\bin\cmake.exe" .. 2>&1
