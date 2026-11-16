#include <cstdint>
static uintptr_t fixtureBase;
static uintptr_t FixtureAddress(uintptr_t original) {return fixtureBase+original-0x800000;}
#define MWCL_SAVE_TEST_ADDRESS(original) FixtureAddress(original)
#include "../CustomizationSave.h"
#include <iostream>
#include <cstdlib>
namespace fs=std::filesystem;
using namespace CustomizationSave;
static fs::path directory;
static void Check(bool ok,const char* message) {if(!ok){std::cerr<<message<<'\n';std::exit(1);}}
static HRESULT WINAPI Folder(HWND,int,HANDLE,DWORD,char* path) {
    strcpy_s(path,MAX_PATH,directory.string().c_str());return S_OK;
}
int main() {
    directory=fs::current_path()/("save-fixture-"+std::to_string(GetCurrentProcessId()));
    fs::create_directories(directory/"NFS Most Wanted"/"Alpha");
    fs::create_directories(directory/"NFS Most Wanted"/"Beta");
    fixtureBase=reinterpret_cast<uintptr_t>(VirtualAlloc(nullptr,0x200000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    Check(fixtureBase!=0,"Fixture globals allocation");
    *reinterpret_cast<void**>(FixtureAddress(0x8902ec))=reinterpret_cast<void*>(&Folder);
    const char* title="NFS Most Wanted";
    *reinterpret_cast<const char**>(FixtureAddress(0x901748))=title;
    std::vector<BYTE> profile(0x3000);
    strcpy_s(reinterpret_cast<char*>(profile.data()),32,"Alpha");profile[32]=1;
    DWORD database[8]{};database[4]=reinterpret_cast<DWORD>(profile.data());
    *reinterpret_cast<DWORD**>(FixtureAddress(0x91cf90))=database;
    *reinterpret_cast<int*>(FixtureAddress(0x925e90))=6; // No frontend getter code is executed in this fixture.
    auto stable=Stable();for(int i=0;i<200;++i) stable[i*5]=0xffffffff;
    stable[0]=7;stable[1]=11;stable[2]=123;stable[3]=2;
    reinterpret_cast<BYTE*>(stable)[16]=1;
    Key key{};Check(ReadKey(stable,key),"Owned fixture key");
    const auto native=directory/"NFS Most Wanted"/"Alpha"/"Alpha";
    {std::ofstream out(native,std::ios::binary);out<<"original native profile";}
    std::array<uint8_t,32> before{},after{};Check(Digest(native,before),"Hash native fixture");
    LoadLocked("Alpha");HooksReady=true;Tokens[key]=9;ActiveEditorToken=9;
    Values value{};value.mask=1u<<4;value.values[4]=-2.5f;
    Check(Edit(9,&value,0)==1&&Pending.empty()&&!Preview.empty(),"Preview must not stage");
    Check(Edit(9,nullptr,2)==1&&Preview.empty(),"Cancel restores preview");
    Check(Edit(9,&value,1)==1&&Pending.at(key).values[4]==-2.5f,"Commit only stages");
    auto target=directory/"NFS Most Wanted"/"Alpha"/"NFSMWCustomizationLab.bin";
    Check(!fs::exists(target),"Staging must not write sidecar before native save");
    Commit("Alpha");Check(!fs::exists(target),"No save snapshot means no disk commit");
    Saving=Pending;SavingProfile="Alpha";SaveCaptured=true;
    Commit("Beta");Check(!fs::exists(target),"Wrong profile SaveDone cannot commit");
    Commit("Alpha");Check(fs::exists(target),"Native success writes extension data");
    Check(Digest(native,after)&&before==after,"Native profile bytes unchanged");
    Pending.clear();LoadLocked("Alpha");Check(Pending.size()==1&&Pending.at(key).values[4]==-2.5f,"Absolute-value roundtrip");
    // An unknown AI RideInfo with identical equipment must not inherit this owned car.
    std::vector<DWORD> ride(0xc4);ride[0]=123;
    Rides[ride.data()]={key,Fingerprint(ride.data())};
    Check(Resolve(ride.data(),0,2,0)==-2.5f,"Known owned ride resolves");
    auto ai=ride;Check(Resolve(ai.data(),0,2,0)==0,"Unknown identical AI remains native");
    ActiveEditorToken=0;Check(Edit(9,&value,1)==0,"Old editor token cannot stage");
    auto invalid=value;invalid.values[4]=NAN;Check(!ValidValues(invalid),"NaN rejected");
    invalid=value;invalid.mask=1u<<2;invalid.values[2]=-.1f;Check(!ValidValues(invalid),"Negative half-track rejected");
    invalid=value;invalid.mask=1u<<6;invalid.values[6]=5.f;Check(ValidValues(invalid),"Inclusive hard width bound");
    {std::fstream out(target,std::ios::binary|std::ios::in|std::ios::out);out.seekp(sizeof(Header)+sizeof(Key)+8);out.put('x');}
    LoadLocked("Alpha");Check(Pending.empty(),"Corrupt CRC rejected");
    Pending[key]=value;Saving=Pending;SavingProfile="Alpha";SaveCaptured=true;Commit("Alpha");
    {std::ofstream out(native,std::ios::binary|std::ios::app);out<<"new revision";}
    LoadLocked("Alpha");Check(Pending.empty(),"Different native revision cannot load old owned data");
    strcpy_s(reinterpret_cast<char*>(profile.data()),32,"Beta");EnsureProfileLocked();
    Check(Profile=="Beta"&&Pending.empty()&&Preview.empty()&&Tokens.empty(),"Profile switch clears all session ownership");
    Check(!ValidName("../Alpha")&&!ValidName("Alpha/Beta")&&ValidName("Alpha"),"Profile path traversal rejected");
    std::cout<<"Save lifecycle, isolation, CRC and native-byte preservation passed\n";
}
