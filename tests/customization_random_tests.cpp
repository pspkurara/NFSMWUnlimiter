#include <cstdint>
static uintptr_t fixtureBase;
static uintptr_t FixtureAddress(uintptr_t original) {return fixtureBase+original-0x800000;}
#define MWCL_SAVE_TEST_ADDRESS(original) FixtureAddress(original)
#include "../CustomizationSave.h"
static const char* GetCarTypeName(unsigned) {return "GOLF";}
static DWORD* RideInfo_GetPart(DWORD* ride,int slot) {return reinterpret_cast<DWORD*>(ride[0x48/4+slot]);}
static void(__cdecl* RideInfoRandomObserver)(DWORD*)=nullptr;
#include "../CustomizationRandom.h"
#include <iostream>
#include <cstdlib>
static void Check(bool ok,const char* message) {if(!ok){std::cerr<<message<<'\n';std::exit(1);}}
static unsigned samples=0;
static int __cdecl Sampler(const char*,uint32_t,uint32_t,CustomizationSave::Values* values) {
    ++samples;values->mask=3;values->values[0]=values->values[1]=-.01f*samples;return 1;
}
static void __fastcall NativeWrite(void* record,void*,DWORD* ride) {std::memcpy(record,ride,sizeof(DWORD));}
static void __fastcall NativeRead(void* record,void*,DWORD* ride) {std::memcpy(ride,record,sizeof(DWORD));}
static void __declspec(naked) NativeCtor() {__asm {mov eax,ecx} __asm {ret}}
static void __declspec(naked) NativeCopy() {
    __asm {mov ecx,0x66} __asm {mov edi,eax} __asm {rep movsd} __asm {ret}
}
int main() {
    namespace R=CustomizationRandom;
    using CustomizationSave::Fingerprint;
    fixtureBase=reinterpret_cast<uintptr_t>(VirtualAlloc(nullptr,0x200000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    Check(fixtureBase!=0,"Fixture globals allocated");
    R::HooksReady=true;R::Select=&Sampler;
    CustomizationSave::TransientReset=&R::Reset;
    CustomizationSave::TransientRideCopy=&R::RideCopy;
    CustomizationSave::TransientRecordCopy=&R::RecordCopy;
    R::OriginalRead=reinterpret_cast<R::WriteFn>(&NativeRead);
    R::OriginalWrite=reinterpret_cast<R::WriteFn>(&NativeWrite);
    std::vector<DWORD> source(0xc4),viewer(0xc4),race(0xc4),identical(0xc4);
    source[0]=123;identical=source;
    R::Randomized(source.data());
    Check(samples==1&&R::Resolve(source.data(),0,0,0)==-.01f,"One sample at random-parts completion");
    Check(R::Resolve(identical.data(),0,0,0)==0,"Identical equipment never creates lineage");
    // The real shared copy observer runs before native memcpy.
    CustomizationSave::TrackRideCopy(source.data(),viewer.data());
    std::memcpy(viewer.data(),source.data(),source.size()*4);
    Check(R::Resolve(viewer.data(),1,0,0)==-.01f&&samples==1,"Frontend copy preserves selection without reroll");
    std::array<BYTE,0x198> record{},copy{};
    R::WriteRecord(record.data(),nullptr,viewer.data());
    CustomizationSave::OriginalCustomizationCopy=reinterpret_cast<void*>(&NativeCopy);
    auto from=record.data();auto to=copy.data();
    __asm {
        push esi
        push edi
        mov esi,from
        mov eax,to
        call CustomizationSave::CustomizationCopyCave
        pop edi
        pop esi
    }
    Check(record==copy,"Random observer preserves the native customization deep copy");
    R::ReadRecord(copy.data(),nullptr,race.data());
    Check(R::Resolve(race.data(),0,0,0)==-.01f&&samples==1,"Quick Race accept / AI record copies reach race");
    // Rendering, race restart and replay of the same native record retain the choice.
    for(int i=0;i<20;++i)Check(R::Resolve(race.data(),1,0,0)==-.01f,"Stable render value");
    R::ReadRecord(copy.data(),nullptr,race.data());
    Check(samples==1&&R::Resolve(race.data(),0,0,0)==-.01f,"Race rebuild does not resample");
    R::Randomized(source.data());
    Check(samples==2&&R::Resolve(source.data(),0,0,0)==-.02f&&R::Resolve(race.data(),0,0,0)==-.01f,
        "Reroll replaces only the new generation, preserving existing copies");
    CustomizationSave::TrackRideCopy(identical.data(),viewer.data());
    std::memcpy(viewer.data(),identical.data(),source.size()*4);
    Check(R::Resolve(viewer.data(),0,0,0)==0,"Unknown source erases reused destination");
    copy[10]^=1;R::ReadRecord(copy.data(),nullptr,race.data());
    Check(R::Resolve(race.data(),0,0,0)==0,"Mutated record loses stale transient selection");
    // Constructor/stock/preset caves must preserve ECX and the original return value.
    R::OriginalRideInit=R::OriginalRideStock=reinterpret_cast<void*>(&NativeCtor);
    R::OriginalRecordInit=R::OriginalRecordDefault=R::OriginalRecordPreset=reinterpret_cast<void*>(&NativeCtor);
    auto callCtor=[](void* function,void* object) {return reinterpret_cast<void*(__thiscall*)(void*)>(function)(object);};
    Check(callCtor(reinterpret_cast<void*>(&R::RideInit),source.data())==source.data()&&R::Resolve(source.data(),0,0,0)==0,
        "Ride initialization forgets even identical-address/equipment generations");
    R::Randomized(source.data());
    Check(callCtor(reinterpret_cast<void*>(&R::RideStock),source.data())==source.data()&&R::Resolve(source.data(),0,0,0)==0,
        "Stock rebuild clears stale generation");
    for(auto cave:{&R::RecordInit,&R::RecordDefault,&R::RecordPreset}) {
        R::Randomized(source.data());R::WriteRecord(record.data(),nullptr,source.data());
        Check(callCtor(reinterpret_cast<void*>(cave),record.data())==record.data(),"Record cave preserves constructor return");
        R::ReadRecord(record.data(),nullptr,race.data());
        Check(R::Resolve(race.data(),0,0,0)==0,"Record initialization/default/preset invalidates selection");
    }
    // A native owned customization record must never carry an automatic choice.
    std::vector<BYTE> profile(0xc000);strcpy_s(reinterpret_cast<char*>(profile.data()),32,"Alpha");profile[32]=1;
    DWORD database[8]{};database[4]=reinterpret_cast<DWORD>(profile.data());
    *reinterpret_cast<DWORD**>(FixtureAddress(0x91cf90))=database;
    CustomizationSave::Profile="Alpha";
    auto stable=CustomizationSave::Stable();for(int i=0;i<200;++i)stable[i*5]=0xffffffff;
    stable[0]=7;stable[1]=11;stable[2]=123;stable[3]=2;reinterpret_cast<BYTE*>(stable)[16]=1;
    auto owned=reinterpret_cast<BYTE*>(stable)+0xfa0+0x198;owned[0x194]=1;
    R::Randomized(source.data());R::WriteRecord(owned,nullptr,source.data());
    R::ReadRecord(owned,nullptr,race.data());
    Check(R::Resolve(race.data(),0,0,0)==0&&CustomizationSave::Pending.empty(),"Owned data is isolated from random selections");
    CustomizationSave::LoadLocked("");
    Check(R::Resolve(source.data(),0,0,0)==0,"Profile teardown clears transient generations");
    std::cout<<"Random selection lifecycle, native copies, reuse and owned isolation passed\n";
    return 0;
}
