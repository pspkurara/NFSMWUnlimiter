#include <windows.h>
#include <cstring>
#include <iostream>
#include "../CustomizationFrontendList.h"

static bool CheckDuplicateGroups() {
    // PC ForAllChildren/ForAllObjects (5B1510..5B15A5). Relative calls stay
    // inside this block, exercising the real callback vtable and nested walk.
    const unsigned char walk[]={
        0x8b,0x44,0x24,0x04,0x53,0x56,0x8b,0x70,0x64,0x85,0xf6,0x57,0x8b,0xd9,0x74,0x2a,
        0x8b,0x7c,0x24,0x14,0x8b,0x17,0x56,0x8b,0xcf,0xff,0x52,0x04,0x84,0xc0,0x74,0x22,
        0x83,0x7e,0x18,0x05,0x75,0x0d,0x57,0x56,0x8b,0xcb,0xe8,0xd1,0xff,0xff,0xff,0x84,
        0xc0,0x74,0x0f,0x8b,0x76,0x04,0x85,0xf6,0x75,0xda,0x5f,0x5e,0xb0,0x01,0x5b,0xc2,
        0x08,0x00,0x5f,0x5e,0x32,0xc0,0x5b,0xc2,0x08,0x00,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,
        0x53,0x56,0x8b,0xd9,0x8b,0x73,0x4c,0x85,0xf6,0x57,0x74,0x2a,0x8b,0x7c,0x24,0x10,
        0x8b,0x07,0x56,0x8b,0xcf,0xff,0x50,0x04,0x84,0xc0,0x74,0x22,0x83,0x7e,0x18,0x05,
        0x75,0x0d,0x57,0x56,0x8b,0xcb,0xe8,0x85,0xff,0xff,0xff,0x84,0xc0,0x74,0x0f,0x8b,
        0x76,0x04,0x85,0xf6,0x75,0xda,0x5f,0x5e,0xb0,0x01,0x5b,0xc2,0x04,0x00,0x5f,0x5e,
        0x32,0xc0,0x5b,0xc2,0x04,0x00};
    auto code=VirtualAlloc(nullptr,sizeof(walk),MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    if(!code)return false;
    std::memcpy(code,walk,sizeof(walk));DWORD previous=0;
    if(!VirtualProtect(code,sizeof(walk),PAGE_EXECUTE_READ,&previous)){VirtualFree(code,0,MEM_RELEASE);return false;}
    FlushInstructionCache(GetCurrentProcess(),code,sizeof(walk));
    uint32_t package[21]{},groups[4][27]{},label[26]{};
    package[0x4C/4]=reinterpret_cast<uint32_t>(groups[0]);
    for(auto& group:groups){group[0x18/4]=5;group[0x10/4]=0xDEADBEEF;}
    groups[0][1]=reinterpret_cast<uint32_t>(groups[1]);
    groups[1][1]=reinterpret_cast<uint32_t>(groups[2]);
    groups[1][0x10/4]=0;groups[1][0x64/4]=reinterpret_cast<uint32_t>(groups[3]);
    // Visible PC library prompt has a different group name; its localized Help
    // string establishes its purpose without depending on that generated name.
    groups[2][0x10/4]=0xD9E63061;groups[2][0x60/4]=1;
    groups[2][0x64/4]=reinterpret_cast<uint32_t>(label);
    label[0x18/4]=2;label[0x60/4]=0x61D8F34F;
    CustomizationFrontendList::GroupsByName<uint32_t> found(0xDEADBEEF,0x61D8F34F);
    using Walk=bool(__thiscall*)(uint32_t*,decltype(found)*);
    const bool complete=reinterpret_cast<Walk>(static_cast<unsigned char*>(code)+0x50)(package,&found);
    VirtualFree(code,0,MEM_RELEASE);
    return complete&&found.groups.size()==3&&found.groups[0]==groups[0]&&
        found.groups[1]==groups[3]&&found.groups[2]==groups[2];
}

int main() {
    static_assert(sizeof(void*)==4,"Exercise the actual PC x86 calling convention");
    uint32_t source[23]{},clone[23]{},draw[17]{},copy[17]{};
    for(unsigned i=0;i<17;++i)draw[i]=0x3F800000+i;
    source[11]=reinterpret_cast<uint32_t>(draw);source[12]=sizeof(draw);
    clone[11]=reinterpret_cast<uint32_t>(copy);clone[12]=sizeof(copy);
    if(!CustomizationFrontendList::CopyDrawData(clone,source)||std::memcmp(draw,copy,sizeof(draw))||
       !(clone[7]&0x400000))return 6;
    copy[7]=0;
    if(draw[7]==0||source[11]==clone[11])return 7;
    clone[12]=sizeof(copy)-4;
    if(CustomizationFrontendList::CopyDrawData(clone,source))return 8;
    uint32_t imageDraw[21]{},imageCopy[21]{};
    imageDraw[14]=imageDraw[15]=0x42000000; // 32px button dimensions.
    imageDraw[19]=imageDraw[20]=0x3F800000; // FEImage's lower-right UV.
    source[11]=reinterpret_cast<uint32_t>(imageDraw);source[12]=sizeof(imageDraw);
    clone[11]=reinterpret_cast<uint32_t>(imageCopy);clone[12]=sizeof(imageCopy);
    if(!CustomizationFrontendList::CopyDrawData(clone,source,0x54)||
       std::memcmp(imageDraw,imageCopy,sizeof(imageDraw)))return 9;
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
    if(!CheckDuplicateGroups())return 5;
    std::cout<<"Native PC frontend insertion: empty list, append, links and sentinels passed\n";
    std::cout<<"Native PC traversal: duplicate names, nested groups and callback ABI passed\n";
}
