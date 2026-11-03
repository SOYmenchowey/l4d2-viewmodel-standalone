#pragma once
#include <windows.h>
#include <cmath>
#include <cstring>

namespace tp_camera {
    // client.dll inspected on 2026-10-03. No executable bytes are patched.
    // CalcView approaches player+0x2630 at 58 units/s out and 256 units/s in.
    constexpr DWORD RVA_LOCAL_PLAYER = 0x726BD8;
    constexpr DWORD RVA_LOCAL_REFERENCE = 0x634FB;
    constexpr DWORD RVA_DESIRED_DISTANCE = 0x26B3F0;
    constexpr DWORD RVA_INTERPOLATION = 0x256908;
    constexpr DWORD RVA_DISTANCE_PARENT = 0x7C15A4;
    constexpr DWORD OFF_CURRENT_DISTANCE = 0x2630;

    inline bool Compatible(DWORD client) {
        const BYTE interpolation[] = {0xF3,0x0F,0x10,0x8E,0x30,0x26,0,0,
                                      0xF3,0x0F,0x10,0x55,0x88,0x0F,0x2F,0xD1,0x76,0x2C};
        const BYTE desiredStart[] = {0x55,0x8B,0xEC,0x51,0xA1};
        const BYTE desiredRead[] = {0xF3,0x0F,0x10,0x40,0x2C};
        const BYTE localRead[] = {0x8B,0x04,0x85};
        __try {
            return memcmp((void*)(client+RVA_INTERPOLATION),interpolation,sizeof(interpolation))==0 &&
                memcmp((void*)(client+RVA_DESIRED_DISTANCE),desiredStart,sizeof(desiredStart))==0 &&
                *(DWORD*)(client+RVA_DESIRED_DISTANCE+5)==client+RVA_DISTANCE_PARENT &&
                memcmp((void*)(client+RVA_DESIRED_DISTANCE+9),desiredRead,sizeof(desiredRead))==0 &&
                memcmp((void*)(client+RVA_LOCAL_REFERENCE),localRead,sizeof(localRead))==0 &&
                *(DWORD*)(client+RVA_LOCAL_REFERENCE+3)==client+RVA_LOCAL_PLAYER;
        } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
    }

    inline bool Sincronizar(DWORD client, float requested) {
        if (!client || !std::isfinite(requested) || requested < -600 || requested > 0 || !Compatible(client)) return false;
        __try {
            DWORD player=*(DWORD*)(client+RVA_LOCAL_PLAYER);
            if (!player) return false; // Loading/menu: retry on the next active frame.
            float* current=(float*)(player+OFF_CURRENT_DISTANCE);
            if (!std::isfinite(*current)) return false;
            if (*current==requested) return true;
            // Preserve game-owned camera modes (pinned, dead, hanging, etc.).
            // Only snap when the game's own target agrees with our requested mode.
            using DesiredDistance = float (__thiscall*)(void*);
            float desired=((DesiredDistance)(client+RVA_DESIRED_DISTANCE))((void*)player);
            if (!std::isfinite(desired) || fabsf(desired-requested)>0.001f) return false;
            *current=requested;
            return true;
        } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
    }
}
