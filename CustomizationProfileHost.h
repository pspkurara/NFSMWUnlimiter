#pragma once
#include "CustomizationProfileStorage.h"
#include <MinHook.h>
#include <atomic>
#include <mutex>
#include <cstdio>

// Shared source, not a mandatory shared DLL. The first ready ASI owns the three
// native transactions; later ASIs subscribe to it instead of patching them again.
namespace CustomizationProfileHost {
using BeginSave=void(__cdecl*)(void*,void*,uint32_t);
using EndSave=void(__cdecl*)(void*,void*,uint32_t);
using Done=void(__cdecl*)(void*,const char*);
using BeginLoad=void(__cdecl*)(void*);
using EndLoad=void(__cdecl*)(void*,const char*,void*,uint32_t,uint32_t,uint32_t,int32_t);
struct Observer {
    uint32_t size=sizeof(Observer),version=1,identity=0;
    void* context=nullptr;
    BeginSave beginSave=nullptr;EndSave endSave=nullptr;Done done=nullptr;
    BeginLoad beginLoad=nullptr;EndLoad endLoad=nullptr;
};
static_assert(sizeof(Observer)==36,"Profile observer ABI 1 requires Win32");
struct Api {
    uint32_t size=sizeof(Api),version=1;
    uint32_t(__cdecl* subscribe)(const Observer*)=nullptr;
};
static_assert(sizeof(Api)==12,"Profile host ABI 1");
struct Snapshot {std::array<Observer,8> entries{};size_t count=0;};
class Registry {
    std::mutex mutex;
    Snapshot listeners;
public:
    bool Add(const Observer& observer) {
        if(observer.size!=sizeof(Observer)||observer.version!=1||!observer.identity||
            !observer.beginSave||!observer.endSave||!observer.done||!observer.beginLoad||!observer.endLoad)return false;
        std::lock_guard<std::mutex> lock(mutex);
        for(size_t i=0;i<listeners.count;++i) {
            const auto& old=listeners.entries[i];
            if(old.identity==observer.identity)return old.context==observer.context&&old.beginSave==observer.beginSave&&
                old.endSave==observer.endSave&&old.done==observer.done&&old.beginLoad==observer.beginLoad&&old.endLoad==observer.endLoad;
        }
        if(listeners.count==listeners.entries.size())return false;
        listeners.entries[listeners.count++]=observer;return true;
    }
    Snapshot Read() {std::lock_guard<std::mutex> lock(mutex);return listeners;}
};
static Registry Listeners;
static std::atomic<const Api*> Service{nullptr};
static bool Pin(const void* address) {
    HMODULE module=nullptr;
    return GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCSTR>(address),&module)!=FALSE;
}
static uint32_t __cdecl Subscribe(const Observer* observer) {
    if(!observer||observer->size!=sizeof(Observer)||observer->version!=1)return 0;
    // All callback code and context must remain valid until process termination.
    if(!observer->beginSave||!observer->endSave||!observer->done||!observer->beginLoad||!observer->endLoad)return 0;
    for(const void* address:{reinterpret_cast<const void*>(observer->beginSave),reinterpret_cast<const void*>(observer->endSave),
        reinterpret_cast<const void*>(observer->done),reinterpret_cast<const void*>(observer->beginLoad),reinterpret_cast<const void*>(observer->endLoad)})
        if(!Pin(address))return 0;
    return Listeners.Add(*observer)?1u:0u;
}
static const Api LocalApi={sizeof(Api),1,&Subscribe};
static const Api* Published() {return Service.load(std::memory_order_acquire);}
static bool Register(const Observer& observer) {const auto api=Published();return api&&api->subscribe(&observer)!=0;}
static void* NativeSaveBuffer() {
    auto card=*reinterpret_cast<DWORD**>(CustomizationProfileStorage::GlobalAddress(0x91CB20));
    return card?reinterpret_cast<void*>(card[0x44/4]):nullptr;
}
using SaveFn=void(__thiscall*)(DWORD*,void*,int);
using DoneFn=void(__thiscall*)(DWORD*,const char*);
using LoadFn=bool(__thiscall*)(DWORD*,void*,int,bool,int);
static SaveFn OriginalSave=nullptr;
static DoneFn OriginalDone=nullptr;
static LoadFn OriginalLoad=nullptr;
template<class F> static bool Invoke(F call) noexcept {
    try {call();return true;}
    catch(...) {CustomizationProfileStorage::Log("Profile observer failed; native transaction continues.");return false;}
}
static void __fastcall Save(DWORD* profile,void*,void* buffer,int size) {
    const bool active=profile==CustomizationProfileStorage::CurrentProfile();
    const uint32_t native=buffer==NativeSaveBuffer();
    // A subscriber registered during the native call must not receive an unmatched End.
    const auto listeners=Listeners.Read();std::array<bool,8> began{};
    if(active)for(size_t i=0;i<listeners.count;++i) {
        const auto& o=listeners.entries[i];began[i]=Invoke([&]{o.beginSave(o.context,buffer,native);});
    }
    OriginalSave(profile,buffer,size);
    if(active)for(size_t i=0;i<listeners.count;++i)if(began[i]) {
        const auto& o=listeners.entries[i];Invoke([&]{o.endSave(o.context,buffer,native);});
    }
}
static void __fastcall Saved(DWORD* callback,void*,const char* filename) {
    const auto name=CustomizationProfileStorage::CompletedProfileName(filename);
    const auto listeners=Listeners.Read();
    OriginalDone(callback,filename);
    if(name.empty())return;
    for(size_t i=0;i<listeners.count;++i) {const auto& o=listeners.entries[i];Invoke([&]{o.done(o.context,name.c_str());});}
}
static bool __fastcall Load(DWORD* profile,void*,void* buffer,int size,bool commit,int player) {
    const uint32_t native=buffer==NativeSaveBuffer();
    const auto listeners=Listeners.Read();std::array<bool,8> began{};
    for(size_t i=0;i<listeners.count;++i) {const auto& o=listeners.entries[i];began[i]=Invoke([&]{o.beginLoad(o.context);});}
    const bool result=OriginalLoad(profile,buffer,size,commit,player);
    const auto name=CustomizationProfileStorage::Name(reinterpret_cast<const char*>(profile));
    const uint32_t activeSuccess=result&&profile==CustomizationProfileStorage::CurrentProfile();
    for(size_t i=0;i<listeners.count;++i)if(began[i]) {
        const auto& o=listeners.entries[i];Invoke([&]{o.endLoad(o.context,name.c_str(),buffer,native,activeSuccess,commit,player);});
    }
    return result;
}
static const Api* FindPeer() {
    for(const char* name:{"NFSMWUnlimiter.asi","NFSMWExtendedCamera.asi","NFSMWStanceHost.asi"}) {
        auto module=GetModuleHandleA(name);if(!module)continue;
        auto getter=reinterpret_cast<const Api*(__cdecl*)()>(GetProcAddress(module,"MWCL_GetProfileHostV1"));
        if(!getter)getter=reinterpret_cast<decltype(getter)>(GetProcAddress(module,"_MWCL_GetProfileHostV1"));
        if(!getter)continue;
        const auto api=getter();
        if(api&&api->size==sizeof(Api)&&api->version==1&&api->subscribe&&Pin(reinterpret_cast<const void*>(api->subscribe)))return api;
    }
    return nullptr;
}
static bool Install() {
    if(Published())return true;
    wchar_t name[96]{};swprintf_s(name,L"Local\\NFSMWProfileHost-%lu",GetCurrentProcessId());
    HANDLE gate=CreateMutexW(nullptr,FALSE,name);if(!gate)return false;
    const DWORD acquired=WaitForSingleObject(gate,0); // Never wait under a loader lock.
    if(acquired!=WAIT_OBJECT_0&&acquired!=WAIT_ABANDONED){CloseHandle(gate);return false;}
    struct Unlock {HANDLE gate;~Unlock(){ReleaseMutex(gate);CloseHandle(gate);}} unlock{gate};
    if(Published())return true;
    if(const auto peer=FindPeer()){Service.store(peer,std::memory_order_release);return true;}
    struct Hook {uintptr_t address;void* replacement;void** original;const BYTE* bytes;size_t count;};
    const BYTE save[]={0x8B,0x54,0x24,4,0x53,0x55};
    const BYTE done[]={0xE8,0xDB,0x5E,0x12,0,0x85,0xC0};
    const BYTE load[]={0x83,0xEC,0x24,0x8B,0x44,0x24,0x28};
    const std::array<Hook,3> hooks={{{0x58FD10,&Save,reinterpret_cast<void**>(&OriginalSave),save,sizeof(save)},
        {0x526AC0,&Saved,reinterpret_cast<void**>(&OriginalDone),done,sizeof(done)},
        {0x5ACB70,&Load,reinterpret_cast<void**>(&OriginalLoad),load,sizeof(load)}}};
    for(const auto& hook:hooks)if(std::memcmp(reinterpret_cast<void*>(hook.address),hook.bytes,hook.count)) {
        CustomizationProfileStorage::Log("Profile host disabled: unknown executable or unrecognized hook owner.");return false;
    }
    const auto status=MH_Initialize();if(status!=MH_OK&&status!=MH_ERROR_ALREADY_INITIALIZED)return false;
    size_t created=0;
    for(const auto& hook:hooks){if(MH_CreateHook(reinterpret_cast<void*>(hook.address),hook.replacement,hook.original)!=MH_OK)break;++created;}
    bool enabled=created==hooks.size();
    if(enabled)for(const auto& hook:hooks)if(MH_EnableHook(reinterpret_cast<void*>(hook.address))!=MH_OK){enabled=false;break;}
    if(enabled&&Pin(reinterpret_cast<const void*>(&Subscribe))) {
        Service.store(&LocalApi,std::memory_order_release);
        CustomizationProfileStorage::Log("Shared native profile host installed.");return true;
    }
    for(size_t i=0;i<created;++i){MH_DisableHook(reinterpret_cast<void*>(hooks[i].address));MH_RemoveHook(reinterpret_cast<void*>(hooks[i].address));}
    CustomizationProfileStorage::Log("Profile host installation rolled back.");return false;
}
}
extern "C" __declspec(dllexport) const CustomizationProfileHost::Api* __cdecl MWCL_GetProfileHostV1() {
    return CustomizationProfileHost::Published();
}
