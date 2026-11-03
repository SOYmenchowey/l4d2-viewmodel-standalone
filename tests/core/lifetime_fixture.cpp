#include <windows.h>
#include <cstring>
static volatile LONG vmInstalls=0, dxInstalls=0, setupDone=0;
namespace vmh { bool Instalar() { InterlockedIncrement(&vmInstalls); return true; } }
namespace dx9h { bool Instalar(HWND) { InterlockedIncrement(&dxInstalls); return true; } }
static HMODULE TestModule(LPCSTR name) {
    if(name && (!strcmp(name,"client.dll") || !strcmp(name,"d3d9.dll"))) return (HMODULE)1;
    return GetModuleHandleA(name);
}
static HANDLE TestThread(LPSECURITY_ATTRIBUTES a, SIZE_T s, LPTHREAD_START_ROUTINE f,
                         LPVOID p, DWORD flags, LPDWORD id) {
    // The real Setup still runs on a real worker, outside DllMain's loader lock.
    return CreateThread(a,s,[](LPVOID fn)->DWORD {
        ((LPTHREAD_START_ROUTINE)fn)(nullptr);
        InterlockedExchange(&setupDone,1);
        return 0;
    },(LPVOID)f,flags,id);
}
#define GetModuleHandleA TestModule
#define CreateThread TestThread
#include "../../src/core/dllmain.cpp"
#undef GetModuleHandleA
#undef CreateThread
extern "C" __declspec(dllexport) LONG Ready() { return InterlockedCompareExchange(&setupDone,0,0); }
extern "C" __declspec(dllexport) LONG Installs() { return vmInstalls+dxInstalls; }
