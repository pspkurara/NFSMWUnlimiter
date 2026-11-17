#pragma once
#include <windows.h>
#include <cstdint>

namespace CustomizationHookChain {
// Accept an existing branch only when it leads to executable code in the known
// dependency. No RVA inside that dependency is assumed or overwritten.
inline uintptr_t OwnedTarget(uintptr_t site,BYTE opcode,HMODULE dependency) {
    if(!dependency||*reinterpret_cast<const BYTE*>(site)!=opcode)return 0;
    const auto target=site+5+*reinterpret_cast<const int32_t*>(site+1);
    HMODULE owner=nullptr;MEMORY_BASIC_INFORMATION memory{};
    if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<const char*>(target),&owner)||owner!=dependency||
       !VirtualQuery(reinterpret_cast<const void*>(target),&memory,sizeof(memory))||
       memory.State!=MEM_COMMIT||(memory.Protect&(PAGE_GUARD|PAGE_NOACCESS))||
       !(memory.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE)))return 0;
    return target;
}
}
