# l4d2-viewmodel-standalone

DLL en C++ (x86) para Left 4 Dead 2 que ajusta en vivo la vista de las armas
(viewmodel) sobre los ejes X/Y/Z, con panel ImGui, crosshair personalizable y
congelado de cámara mientras el menú está abierto.

## Características

- Sliders X/Y/Z (-20 a 20) aplicados en vivo sobre el viewmodel
- Mira fija (crosshair): tipo cruz/X, tamaño 2..40 px, color con selector
- Panel ImGui con tecla propia (Insert por defecto, reasignable desde el panel)
- Guardado/carga de preset en `vmconfig.ini` (se genera automáticamente)
- Congelado de cámara con el menú abierto (con validaciones de sanidad)
- Soporta reset de Direct3D9 (cambio de resolución / fullscreen)
- `ESC` cierra el panel y reenvía la tecla al juego
- Log en `%TEMP%\vm_standalone_log.txt`

## Requisitos

- Windows con Visual Studio BuildTools 2022 (MSVC **x86**) + Windows SDK
- Left 4 Dead 2 (Steam), proceso x86

## Compilar

```
build.bat
```

Genera `build\viewmodel.dll` y lo copia a `.\viewmodel.dll`. Si no existe,
crea `vmconfig.ini` con valores por defecto.

## Pruebas de interfaz

```
tests\build_preview.bat after
```

Compila el menú real en una ventana oculta con eventos sintéticos y comprueba
sliders, guardado, captura de tecla, crosshair y panel. Imprime `PASS` si todo
cuadra. No toca el juego ni inyecta nada.

## Offsets del juego

Los offsets de memoria van incluidos en el código y son **específicos de una
build concreta** de Left 4 Dead 2:

- `viewmodel_hook.cpp` — `CALLER_RVA` (ubicación del hook en `client.dll`) y
  `PROLOGO` (firma de la función objetivo).
- `dx9_hook.cpp` — `OFF_ENGINE_PTR` y `OFF_ANGULOS` (congelado de cámara en
  `engine.dll`).

Si Steam actualiza el juego y los offsets dejan de coincidir, la comprobación
de firma hace que `vmh::Instalar()` devuelva `false` sin parchear nada y el
congelado de cámara se omite: la DLL queda inerte en lugar de provocar un
crash. Para adaptarla a otra build, actualiza esos valores en los dos archivos
indicados.

Usa estos offsets bajo tu propia responsabilidad.

## Uso

1. Compilar y obtener `viewmodel.dll`.
2. Inyectarla en el proceso de Left 4 Dead 2 (x86) con la herramienta que
   prefieras.
3. En partida, pulsa `Insert` (tecla por defecto) para abrir o cerrar el panel.

## Estructura

- `dllmain.cpp` — arranque, mutex anti-doble-inyección, log
- `viewmodel_hook.*` — cave de hook del viewmodel
- `dx9_hook.*` — hook de Direct3D9, ImGui, input y ESC
- `menu.*` — panel (sliders, crosshair, hotkey, preset)
- `imgui/` — Dear ImGui + fuentes (Inter, iconos Font Awesome)
- `tests/` — prueba aislada de la interfaz

## Créditos y licencias de fuentes

- [Dear ImGui](https://github.com/ocornut/imgui) — MIT, ver `imgui/LICENSE.txt`
- [Inter](https://github.com/rsms/inter/) (Rasmus Andersson) — SIL OFL 1.1,
  incrustada en `imgui/inter_font.h`
- [Font Awesome Free](https://fontawesome.com/) — glifos de fuente
  incrustados en `imgui/fa_icons.h`, SIL OFL 1.1

Avisos de copyright y texto completo de la SIL Open Font License 1.1 para
ambas fuentes: [`imgui/OFL-1.1.txt`](imgui/OFL-1.1.txt).

## Licencia

MIT — ver [LICENSE.txt](LICENSE.txt) (el código de este proyecto). Las
fuentes anteriores conservan su propia licencia (OFL 1.1).
