#include "../CustomizationHookChain.h"
#include <MinHook.h>
#include <cstring>
#include <iostream>

using Setup=int(__thiscall*)(unsigned*);
static Setup Original=nullptr;
static int __fastcall ExistingMod(unsigned* state,void*) {state[0]=state[0]*10+1;return 37;}
static int __fastcall Extension(unsigned* state,void*) {
    const int result=Original(state);state[0]=state[0]*10+2;return result;
}
int main() {
    // A native entry already redirected by a dependency, as with CustomizeSub.
    // Real MinHook relocation must preserve the existing JMP and ECX convention.
    auto code=static_cast<BYTE*>(VirtualAlloc(nullptr,16,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    if(!code)return 1;
    std::memset(code,0x90,16);code[0]=0xE9;
    *reinterpret_cast<int32_t*>(code+1)=int32_t(reinterpret_cast<uintptr_t>(&ExistingMod)-reinterpret_cast<uintptr_t>(code)-5);
    DWORD previous=0;if(!VirtualProtect(code,16,PAGE_EXECUTE_READ,&previous))return 2;
    FlushInstructionCache(GetCurrentProcess(),code,16);
    const auto site=reinterpret_cast<uintptr_t>(code);
    if(CustomizationHookChain::OwnedTarget(site,0xE9,GetModuleHandleW(nullptr))!=reinterpret_cast<uintptr_t>(&ExistingMod))return 3;
    if(CustomizationHookChain::OwnedTarget(site,0xE8,GetModuleHandleW(nullptr))||
       CustomizationHookChain::OwnedTarget(site,0xE9,GetModuleHandleW(L"kernel32.dll")))return 4;
    if(MH_Initialize()!=MH_OK||MH_CreateHook(code,&Extension,reinterpret_cast<void**>(&Original))!=MH_OK||
       MH_EnableHook(code)!=MH_OK)return 5;
    unsigned state=0;const int result=reinterpret_cast<Setup>(code)(&state);
    if(result!=37||state!=12)return 6;
    if(MH_DisableHook(code)!=MH_OK||MH_RemoveHook(code)!=MH_OK)return 7;
    state=0;if(reinterpret_cast<Setup>(code)(&state)!=37||state!=1)return 8;
    MH_Uninitialize();VirtualFree(code,0,MEM_RELEASE);
    std::cout<<"Dependency JMP: owner checks, ECX, callback order, return and restoration passed\n";
}
