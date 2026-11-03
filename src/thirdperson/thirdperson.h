#pragma once

namespace tp {
    constexpr float DISTANCIA_MIN = 30.0f;
    constexpr float DISTANCIA_MAX = 600.0f;
    constexpr float DISTANCIA_DEFAULT = 100.0f;
    extern bool g_activado;
    extern float g_distancia;
    float LimitarDistancia(float valor);
    bool Aplicar(); // Llamar desde Present; no modifica memoria si no se activo.
    bool HayError();
}
