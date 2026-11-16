#pragma once
#include <windows.h>
#include <bcrypt.h>
#include <MinHook.h>
#include "CustomizationProfileStorage.h"
#include "CustomizationProfileHost.h"
#include <array>
#include <vector>
#include <map>
#include <mutex>
#include <string>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <tuple>
#pragma comment(lib, "bcrypt.lib")

// Profile-adjacent extension data. The native save is opened for reading only.
// Host owns pending transactions; the independent ASI owns just its editor UI.
namespace CustomizationSave {
using CustomizationProfileStorage::GlobalAddress;
using CustomizationProfileStorage::Log;
using CustomizationProfileStorage::ValidName;
using CustomizationProfileStorage::Name;
using CustomizationProfileStorage::CompletedProfileName;
using CustomizationProfileStorage::FolderCallTarget;
using CustomizationProfileStorage::ProfileDirectory;
using CustomizationProfileStorage::Crc;
using CustomizationProfileStorage::Digest;
using CustomizationProfileStorage::CurrentProfile;
struct Values { uint32_t size = sizeof(Values), mask = 0; float values[12]{}; };
static_assert(sizeof(Values) == 56, "Owned editor ABI 1");
struct Key {
    uint32_t handle{}, frontend{}, vehicle{};
    uint8_t customization{}, career{};
    uint16_t reserved{};
    bool operator<(const Key& other) const {
        return std::tie(handle,frontend,vehicle,customization,career) <
               std::tie(other.handle,other.frontend,other.vehicle,other.customization,other.career);
    }
    bool operator==(const Key& other) const { return !(*this<other)&&!(other<*this); }
};
static_assert(sizeof(Key) == 16, "Sidecar key schema 1");
struct Record { Key key; Values value; };
struct Header {
    char magic[8] = {'M','W','C','L','S','V','1',0};
    uint32_t version = 1, recordSize = sizeof(Record), count = 0, crc = 0;
    uint8_t nativeDigest[32]{};
};
static_assert(sizeof(Header)==56 && sizeof(Record)==72, "Sidecar schema 1");
struct Binding { Key key; uint64_t fingerprint{}; };
using State = std::map<Key, Values>;
static std::mutex Mutex;
static State Pending, Preview, Saving;
static std::map<const DWORD*,Binding> Rides;
static std::map<const void*,Binding> CustomizationCopies;
static std::map<Key,uint64_t> Tokens;
static uint64_t NextToken = 1;
static uint64_t ActiveEditorToken = 0;
static std::string Profile, SavingProfile;
static bool HooksReady = false, SaveCaptured = false;
static void(__cdecl* RegisterEditor)(int(__cdecl*)(uint64_t,const Values*,int)) = nullptr;
static void(__cdecl* SetOwnedContext)(uint64_t,const Values*) = nullptr;

static bool ValidValues(const Values& value) {
    if(value.size!=sizeof(Values) || (value.mask&~0xFFFu)) return false;
    for(unsigned i=0;i<12;++i) if((value.mask>>i)&1) {
        float v=value.values[i]; if(!std::isfinite(v)) return false;
        switch(i/2) {
        case 0: if(std::fabs(v)>2) return false; break;
        case 1: if(v<0||v>5) return false; break;
        case 2: if(std::fabs(v)>45) return false; break;
        case 3: case 4: if(v<.005f||v>5) return false; break;
        case 5: if(std::fabs(v)>2) return false; break;
        }
    }
    return true;
}
static DWORD* Stable() {
    auto profile=CurrentProfile();
    return profile ? reinterpret_cast<DWORD*>(reinterpret_cast<BYTE*>(profile)+0x414) : nullptr;
}
static bool ReadKey(const DWORD* record,Key& key) {
    if(!record || record[0]==0xFFFFFFFFu || !(record[3]&6)) return false;
    auto bytes=reinterpret_cast<const BYTE*>(record);
    if(bytes[16]==0xFF) return false;
    key={record[0],record[1],record[2],bytes[16],bytes[17],0};
    return true;
}
static bool CurrentKey(const Key& key) {
    auto stable=Stable(); if(!stable) return false;
    for(int i=0;i<200;++i) {Key present; if(ReadKey(stable+i*5,present)&&present==key) return true;}
    return false;
}
static void LoadLocked(const std::string& name) {
    Profile=name; Pending.clear();Preview.clear();Rides.clear();CustomizationCopies.clear();Tokens.clear();ActiveEditorToken=0;
    Saving.clear();SaveCaptured=false;SavingProfile.clear();
    try {
        auto directory=ProfileDirectory(name); if(directory.empty()) return;
        std::ifstream stream(directory/L"NFSMWCustomizationLab.bin",std::ios::binary);
        if(!stream) return;
        Header h; stream.read(reinterpret_cast<char*>(&h),sizeof(h));
        if(!stream || std::memcmp(h.magic,"MWCLSV1",8) || h.version!=1 ||
           h.recordSize!=sizeof(Record) || h.count>200) {Log("Sidecar schema invalid; ignored.");return;}
        std::vector<Record> records(h.count);
        stream.read(reinterpret_cast<char*>(records.data()),std::streamsize(records.size()*sizeof(Record)));
        if(!stream || stream.peek()!=EOF || Crc(records.data(),records.size()*sizeof(Record))!=h.crc) {
            Log("Sidecar length/CRC invalid; ignored.");return;
        }
        std::array<uint8_t,32> digest{};
        if(!Digest(directory/name,digest)||std::memcmp(digest.data(),h.nativeDigest,32)) {
            Log("Sidecar belongs to a different native save revision; ignored.");return;
        }
        for(const auto& record:records) {
            if(!ValidValues(record.value) || !CurrentKey(record.key) || Pending.count(record.key)) {
                Pending.clear();Log("Sidecar owned-car identity/value invalid; ignored.");return;
            }
            Pending.emplace(record.key,record.value);
        }
        Log("Owned-car sidecar loaded after native profile success.");
    } catch(...) {Pending.clear();Log("Sidecar read failed; native profile unchanged.");}
}
static void EnsureProfileLocked() {
    auto native=CurrentProfile();
    if(!native || !reinterpret_cast<BYTE*>(native)[32]) {
        if(!Profile.empty()) LoadLocked("");
        return;
    }
    std::string name=Name(reinterpret_cast<const char*>(native));
    if(ValidName(name)&&name!=Profile) LoadLocked(name);
}
static uint64_t Fingerprint(const DWORD* ride) {
    uint64_t hash=14695981039346656037ull;
    auto add=[&](uint32_t v){hash^=v;hash*=1099511628211ull;};
    add(ride[0]);
    // Native PC RideInfo parts table: +0x48, 139 entries (verified in WriteRecordIntoRide).
    auto parts=reinterpret_cast<DWORD* const*>(reinterpret_cast<const BYTE*>(ride)+0x48);
    for(int slot=0;slot<139;++slot) add(parts[slot]?parts[slot][0]:0);
    return hash;
}
static bool IdentityLocked(DWORD* ride,Key& key,bool editor) {
    if(!ride)return false;
    auto found=Rides.find(ride);
    if(found!=Rides.end()&&found->second.fingerprint==Fingerprint(ride)&&CurrentKey(found->second.key)) {
        key=found->second.key;return true;
    }
    // Only the active editor may establish a selected frontend record. Rendering
    // other cached cars must not borrow that global selection, even for one type.
    if(editor&&*reinterpret_cast<int*>(GlobalAddress(0x925E90))==3) {
        auto record=*reinterpret_cast<DWORD**>(GlobalAddress(0x9B9EC4));
        auto stable=Stable();
        uintptr_t at=reinterpret_cast<uintptr_t>(record),first=reinterpret_cast<uintptr_t>(stable);
        if(stable&&at>=first&&at<first+200*20&&(at-first)%20==0&&ReadKey(record,key)&&
           reinterpret_cast<int(__thiscall*)(const DWORD*)>(0x5816B0)(record)==int(ride[0])) {
            Rides[ride]={key,Fingerprint(ride)};return true;
        }
    }
    return false;
}
static int __cdecl Edit(uint64_t token,const Values* values,int action) {
    std::lock_guard<std::mutex> lock(Mutex);
    EnsureProfileLocked();
    auto owner=std::find_if(Tokens.begin(),Tokens.end(),[&](const auto& entry){return entry.second==token;});
    if(!HooksReady || owner==Tokens.end() || !CurrentKey(owner->first)) return 0;
    if(action==2) {Preview.erase(owner->first);return 1;}
    if(token!=ActiveEditorToken) return 0;
    if((action!=0 && action!=1)||!values||!ValidValues(*values)) return 0;
    if(action==0) Preview[owner->first]=*values;
    else {
        if(values->mask) Pending[owner->first]=*values; else Pending.erase(owner->first);
        Preview.erase(owner->first);
    }
    return 1;
}
static void EditorContext(DWORD* ride) {
    if(!HooksReady) return;
    if(!SetOwnedContext) {
        HMODULE core=GetModuleHandleA("NFSMWCustomizationLab.asi"); if(!core) return;
        RegisterEditor=reinterpret_cast<decltype(RegisterEditor)>(GetProcAddress(core,"MWCL_RegisterOwnedEditorV1"));
        SetOwnedContext=reinterpret_cast<decltype(SetOwnedContext)>(GetProcAddress(core,"MWCL_SetOwnedContextV1"));
        if(!RegisterEditor||!SetOwnedContext) {RegisterEditor=nullptr;SetOwnedContext=nullptr;return;}
        RegisterEditor(&Edit);
    }
    uint64_t token=0; Values values;
    {
        std::lock_guard<std::mutex> lock(Mutex);
        EnsureProfileLocked(); Key key;
        if(IdentityLocked(ride,key,true)) {
            auto inserted=Tokens.emplace(key,0);
            if(inserted.first->second==0) inserted.first->second=NextToken++;
            token=inserted.first->second;
            auto found=Pending.find(key);if(found!=Pending.end()) values=found->second;
        }
        if(token!=ActiveEditorToken) Preview.clear();
        ActiveEditorToken=token;
    }
    // The core may call Edit from its UI thread. Never call it while holding Mutex.
    SetOwnedContext(token,token?&values:nullptr);
}
static float Resolve(DWORD* ride,int axle,int channel,float fallback) {
    if(!HooksReady||axle<0||axle>1||channel<0||channel>5) return fallback;
    std::lock_guard<std::mutex> lock(Mutex);
    EnsureProfileLocked(); Key key;
    if(!IdentityLocked(ride,key,false)) return fallback;
    const Values* value=nullptr;
    auto preview=Preview.find(key);if(preview!=Preview.end()) value=&preview->second;
    else {auto pending=Pending.find(key);if(pending!=Pending.end()) value=&pending->second;}
    unsigned bit=unsigned(channel*2+axle);
    return value && (value->mask&(1u<<bit)) ? value->values[bit] : fallback;
}
static void Commit(const std::string& name) {
    State snapshot;
    {
        std::lock_guard<std::mutex> lock(Mutex);
        if(!SaveCaptured||name!=SavingProfile||name!=Profile) return;
        snapshot=Saving;SaveCaptured=false;
    }
    try {
        auto directory=ProfileDirectory(name); if(directory.empty()) return;
        Header header; std::array<uint8_t,32> hash{};
        if(!Digest(directory/name,hash)) {Log("Native save cannot be hashed; sidecar not committed.");return;}
        std::memcpy(header.nativeDigest,hash.data(),hash.size());
        std::vector<Record> records;
        for(const auto& entry:snapshot) if(CurrentKey(entry.first)) records.push_back({entry.first,entry.second});
        header.count=uint32_t(records.size());header.crc=Crc(records.data(),records.size()*sizeof(Record));
        auto target=directory/L"NFSMWCustomizationLab.bin",temp=directory/L"NFSMWCustomizationLab.bin.tmp";
        HANDLE file=CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(file==INVALID_HANDLE_VALUE) {Log("Cannot create sidecar temporary file.");return;}
        DWORD written=0;
        bool ok=WriteFile(file,&header,sizeof(header),&written,nullptr)&&written==sizeof(header);
        if(ok&&!records.empty()) ok=WriteFile(file,records.data(),DWORD(records.size()*sizeof(Record)),&written,nullptr)&&written==records.size()*sizeof(Record);
        if(ok) ok=FlushFileBuffers(file)!=FALSE;
        CloseHandle(file);
        if(ok) {
            auto backup=directory/L"NFSMWCustomizationLab.bin.bak";
            if(std::filesystem::exists(target)) CopyFileW(target.c_str(),backup.c_str(),FALSE);
            ok=MoveFileExW(temp.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=FALSE;
        }
        if(!ok) {DeleteFileW(temp.c_str());Log("Atomic sidecar commit failed; previous file retained.");}
        else Log("Owned-car sidecar committed after successful native SaveDone.");
    } catch(...) {Log("Sidecar commit failed; native save untouched.");}
}

using BuildFn=void(__thiscall*)(DWORD*,uint32_t,int,DWORD*);
using NewFn=DWORD*(__thiscall*)(DWORD*);
static BuildFn OriginalBuild=nullptr;static NewFn OriginalNew=nullptr;
struct Backup {std::string profile;State pending;};
static std::map<void*,Backup> Backups;
static thread_local DWORD* SaveSource=nullptr;
static void __cdecl BeginOwnedSave(void*,void* buffer,uint32_t native) {
    SaveSource=CurrentProfile();
    std::lock_guard<std::mutex> lock(Mutex);
    if(native)SaveCaptured=false;else Backups.erase(buffer);
}
static void __cdecl EndOwnedSave(void*,void* buffer,uint32_t native) {
    if(!SaveSource||SaveSource!=CurrentProfile())return;
    std::lock_guard<std::mutex> lock(Mutex);EnsureProfileLocked();
    if(native){Saving=Pending;SavingProfile=Profile;SaveCaptured=true;Log("Native disk-save snapshot captured.");}
    else {if(Backups.size()>8)Backups.clear();Backups[buffer]={Profile,Pending};}
}
static void __cdecl OwnedSaveDone(void*,const char* name) {Commit(name);}
static void __cdecl BeginOwnedLoad(void*) {}
static void __cdecl EndOwnedLoad(void*,const char* profile,void* buffer,uint32_t native,uint32_t success,uint32_t commit,int32_t player) {
    if(!success||!commit||player!=0)return;
    std::lock_guard<std::mutex> lock(Mutex);
    const std::string name=Name(profile);
    auto backup=Backups.find(buffer);
    if(!native&&backup!=Backups.end()&&backup->second.profile==name) {
        Profile=name;Pending=backup->second.pending;Preview.clear();Tokens.clear();Rides.clear();CustomizationCopies.clear();
    } else {LoadLocked(name);Backups.clear();}
}
static const CustomizationProfileHost::Observer OwnedObserver={sizeof(CustomizationProfileHost::Observer),1,0x4F574E44,nullptr,
    &BeginOwnedSave,&EndOwnedSave,&OwnedSaveDone,&BeginOwnedLoad,&EndOwnedLoad};
static void __fastcall Build(DWORD* stable,void*,uint32_t handle,int player,DWORD* ride) {
    OriginalBuild(stable,handle,player,ride);
    if(!ride||player!=0||stable!=Stable()) return;
    Key key;
    for(int i=0;i<200;++i) if(stable[i*5]==handle && ReadKey(stable+i*5,key)) {
        std::lock_guard<std::mutex> lock(Mutex);EnsureProfileLocked();
        if(Rides.size()>512) Rides.clear();Rides[ride]={key,Fingerprint(ride)};return;
    }
}
// Racing creates a fresh RideInfo from the original frontend customization
// record, bypassing BuildRideForPlayer. Bind only the local-player usage and
// exact active-profile record address; matching equipment is never ownership.
static bool TrackRideCopy(DWORD* source,DWORD* destination) {
    if(!destination)return false;
    std::lock_guard<std::mutex> lock(Mutex);EnsureProfileLocked();
    Binding binding{};bool known=false;
    auto found=Rides.find(source);
    if(source&&found!=Rides.end()&&CurrentKey(found->second.key)&&found->second.fingerprint==Fingerprint(source)) {
        binding=found->second;known=true;
    }
    Rides.erase(destination);
    if(known) {if(Rides.size()>512)Rides.clear();Rides[destination]=binding;}
    return known;
}
using ReInitFn=void(__thiscall*)(DWORD*,DWORD*);
using LoadedRideFn=DWORD*(__thiscall*)(DWORD*,DWORD*,int,int,int);
static ReInitFn OriginalReInit=nullptr;
static LoadedRideFn OriginalLoadedRide=nullptr;
static void __fastcall ReInit(DWORD* car,void*,DWORD* ride) {
    // Both first construction and later menu refresh pass through ReInit.
    TrackRideCopy(ride,car+8/4);
    OriginalReInit(car,ride);
}
static DWORD* __fastcall LoadedRide(DWORD* loaded,void*,DWORD* ride,int frontend,int twoPlayers,int player) {
    TrackRideCopy(ride,loaded+0x1C/4);
    return OriginalLoadedRide(loaded,ride,frontend,twoPlayers,player);
}
static uint64_t CustomizationFingerprint(const void* record) {
    // Parts and record handle stay stable when native race tuning changes.
    return (uint64_t(Crc(record,139*2))<<8)|reinterpret_cast<const BYTE*>(record)[0x194];
}
static bool CustomizationOwnerLocked(const void* customization,Key& owner) {
    auto stable=Stable();if(!stable||!customization)return false;
    const uintptr_t first=reinterpret_cast<uintptr_t>(stable)+0xFA0;
    const uintptr_t address=reinterpret_cast<uintptr_t>(customization);
    if(address>=first&&address<first+76*0x198&&(address-first)%0x198==0) {
        const unsigned index=unsigned((address-first)/0x198);
        if(reinterpret_cast<const BYTE*>(customization)[0x194]!=index)return false;
        bool found=false;
        for(int i=0;i<200;++i) {
            Key candidate;
            if(ReadKey(stable+i*5,candidate)&&candidate.customization==index) {
                if(found)return false;
                owner=candidate;found=true;
            }
        }
        return found;
    }
    auto copy=CustomizationCopies.find(customization);
    if(copy==CustomizationCopies.end()||!CurrentKey(copy->second.key)||
       copy->second.fingerprint!=CustomizationFingerprint(customization))return false;
    owner=copy->second.key;return true;
}
static void __cdecl TrackCustomizationCopy(const void* source,void* destination) noexcept {
    try {
        std::lock_guard<std::mutex> lock(Mutex);EnsureProfileLocked();
        CustomizationCopies.erase(destination);
        Key key;
        if(destination&&CustomizationOwnerLocked(source,key)) {
            if(CustomizationCopies.size()>512)CustomizationCopies.clear();
            CustomizationCopies[destination]={key,CustomizationFingerprint(source)};
        }
    } catch(...) {Log("Customization copy lineage unavailable; native copy retained.");}
}
static void* OriginalCustomizationCopy=nullptr;
static void __declspec(naked) CustomizationCopyCave() {
    __asm {
        // PVehicle's verified 0x198-byte deep copy: ESI source, EAX allocation.
        pushfd
        pushad
        push eax
        push esi
        call TrackCustomizationCopy
        add esp,8
        popad
        popfd
        jmp dword ptr [OriginalCustomizationCopy]
    }
}
static bool BindRaceCustomization(DWORD* ride,const void* customization,int usage) {
    if(!ride)return false;
    std::lock_guard<std::mutex> lock(Mutex);EnsureProfileLocked();
    Rides.erase(ride); // Reused allocations must not retain an old owner.
    Key owner{};
    if(usage!=0||!CustomizationOwnerLocked(customization,owner))return false;
    if(Rides.size()>512)Rides.clear();
    Rides[ride]={owner,Fingerprint(ride)};return true;
}
using RaceLoadFn=bool(__thiscall*)(DWORD*,unsigned,int,bool,const void*);
static RaceLoadFn OriginalRaceLoad=nullptr;
static bool __fastcall LoadRace(DWORD* connection,void*,unsigned world,int usage,bool commit,const void* customization) {
    const bool result=OriginalRaceLoad(connection,world,usage,commit,customization);
    if(result)BindRaceCustomization(reinterpret_cast<DWORD*>(connection[0x40/4]),customization,usage);
    return result;
}
static DWORD* __fastcall NewCar(DWORD* stable,void*) {
    DWORD* record=OriginalNew(stable);
    if(record&&stable==Stable()) {
        std::lock_guard<std::mutex> lock(Mutex);
        auto remove=[&](auto& table){for(auto it=table.begin();it!=table.end();) if(it->first.handle==record[0]) it=table.erase(it);else ++it;};
        remove(Pending);remove(Preview);remove(Tokens);
        for(auto it=Rides.begin();it!=Rides.end();) if(it->second.key.handle==record[0]) it=Rides.erase(it);else ++it;
        for(auto it=CustomizationCopies.begin();it!=CustomizationCopies.end();)
            if(it->second.key.handle==record[0])it=CustomizationCopies.erase(it);else ++it;
    }
    return record;
}
static bool Install() {
    if(HooksReady) return true;
    if(!CustomizationProfileHost::Install())return false;
    struct Hook {uintptr_t address;void* replacement;void** original;std::vector<unsigned char> bytes;};
    std::vector<Hook> hooks={
        {0x5910E0,&Build,reinterpret_cast<void**>(&OriginalBuild),{0x8B,0x54,0x24,4,0x53,0x57}},
        {0x590380,&NewCar,reinterpret_cast<void**>(&OriginalNew),{0x56,0x33,0xF6,0x8B,0xC1,0x83,0x38,0xFF}},
        {0x75D220,&LoadRace,reinterpret_cast<void**>(&OriginalRaceLoad),{0x56,0x8B,0xF1,0x8B,0x46,0x44}},
        {0x689361,&CustomizationCopyCave,&OriginalCustomizationCopy,{0xB9,0x66,0,0,0,0x8B,0xF8,0xF3,0xA5}},
        {0x7609B0,&ReInit,reinterpret_cast<void**>(&OriginalReInit),{0x64,0xA1,0,0,0,0}},
        {0x752D20,&LoadedRide,reinterpret_cast<void**>(&OriginalLoadedRide),{0x8B,0x44,0x24,0x0C,0x53,0x55}}
    };
    for(const auto& hook:hooks) if(std::memcmp(reinterpret_cast<void*>(hook.address),hook.bytes.data(),hook.bytes.size())) {
        Log("Save extension disabled: unknown executable or competing hook.");return false;
    }
    MH_STATUS status=MH_Initialize();if(status!=MH_OK&&status!=MH_ERROR_ALREADY_INITIALIZED) return false;
    size_t created=0;
    for(auto& hook:hooks) {
        if(MH_CreateHook(reinterpret_cast<void*>(hook.address),hook.replacement,hook.original)!=MH_OK) break;
        ++created;
    }
    if(created==hooks.size()) {
        bool enabled=true;
        for(auto& hook:hooks) if(MH_EnableHook(reinterpret_cast<void*>(hook.address))!=MH_OK) {enabled=false;break;}
        if(enabled&&CustomizationProfileHost::Register(OwnedObserver)) {HooksReady=true;Log("Owned-car hooks subscribed to native profile host.");return true;}
    }
    for(size_t i=0;i<created;++i) {MH_DisableHook(reinterpret_cast<void*>(hooks[i].address));MH_RemoveHook(reinterpret_cast<void*>(hooks[i].address));}
    Log("Save extension disabled: hook transaction failed.");return false;
}
}
