#include "viewmodel_hook.h"
#include <windows.h>
#include <math.h>
#include <vector>
#include <cstring>

namespace vmh {
    volatile float g_offx = 0.0f;
    volatile float g_offy = 0.0f;
    volatile float g_offz = 0.0f;
    volatile float g_factor = 1.0f;
}

namespace {

    // RVA del caller de CalcViewModelView dentro de client.dll.
    // Específico de la build del juego (ver README: si el juego se actualiza, la
    // comprobación de firma hace fallar Instalar() sin parchear nada).
    const DWORD CALLER_RVA = 0x1EFE0;
    const DWORD VUELTA_OFF = 0x7;
    const DWORD COPY_OFF = 0x320;
    const DWORD TAMANO_HOOK = 5;

    enum { SX = 0x00, CX = 0x04, SY = 0x08, CY = 0x0C, SZ = 0x10, CZ = 0x14,
           FWD_X = 0x18, FWD_Y = 0x1C, FWD_Z = 0x20,
           RGT_X = 0x24, RGT_Y = 0x28, RGT_Z = 0x2C,
           UP_X = 0x30, UP_Y = 0x34, UP_Z = 0x38,
           D2R = 0x40, HB = 0x44 };

    DWORD g_caller = 0;
    BYTE g_orig[TAMANO_HOOK];
    DWORD g_cave = 0;
    bool g_instalado = false;
    BYTE g_patch[TAMANO_HOOK];

    bool LeerBytes(DWORD direccion, void* destino, size_t longitud) {
        __try { memcpy(destino, (void*)direccion, longitud); return true; }
        __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
    }

    std::vector<unsigned char> code;

    void put8(unsigned char x) { code.push_back(x); }
    void put32(DWORD x) { for (int i = 0; i < 4; i++) put8((unsigned char)((x >> (8 * i)) & 0xFF)); }
    void m32(unsigned char opcode, unsigned char modrm, DWORD addr) {
        put8(opcode); put8(modrm); put32(addr);
    }
    void fld_m32(DWORD a) { m32(0xD9, 0x05, a); }
    void fstp_m32(DWORD a) { m32(0xD9, 0x1D, a); }
    void fmul_m32(DWORD a) { m32(0xD8, 0x0D, a); }
    void fld_edi(DWORD off) { put8(0xD9); put8(0x47); put8((unsigned char)off); }
    void fadd_esi(DWORD off) { put8(0xD8); put8(0x46); put8((unsigned char)off); }
    void fstp_esi(DWORD off) { put8(0xD9); put8(0x5E); put8((unsigned char)off); }
    void faddp() { put8(0xDE); put8(0xC1); }
    void fsubp() { put8(0xDE); put8(0xE9); }
    void fchs() { put8(0xD9); put8(0xE0); }
    void call32(DWORD cave, DWORD destino) {
        DWORD rel = destino - (cave + code.size() + 5);
        put8(0xE8); put32(rel);
    }
    void jmp32(DWORD cave, DWORD destino) {
        DWORD rel = destino - (cave + code.size() + 5);
        put8(0xE9); put32(rel);
    }

    void llamar_sin_cos(DWORD cave, DWORD scratch, DWORD off_ang, DWORD slot_sin, DWORD slot_cos) {
        DWORD fns[2] = { (DWORD)&sin, (DWORD)&cos };
        DWORD slots[2] = { slot_sin, slot_cos };
        for (int k = 0; k < 2; k++) {
            fld_edi(off_ang);
            fmul_m32(scratch + D2R);
            put8(0x83); put8(0xEC); put8(0x08);       // sub esp, 8
            put8(0xDD); put8(0x1C); put8(0x24);       // fstp [esp]
            call32(cave, fns[k]);
            put8(0x83); put8(0xC4); put8(0x08);       // add esp, 8
            fstp_m32(scratch + slots[k]);
        }
    }

