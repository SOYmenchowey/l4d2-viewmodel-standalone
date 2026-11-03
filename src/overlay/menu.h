#pragma once
#include <windows.h>
namespace menu {
    void Inicializar(HWND hwnd);
    bool estaVisible();
    bool estaEnMenuJuego();
    bool tieneFoco();
    bool CancelarCaptura();
    void PerderFoco();
    void Cerrar();
    void Dibujar();
}
