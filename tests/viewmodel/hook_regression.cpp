#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// Use the real emitter/installer against private executable memory, never a game.
static HMODULE testClient = nullptr;
static bool failProtection = false;
static HMODULE TestModule(LPCSTR) { return testClient; }
static BOOL TestProtect(LPVOID p, SIZE_T n, DWORD access, PDWORD old) {
    return failProtection ? FALSE : VirtualProtect(p,n,access,old);
}
#define GetModuleHandleA TestModule
#define VirtualProtect TestProtect
#include "../../src/viewmodel/viewmodel_hook.cpp"
#undef GetModuleHandleA
#undef VirtualProtect

static void Check(bool ok, const char* name) {
    if (!ok) { fprintf(stderr,"FAIL: %s\n",name); exit(1); }
}
int main() {
    testClient=(HMODULE)VirtualAlloc(nullptr,0x30000,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);
    Check(testClient!=nullptr,"private test allocation");
    BYTE* target=(BYTE*)testClient+CALLER_RVA;
    const BYTE foreign[7]={0xE9,0x11,0x22,0x33,0x44,0x5D,0x0C};
    memcpy(target,foreign,sizeof(foreign));
    Check(!vmh::Instalar(),"reject existing foreign hook");
    vmh::Desinstalar();
    Check(memcmp(target,foreign,sizeof(foreign))==0,"failed installation never destroys foreign hook");
    Check(!g_caller && !g_cave && !g_instalado,"failed install retains no ownership");

    memcpy(target,PROLOGO,sizeof(PROLOGO));
    // Synthetic original function: return its first argument, restoring prologue.
    const BYTE tail[]={0x8B,0x45,0x08,0x5B,0x5D,0xC3};
    memcpy(target+sizeof(PROLOGO),tail,sizeof(tail));
    failProtection=true;
    Check(!vmh::Instalar(),"protection failure is reported");
    Check(memcmp(target,PROLOGO,sizeof(PROLOGO))==0 && !g_caller && !g_cave,
          "protection failure leaves original bytes and no ownership");
    failProtection=false;
    Check(vmh::Instalar(),"install into private function");
    DWORD installedCave=g_cave;
    Check(vmh::Instalar() && g_cave==installedCave,"repeated install is idempotent");
    using TestFunction = const float* (__cdecl*)(const float*, const float*);
    float position[]={1,2,3}, angles[]={0,0,0};
    vmh::g_offx=1; vmh::g_offy=2; vmh::g_offz=3;
    const float* result=((TestFunction)target)(position,angles);
    Check(result[0]==2 && result[1]==0 && result[2]==6,"actual x86 cave applies all offsets");
    Check(position[0]==1 && position[1]==2 && position[2]==3,"cave preserves caller position");
    BYTE ownPatch[TAMANO_HOOK]; memcpy(ownPatch,target,sizeof(ownPatch));
    memcpy(target,foreign,TAMANO_HOOK);
    vmh::Desinstalar();
    Check(memcmp(target,foreign,TAMANO_HOOK)==0 && g_instalado,
          "later foreign hook is never overwritten");
    memcpy(target,ownPatch,sizeof(ownPatch));
    failProtection=true; vmh::Desinstalar(); failProtection=false;
    Check(memcmp(target,ownPatch,sizeof(ownPatch))==0 && g_instalado,
          "failed restore keeps ownership for retry");
    vmh::Desinstalar();
    Check(memcmp(target,PROLOGO,sizeof(PROLOGO))==0 && !g_instalado,
          "owned hook restores original prologue");
    MEMORY_BASIC_INFORMATION info={};
    Check(VirtualQuery((void*)installedCave,&info,sizeof(info)) && info.State==MEM_COMMIT,
          "published cave survives possible in-flight calls");
    vmh::Desinstalar();
    Check(memcmp(target,PROLOGO,sizeof(PROLOGO))==0,"repeated restore is harmless");
    VirtualFree(testClient,0,MEM_RELEASE);
    puts("PASS: hook ownership, failed protection, x86 execution, retained cave");
}
