#include <windows.h>
#include <cstdio>
#include <cstdlib>
static void Check(bool ok,const char* name) {
    if(!ok){fprintf(stderr,"FAIL: %s\n",name);exit(1);}
}
using Query = LONG (__cdecl*)();
static void WaitReady(HMODULE dll) {
    auto ready=(Query)GetProcAddress(dll,"Ready");
    Check(ready!=nullptr,"fixture query");
    for(int i=0;i<200 && !ready();++i) Sleep(10);
    Check(ready()!=0,"setup finishes");
}
int main() {
    HMODULE first=LoadLibraryA("lifetime_fixture.dll");
    Check(first!=nullptr,"load real entrypoint");
    // Drop the injector's reference immediately, including the worker startup race.
    Check(FreeLibrary(first)!=0,"release initial reference");
    Check(GetModuleHandleA("lifetime_fixture.dll")==first,"module stays resident before worker finishes");
    WaitReady(first);
    auto installs=(Query)GetProcAddress(first,"Installs");
    Check(installs && installs()==2,"first instance installs once");
    Check(CopyFileA("lifetime_fixture.dll","lifetime_duplicate.dll",FALSE)!=0,"prepare separate duplicate module");
    HMODULE second=LoadLibraryA("lifetime_duplicate.dll");
    Check(second!=nullptr,"load duplicate file");
    WaitReady(second);
    auto duplicateInstalls=(Query)GetProcAddress(second,"Installs");
    Check(duplicateInstalls && duplicateInstalls()==0,"second file cannot install duplicate hooks");
    for(int i=0;i<3;++i) Check(FreeLibrary(first)!=0,"repeated external release");
    Check(GetModuleHandleA("lifetime_fixture.dll")==first && installs()==2,"callbacks remain valid after release attempts");
    FreeLibrary(second);
    puts("PASS: entrypoint residency, worker lifetime, duplicate injection guard");
}
