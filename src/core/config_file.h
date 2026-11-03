#pragma once
#include <windows.h>
#include <cstdio>
#include <string>
#include <io.h>
#include <fcntl.h>

namespace configfile {
inline std::wstring BesideModule(const void* address) {
    HMODULE module=nullptr;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(address),&module)) return {};
    wchar_t buffer[32768];
    const DWORD count=GetModuleFileNameW(module,buffer,32768);
    if(!count || count>=32768) return {};
    std::wstring path(buffer,count);
    const size_t slash=path.find_last_of(L"\\/");
    if(slash==std::wstring::npos) return {};
    path.resize(slash+1); path+=L"vmconfig.ini";
    if(path.compare(0,4,L"\\\\?\\")!=0) {
        if(path.compare(0,2,L"\\\\")==0) path=L"\\\\?\\UNC\\"+path.substr(2);
        else path=L"\\\\?\\"+path;
    }
    return path;
}

// Write to a new file in the same directory. Never truncate the user's preset.
class AtomicFile {
    FILE* stream_=nullptr;
    std::wstring target_,temporary_;
public:
    AtomicFile()=default;
    AtomicFile(const AtomicFile&)=delete;
    AtomicFile& operator=(const AtomicFile&)=delete;
    ~AtomicFile() {
        if(stream_) fclose(stream_);
        if(!temporary_.empty()) DeleteFileW(temporary_.c_str());
    }
    FILE* Open(const std::wstring& path) {
        if(path.empty() || stream_ || !temporary_.empty()) return nullptr;
        target_=path;
        static volatile LONG serial=0;
        for(int attempt=0;attempt<16;++attempt) {
            wchar_t suffix[80];
            swprintf_s(suffix,L".tmp-%lu-%lu-%lu",GetCurrentProcessId(),GetTickCount(),
                static_cast<DWORD>(InterlockedIncrement(&serial)));
            const std::wstring candidate=path+suffix;
            HANDLE file=CreateFileW(candidate.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
            if(file==INVALID_HANDLE_VALUE) {
                if(GetLastError()==ERROR_FILE_EXISTS || GetLastError()==ERROR_ALREADY_EXISTS) continue;
                return nullptr;
            }
            temporary_=candidate;
            const int fd=_open_osfhandle(reinterpret_cast<intptr_t>(file),_O_WRONLY|_O_TEXT);
            if(fd<0) { CloseHandle(file); return nullptr; }
            stream_=_fdopen(fd,"w");
            if(!stream_) _close(fd);
            return stream_;
        }
        return nullptr;
    }
    bool Commit(bool complete) {
        if(!stream_) return false;
        if(ferror(stream_)) complete=false;
        if(complete && fflush(stream_)!=0) complete=false;
        if(complete && _commit(_fileno(stream_))!=0) complete=false;
        if(fclose(stream_)!=0) complete=false;
        stream_=nullptr;
        if(!complete || !MoveFileExW(temporary_.c_str(),target_.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
            return false;
        temporary_.clear(); return true;
    }
};
}
