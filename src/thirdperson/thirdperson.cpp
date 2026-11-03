#include "thirdperson.h"
#include "camera_transition.h"
#include <windows.h>
#include <cmath>
#include <cstring>

namespace {
    // Mismo modo normal de 06_thirdperson: z_view_distance negativo.
    constexpr DWORD RVA_Z_VIEW_DISTANCE = 0x7C1588;
    constexpr DWORD OFS_NOMBRE = 0x0C;
    constexpr DWORD OFS_FLOAT = 0x2C;
    constexpr DWORD OFS_INT = 0x30;
    bool g_aplicado = false;
    bool g_error = false;
    DWORD g_cvarAplicada = 0;
    float g_valorAplicado = 0;
    ULONGLONG g_reintento = 0;
    bool g_solicitudActiva = false;
    float g_solicitudDistancia = tp::DISTANCIA_DEFAULT;

    DWORD Localizar() {
        HMODULE client = GetModuleHandleA("client.dll");
        if (!client) return 0;
        DWORD cvar = (DWORD)client + RVA_Z_VIEW_DISTANCE;
        // Verificar el nombre real antes de escribir en un offset de otra build.
        __try {
            const char* nombre = *(const char**)(cvar + OFS_NOMBRE);
            if (!nombre || memcmp(nombre, "z_view_distance", sizeof("z_view_distance")) != 0) return 0;
            return cvar;
        } __except(EXCEPTION_EXECUTE_HANDLER) { return 0; }
    }

    bool Escribir(DWORD cvar, float valor) {
        DWORD old = 0;
        if (!VirtualProtect((void*)(cvar + OFS_FLOAT), 8, PAGE_READWRITE, &old)) return false;
        bool ok = false;
        __try {
            *(float*)(cvar + OFS_FLOAT) = valor;
            *(int*)(cvar + OFS_INT) = (int)valor;
            // Incluso si restaurar la proteccion falla, OFF debe revertir lo escrito.
            g_aplicado = true;
            ok = true;
        } __except(EXCEPTION_EXECUTE_HANDLER) { }
        DWORD ignored = 0;
        if (!VirtualProtect((void*)(cvar + OFS_FLOAT), 8, old, &ignored)) ok = false;
        return ok;
    }
    bool Coincide(DWORD cvar, float valor) {
        __try {
            return *(float*)(cvar + OFS_FLOAT) == valor && *(int*)(cvar + OFS_INT) == (int)valor;
        } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
    }
}

namespace tp {
    bool g_activado = false;
    float g_distancia = DISTANCIA_DEFAULT;

    float LimitarDistancia(float valor) {
        if (!std::isfinite(valor)) return DISTANCIA_DEFAULT;
        if (valor < DISTANCIA_MIN) return DISTANCIA_MIN;
        if (valor > DISTANCIA_MAX) return DISTANCIA_MAX;
        return valor;
    }
    bool HayError() { return g_error; }
    bool Aplicar() {
        g_distancia = LimitarDistancia(g_distancia);
        // A new toggle/distance must not inherit the backoff of an old failure.
        if (g_activado!=g_solicitudActiva || g_distancia!=g_solicitudDistancia) {
            g_reintento=0;
            g_solicitudActiva=g_activado;
            g_solicitudDistancia=g_distancia;
        }
        if (!g_activado && !g_aplicado) { g_error = false; g_reintento = 0; return true; }
        ULONGLONG now = GetTickCount64();
        if (g_error && now < g_reintento) return false;
        DWORD cvar = Localizar();
        float valor = g_activado ? -g_distancia : 0.0f;
        bool sinCambios = cvar && !g_error && g_aplicado && cvar == g_cvarAplicada &&
            valor == g_valorAplicado && Coincide(cvar, valor);
        if (!cvar || !(sinCambios || Escribir(cvar, valor))) {
            g_error = true;
            g_reintento = now + 1000;
            return false;
        }
        g_aplicado = g_activado;
        g_cvarAplicada = cvar;
        g_valorAplicado = valor;
        g_error = false;
        g_reintento = 0;
        // The CVar is the target; this is the separately interpolated distance.
        // Includes the OFF frame, while keeping native wall/camera calculations.
        tp_camera::Sincronizar(cvar-RVA_Z_VIEW_DISTANCE, valor);
        return true;
    }
}
