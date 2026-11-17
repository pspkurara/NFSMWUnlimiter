#pragma once
#include <windows.h>
#include <cstdio>
#include <initializer_list>

// Source-only coordination for optional ASIs. No extra runtime DLL is required.
// Installation never waits under the loader lock, and published callbacks are
// pinned for the process lifetime just like the native profile host.
namespace CustomizationRuntimePeer {
class Gate {
    HANDLE handle=nullptr;
    bool acquired=false;
public:
    explicit Gate(const wchar_t* service) {
        wchar_t name[128]{};swprintf_s(name,L"Local\\NFSMW-%ls-%lu",service,GetCurrentProcessId());
        handle=CreateMutexW(nullptr,FALSE,name);
        if(handle) {auto result=WaitForSingleObject(handle,0);acquired=result==WAIT_OBJECT_0||result==WAIT_ABANDONED;}
    }
    ~Gate() {if(acquired)ReleaseMutex(handle);if(handle)CloseHandle(handle);}
    explicit operator bool() const {return acquired;}
    Gate(const Gate&)=delete;
    Gate& operator=(const Gate&)=delete;
};
static bool Pin(const void* code) {
    HMODULE module=nullptr;
    return code&&GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCSTR>(code),&module)!=FALSE;
}
template<class Api,class Validate> static const Api* Find(const char* symbol,Validate valid) {
    for(const char* name:{"NFSMWUnlimiter.asi","NFSMWExtendedCamera.asi","NFSMWStanceHost.asi"}) {
        auto module=GetModuleHandleA(name);if(!module)continue;
        auto getter=reinterpret_cast<const Api*(__cdecl*)()>(GetProcAddress(module,symbol));
        if(!getter) {
            char decorated[96]{};sprintf_s(decorated,"_%s",symbol);
            getter=reinterpret_cast<decltype(getter)>(GetProcAddress(module,decorated));
        }
        if(!getter)continue;
        const auto api=getter();if(api&&valid(*api))return api;
    }
    return nullptr;
}
}
