@echo off
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x86 >nul 2>&1
if errorlevel 1 call "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x86 >nul 2>&1
if errorlevel 1 call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x86 >nul 2>&1
if errorlevel 1 (
    echo [!] No se pudo cargar vcvarsall x86 - instala BuildTools MSVC x86
    exit /b 1
)
cd /d "%~dp0"
if not exist build mkdir build
echo [*] Compilando viewmodel standalone (solo X/Y/Z, sin caducidad)...
cl /nologo /O2 /MT /EHsc /DUNICODE /D_UNICODE /D_WIN32_WINNT=0x0600 /LD ^
    dllmain.cpp ^
    viewmodel_hook.cpp ^
    dx9_hook.cpp ^
    menu.cpp ^
    imgui\imgui.cpp ^
    imgui\imgui_draw.cpp ^
    imgui\imgui_tables.cpp ^
    imgui\imgui_widgets.cpp ^
    imgui\imgui_impl_dx9.cpp ^
    imgui\imgui_impl_win32.cpp ^
    /I"." /I"imgui" ^
    /Fe:build\viewmodel.dll /link /OUT:build\viewmodel.dll user32.lib d3d9.lib
if errorlevel 1 (
    echo [!] Compilacion fallo
    exit /b 1
)
echo [OK] build\viewmodel.dll generado
copy /Y "build\viewmodel.dll" "viewmodel.dll" >nul
echo [OK] viewmodel.dll copiado a .\viewmodel.dll
rem Crear vmconfig.ini por defecto si no existe
if not exist "vmconfig.ini" (
    (echo vmOffx = 0) > vmconfig.ini
    (echo vmOffy = 0) >> vmconfig.ini
    (echo vmOffz = 0) >> vmconfig.ini
    (echo hotkey = 45) >> vmconfig.ini
    echo [*] vmconfig.ini creado (hotkey 45 = INSERT)
)
del /q "*.obj" >nul 2>&1
echo [OK] Listo - inyecta viewmodel.dll con tu inyector
endlocal
