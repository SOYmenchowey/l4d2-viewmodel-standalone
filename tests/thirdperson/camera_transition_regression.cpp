#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
static HMODULE testClient=nullptr;
static HMODULE TestModule(LPCSTR) { return testClient; }
static ULONGLONG TestTime() { return 1000; } // All actions happen without advancing time.
#define GetModuleHandleA TestModule
#define GetTickCount64 TestTime
#include "../../src/thirdperson/thirdperson.cpp"
#undef GetModuleHandleA
#undef GetTickCount64
static void Check(bool ok,const char* name) {
    if(!ok){ fprintf(stderr,"FAIL: %s\n",name); exit(1); }
}
int main(int argc, char** argv) {
    if (argc>1) {
        // Read/relocate the user's binary in this test process; never run its entrypoint.
        HMODULE image=LoadLibraryExA(argv[1],nullptr,DONT_RESOLVE_DLL_REFERENCES);
        Check(image!=nullptr,"map actual client binary without initialization");
        Check(tp_camera::Compatible((DWORD)image),"actual client matches camera signatures after relocation");
        FreeLibrary(image);
        puts("PASS: installed client.dll camera signatures (read-only check)");
    }
    BYTE* client=(BYTE*)VirtualAlloc(nullptr,0x800000,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);
    BYTE* player=(BYTE*)VirtualAlloc(nullptr,0x3000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    Check(client && player,"private client/player allocations");
    testClient=(HMODULE)client;
    BYTE* cvar=client+RVA_Z_VIEW_DISTANCE;
    *(const char**)(cvar+OFS_NOMBRE)="z_view_distance";
    *(DWORD*)(client+tp_camera::RVA_DISTANCE_PARENT)=(DWORD)cvar;
    *(DWORD*)(client+tp_camera::RVA_LOCAL_PLAYER)=(DWORD)player;
    float* current=(float*)(player+tp_camera::OFF_CURRENT_DISTANCE);
    float* target=(float*)(cvar+OFS_FLOAT);
    // A real callable x86 fixture with the inspected, relocated function prefix.
    float gameTarget=-100;
    BYTE desired[]={0x55,0x8B,0xEC,0x51,0xA1,0,0,0,0,
                    0xF3,0x0F,0x10,0x40,0x2C,
                    0xD9,0x05,0,0,0,0,0x8B,0xE5,0x5D,0xC3};
    DWORD parent=(DWORD)client+tp_camera::RVA_DISTANCE_PARENT, result=(DWORD)&gameTarget;
    memcpy(desired+5,&parent,4); memcpy(desired+16,&result,4);
    memcpy(client+tp_camera::RVA_DESIRED_DISTANCE,desired,sizeof(desired));
    BYTE local[]={0x8B,0x04,0x85,0,0,0,0,0x5D,0xC3};
    DWORD localAddress=(DWORD)client+tp_camera::RVA_LOCAL_PLAYER;
    memcpy(local+3,&localAddress,4); memcpy(client+tp_camera::RVA_LOCAL_REFERENCE,local,sizeof(local));
    const BYTE interpolation[]={0xF3,0x0F,0x10,0x8E,0x30,0x26,0,0,
                               0xF3,0x0F,0x10,0x55,0x88,0x0F,0x2F,0xD1,0x76,0x2C};
    memcpy(client+tp_camera::RVA_INTERPOLATION,interpolation,sizeof(interpolation));
    FlushInstructionCache(GetCurrentProcess(),client,0x800000);
    *current=17;
    Check(tp::Aplicar() && *current==17,"disabled startup leaves actual camera alone");
    tp::g_activado=true;
    Check(tp::Aplicar() && *target==-100 && *current==-100,"ON snaps actual camera in the same frame");
    tp::g_distancia=600; gameTarget=-600;
    Check(tp::Aplicar() && *target==-600 && *current==-600,"slider 100 to 600 has no distance-dependent ramp");
    tp::g_distancia=30; gameTarget=-30;
    Check(tp::Aplicar() && *target==-30 && *current==-30,"slider also snaps inward");
    tp::g_activado=false; gameTarget=0;
    Check(tp::Aplicar() && *target==0 && *current==0,"OFF snaps in the same frame");
    *current=80;
    Check(tp::Aplicar() && *current==80,"disabled mode does not keep overriding game camera");
    tp::g_activado=true; tp::g_distancia=100; gameTarget=200;
    Check(tp::Aplicar() && *target==-100 && *current==80,"special game camera target retains priority");
    gameTarget=-100;
    Check(tp::Aplicar() && *current==-100,"normal camera recovers after special mode");
    *(DWORD*)(client+tp_camera::RVA_LOCAL_PLAYER)=0;
    tp::g_distancia=600; gameTarget=-600;
    Check(tp::Aplicar() && *current==-100,"loading without local player does not write stale entity");
    *(DWORD*)(client+tp_camera::RVA_LOCAL_PLAYER)=(DWORD)player;
    Check(tp::Aplicar() && *current==-600,"camera applies on player availability");
    client[tp_camera::RVA_INTERPOLATION]^=1;
    tp::g_distancia=100; gameTarget=-100;
    Check(tp::Aplicar() && *target==-100 && *current==-600,"unknown interpolation code safely keeps native transition");
    client[tp_camera::RVA_INTERPOLATION]^=1;
    client[tp_camera::RVA_DESIRED_DISTANCE]^=1;
    Check(!tp_camera::Sincronizar((DWORD)client,-100) && *current==-600,"wrong function signature cannot be called");
    client[tp_camera::RVA_DESIRED_DISTANCE]^=1;
    client[tp_camera::RVA_LOCAL_REFERENCE+3]^=1;
    Check(!tp_camera::Sincronizar((DWORD)client,-100) && *current==-600,"wrong local-player reference cannot be used");
    client[tp_camera::RVA_LOCAL_REFERENCE+3]^=1;
    *current=std::numeric_limits<float>::quiet_NaN();
    Check(!tp_camera::Sincronizar((DWORD)client,-100),"invalid camera state fails safely");
    *current=-600;
    Check(!tp_camera::Sincronizar((DWORD)client,std::numeric_limits<float>::infinity()) && *current==-600,
          "invalid requested distance never enters camera state");
    Check(tp::Aplicar() && *current==-100,"compatible state resumes instant transitions");
    // Error retries keep their throttle, but a fresh user request is immediate.
    tp::g_distancia=200; testClient=nullptr;
    Check(!tp::Aplicar(),"missing module begins backoff");
    testClient=(HMODULE)client;
    Check(!tp::Aplicar(),"unchanged failed request remains throttled");
    tp::g_activado=false; gameTarget=0;
    Check(tp::Aplicar() && *target==0 && *current==0,"OFF bypasses stale one-second retry backoff");
    VirtualFree(player,0,MEM_RELEASE); VirtualFree(client,0,MEM_RELEASE);
    puts("PASS: immediate camera ON/OFF and slider, native camera priority, signatures, fresh requests");
}
