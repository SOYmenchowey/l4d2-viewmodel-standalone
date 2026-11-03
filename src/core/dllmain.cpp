#include <windows.h>
#include <cstdio>
#include <cstring>
#include "viewmodel/viewmodel_hook.h"
#include "overlay/dx9_hook.h"

namespace {
    HANDLE g_instanceMutex = nullptr;
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
        // Una instancia por proceso; conservar el handle mientras la DLL resida.
        char nombre[96];
        sprintf_s(nombre, "Local\\l4d2_viewmodel_standalone_%lu", GetCurrentProcessId());
        HANDLE mtx = CreateMutexA(nullptr, FALSE, nombre);
        DWORD error = GetLastError();
        if(!mtx){ Log("[setup] no se pudo crear mutex"); return; }
        if(error==ERROR_ALREADY_EXISTS){
            Log("[setup] otra instancia del standalone ya corre");
            CloseHandle(mtx);
            return;
        }
        g_instanceMutex = mtx;
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
    if(reason==DLL_PROCESS_ATTACH){
        // Los callbacks y la cave pueden seguir en ejecucion tras FreeLibrary.
        // Fijar la DLL antes de iniciar Setup evita tambien descargar su hilo.
        HMODULE residente = nullptr;
        if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                              (LPCSTR)hModule, &residente)) return FALSE;
        DisableThreadLibraryCalls(hModule);
        HANDLE t=CreateThread(nullptr,0,[](LPVOID)->DWORD{ Setup(); return 0; }, nullptr,0,nullptr);
        if(t) CloseHandle(t);
    }
    // Sin esperas, destruccion de ImGui ni parches dentro del loader lock.
    // Windows recupera los recursos al terminar el proceso.
    return TRUE;
}
