@echo off
setlocal
call :load_msvc
if errorlevel 1 exit /b 1
cd /d "%~dp0"
if not exist "build\obj" mkdir "build\obj"
cl /nologo /O2 /MT /c third_party\minhook\src\buffer.c third_party\minhook\src\hook.c third_party\minhook\src\trampoline.c third_party\minhook\src\hde\hde32.c /Fo"build\obj\\"
if errorlevel 1 exit /b 1
echo [*] Compilando viewmodel + tercera persona (overlay ImGui)...
cl /nologo /O2 /MT /EHsc /DUNICODE /D_UNICODE /D_WIN32_WINNT=0x0600 /LD ^
    src\core\dllmain.cpp ^
    src\viewmodel\viewmodel_hook.cpp ^
    src\thirdperson\thirdperson.cpp ^
    src\overlay\dx9_hook.cpp ^
    src\overlay\menu.cpp ^
    third_party\imgui\imgui.cpp ^
    third_party\imgui\imgui_draw.cpp ^
    third_party\imgui\imgui_tables.cpp ^
    third_party\imgui\imgui_widgets.cpp ^
    third_party\imgui\imgui_impl_dx9.cpp ^
    third_party\imgui\imgui_impl_win32.cpp ^
    build\obj\buffer.obj build\obj\hook.obj build\obj\trampoline.obj build\obj\hde32.obj ^
    /I"src" /I"third_party" /I"third_party\imgui" /Fo"build\obj\\" ^
    /Fe:build\viewmodel_thirdperson.dll /link /OUT:build\viewmodel_thirdperson.dll user32.lib d3d9.lib
if errorlevel 1 (
    echo [!] Compilacion fallo
    exit /b 1
)
echo [OK] build\viewmodel_thirdperson.dll generado
copy /Y "build\viewmodel_thirdperson.dll" "viewmodel.dll" >nul
if errorlevel 1 (
    echo [!] No se pudo actualizar .\viewmodel.dll - el archivo puede estar en uso
    exit /b 1
)
rem Crear vmconfig.ini por defecto si no existe
if not exist "vmconfig.ini" (
    (echo vmOffx = 0) > vmconfig.ini
    (echo vmOffy = 0) >> vmconfig.ini
    (echo vmOffz = 0) >> vmconfig.ini
    (echo hotkey = 45) >> vmconfig.ini
    echo [*] vmconfig.ini creado (hotkey 45 = INSERT)
)
echo [OK] Listo - inyecta build\viewmodel_thirdperson.dll con tu inyector
endlocal
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
