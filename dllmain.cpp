#include <windows.h>
#include <cstdio>
#include <cstring>
#include "viewmodel_hook.h"
#include "dx9_hook.h"

namespace {
    FILE* AbrirLog(){
        char ruta[MAX_PATH];
        DWORD n=GetTempPathA(MAX_PATH,ruta);
        if(!n||n>=MAX_PATH) return nullptr;
        strcat_s(ruta,"vm_standalone_log.txt");
        return fopen(ruta,"a");
    }
    void Log(const char* s){
        FILE* f=AbrirLog();
        if(f){ fprintf(f,"%s\n",s); fclose(f); }
    }
    HWND BuscarVentana(){
        HWND found=nullptr;
        EnumWindows([](HWND h, LPARAM lp)->BOOL{
            HWND* out=(HWND*)lp; DWORD pid=0; GetWindowThreadProcessId(h,&pid);
            if(pid!=GetCurrentProcessId() || !IsWindowVisible(h)) return TRUE;
            char t[256]; GetWindowTextA(h,t,sizeof(t));
            if(strstr(t,"Left 4 Dead")){ *out=h; return FALSE; }
            if(!*out) *out=h;
            return TRUE;
        }, (LPARAM)&found);
        return found;
    }
    void Setup(){
        // Anti doble inyección: mutex global
        HANDLE mtx = CreateMutexA(nullptr, FALSE, "l4d2_viewmodel_standalone_mtx");
        if(GetLastError()==ERROR_ALREADY_EXISTS){
            Log("[setup] otra instancia del standalone ya corre");
            // Permite continuar pero avisa; el hook fallará si ya está parcheado
        }
        for(int i=0;i<300 && !GetModuleHandleA("client.dll"); ++i) Sleep(100);
        for(int i=0;i<300 && !GetModuleHandleA("d3d9.dll"); ++i) Sleep(100);
        if(!vmh::Instalar()){
            Log("[vmh] fallo hook viewmodel (¿ya inyectado overlay? reinicia juego)");
            // No abortamos dx9 hook para que al menos se vea mensaje en menú, pero el viewmodel no moverá
        } else {
            Log("[vmh] hook viewmodel instalado");
        }
        HWND hwnd=BuscarVentana();
        if(!dx9h::Instalar(hwnd)){
            Log("[dx9] fallo hook d3d9");
        } else {
            Log("[dx9] hook d3d9 instalado");
        }
    }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved){
    if(reason==DLL_PROCESS_ATTACH || reason==DLL_PROCESS_DETACH){
        FILE* f=AbrirLog();
        if(f){ fprintf(f,"DllMain reason=%u h=%p\n", (unsigned)reason, hModule); fclose(f); }
    }
    if(reason==DLL_PROCESS_ATTACH){
        DisableThreadLibraryCalls(hModule);
        HANDLE t=CreateThread(nullptr,0,[](LPVOID)->DWORD{ Setup(); return 0; }, nullptr,0,nullptr);
        if(t) CloseHandle(t);
    } else if(reason==DLL_PROCESS_DETACH && lpReserved==nullptr){
        vmh::Desinstalar();
        dx9h::Desinstalar();
    }
    return TRUE;
}
