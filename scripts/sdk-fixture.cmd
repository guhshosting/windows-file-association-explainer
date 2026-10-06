@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /c /std:c++17 /W4 /analyze /Foartifacts\sdk-fixture.obj scripts\sdk-fixture.cpp
