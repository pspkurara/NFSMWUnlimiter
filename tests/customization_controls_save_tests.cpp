#include <cstdint>
static uintptr_t fixtureBase;
static uintptr_t FixtureAddress(uintptr_t address) {return fixtureBase+address-0x800000;}
#define MWCL_SAVE_TEST_ADDRESS(address) FixtureAddress(address)
#include "../CustomizationControls.h"
#include <iostream>
#include <stdexcept>
namespace fs=std::filesystem;
namespace C=CustomizationControls;
namespace S=CustomizationSave;
static fs::path directory;
static void Require(bool ok,const char* why) {if(!ok)throw std::runtime_error(why);}
static HRESULT WINAPI Folder(HWND,int,HANDLE,DWORD,char* path) {
    strcpy_s(path,MAX_PATH,directory.string().c_str());return S_OK;
}
int main() {try {
    directory=fs::current_path()/("controls-fixture-"+std::to_string(GetCurrentProcessId()));
    auto alpha=directory/"NFS Most Wanted"/"Alpha";
    auto beta=directory/"NFS Most Wanted"/"Beta";
    fs::create_directories(alpha);fs::create_directories(beta);
    {std::ofstream out(alpha/"Alpha",std::ios::binary);out<<"native input profile alpha";}
    {std::ofstream out(beta/"Beta",std::ios::binary);out<<"native input profile beta";}
    fixtureBase=reinterpret_cast<uintptr_t>(VirtualAlloc(nullptr,0x200000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    Require(fixtureBase!=0,"fixture globals");
    *reinterpret_cast<void**>(FixtureAddress(0x8902EC))=reinterpret_cast<void*>(&Folder);
    *reinterpret_cast<const char**>(FixtureAddress(0x901748))="NFS Most Wanted";
    DWORD profile[64]{};strcpy_s(reinterpret_cast<char*>(profile),32,"Alpha");
    DWORD database[8]{};database[4]=reinterpret_cast<DWORD>(profile);
    *reinterpret_cast<DWORD**>(FixtureAddress(0x91CF90))=database;
    std::array<uint8_t,32> before{},after{};Require(S::Digest(alpha/"Alpha",before),"hash native profile");
    auto target=alpha/"NFSMWCustomizationControls.bin";
    C::ControlSession.active.enabled=1;C::ControlSession.active.keys[0]='J';C::ControlSession.active.orbitMode=1;
    C::OnBeginSave(nullptr,true);C::OnEndSave(nullptr,true);C::OnSaveDone("Alpha");
    Require(!fs::exists(target),"save without native input writer cannot commit");
    C::WrittenState=C::ControlSession.active;C::WroteInput=true;
    C::OnEndSave(nullptr,true);
    C::ControlSession.active.keys[0]='K'; // An edit after serialization belongs to the next save.
    C::OnSaveDone("Beta");Require(!fs::exists(target),"other profile callback cannot commit");
    C::OnSaveDone("Alpha");Require(fs::exists(target),"native success commits sidecar");
    auto loaded=C::LoadState("Alpha");Require(loaded.enabled&&loaded.keys[0]=='J',"exact serialization snapshot roundtrip");
    Require(S::Digest(alpha/"Alpha",after)&&before==after,"native save bytes unchanged");
    Require(loaded.orbitMode==1,"direction mode persists in actual sidecar");
    C::State legacy=loaded;legacy.version=1;
    C::FileHeader legacyHeader;legacyHeader.size=C::LegacyStateBytes;
    legacyHeader.crc=S::Crc(&legacy,C::LegacyStateBytes);std::memcpy(legacyHeader.digest,before.data(),32);
    {std::ofstream file(target,std::ios::binary|std::ios::trunc);
        file.write(reinterpret_cast<const char*>(&legacyHeader),sizeof(legacyHeader));
        file.write(reinterpret_cast<const char*>(&legacy),C::LegacyStateBytes);}
    auto migrated=C::LoadState("Alpha");
    Require(migrated.version==3&&migrated.keys[0]=='J'&&migrated.orbitMode==0,"version-1 disk settings migrate to rate mode");
    legacy=loaded;legacy.version=2;legacy.orbitMode=1;
    legacyHeader.size=C::Version2StateBytes;legacyHeader.crc=S::Crc(&legacy,C::Version2StateBytes);
    {std::ofstream file(target,std::ios::binary|std::ios::trunc);
        file.write(reinterpret_cast<const char*>(&legacyHeader),sizeof(legacyHeader));
        file.write(reinterpret_cast<const char*>(&legacy),C::Version2StateBytes);}
    migrated=C::LoadState("Alpha");
    Require(migrated.version==3&&migrated.orbitMode==1&&migrated.mouseEnabled==1,"version-2 disk settings preserve mode and initialize mouse defaults");

    C::OnBeginLoad();C::OnEndLoad("Beta",nullptr,true,true,true,0);
    Require(C::Runtime().keys[0]=='K',"profile creation without input reader preserves active configuration");
    C::ReadInput=true;auto generation=C::ScreenGeneration;
    C::OnEndLoad("Alpha",nullptr,true,false,false,1);
    Require(C::Runtime().keys[0]=='J'&&C::ScreenGeneration!=generation,"actual reader owns global input even on temporary noncommitted load");
    int buffer=0;
    C::OnBeginSave(&buffer,false);C::WroteInput=true;
    C::WrittenState=C::Runtime();C::WrittenState.yawAxis=1;C::OnEndSave(&buffer,false);
    C::ControlSession.active.yawAxis=2;
    C::OnBeginLoad();C::ReadInput=true;C::OnEndLoad("Alpha",&buffer,false,true,true,0);
    Require(C::Runtime().yawAxis==1,"native backup restores matching extension snapshot");
    C::OnBeginLoad();C::ReadInput=true;C::ResetLoadedController=true;
    C::OnEndLoad("Alpha",&buffer,false,true,true,0);
    Require(C::Runtime().yawAxis==3&&C::Runtime().keys[0]=='J',"native device mismatch resets controller only");
    C::OnBeginLoad();C::ReadInput=true;C::ReadInputCrc=1;
    C::OnEndLoad("Alpha",&buffer,false,true,true,0);
    Require(!C::Runtime().enabled,"reused backup address with different input bytes cannot borrow disk settings");
    C::OnBeginLoad();C::ReadInput=true;C::OnEndLoad("Beta",&buffer,false,true,true,0);
    Require(!C::Runtime().enabled&&!C::Runtime().keys[0],"backup pointer with different profile cannot leak bindings");
    {std::fstream file(target,std::ios::binary|std::ios::in|std::ios::out);file.seekp(sizeof(C::FileHeader)+4);file.put('x');}
    Require(!C::LoadState("Alpha").enabled,"corrupt controls CRC rejected");
    C::WrittenState=loaded;C::WroteInput=true;C::OnEndSave(nullptr,true);C::OnSaveDone("Alpha");
    {std::ofstream out(alpha/"Alpha",std::ios::binary|std::ios::app);out<<"revision";}
    Require(!C::LoadState("Alpha").enabled,"other native revision cannot load old controls");
    char bytes[C::NativeInputBytes+1]{};
    Require(C::CompleteStream(bytes,bytes+sizeof(bytes),bytes+C::NativeInputBytes),"complete native action stream");
    Require(!C::CompleteStream(bytes,bytes+20,bytes+4),"partial native stream cannot capture extension");
    fs::remove_all(directory);VirtualFree(reinterpret_cast<void*>(fixtureBase),0,MEM_RELEASE);
    std::cout<<"Controls save snapshots, input ownership, backup, CRC and native revision checks passed\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
