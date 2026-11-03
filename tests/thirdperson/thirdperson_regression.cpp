#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <limits>

static HMODULE testClient=nullptr;
static ULONGLONG testTime=1000;
static int protectCalls=0, failCall=0;
static HMODULE TestModule(LPCSTR) { return testClient; }
static ULONGLONG TestTime() { return testTime; }
static BOOL TestProtect(LPVOID p,SIZE_T n,DWORD mode,PDWORD old) {
    if(++protectCalls==failCall) return FALSE;
    return VirtualProtect(p,n,mode,old);
}
#define GetModuleHandleA TestModule
#define GetTickCount64 TestTime
#define VirtualProtect TestProtect
#include "../../src/thirdperson/thirdperson.cpp"
#undef GetModuleHandleA
#undef GetTickCount64
#undef VirtualProtect

static void Check(bool ok,const char* name) {
    if(!ok){ fprintf(stderr,"FAIL: %s\n",name); exit(1); }
}
int main() {
    HMODULE memory=(HMODULE)VirtualAlloc(nullptr,0x800000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    Check(memory!=nullptr,"private client memory");
    testClient=memory;
    BYTE* cvar=(BYTE*)memory+RVA_Z_VIEW_DISTANCE;
    const char** name=(const char**)(cvar+OFS_NOMBRE);
    float* distance=(float*)(cvar+OFS_FLOAT);
    int* integer=(int*)(cvar+OFS_INT);
    *name="z_view_distance"; *distance=17; *integer=17;
    Check(tp::Aplicar() && *distance==17 && protectCalls==0,"disabled default does not touch game camera");
    tp::g_activado=true;
    testClient=nullptr;
    Check(!tp::Aplicar() && tp::HayError() && *distance==17,"missing client fails without writing");
    testClient=memory; *name="another_cvar"; testTime+=1001;
    Check(!tp::Aplicar() && *distance==17 && protectCalls==0,"wrong build or name never writes unrelated memory");
    *name=(const char*)1; testTime+=1001;
    Check(!tp::Aplicar() && *distance==17,"unreadable name fails safely");
    *name="z_view_distance"; testTime+=1001; failCall=1;
    Check(!tp::Aplicar() && *distance==17 && *integer==17,"protection failure leaves both values untouched");
    failCall=0; testTime+=1001;
    Check(tp::Aplicar() && !tp::HayError() && *distance==-100 && *integer==-100,"third person defaults to minus 100");
    int calls=protectCalls;
    Check(tp::Aplicar() && protectCalls==calls,"unchanged settings avoid repeated memory writes");
    *distance=0; *integer=0;
    Check(tp::Aplicar() && *distance==-100 && *integer==-100,"camera settings recover after game resets the cvar");
    tp::g_distancia=325.5f;
    Check(tp::Aplicar() && *distance==-325.5f && *integer==-325,"distance updates float and integer together");
    tp::g_distancia=900;
    Check(tp::Aplicar() && tp::g_distancia==600 && *distance==-600,"distance is bounded above");
    tp::g_distancia=-1;
    Check(tp::Aplicar() && tp::g_distancia==30 && *distance==-30,"distance is bounded below");
    tp::g_distancia=std::numeric_limits<float>::quiet_NaN();
    Check(tp::Aplicar() && tp::g_distancia==100 && *distance==-100,"NaN falls back to default");
    tp::g_distancia=std::numeric_limits<float>::infinity();
    Check(tp::Aplicar() && *distance==-100,"infinite distance is sanitized");
    tp::g_activado=false;
    Check(tp::Aplicar() && *distance==0 && *integer==0,"disable returns to first person");
    *distance=9; *integer=9;
    Check(tp::Aplicar() && *distance==9,"disabled camera stops overriding external settings");
    tp::g_activado=true; protectCalls=0; failCall=2;
    Check(!tp::Aplicar() && *distance==-100,"restore-protection failure remains visible");
    tp::g_activado=false; failCall=0; testTime+=1001;
    Check(tp::Aplicar() && *distance==0,"OFF reverts a write even if restoring protection failed");
    VirtualFree(memory,0,MEM_RELEASE);
    puts("PASS: third person on/off, distance, validation, failures, no unintended camera writes");
}
