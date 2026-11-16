#include <cstdint>
static uintptr_t fixtureBase;
static uintptr_t FixtureAddress(uintptr_t address){return fixtureBase+address-0x800000;}
#define MWCL_SAVE_TEST_ADDRESS(address) FixtureAddress(address)
#include "../CustomizationProfileHost.h"
#include <iostream>
#include <vector>
#include <string>
#include <stdexcept>
namespace H=CustomizationProfileHost;
static std::vector<std::string> journal;
struct Context {int id;bool fail=false;uint32_t native=0,success=0,commit=0;int32_t player=0;};
static Context first{1},late{2};
static void Require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static void __cdecl Begin(void* context,void*,uint32_t native){auto& c=*static_cast<Context*>(context);c.native=native;journal.push_back("B"+std::to_string(c.id));if(c.fail)throw std::runtime_error("fixture observer failure");}
static void __cdecl End(void* context,void*,uint32_t){journal.push_back("E"+std::to_string(static_cast<Context*>(context)->id));}
static void __cdecl Done(void* context,const char* name){journal.push_back("D"+std::to_string(static_cast<Context*>(context)->id)+name);}
static void __cdecl BeforeLoad(void* context){journal.push_back("L"+std::to_string(static_cast<Context*>(context)->id));}
static void __cdecl AfterLoad(void* context,const char*,void*,uint32_t native,uint32_t success,uint32_t commit,int32_t player){
    auto& c=*static_cast<Context*>(context);c.native=native;c.success=success;c.commit=commit;c.player=player;
    journal.push_back("R"+std::to_string(c.id));
}
static H::Observer Make(uint32_t id,Context* context){return {sizeof(H::Observer),1,id,context,&Begin,&End,&Done,&BeforeLoad,&AfterLoad};}
static H::Observer later=Make(2,&late);
static void __fastcall NativeSave(DWORD*,void*,void*,int){journal.push_back("native-save");Require(H::Subscribe(&later)!=0,"late subscription");}
static void __fastcall NativeDone(DWORD*,void*,const char*){journal.push_back("native-done");}
static bool __fastcall NativeLoad(DWORD*,void*,void*,int,bool,int){journal.push_back("native-load");return false;}
int main(){try {
    H::Registry registry;auto observer=Make(1,&first);
    Require(registry.Add(observer)&&registry.Add(observer),"same observer registration is idempotent");
    auto conflict=observer;conflict.context=&late;Require(!registry.Add(conflict),"conflicting identity rejected");
    auto invalid=observer;invalid.size=0;Require(!registry.Add(invalid),"invalid ABI rejected");
    invalid=observer;invalid.endLoad=nullptr;Require(!registry.Add(invalid),"partial callback table rejected");
    const auto snapshot=registry.Read();
    for(uint32_t id=2;id<=8;++id)Require(registry.Add(Make(id,&first)),"bounded listener insertion");
    Require(!registry.Add(Make(9,&first))&&snapshot.count==1,"capacity bound and immutable transaction snapshot");
    fixtureBase=reinterpret_cast<uintptr_t>(VirtualAlloc(nullptr,0x200000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    Require(fixtureBase!=0,"fixture globals");
    DWORD profile[16]{};strcpy_s(reinterpret_cast<char*>(profile),32,"Alpha");
    DWORD database[8]{};database[4]=reinterpret_cast<DWORD>(profile);
    *reinterpret_cast<DWORD**>(FixtureAddress(0x91CF90))=database;
    int buffer=0;DWORD card[20]{};card[0x44/4]=reinterpret_cast<DWORD>(&buffer);
    *reinterpret_cast<DWORD**>(FixtureAddress(0x91CB20))=card;
    H::OriginalSave=reinterpret_cast<H::SaveFn>(&NativeSave);H::OriginalDone=reinterpret_cast<H::DoneFn>(&NativeDone);H::OriginalLoad=reinterpret_cast<H::LoadFn>(&NativeLoad);
    Require(H::Subscribe(&observer)!=0,"valid callback module pin and registration");
    H::Save(profile,nullptr,&buffer,sizeof(buffer));
    Require((journal==std::vector<std::string>{"B1","native-save","E1"})&&first.native==1,"registration during native save cannot receive unmatched end");
    journal.clear();H::Save(profile,nullptr,&buffer,sizeof(buffer));
    Require((journal==std::vector<std::string>{"B1","B2","native-save","E1","E2"}),"next save sees both subscribers around one native call");
    DWORD other[16]{};journal.clear();H::Save(other,nullptr,&buffer,sizeof(buffer));
    Require((journal==std::vector<std::string>{"native-save"}),"inactive profile does not capture extension state");
    journal.clear();H::Saved(nullptr,nullptr,"C:\\Profiles\\Alpha\\Alpha");
    Require((journal==std::vector<std::string>{"native-done","D1Alpha","D2Alpha"}),"disk callbacks follow native completion and use normalized profile name");
    journal.clear();Require(!H::Load(profile,nullptr,&buffer,sizeof(buffer),false,1),"native load result retained");
    Require((journal==std::vector<std::string>{"L1","L2","native-load","R1","R2"})&&first.native==1&&first.success==0&&first.commit==0&&first.player==1,
        "actual reader lifecycle remains observable for failed temporary non-player-zero loads");
    first.fail=true;journal.clear();H::Save(profile,nullptr,&buffer,sizeof(buffer));
    Require((journal==std::vector<std::string>{"B1","B2","native-save","E2"}),"observer exception cannot cancel native save or another subscriber");
    VirtualFree(reinterpret_cast<void*>(fixtureBase),0,MEM_RELEASE);
    std::cout<<"Profile host ABI, registry and native transaction ordering passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
