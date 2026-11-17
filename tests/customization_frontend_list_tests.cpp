#include <windows.h>
#include <cstring>
#include <iostream>
#include "../CustomizationFrontendList.h"

int main() {
    static_assert(sizeof(void*)==4,"Exercise the actual PC x86 calling convention");
    // Relocation-free instructions of the inspected PC function 5B2160..5B21AB.
    // Run the production adapter against native insertion, including its null
    // predecessor branch and unlinked-node ABADCAFE sentinels from the crash.
    const unsigned char native[]={
        0x8B,0x44,0x24,0x04,0x85,0xC0,0x74,0x41,0x56,0x8B,0x74,0x24,0x0C,0x85,0xF6,0x74,
        0x15,0x8B,0x56,0x04,0x85,0xD2,0x89,0x50,0x04,0x74,0x03,0x89,0x42,0x08,0x89,0x70,
        0x08,0x89,0x46,0x04,0xEB,0x17,0x8B,0x51,0x08,0x85,0xD2,0x89,0x50,0x04,0x74,0x03,
        0x89,0x42,0x08,0xC7,0x40,0x08,0,0,0,0,0x89,0x41,0x08,0x39,0x71,0x0C,0x5E,0x75,
        0x03,0x89,0x41,0x0C,0xFF,0x41,0x04,0xC2,0x08,0};
    auto code=VirtualAlloc(nullptr,sizeof(native),MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    if(!code)return 1;
    std::memcpy(code,native,sizeof(native));DWORD previous=0;
    if(!VirtualProtect(code,sizeof(native),PAGE_EXECUTE_READ,&previous))return 2;
    FlushInstructionCache(GetCurrentProcess(),code,sizeof(native));
    using Add=void(__thiscall*)(uint32_t*,uint32_t*,uint32_t*);
    const auto add=reinterpret_cast<Add>(code);
    uint32_t group[27]{};
    uint32_t nodes[3][3]={{0,0xABADCAFE,0xABADCAFE},{0,0xABADCAFE,0xABADCAFE},{0,0xABADCAFE,0xABADCAFE}};
    for(unsigned i=0;i<3;++i) {
        CustomizationFrontendList::AppendChild(group,nodes[i],add);
        if(group[24]!=i+1||group[25]!=reinterpret_cast<uint32_t>(nodes[0])||
           group[26]!=reinterpret_cast<uint32_t>(nodes[i])||nodes[i][1]!=0||
           nodes[i][2]!=(i?reinterpret_cast<uint32_t>(nodes[i-1]):0))return 3;
        if(i&&nodes[i-1][1]!=reinterpret_cast<uint32_t>(nodes[i]))return 4;
    }
    VirtualFree(code,0,MEM_RELEASE);
    std::cout<<"Native PC frontend insertion: empty list, append, links and sentinels passed\n";
}
