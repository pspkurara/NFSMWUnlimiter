#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

// Read only the installed game's tuning template. No copied EA assets are shipped.
namespace CustomizationStanceAssets {
constexpr uint32_t OriginalHash=0xAFAD612B;
constexpr const char* Name="MWCL_Stance_Editor.fng";
constexpr size_t MaxPackage=4*1024*1024,MaxBundle=256*1024*1024;
inline uint32_t U32(const uint8_t* p) {uint32_t n;std::memcpy(&n,p,4);return n;}
inline void Put(uint8_t* p,uint32_t n) {std::memcpy(p,&n,4);}
inline bool Expand(const uint8_t* p,size_t size,std::vector<uint8_t>& out,size_t limit) {
    if(size<18||std::memcmp(p,"JDLZ",4)||p[4]!=2||p[5]!=16)return false;
    const size_t count=U32(p+8),end=U32(p+12);
    if(!count||count>limit||end<18||end>size)return false;
    std::vector<uint8_t> result;result.reserve(count);
    size_t at=18;unsigned control=p[16]|256u,types=p[17]|256u;
    while(result.size()<count) {
        if(at>=end)return false;
        if(control&1) {
            if(at+2>end)return false;
            const unsigned a=p[at++],b=p[at++];
            const size_t length=(types&1)?((a>>4)*256+b+3):(a&31)+3;
            const size_t distance=(types&1)?(a&15)+1:((a>>5)*256+b+17);
            if(distance>result.size()||length>count-result.size())return false;
            for(size_t i=0;i<length;++i)result.push_back(result[result.size()-distance]);
            types>>=1;
        } else result.push_back(p[at++]);
        control>>=1;
        if(control==1&&result.size()<count){if(at>=end)return false;control=p[at++]|256u;}
        if(types==1&&result.size()<count){if(at>=end)return false;types=p[at++]|256u;}
    }
    out=std::move(result);return true;
}
inline bool Clone(const uint8_t* data,size_t size,std::vector<uint8_t>& wrapper) {
    // PC FE package and PkHd chunk. Keep the original string allocation lengths,
    // object data, scripts and resource references unchanged; rename within padding.
    if(size<40||size>MaxPackage||U32(data)!=0xE76E4546||U32(data+4)!=size-8||
       U32(data+8)!=0x64486B50||U32(data+12)>size-16||U32(data+12)<24)return false;
    const auto nameLength=U32(data+32),fileLength=U32(data+36);
    if(nameLength<sizeof("MWCL_Stance_Editor.fng")||nameLength>1024||fileLength>4096||
       size_t(nameLength)+fileLength>U32(data+12)-24||
       nameLength!=sizeof("Pause_Performance_Tuning.fng")||
       std::memcmp(data+40,"Pause_Performance_Tuning.fng",nameLength))return false;
    std::vector<uint8_t> result(size+8);Put(result.data(),0x30203);Put(result.data()+4,uint32_t(size));
    std::memcpy(result.data()+8,data,size);std::memset(result.data()+48,0,nameLength);
    std::memcpy(result.data()+48,Name,std::strlen(Name)+1);wrapper=std::move(result);return true;
}
inline bool Scan(const uint8_t* data,size_t size,std::vector<uint8_t>& wrapper,unsigned depth=0) {
    if(depth>8)return false;
    for(size_t at=0;at+8<=size;) {
        const auto id=U32(data+at),length=U32(data+at+4);
        if(length>size-at-8)return false;
        const auto payload=data+at+8;
        if(id==0x30203&&Clone(payload,length,wrapper))return true;
        if(id==0x30210&&length>=22&&U32(payload)==OriginalHash) {
            std::vector<uint8_t> expanded;
            if(Expand(payload+4,length-4,expanded,MaxPackage)&&Clone(expanded.data(),expanded.size(),wrapper))return true;
        }
        if((id&0x80000000)&&Scan(payload,length,wrapper,depth+1))return true;
        at+=8+size_t(length);
    }
    return false;
}
inline bool Extract(const std::vector<uint8_t>& bundle,std::vector<uint8_t>& wrapper) {
    if(bundle.empty()||bundle.size()>MaxBundle)return false;
    if(bundle.size()>=4&&!std::memcmp(bundle.data(),"JDLZ",4)) {
        std::vector<uint8_t> expanded;
        return Expand(bundle.data(),bundle.size(),expanded,MaxBundle)&&Scan(expanded.data(),expanded.size(),wrapper);
    }
    return Scan(bundle.data(),bundle.size(),wrapper);
}
}
