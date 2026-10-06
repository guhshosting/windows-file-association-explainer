@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b %errorlevel%
dumpbin /imports build\windows-file-association-explainer.exe > artifacts\imports.txt
if errorlevel 1 exit /b %errorlevel%
dumpbin /headers /loadconfig build\windows-file-association-explainer.exe > artifacts\headers.txt
