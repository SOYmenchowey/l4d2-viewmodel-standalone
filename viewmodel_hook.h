#pragma once
#include <windows.h>

namespace vmh {
    extern volatile float g_offx;
    extern volatile float g_offy;
    extern volatile float g_offz;
    bool Instalar();
    void Desinstalar();
}
