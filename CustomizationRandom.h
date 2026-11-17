#pragma once
#include "CustomizationRandomState.h"

// Native lifecycle adapter. The independent companion owns configuration/sampling;
// this host owns only transient associations, never serialized owned-car values.
namespace CustomizationRandom {
using Values=CustomizationSave::Values;
using Table=CustomizationRandomState::Table<Values>;
using SelectFn=int(__cdecl*)(const char*,uint32_t,uint32_t,Values*);
static Table Rides,Records;
static std::mutex Mutex;
static uint64_t NextGeneration=0;
static bool HooksReady=false;
static SelectFn Select=nullptr;

static void Reset() noexcept {
    std::lock_guard<std::mutex> lock(Mutex);Rides.Clear();Records.Clear();
}
static void RideCopy(const DWORD* source,DWORD* destination) noexcept {
    try {
        std::lock_guard<std::mutex> lock(Mutex);
        Rides.Copy(source,source?CustomizationSave::Fingerprint(source):0,destination);
    } catch(...) { Reset(); }
}
static void RecordCopy(const void* source,void* destination) noexcept {
    try {
        std::lock_guard<std::mutex> lock(Mutex);
        Records.Copy(source,source?CustomizationSave::CustomizationFingerprint(source):0,destination);
    } catch(...) { Reset(); }
}
static void __cdecl Randomized(DWORD* ride) noexcept {
    if(!HooksReady||!ride)return;
    try {
        // No Save lock is held while asking the companion to sample.
        CustomizationSave::SynchronizeProfile();
        if(!Select) {
            auto module=GetModuleHandleA("NFSMWCustomizationLab.asi");
            if(module) {
                auto version=reinterpret_cast<unsigned(__cdecl*)()>(GetProcAddress(module,"MWCL_GetApiVersion"));
                if(version&&version()==1)Select=reinterpret_cast<SelectFn>(GetProcAddress(module,"MWCL_SelectRandomStanceV1"));
            }
        }
        Values values;
        uint64_t generation;
        {
            std::lock_guard<std::mutex> lock(Mutex);
            Rides.Erase(ride);generation=++NextGeneration;
        }
        auto body=RideInfo_GetPart(ride,23);
        // Do not consume the game's RNG: enabling stance must not change its cars/parts.
        const uint32_t seed=uint32_t(generation)*0x9E3779B9u^GetTickCount();
        if(!Select||!Select(GetCarTypeName(ride[0]),body?body[0]:0,seed,&values)||
            !values.mask||!CustomizationSave::ValidValues(values))return;
        std::lock_guard<std::mutex> lock(Mutex);
        Rides.Put(ride,{CustomizationSave::Fingerprint(ride),generation,values});
    } catch(...) { Reset(); }
}
static float Resolve(DWORD* ride,int axle,int channel,float fallback) noexcept {
    if(!HooksReady||!ride||axle<0||axle>1||channel<0||channel>5)return fallback;
    std::lock_guard<std::mutex> lock(Mutex);
    auto entry=Rides.Find(ride,CustomizationSave::Fingerprint(ride));
    const auto bit=unsigned(channel*2+axle);
    return entry&&(entry->values.mask&(1u<<bit))?entry->values.values[bit]:fallback;
}
static void __cdecl ForgetRide(void* ride) noexcept {
    std::lock_guard<std::mutex> lock(Mutex);Rides.Erase(ride);
}
static void __cdecl ForgetRecord(void* record) noexcept {
    std::lock_guard<std::mutex> lock(Mutex);Records.Erase(record);
}
using WriteFn=void(__thiscall*)(void*,DWORD*);
static WriteFn OriginalRead=nullptr;
static void __fastcall ReadRecord(void* record,void*,DWORD* ride) {
    OriginalRead(record,ride);
    if(!ride)return;
    try {
        const bool owned=CustomizationSave::IsOwnedCustomization(record);
        std::lock_guard<std::mutex> lock(Mutex);Rides.Erase(ride);
        if(owned)return;
        auto entry=Records.Find(record,CustomizationSave::CustomizationFingerprint(record));
        if(entry)Rides.Put(ride,{CustomizationSave::Fingerprint(ride),entry->generation,entry->values});
    } catch(...) { Reset(); }
}
static void RecordWritten(void* record,DWORD* ride) {
    if(!record)return;
    try {
        const bool owned=CustomizationSave::IsOwnedCustomization(record);
        std::lock_guard<std::mutex> lock(Mutex);Records.Erase(record);
        if(owned||!ride)return;
        auto entry=Rides.Find(ride,CustomizationSave::Fingerprint(ride));
        if(entry)Records.Put(record,{CustomizationSave::CustomizationFingerprint(record),entry->generation,entry->values});
    } catch(...) { Reset(); }
}
static void* OriginalRideInit=nullptr;
static void* OriginalRideStock=nullptr;
static void* OriginalRecordInit=nullptr;
static void* OriginalRecordDefault=nullptr;
static void* OriginalRecordPreset=nullptr;
// Preserve the complete native calling convention, including constructor EAX.
#define MWCL_FORGET_CAVE(name,observer,original) \
    static void __declspec(naked) name() { \
        __asm { pushfd } __asm { pushad } __asm { push ecx } \
        __asm { call observer } __asm { add esp,4 } \
        __asm { popad } __asm { popfd } __asm { jmp dword ptr [original] } \
    }
MWCL_FORGET_CAVE(RideInit,ForgetRide,OriginalRideInit)
MWCL_FORGET_CAVE(RideStock,ForgetRide,OriginalRideStock)
MWCL_FORGET_CAVE(RecordInit,ForgetRecord,OriginalRecordInit)
MWCL_FORGET_CAVE(RecordDefault,ForgetRecord,OriginalRecordDefault)
MWCL_FORGET_CAVE(RecordPreset,ForgetRecord,OriginalRecordPreset)
#undef MWCL_FORGET_CAVE

static bool Install(bool randomPartsEnabled) {
    if(HooksReady)return true;
    if(!randomPartsEnabled||!CustomizationSave::HooksReady)return false;
    struct Hook {uintptr_t address;void* replacement;void** original;std::vector<unsigned char> bytes;};
    std::vector<Hook> hooks={
        {0x56F2B0,&ReadRecord,reinterpret_cast<void**>(&OriginalRead),{0x53,0x56,0x57,0x8B,0x7C,0x24,0x10}},
        {0x739A70,&RideInit,&OriginalRideInit,{0x8B,0x44,0x24,4,0x8B,0xD1}},
        {0x7594A0,&RideStock,&OriginalRideStock,{0x83,0xEC,0x10,0x53,0x55,0x56}},
        {0x581C20,&RecordInit,&OriginalRecordInit,{0x8B,0xD1,0x33,0xC0,0x53,0x55}},
        {0x56F1B0,&RecordDefault,&OriginalRecordDefault,{0x8B,0xD1,0x56,0x57,0x83,0xC8,0xFF}},
        {0x56F340,&RecordPreset,&OriginalRecordPreset,{0x53,0x55,0x56,0x8B,0xF1,0x57}}
    };
    for(const auto& hook:hooks)if(std::memcmp(reinterpret_cast<void*>(hook.address),hook.bytes.data(),hook.bytes.size())) {
        CustomizationSave::Log("Random stance disabled: unknown executable or competing lifecycle hook.");return false;
    }
    size_t created=0;
    for(auto& hook:hooks) {
        if(MH_CreateHook(reinterpret_cast<void*>(hook.address),hook.replacement,hook.original)!=MH_OK)break;
        ++created;
    }
    bool enabled=created==hooks.size();
    if(enabled)for(const auto& hook:hooks)if(MH_EnableHook(reinterpret_cast<void*>(hook.address))!=MH_OK) {enabled=false;break;}
    if(enabled) {
        CustomizationSave::TransientReset=&Reset;
        CustomizationSave::TransientRideCopy=&RideCopy;
        CustomizationSave::TransientRecordCopy=&RecordCopy;
        CustomizationSave::TransientRecordWrite=&RecordWritten;
        HooksReady=true;RideInfoRandomObserver=&Randomized;
        CustomizationSave::Log("Random stance lifecycle installed; sampling follows random parts only.");return true;
    }
    for(size_t i=0;i<created;++i) {MH_DisableHook(reinterpret_cast<void*>(hooks[i].address));MH_RemoveHook(reinterpret_cast<void*>(hooks[i].address));}
    CustomizationSave::Log("Random stance disabled: lifecycle hook transaction failed.");return false;
}
}
