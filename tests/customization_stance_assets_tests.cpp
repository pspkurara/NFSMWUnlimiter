#include "../CustomizationStanceAssets.h"
#include <fstream>
#include <iostream>
using namespace CustomizationStanceAssets;
int main(int argc,char** argv) {
    // Minimal valid PC package header with an untouched source-path allocation.
    std::vector<uint8_t> source(128);Put(source.data(),0xE76E4546);Put(source.data()+4,120);
    Put(source.data()+8,0x64486B50);Put(source.data()+12,112);Put(source.data()+32,29);Put(source.data()+36,58);
    std::memcpy(source.data()+40,"Pause_Performance_Tuning.fng",29);
    std::vector<uint8_t> result;
    if(!Clone(source.data(),source.size(),result)||result.size()!=136||U32(result.data())!=0x30203||
       std::strcmp(reinterpret_cast<const char*>(result.data()+48),Name)||U32(result.data()+40)!=29)return 1;
    auto bad=source;Put(bad.data()+32,0xFFFFFFFF);if(Clone(bad.data(),bad.size(),result))return 2;
    std::vector<uint8_t> bundle(8+source.size());Put(bundle.data(),0x30203);Put(bundle.data()+4,uint32_t(source.size()));
    std::memcpy(bundle.data()+8,source.data(),source.size());if(!Extract(bundle,result))return 3;
    Put(bundle.data()+4,0xFFFFFFFF);if(Extract(bundle,result))return 4;
    std::vector<uint8_t> compressed(19);std::memcpy(compressed.data(),"JDLZ",4);compressed[4]=2;compressed[5]=16;
    Put(compressed.data()+8,1);Put(compressed.data()+12,19);compressed[18]='A';
    if(!Expand(compressed.data(),compressed.size(),result,8)||result!=std::vector<uint8_t>{'A'})return 5;
    compressed[16]=1;if(Expand(compressed.data(),compressed.size(),result,8))return 6; // truncated match
    compressed.push_back(0);Put(compressed.data()+12,20);
    if(Expand(compressed.data(),compressed.size(),result,8))return 7; // backreference before output
    if(argc==2) {
        std::ifstream file(argv[1],std::ios::binary);
        std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)),{});
        if(!Extract(bytes,result))return 8;
        std::cout<<"Installed-game template extracted into independent package: "<<result.size()<<" bytes\n";
    }
    std::cout<<"Stance assets: bounded chunks, rename and malformed compression passed\n";
}
