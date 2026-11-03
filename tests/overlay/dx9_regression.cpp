#include <windows.h>
#include <cstdio>
#include <cmath>
static bool denyProtection=false;
static DWORD* deniedSlot=nullptr;
static BOOL WINAPI TestProtect(LPVOID address,SIZE_T size,DWORD protection,PDWORD old) {
    if(denyProtection || address==deniedSlot) { SetLastError(ERROR_ACCESS_DENIED);return FALSE; }
    return VirtualProtect(address,size,protection,old);
}
#define VirtualProtect TestProtect
#include "../../src/overlay/dx9_hook.cpp"
#undef VirtualProtect
namespace menu {
    static int frames=0;
    void Inicializar(HWND) {} bool estaVisible(){return true;} bool estaEnMenuJuego(){return false;}
    bool tieneFoco(){return true;} bool CancelarCaptura(){return false;}
    void PerderFoco(){} void Cerrar(){} void Dibujar(){++frames;}
}
namespace tp { bool Aplicar(){return false;} }
static int failed=0;
static void Check(bool ok,const char* name) { if(!ok){++failed;fprintf(stderr,"FAIL: %s\n",name);} }
static bool PatchWithoutException(DWORD* slot) {
    __try { dx9h::ParchearSlot(slot,0x5678);return true; }
    __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
static HRESULT WINAPI ForwardPresent(IDirect3DDevice9*,const RECT*,const RECT*,HWND,const RGNDATA*) {return S_OK;}
static int InstallSmoke() {
    HWND window=CreateWindowW(L"STATIC",L"DX9 install audit",WS_OVERLAPPEDWINDOW,0,0,900,760,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    auto* d3d=Direct3DCreate9(D3D_SDK_VERSION); if(!window || !d3d)return 2;
    D3DPRESENT_PARAMETERS pp={};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=window;
    IDirect3DDevice9* device=nullptr;
    if(FAILED(d3d->CreateDevice(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,&device)))return 2;
    Check(dx9h::Instalar(window),"real DX9 hook installation in isolated process");
    if(!g_setupListo)return 1;
    Check(SUCCEEDED(device->Present(nullptr,nullptr,nullptr,nullptr)) && menu::frames==1,"real Present forwards once and renders the menu");
    Check(SUCCEEDED(device->Reset(&pp)),"real device Reset succeeds with installed hooks");
    Check(SUCCEEDED(device->Present(nullptr,nullptr,nullptr,nullptr)) && menu::frames==2,"overlay recovers after device reset");
    g_setupListo=false;
    ImGui_ImplDX9_Shutdown();ImGui_ImplWin32_Shutdown();ImGui::DestroyContext();
    device->Release();d3d->Release();DestroyWindow(window);
    if(!failed)puts("PASS: actual DX9 installation, Present forwarding, Reset and recovery (isolated hidden window)");
    return failed ? 1:0;
}
int main(int argc,char** argv) {
    if(argc>1 && !strcmp(argv[1],"install"))return InstallSmoke();
    auto* slot=static_cast<DWORD*>(VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    if(!slot)return 2;
    *slot=0x1234;DWORD old=0;VirtualProtect(slot,4096,PAGE_READONLY,&old);
    denyProtection=true;
    Check(PatchWithoutException(slot),"failed VirtualProtect never writes to a read-only slot");
    Check(*slot==0x1234,"failed patch keeps the original callback");
    denyProtection=false;
    Check(PatchWithoutException(nullptr),"null callback slot fails without crashing");
    Check(PatchWithoutException(slot) && *slot==0x5678,"read-only callback can be patched safely");
    MEMORY_BASIC_INFORMATION info={};VirtualQuery(slot,&info,sizeof(info));
    Check(info.Protect==PAGE_READONLY,"callback page protection is restored");
    Check(!dx9h::ParchearSlot(slot,0x9999,0x1111) && *slot==0x5678,"callback changed by another owner is not overwritten");
    VirtualProtect(slot,4096,PAGE_NOACCESS,&old);
    Check(dx9h::EscanearRegion(slot,4096,0)==0,"region made unreadable after enumeration does not crash scanning");
    Check(dx9h::EscanearRegion(slot,16,0)==0,"short regions cannot contain a vtable");
    VirtualFree(slot,0,MEM_RELEASE);
    Check(dx9h::EscanearRegion(slot,4096,0)==0,"region freed after enumeration does not crash scanning");
    auto* table=static_cast<DWORD*>(VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    if(!table)return 2;
    g_origPresent=reinterpret_cast<Present_t>(0x1000);g_origReset=reinterpret_cast<Reset_t>(0x2000);
    table[16]=0x2000;table[17]=0x1000;table[42]=0x3000;
    deniedSlot=&table[17];
    Check(dx9h::EscanearRegion(table,43*sizeof(DWORD),0x3000)==0 && table[16]==0x2000 && table[17]==0x1000,
        "failed second callback rolls back the first");
    deniedSlot=nullptr;
    Check(dx9h::EscanearRegion(table,43*sizeof(DWORD),0x3000)==1 && table[16]==reinterpret_cast<DWORD>(&Reset) &&
        table[17]==reinterpret_cast<DWORD>(&Present),"complete vtable hooks both callbacks");
    Check(dx9h::EscanearRegion(table,43*sizeof(DWORD),0x3000)==0,"already hooked table is not hooked again");
    VirtualFree(table,0,MEM_RELEASE);

    HWND window=CreateWindowW(L"STATIC",L"DX9 audit",WS_OVERLAPPEDWINDOW,0,0,900,760,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    auto* d3d=Direct3DCreate9(D3D_SDK_VERSION); if(!window || !d3d)return 2;
    D3DPRESENT_PARAMETERS pp={};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=window;
    IDirect3DDevice9* device=nullptr;
    if(FAILED(d3d->CreateDevice(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,&device)))return 2;
    ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;ImGui_ImplWin32_Init(window);ImGui_ImplDX9_Init(device);
    g_hwnd=window;g_imguiInit=true;g_setupListo=true;g_origPresent=ForwardPresent;
    dx9h::g_mouseX=20;dx9h::g_mouseY=120;Present(device,nullptr,nullptr,nullptr,nullptr);
    dx9h::g_mouseX=-3;Present(device,nullptr,nullptr,nullptr,nullptr);
    Check(dx9h::g_mouseX==0 && dx9h::g_mouseY==120,"moving past the left border clamps instead of teleporting to center");
    ImGui_ImplDX9_Shutdown();ImGui_ImplWin32_Shutdown();ImGui::DestroyContext();
    device->Release();d3d->Release();DestroyWindow(window);
    if(!failed)puts("PASS: failed/invalid callback patches, page restoration and real DX9 cursor border");
    return failed ? 1:0;
}
