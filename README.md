# L4D2 Viewmodel + Third Person

DLL x86 en C++ para ajustar el viewmodel de Left 4 Dead 2 y usar una cámara de tercera persona. Incluye un overlay ImGui con controles para el viewmodel, la mira y la cámara.

> Proyecto comunitario, sin afiliación con Valve. Los offsets son específicos de una versión del juego y pueden dejar de coincidir después de una actualización.

## Funciones

- Ajuste en vivo de los ejes X, Y y Z del viewmodel.
- Cámara de tercera persona con activación por menú o hotkey y distancia configurable.
- Mira personalizable: cruz, X, punto o círculo; tamaño, grosor, color fijo o RGB y cambio automático de forma.
- Overlay Direct3D 9 / ImGui, abierto por defecto con `Insert`.
- Preset local `vmconfig.ini`, guardado de forma atómica junto a la DLL.

## Requisitos

- Windows de 32 o 64 bits.
- Visual Studio 2022 Build Tools o Visual Studio con MSVC C++ x86/x64 y Windows SDK.
- Left 4 Dead 2 para Windows (proceso x86) para el uso dentro del juego.

## Compilar

Abre **Developer Command Prompt for VS** (x86) y ejecuta:

```bat
build.bat
```

El script también intenta localizar MSVC con `vswhere` si `cl.exe` no está en `PATH`. Produce `build\viewmodel_thirdperson.dll` y `viewmodel.dll`; los binarios y objetos están ignorados por Git.

Para compilar y ejecutar las regresiones aisladas:

```bat
tests\build_tests.bat
```

## Uso

1. Compila el proyecto.
2. Inicia Left 4 Dead 2 de 32 bits.
3. Inyecta `build\viewmodel_thirdperson.dll` con un inyector compatible.
4. Pulsa `Insert` para abrir el panel. Las teclas de menú, viewmodel y tercera persona se pueden reasignar desde el overlay.
5. Usa **Guardar** para conservar los ajustes. La DLL también carga el `vmconfig.ini` que esté junto a ella; `vmconfig.example.ini` es una plantilla limpia.

Este proyecto no incluye inyector ni binario precompilado. La carga de la DLL puede no estar permitida por servidores o políticas del juego; úsala bajo tu responsabilidad y de acuerdo con las reglas aplicables.

## Cómo funciona

- `src/core/dllmain.cpp` inicia el componente de forma diferida y evita inicializaciones duplicadas.
- `src/viewmodel/viewmodel_hook.cpp` instala con MinHook el hook del viewmodel después de comprobar la firma esperada del código.
- `src/thirdperson/thirdperson.cpp` localiza `z_view_distance`, confirma el nombre de la ConVar y actualiza sus valores para activar o desactivar la tercera persona.
- `src/thirdperson/camera_transition.h` verifica firmas de la rutina de cámara y sincroniza la distancia interpolada solo cuando el objetivo del juego coincide con el ajuste solicitado.
- `src/overlay/dx9_hook.cpp` dibuja el panel desde `Present`, maneja `Reset` de Direct3D 9 y reenvía la entrada del juego cuando corresponde.
- `src/overlay/menu.cpp` y `src/overlay/native_pages.inl` contienen las páginas y hotkeys del overlay. La configuración se escribe en un archivo temporal y se reemplaza de forma atómica.

## Offsets y compatibilidad

Los valores están incluidos intencionalmente para facilitar la revisión y actualización comunitaria. Son RVA/offsets de 32 bits, relativos a `client.dll` o `engine.dll` según se indica; no son direcciones absolutas válidas para todas las instalaciones.

| Archivo | Módulo | Valor | Uso |
| --- | --- | --- | --- |
| `src/viewmodel/viewmodel_hook.cpp` | `client.dll` | `CALLER_RVA = 0x1EFE0` | Sitio de llamada del hook; además valida el prólogo `55 8B EC 53 8B 5D 0C`. |
| `src/thirdperson/thirdperson.cpp` | `client.dll` | `RVA_Z_VIEW_DISTANCE = 0x7C1588` | ConVar `z_view_distance`; campos de nombre `+0x0C`, float `+0x2C`, entero `+0x30`. |
| `src/thirdperson/camera_transition.h` | `client.dll` | `0x726BD8`, `0x634FB`, `0x26B3F0`, `0x256908`, `0x7C15A4`, `0x2630` | Referencia al jugador local, rutinas/firmas de cámara, ConVar padre y distancia interpolada del jugador. |
| `src/overlay/dx9_hook.cpp` | `engine.dll` | `0x4268EC`, `+0x4AAC` | Referencia y ángulos del jugador usados para mantener la vista mientras el overlay está abierto. |

Las comprobaciones de firma y nombre hacen que varias rutas se omitan cuando no reconocen el binario esperado, pero no garantizan compatibilidad con todas las versiones. Tras una actualización de Steam, vuelve a validar los offsets antes de usar la DLL. No hay una identificación de build del juego asociada a estos valores en este repositorio.

## Configuración local

La configuración real `vmconfig.ini` se mantiene fuera del repositorio para no publicar preferencias del usuario. Puedes copiar `vmconfig.example.ini` y guardarlo junto a la DLL; el programa también crea valores iniciales si no existe.

Claves principales: `vmOffx`, `vmOffy`, `vmOffz`, `vmActivado`, `vmTecla`, `hotkey`, `crosshair*`, `tpActivado`, `tpDistancia` y `tpTecla`.

## Estructura

```text
src/          código de la DLL y del overlay
third_party/  Dear ImGui, MinHook y avisos de licencias
tests/        regresiones aisladas de hooks, cámara, overlay y configuración
build.bat     compilación x86 con MSVC
```

## Créditos y licencias

- Este proyecto: MIT, ver [LICENSE.txt](LICENSE.txt).
- Dear ImGui: MIT, ver `third_party/imgui/LICENSE.txt`.
- MinHook: BSD 2-Clause, ver `third_party/minhook/LICENSE.txt`.
- Fuentes Inter y glifos Font Awesome incluidos: SIL Open Font License 1.1, ver `third_party/imgui/OFL-1.1.txt`.
