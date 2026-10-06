@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b %errorlevel%
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 exit /b %errorlevel%
cmake --build build
if errorlevel 1 exit /b %errorlevel%
ctest --test-dir build --output-on-failure
