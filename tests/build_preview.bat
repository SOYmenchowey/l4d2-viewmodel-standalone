@echo off
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x86 >nul 2>&1
if errorlevel 1 call "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x86 >nul 2>&1
if errorlevel 1 call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x86 >nul 2>&1
if errorlevel 1 exit /b 1
cd /d "%~dp0.."
if not exist "build\preview" mkdir "build\preview"
cl /nologo /O2 /MT /EHsc tests\ui_preview.cpp imgui\imgui.cpp imgui\imgui_draw.cpp ^
 imgui\imgui_tables.cpp imgui\imgui_widgets.cpp imgui\imgui_impl_dx9.cpp ^
 /I. /Iimgui /Fo"build\preview\\" /Fe:build\preview\ui_preview.exe /link user32.lib d3d9.lib gdiplus.lib
if errorlevel 1 exit /b 1
cd build\preview
ui_preview.exe %1
exit /b %errorlevel%
