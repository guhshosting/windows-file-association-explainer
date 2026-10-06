@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b %errorlevel%
cmake -S . -B build-analyze -G Ninja -DCMAKE_BUILD_TYPE=Release -DWFA_ANALYZE=ON
if errorlevel 1 exit /b %errorlevel%
cmake --build build-analyze
if errorlevel 1 exit /b %errorlevel%
set wfa_tidy_failed=0
for %%f in (src\core.cpp src\windows.cpp src\main.cpp tests\tests.cpp tests\windows_tests.cpp) do (
 "C:\Program Files\LLVM\bin\clang-tidy.exe" %%f --header-filter=".*[/\\]src[/\\].*" --checks="-*,clang-analyzer-*,bugprone-*,performance-*,-bugprone-easily-swappable-parameters" --warnings-as-errors="*" --extra-arg=-Wno-ignored-attributes -- -std=c++17 -Isrc -DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN -DNOMINMAX
 if errorlevel 1 set wfa_tidy_failed=1
)
if "%wfa_tidy_failed%"=="1" exit /b 1
"C:\Program Files\Cppcheck\cppcheck.exe" --enable=warning,style,performance,portability --error-exitcode=1 --inline-suppr --suppress=missingIncludeSystem --library=windows --platform=win64 --std=c++17 -Isrc -DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN -DNOMINMAX src tests
if errorlevel 1 exit /b %errorlevel%
"python" scripts\verify.py


