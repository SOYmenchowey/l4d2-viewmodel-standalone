#include "core/config_file.h"
#include <cstdlib>
#include <vector>
static void Check(bool ok,const char* name) { if(!ok){fprintf(stderr,"FAIL: %s\n",name);exit(1);} }
static std::string Read(const std::wstring& path) {
    FILE* file=_wfopen(path.c_str(),L"rb");Check(file!=nullptr,"read test preset");
    std::string result;char buffer[256];size_t n;
    while((n=fread(buffer,1,sizeof(buffer),file))!=0)result.append(buffer,n);
    fclose(file);return result;
}
static bool Save(const std::wstring& path,const char* text,bool complete=true) {
    configfile::AtomicFile file;auto* stream=file.Open(path);
    return stream && file.Commit(fputs(text,stream)>=0 && complete);
}
int main() {
    wchar_t here[32768];Check(GetCurrentDirectoryW(32768,here)>0,"test directory");
    std::wstring dir=L"\\\\?\\"+std::wstring(here)+L"\\config-test-"+std::to_wstring(GetCurrentProcessId())+L"-\u6d4b\u8bd5";
    std::vector<std::wstring> dirs;
    Check(CreateDirectoryW(dir.c_str(),nullptr)!=FALSE,"unicode fixture directory");dirs.push_back(dir);
    for(int i=0;i<5;++i) {dir+=L"\\"+std::wstring(50,L'x');Check(CreateDirectoryW(dir.c_str(),nullptr)!=FALSE,"long directory");dirs.push_back(dir);}
    const std::wstring path=dir+L"\\vmconfig.ini";
    Check(path.size()>MAX_PATH && Save(path,"original"),"Unicode and long-path first save");
    Check(!Save(path,"incomplete",false) && Read(path)=="original","partial writer preserves old contents");
    HANDLE locked=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    Check(locked!=INVALID_HANDLE_VALUE,"lock existing preset against replacement");
    Check(!Save(path,"locked overwrite") && Read(path)=="original","locked target remains intact");CloseHandle(locked);
    Check(SetFileAttributesW(path.c_str(),FILE_ATTRIBUTE_READONLY)!=FALSE,"read-only fixture");
    Check(!Save(path,"read-only overwrite") && Read(path)=="original","read-only target remains intact");
    SetFileAttributesW(path.c_str(),FILE_ATTRIBUTE_NORMAL);
    Check(Save(path,"recovered") && Read(path)=="recovered","retry after target unlock");
    {
        configfile::AtomicFile a,b;
        auto* first=a.Open(path);auto* second=b.Open(path);Check(first && second,"simultaneous writers use distinct temporary files");
        fputs("first",first);fputs("second",second);
        Check(b.Commit(true) && Read(path)=="second","second writer commits complete file");
        Check(a.Commit(true) && Read(path)=="first","first writer commits complete file without mixed bytes");
    }
    Check(!Save(dir+L"\\missing\\vmconfig.ini","bad"),"missing parent fails cleanly");
    WIN32_FIND_DATAW info={};HANDLE find=FindFirstFileW((path+L".tmp-*").c_str(),&info);
    Check(find==INVALID_HANDLE_VALUE && GetLastError()==ERROR_FILE_NOT_FOUND,"failed saves leave no temporary files");
    Check(DeleteFileW(path.c_str())!=FALSE,"remove only owned fixture");
    for(auto i=dirs.rbegin();i!=dirs.rend();++i)Check(RemoveDirectoryW(i->c_str())!=FALSE,"remove empty owned directory");
    puts("PASS: atomic config save, partial writes, locked/read-only targets, retry, Unicode/long paths and overlapping writers");
}
