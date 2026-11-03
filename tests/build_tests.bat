@echo off
setlocal
call :load_msvc
if errorlevel 1 exit /b 1
cd /d "%~dp0.."
if not exist "build\tests" mkdir "build\tests"

cl /nologo /O2 /MT /EHsc /I"src" tests\viewmodel\hook_regression.cpp /Fo"build\tests\\" /Fe:build\tests\hook_regression.exe
if errorlevel 1 exit /b 1
cl /nologo /O2 /MT /EHsc /I"src" tests\thirdperson\thirdperson_regression.cpp /Fo"build\tests\\" /Fe:build\tests\thirdperson_regression.exe
if errorlevel 1 exit /b 1
cl /nologo /O2 /MT /EHsc /I"src" tests\thirdperson\camera_transition_regression.cpp /Fo"build\tests\\" /Fe:build\tests\camera_transition_regression.exe
if errorlevel 1 exit /b 1
cl /nologo /O2 /MT /EHsc /I"src" /LD tests\core\lifetime_fixture.cpp /Fo"build\tests\\" /Fe:build\tests\lifetime_fixture.dll /link user32.lib
if errorlevel 1 exit /b 1
cl /nologo /O2 /MT /EHsc /I"src" tests\core\lifetime_regression.cpp /Fo"build\tests\\" /Fe:build\tests\lifetime_regression.exe
if errorlevel 1 exit /b 1
cl /nologo /O2 /MT /EHsc /I"src" tests\core\config_file_regression.cpp /Fo"build\tests\\" /Fe:build\tests\config_file_regression.exe
if errorlevel 1 exit /b 1
cl /nologo /O2 /MT /EHsc /DUNICODE /D_UNICODE tests\overlay\dx9_regression.cpp ^
 third_party\imgui\imgui.cpp third_party\imgui\imgui_draw.cpp third_party\imgui\imgui_tables.cpp third_party\imgui\imgui_widgets.cpp ^
 third_party\imgui\imgui_impl_dx9.cpp third_party\imgui\imgui_impl_win32.cpp ^
 /I"src" /I"third_party\imgui" /Fo"build\tests\\" /Fe:build\tests\dx9_regression.exe /link user32.lib d3d9.lib
if errorlevel 1 exit /b 1

cd build\tests
hook_regression.exe
if errorlevel 1 exit /b 1
thirdperson_regression.exe
if errorlevel 1 exit /b 1
camera_transition_regression.exe
if errorlevel 1 exit /b 1
lifetime_regression.exe
if errorlevel 1 exit /b 1
config_file_regression.exe
if errorlevel 1 exit /b 1
dx9_regression.exe
if errorlevel 1 exit /b 1
dx9_regression.exe install
exit /b %errorlevel%

:load_msvc
where cl.exe >nul 2>&1
if not errorlevel 1 exit /b 0
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo [!] MSVC no esta en PATH. Ejecuta este archivo desde Developer Command Prompt x86 de Visual Studio.
    exit /b 1
)
set "VSINSTALL="
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%i"
if not defined VSINSTALL (
    echo [!] No se encontro MSVC x86. Instala el componente C++ Build Tools.
    exit /b 1
)
call "%VSINSTALL%\VC\Auxiliary\Build\vcvarsall.bat" x86
if errorlevel 1 exit /b 1
exit /b 0
