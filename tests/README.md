# Pruebas aisladas

Conjunto mínimo de regresiones para el proyecto (viewmodel + tercera persona + overlay).
Los archivos están agrupados como el código de `src/`:

- `viewmodel/`: hooks del viewmodel y ejecución de la cave x86.
- `thirdperson/`: activación, distancia y transición de cámara.
- `core/`: guardado atómico de `vmconfig.ini` y permanencia de la DLL en memoria.
- `overlay/`: `dx9_regression.exe` comprueba la instalación real del hook Present/Reset
  en una ventana oculta y el parcheo seguro de vtables; `install` hace la prueba de humo
  completa (hooks, Present, Reset y recuperación).

## Ejecutar

```
tests\build_tests.bat
```

Compila todo en `build\tests\` y ejecuta cada regresión. Sale con error si alguna falla.