    std::vector<unsigned char> construir(DWORD cave, DWORD scratch, DWORD copy, DWORD caller) {
        code.clear();
        put8(0x55);                                     // push ebp
        put8(0x8B); put8(0xEC);                         // mov ebp, esp
        put8(0x53);                                     // push ebx
        put8(0x8B); put8(0x5D); put8(0x0C);             // mov ebx, [ebp+0xC]
        put8(0xFF); put8(0x05); put32(scratch + HB);    // inc [hb]
        put8(0x60);                                     // pushad
        put8(0x8B); put8(0x75); put8(0x08);             // mov esi, [ebp+8]   &eyePosition
        put8(0x8B); put8(0x7D); put8(0x0C);             // mov edi, [ebp+0xC] &eyeAngles
        put8(0x8B); put8(0x06); put8(0xA3); put32(copy);  // mov eax,[esi]; mov [copy],eax
        put8(0x8B); put8(0x46); put8(0x04);               // mov eax, [esi+4]
        put8(0xA3); put32(copy + 4);
        put8(0x8B); put8(0x46); put8(0x08);             // mov eax, [esi+8]
        put8(0xA3); put32(copy + 8);
        put8(0xBE); put32(copy);                        // mov esi, copy

        llamar_sin_cos(cave, scratch, 0, SX, CX);
        llamar_sin_cos(cave, scratch, 4, SY, CY);
        llamar_sin_cos(cave, scratch, 8, SZ, CZ);

        fld_m32(scratch + CX); fmul_m32(scratch + CY); fstp_m32(scratch + FWD_X);
        fld_m32(scratch + CX); fmul_m32(scratch + SY); fstp_m32(scratch + FWD_Y);
        fld_m32(scratch + SX); fchs(); fstp_m32(scratch + FWD_Z);
        fld_m32(scratch + SZ); fchs(); fmul_m32(scratch + SX); fmul_m32(scratch + CY);
        fld_m32(scratch + CZ); fmul_m32(scratch + SY); faddp(); fstp_m32(scratch + RGT_X);
        fld_m32(scratch + SZ); fchs(); fmul_m32(scratch + SX); fmul_m32(scratch + SY);
        fld_m32(scratch + CZ); fmul_m32(scratch + CY); fsubp(); fstp_m32(scratch + RGT_Y);
        fld_m32(scratch + SZ); fchs(); fmul_m32(scratch + CX); fstp_m32(scratch + RGT_Z);
        fld_m32(scratch + CZ); fmul_m32(scratch + SX); fmul_m32(scratch + CY);
        fld_m32(scratch + SZ); fmul_m32(scratch + SY); faddp(); fstp_m32(scratch + UP_X);
        fld_m32(scratch + CZ); fmul_m32(scratch + SX); fmul_m32(scratch + SY);
        fld_m32(scratch + SZ); fmul_m32(scratch + CY); fsubp(); fstp_m32(scratch + UP_Y);
        fld_m32(scratch + CZ); fmul_m32(scratch + CX); fstp_m32(scratch + UP_Z);

        for (int i = 0; i < 3; i++) {
            DWORD fwd = scratch + (i == 0 ? FWD_X : i == 1 ? FWD_Y : FWD_Z);
            DWORD rgt = scratch + (i == 0 ? RGT_X : i == 1 ? RGT_Y : RGT_Z);
            DWORD up = scratch + (i == 0 ? UP_X : i == 1 ? UP_Y : UP_Z);
            fld_m32((DWORD)&vmh::g_offx); fmul_m32(fwd);
            fld_m32((DWORD)&vmh::g_offy); fmul_m32(rgt); faddp();
            fld_m32((DWORD)&vmh::g_offz); fmul_m32(up); faddp();
            fmul_m32((DWORD)&vmh::g_factor);
            fadd_esi(i * 4);
            fstp_esi(i * 4);
        }

        put8(0x61);                                     // popad
        put8(0xB8); put32(copy);                        // mov eax, copy
        put8(0x89); put8(0x45); put8(0x08);             // mov [ebp+8], eax
        jmp32(cave, caller + VUELTA_OFF);
        return code;
    }

