#pragma once
#include <windows.h>
namespace menu {
    void Inicializar(HWND hwnd);
    bool estaVisible();
    bool estaEnMenuJuego();
    void PerderFoco();
    void Cerrar();
    void Dibujar();
    void Apagar();
}
