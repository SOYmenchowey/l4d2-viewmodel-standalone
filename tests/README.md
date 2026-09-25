# Prueba aislada de interfaz

Ejecutar `tests\build_preview.bat after` desde la carpeta del proyecto.
Requiere MSVC x86, Windows SDK y Direct3D 9.

El ejecutable compila el menu real con offsets de prueba. No inyecta DLLs,
no conecta al juego ni mueve el cursor del sistema. Usa una ventana oculta
y eventos sinteticos de ImGui. Su configuracion y capturas PNG quedan en
`build\preview`, separados del preset real.

Comprueba sliders, edicion numerica, guardado/recarga, reset, captura de tecla,
la X de tecla restaura Insert (nunca deja el menú sin tecla para reabrir),
cierre, crosshair (tipo +/X, tamaño, color, config antigua con defaults,
reinicio XYZ no lo apaga, dibujo con panel cerrado y dimensiones impares),
panel smooth open/close (~200 ms: mantiene render a mitad de caminata y
cierra a 0 verts) y distribucion en superficies de 900x600, 320x560 y 320x300.
El menu cerrado sin crosshair debe producir cero vertices; con crosshair activo
solo la mira (sin animacion de fade). El credito @tokyossz se dibuja fuera
del panel (foreground), con el alpha/slide del panel.

Mediciones de referencia del 2026-09-23, 2000 frames por corrida:
- Original: 0.0095 ms de CPU/frame; 452 vertices y 1536 indices.
- Redisenado: 0.0111 ms de CPU/frame; 864 vertices y 2532 indices.
- Con crosshair simple (panel abierto): ~0.0115 ms de CPU/frame; 932 vertices y 2724 indices.
- Con crosshair tipo/size/color (panel abierto): ~0.0140 ms de CPU/frame; 1204 vertices y 3720 indices.
- Con fade crosshair + @tokyossz (retirado): ~0.0147 ms de CPU/frame; 1240 vertices y 3774 indices.
- Panel smooth + sin anim. crosshair (panel abierto): ~0.0143 ms de CPU/frame; 1240 vertices y 3774 indices.

Es una medicion aislada de construccion de la interfaz, sin tiempo de GPU
ni carga del juego. No demuestra que los FPS en partida sean identicos.
Las capturas usan valores de ejemplo, no los valores del usuario.
