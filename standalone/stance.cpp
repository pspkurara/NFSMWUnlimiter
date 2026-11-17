// Development host beside an unchanged Unlimiter. No Unlimiter implementation
// headers or exported internal C++ symbols are needed: chain native boundaries.
#include "../InGameFunctions.h"
#include "../includes/injector/injector.hpp"
#include "../CustomizationSave.h"
#include "../CustomizationRandom.h"
#include "../CustomizationControls.h"
#include "../CustomizationBridge.h"
#include "../CustomizationCamera.h"
#include "../CustomizationStanceMenu.h"
#include "../CustomizationHookChain.h"
#include <atomic>

namespace StandaloneStance {
static std::atomic<bool> Started{false};
static HMODULE Unlimiter=nullptr;
using SetupFn=int(__thiscall*)(DWORD*);
using RandomFn=void(__thiscall*)(DWORD*);
static SetupFn OriginalSetup=nullptr;
static RandomFn OriginalRandom[2]{};
using GarageFn=void(__thiscall*)(DWORD*);
static GarageFn OriginalGarage=nullptr;
static void __fastcall Garage(DWORD* garage,void*) {
    if(auto update=CustomizationCamera::GarageUpdateObserver.load())update();
    OriginalGarage(garage);
}
static bool InstallGarageTick() {
    constexpr uintptr_t address=0x7A2310;
    const BYTE bytes[]={0xA1,0x90,0xCF,0x91,0,0x83,0xEC,0x0C};
    if(std::memcmp(reinterpret_cast<const void*>(address),bytes,sizeof(bytes))||
       MH_CreateHook(reinterpret_cast<void*>(address),&Garage,reinterpret_cast<void**>(&OriginalGarage))!=MH_OK)return false;
    if(MH_EnableHook(reinterpret_cast<void*>(address))==MH_OK)return true;
    MH_RemoveHook(reinterpret_cast<void*>(address));OriginalGarage=nullptr;return false;
}

static uintptr_t OwnedTarget(uintptr_t site,BYTE opcode) {
    return CustomizationHookChain::OwnedTarget(site,opcode,Unlimiter);
}
static int __fastcall Setup(DWORD* screen,void*) {
    const int result=OriginalSetup(screen);
    if(screen[0x150/4]!=0x801)return result;
    auto list=reinterpret_cast<DWORD*>(reinterpret_cast<BYTE*>(screen)+0x30);
    const int before=bList_TraversebList(list,nullptr);
    CustomizationStanceMenu::AddPartsOption(screen);
    if(bList_TraversebList(list,nullptr)==before)return result;
    // The original already selected an option before the extension existed.
    // Restore the requested ordinal after appending, including return from edit.
    (*reinterpret_cast<void(__thiscall**)(DWORD*,unsigned)>(screen[0x2C/4]+0x40))
        (screen+0x2C/4,screen[0x154/4]&0xFFFF00FF);
    return CustomizeSub_RefreshHeader(screen);
}
template<unsigned Index> static void __fastcall Random(DWORD* ride,void*) {
    OriginalRandom[Index](ride);
    CustomizationRandom::Randomized(ride);
}
static bool LayoutSupported() {
    wchar_t path[MAX_PATH]{};
    if(!GetModuleFileNameW(Unlimiter,path,MAX_PATH))return false;
    DWORD unused=0;const DWORD size=GetFileVersionInfoSizeW(path,&unused);
    if(!size||size>1024*1024)return false;
    std::vector<BYTE> data(size);VS_FIXEDFILEINFO* version=nullptr;UINT length=0;
    if(!GetFileVersionInfoW(path,0,size,data.data())||
       !VerQueryValueW(data.data(),L"\\",reinterpret_cast<void**>(&version),&length)||
       length<sizeof(*version)||version->dwSignature!=0xFEEF04BD)return false;
    // Initial adapter is for 3.3.1. Native copy/structure signatures are checked
    // independently by each feature. Other layout families need their own adapter.
    return version->dwFileVersionMS==0x00030003&&HIWORD(version->dwFileVersionLS)==1;
}
static bool Chain() {
    const auto setup=OwnedTarget(0x7BFBD0,0xE9),name=OwnedTarget(0x7BB17B,0xE8);
    if(!setup||!name)return false;
    if(MH_CreateHook(reinterpret_cast<void*>(0x7BFBD0),&Setup,reinterpret_cast<void**>(&OriginalSetup))!=MH_OK)return false;
    // Stance owns a frame callback, not the optional camera feature. The native
    // Showcase screen remains available without installing garage/race overrides.
    if(!CustomizationStanceMenu::Install(name,false)||MH_EnableHook(reinterpret_cast<void*>(0x7BFBD0))!=MH_OK) {
        MH_RemoveHook(reinterpret_cast<void*>(0x7BFBD0));return false;
    }
    const uintptr_t sites[]={0x5E8FE0,0x7B4907};
    const auto first=OwnedTarget(sites[0],0xE8),second=OwnedTarget(sites[1],0xE8);
    if(first&&second&&CustomizationRandom::Install(true)) {
        OriginalRandom[0]=reinterpret_cast<RandomFn>(first);OriginalRandom[1]=reinterpret_cast<RandomFn>(second);
        injector::MakeCALL(sites[0],&Random<0>,true);
        injector::MakeCALL(sites[1],&Random<1>,true);
    }
    return true;
}
static DWORD WINAPI Initialize(void*) {
    // A worker also supports loaders without InitializeASI. GetModuleHandle waits
    // for a dependency's DllMain to finish; never wait while holding loader lock.
    for(unsigned i=0;i<100&&!Unlimiter;++i){Unlimiter=GetModuleHandleA("NFSMWUnlimiter.asi");if(!Unlimiter)Sleep(100);}
    if(!Unlimiter||!LayoutSupported()) {
        CustomizationSave::Log("Standalone stance host: supported unchanged Unlimiter 3.3.1 is required.");return 0;
    }
    if(GetProcAddress(Unlimiter,"MWCL_GetProfileHostV1")||GetProcAddress(Unlimiter,"_MWCL_GetProfileHostV1")) {
        CustomizationSave::Log("Standalone stance host: integrated fork detected; not installing duplicate features.");return 0;
    }
    if(!CustomizationBridge::Install()||!CustomizationSave::Install())return 0;
    HMODULE xinput=LoadLibraryExW(L"xinput1_4.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!xinput)xinput=LoadLibraryExW(L"xinput9_1_0.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(xinput)CustomizationCamera::GetPadState=reinterpret_cast<decltype(CustomizationCamera::GetPadState)>(GetProcAddress(xinput,"XInputGetState"));
    if(!InstallGarageTick()) {
        CustomizationSave::Log("Standalone stance menu disabled: native garage frame boundary unavailable.");return 0;
    }
    CustomizationSave::Log(Chain()?"Standalone stance host installed beside unchanged Unlimiter.":
        "Standalone stance menu chain unavailable; native shop retained.");
    return 0;
}
static void Start() {
    if(Started.exchange(true))return;
    if(!CustomizationRuntimePeer::Pin(reinterpret_cast<const void*>(&Initialize)))return;
    if(auto thread=CreateThread(nullptr,0,&Initialize,nullptr,0,nullptr))CloseHandle(thread);
}
}
extern "C" __declspec(dllexport) void InitializeASI(){StandaloneStance::Start();}
#pragma comment(linker,"/EXPORT:InitializeASI=_InitializeASI")
BOOL APIENTRY DllMain(HMODULE module,DWORD reason,LPVOID) {
    if(reason!=DLL_PROCESS_ATTACH)return TRUE;
    DisableThreadLibraryCalls(module);
    const auto base=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    if(base!=0x400000)return FALSE;
    const auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE)return FALSE;
    const auto nt=reinterpret_cast<const IMAGE_NT_HEADERS*>(base+dos->e_lfanew);
    if(nt->Signature!=IMAGE_NT_SIGNATURE||nt->OptionalHeader.AddressOfEntryPoint!=0x3C4040)return FALSE;
    StandaloneStance::Start();return TRUE;
}