    // Firma (prologo) de la función objetivo: si el juego se actualiza y cambia,
    // el memcmp falla y Instalar() devuelve false sin parchear nada.
    const BYTE PROLOGO[7] = { 0x55, 0x8B, 0xEC, 0x53, 0x8B, 0x5D, 0x0C };
}

namespace vmh {

    bool Instalar() {
        if (g_instalado) return true;
        // Una cave publicada puede estar en uso; no reutilizarla ni liberarla.
        if (g_cave) return false;
        if (CALLER_RVA == 0) return false;  // safety: sin offset, no parchear nada
        HMODULE client = GetModuleHandleA("client.dll");
        if (!client) return false;
        DWORD caller = (DWORD)client + CALLER_RVA;
        BYTE cur[7];
        if (!LeerBytes(caller, cur, sizeof(cur))) return false;
        if (memcmp(cur, PROLOGO, 7) != 0) return false;
        DWORD cave = (DWORD)VirtualAlloc(NULL, 0x400, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (!cave) return false;
        DWORD scratch = (DWORD)((construir(cave, 0, 0, caller).size() + 15) & ~15);
        std::vector<unsigned char> final = construir(cave, cave + scratch, cave + COPY_OFF, caller);
        if (scratch + HB + sizeof(DWORD) > COPY_OFF || COPY_OFF + 12 > 0x400) {
            VirtualFree((void*)cave, 0, MEM_RELEASE);
            return false;
        }
        memset((void*)cave, 0, 0x400);
        memcpy((void*)cave, final.data(), final.size());
        float d2r = 0.017453292519943295f;
        memcpy((void*)(cave + scratch + D2R), &d2r, 4);
        FlushInstructionCache(GetCurrentProcess(), (void*)cave, final.size());
        DWORD old;
        if (!VirtualProtect((void*)caller, TAMANO_HOOK, PAGE_EXECUTE_READWRITE, &old)) {
            VirtualFree((void*)cave, 0, MEM_RELEASE);
            return false;
        }
        // Revalidar tras preparar la cave, sin adoptar parches de otros overlays.
        if (memcmp((void*)caller, PROLOGO, sizeof(PROLOGO)) != 0) {
            DWORD ignored;
            VirtualProtect((void*)caller, TAMANO_HOOK, old, &ignored);
            VirtualFree((void*)cave, 0, MEM_RELEASE);
            return false;
        }
        memcpy(g_orig, cur, TAMANO_HOOK);
        g_patch[0] = 0xE9;
        DWORD relativo = cave - caller - TAMANO_HOOK;
        memcpy(g_patch + 1, &relativo, sizeof(relativo));
        memcpy((void*)caller, g_patch, TAMANO_HOOK);
        FlushInstructionCache(GetCurrentProcess(), (void*)caller, TAMANO_HOOK);
        DWORD ignored;
        VirtualProtect((void*)caller, TAMANO_HOOK, old, &ignored);
        g_caller = caller;
        g_cave = cave;
        g_instalado = true;
        return true;
    }

    void Desinstalar() {
        if (!g_instalado || !g_caller) return;
        BYTE actual[TAMANO_HOOK];
        if (!LeerBytes(g_caller, actual, sizeof(actual)) ||
            memcmp(actual, g_patch, sizeof(actual)) != 0) return;
        DWORD old;
        if (!VirtualProtect((void*)g_caller, TAMANO_HOOK, PAGE_EXECUTE_READWRITE, &old)) return;
        memcpy((void*)g_caller, g_orig, TAMANO_HOOK);
        FlushInstructionCache(GetCurrentProcess(), (void*)g_caller, TAMANO_HOOK);
        VirtualProtect((void*)g_caller, TAMANO_HOOK, old, &old);
        g_caller = 0;
        g_instalado = false;
        // Conservar la cave hasta el fin del proceso para llamadas en vuelo.
    }
}
