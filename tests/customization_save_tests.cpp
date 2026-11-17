#include <cstdint>
static uintptr_t fixtureBase;
static uintptr_t FixtureAddress(uintptr_t original) {return fixtureBase+original-0x800000;}
#define MWCL_SAVE_TEST_ADDRESS(original) FixtureAddress(original)
#include "../CustomizationSave.h"
#include "../includes/injector/injector.hpp"
static uintptr_t FixtureCartAddress(uintptr_t original);
#define MWCL_CART_TEST_ADDRESS(original) FixtureCartAddress(original)
static void __fastcall FEShoppingCartItem_DrawPartName(DWORD*) {}
#include "../CustomizationStanceCart.h"
#include <iostream>
#include <cstdlib>
namespace fs=std::filesystem;
using namespace CustomizationSave;
static fs::path directory;
static void Check(bool ok,const char* message) {if(!ok){std::cerr<<message<<'\n';std::exit(1);}}
static DWORD cartManager[0x1C4/4]{};
static int cartAllocations=0,nativeCheckouts=0,previewParts=0,nativePartInstalls=0;
static bool failCartCommit=false,inBackroom=false;
static void* __cdecl CartAllocate(int size){++cartAllocations;return std::calloc(1,size);}
static void __cdecl CartFree(void* pointer){if(pointer){--cartAllocations;std::free(pointer);}}
static bool __cdecl CartBackroom(){return inBackroom;}
static DWORD* __fastcall CartPart(DWORD* self,void*,void* part,int slot,unsigned level,int physics,bool performance,int state,int price,bool junkman) {
    self[0]=0x8B7518;self[3]=reinterpret_cast<DWORD>(part);self[4]=slot;self[5]=level;self[6]=physics;
    reinterpret_cast<BYTE*>(self)[0x1C]=performance;self[8]=state;self[9]=price;reinterpret_cast<BYTE*>(self)[0x28]=junkman;return self;
}
static DWORD* __fastcall CartItem(DWORD* self,void*,DWORD* part,void* trade) {
    self[0]=0x8B751C;self[3]=reinterpret_cast<DWORD>(part);self[4]=reinterpret_cast<DWORD>(trade);
    reinterpret_cast<BYTE*>(self)[0x14]=1;return self;
}
static void* __fastcall CartDelete(DWORD* self,void*,unsigned) {CartFree(reinterpret_cast<void*>(self[3]));CartFree(self);return self;}
static bool __fastcall CartRemove(DWORD*,void*,DWORD* item) {
    CustomizationStanceCart::Detach(item);CustomizationStanceCart::Delete(item,nullptr,1);return true;
}
static void __fastcall CartCheckout(DWORD* manager,void*) {
    ++nativeCheckouts;auto sentinel=manager+0x1B4/4;
    while(sentinel[0]!=reinterpret_cast<DWORD>(sentinel)) {
        auto item=reinterpret_cast<DWORD*>(sentinel[0])-1;
        Check(reinterpret_cast<DWORD*>(item[3])[4]!=0x8F1||!reinterpret_cast<BYTE*>(item)[0x14],
              "Synthetic stance slot must never reach active native checkout");
        if(reinterpret_cast<BYTE*>(item)[0x14])++nativePartInstalls;
        CartRemove(manager,nullptr,item);
    }
}
static void __fastcall CartPreviewPart(DWORD*,void*,int,void*){++previewParts;}
static void __fastcall CartSetupNative(DWORD*,void*) {
    Check(!inBackroom||!CustomizationStanceCart::Item,"Backroom marker setup cannot see synthetic item");
}
static int __cdecl CartControl(uint64_t,int operation,int){return operation==1&&failCartCommit?0:1;}
static uintptr_t FixtureCartAddress(uintptr_t original) {
    switch(original) {
    case 0x9B9EC0:return reinterpret_cast<uintptr_t>(cartManager);
    case 0x652AD0:return reinterpret_cast<uintptr_t>(&CartAllocate);
    case 0x652B00:return reinterpret_cast<uintptr_t>(&CartFree);
    case 0x575B00:return reinterpret_cast<uintptr_t>(&CartBackroom);
    case 0x7A9EA0:return reinterpret_cast<uintptr_t>(&CartPart);
    case 0x7A9F20:return reinterpret_cast<uintptr_t>(&CartItem);
    case 0x7AEB60:return reinterpret_cast<uintptr_t>(&CartRemove);
    default:return original;
    }
}
static HRESULT WINAPI Folder(HWND,int,HANDLE,DWORD,char* path) {
    strcpy_s(path,MAX_PATH,directory.string().c_str());return S_OK;
}
static void __declspec(naked) NativeCustomizationCopyFixture() {
    __asm { mov ecx,0x66
        mov edi,eax
        rep movsd
        ret }
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
    Tokens[key]=9;ActiveEditorToken=9;
    auto previewValue=value;previewValue.values[4]=-4.f;
    *reinterpret_cast<int*>(FixtureAddress(0x925e90))=3;
    Check(Edit(9,&previewValue,0)==1&&Resolve(ride.data(),0,2,0)==-4.f,
        "Frontend previews show immediately without changing committed stance");
    int saveBuffer=0;
    BeginOwnedSave(nullptr,&saveBuffer,1);EndOwnedSave(nullptr,&saveBuffer,1);
    Check(SaveCaptured&&Saving.at(key).values[4]==-2.5f,"Native save snapshots exclude unconfirmed preview values");
    *reinterpret_cast<int*>(FixtureAddress(0x925e90))=6;
    Check(!Preview.empty()&&Resolve(ride.data(),0,2,0)==-2.5f,
        "First race update ignores a lingering menu preview before render clears its context");
    Check(Edit(9,nullptr,2)==1&&Resolve(ride.data(),0,2,0)==-2.5f,"Discard restores committed stance after transition");
    *reinterpret_cast<int*>(FixtureAddress(0x925e90))=3;
    Check(BeginShop(9,true),"Native shop starts from purchased owned values");
    auto cartValue=value;cartValue.values[4]=-6.f;
    Check(PreviewShop(9,cartValue,true)&&Resolve(ride.data(),0,2,0)==-6.f,"Shop edit previews in frontend");
    const auto revision=AcceptShop(9);
    Check(revision&&Pending.at(key).values[4]==-2.5f,"Accept queues without purchasing");
    uint64_t menuOwner=0;Values menuValues;
    Check(NativeMenuOwner(menuOwner,menuValues)&&menuOwner==9&&menuValues.values[4]==-6.f,
          "Reopening native sliders reads queued absolute values");
    BeginOwnedSave(nullptr,&saveBuffer,1);EndOwnedSave(nullptr,&saveBuffer,1);
    Check(Saving.at(key).values[4]==-2.5f,"Native disk save excludes shopping cart");
    *reinterpret_cast<int*>(FixtureAddress(0x925e90))=6;
    Check(Resolve(ride.data(),0,2,0)==-2.5f,"Race ignores unpurchased cart");
    *reinterpret_cast<int*>(FixtureAddress(0x925e90))=3;
    Check(BeginShop(9,true)&&PreviewShop(9,previewValue,true)&&CancelShop(9),"Cancel a queued item edit");
    Check(Resolve(ride.data(),0,2,0)==-6.f,"Editor cancel restores existing cart");
    Check(BeginShopCheckout(9,revision)&&EndShopCheckout(9,false)&&Resolve(ride.data(),0,2,0)==-6.f,
          "Failed checkout retains cart preview");
    Check(BeginShopCheckout(9,revision)&&EndShopCheckout(9,true)&&Pending.at(key).values[4]==-6.f,
          "Successful native checkout promotes once");
    Check(!EndShopCheckout(9,true)&&!EndShopCheckout(9,false),"Stale checkout cannot finalize");
    Check(NativeMenuOwner(menuOwner,menuValues)&&menuValues.values[4]==-6.f,"Purchased values survive reopen");
    Shop.Clear();Check(Edit(9,&value,1)==1,"Restore fixture purchased baseline");
    namespace cart=CustomizationStanceCart;
    auto cartSentinel=cartManager+0x1B4/4;
    cartSentinel[0]=cartSentinel[1]=reinterpret_cast<DWORD>(cartSentinel);
    cart::Installed=true;cart::CoreControl=&CartControl;
    cart::OriginalDelete=reinterpret_cast<cart::DeleteFn>(&CartDelete);
    cart::OriginalCheckout=reinterpret_cast<cart::CheckoutFn>(&CartCheckout);
    cart::OriginalPreviewPart=reinterpret_cast<cart::PreviewPartFn>(&CartPreviewPart);
    cart::OriginalCartSetup=reinterpret_cast<cart::CheckoutFn>(&CartSetupNative);
    Check(BeginShop(9,true)&&PreviewShop(9,cartValue,true)&&cart::Accept(9),"Native adapter inserts stance item");
    Check(cartAllocations==2&&cartManager[0x1BC/4]==1&&cartSentinel[0]==reinterpret_cast<DWORD>(cart::Item+1),
          "Native item/part allocations and list ownership agree");
    Check(Pending.at(key).values[4]==-2.5f&&Resolve(ride.data(),0,2,0)==-6.f,"Native cart insertion only previews");
    cart::PreviewPart(cartManager,nullptr,0x8F1,nullptr);
    cart::PreviewPart(cartManager,nullptr,23,reinterpret_cast<void*>(1));
    Check(previewParts==1,"Native preview rebuild skips only the synthetic stance slot");
    Check(BeginShop(9,true)&&PreviewShop(9,previewValue,true)&&cart::Accept(9),"Replace native stance cart entry");
    Check(cartAllocations==2&&cartManager[0x1BC/4]==1&&Resolve(ride.data(),0,2,0)==-4.f,
          "Replacement deletes old allocations without removing new snapshot");
    failCartCommit=true;cart::Checkout(cartManager,nullptr);
    Check(nativeCheckouts==0&&cart::Item&&cartManager[0x1BC/4]==1&&Pending.at(key).values[4]==-2.5f,
          "Settings save failure leaves cart and native purchase untouched");
    failCartCommit=false;
    reinterpret_cast<BYTE*>(cart::Item)[0x14]=0;cart::Tick();
    Check(Resolve(ride.data(),0,2,0)==-2.5f,"Unchecked item restores purchased rendering");
    cart::Checkout(cartManager,nullptr);
    Check(!cart::Item&&cartAllocations==0&&cartManager[0x1BC/4]==0&&Pending.at(key).values[4]==-2.5f,
          "Unchecked item is discarded by native cleanup without purchase");
    Check(BeginShop(9,true)&&PreviewShop(9,cartValue,true)&&cart::Accept(9),"Queue native purchase case");
    cart::Checkout(cartManager,nullptr);
    Check(!cart::Item&&cartAllocations==0&&cartManager[0x1BC/4]==0&&Pending.at(key).values[4]==-6.f,
          "Native checkout detaches synthetic slot, promotes absolute values and frees allocations once");
    Check(BeginShop(9,true)&&PreviewShop(9,cartValue,true)&&cart::Accept(9),"Queue stance in mixed native cart");
    auto ordinaryPart=reinterpret_cast<DWORD*>(CartAllocate(0x2C));
    CartPart(ordinaryPart,nullptr,nullptr,23,0,7,false,0,0,false);
    auto ordinaryItem=reinterpret_cast<DWORD*>(CartAllocate(0x18));CartItem(ordinaryItem,nullptr,ordinaryPart,nullptr);
    auto ordinaryNode=ordinaryItem+1,tail=reinterpret_cast<DWORD*>(cartSentinel[1]);
    tail[0]=reinterpret_cast<DWORD>(ordinaryNode);cartSentinel[1]=reinterpret_cast<DWORD>(ordinaryNode);
    ordinaryNode[0]=reinterpret_cast<DWORD>(cartSentinel);ordinaryNode[1]=reinterpret_cast<DWORD>(tail);
    ++cartManager[0x1BC/4];
    const auto installsBefore=nativePartInstalls;
    cart::Checkout(cartManager,nullptr);
    Check(nativePartInstalls==installsBefore+1&&cartAllocations==0&&cartSentinel[0]==reinterpret_cast<DWORD>(cartSentinel),
          "Mixed checkout preserves native item processing and empties links without synthetic installation");
    Check(BeginShop(9,true)&&PreviewShop(9,previewValue,true)&&cart::Accept(9),"Queue item before backroom transition");
    inBackroom=true;cart::CartSetup(cartManager,nullptr);inBackroom=false;
    Check(!cart::Item&&cartAllocations==0&&Pending.at(key).values[4]==-6.f,
          "Backroom transition discards unpurchased stance without touching installed values");
    Shop.Clear();Check(Edit(9,&value,1)==1,"Restore purchased baseline after native adapter tests");
    *reinterpret_cast<int*>(FixtureAddress(0x925e90))=6;
    auto ai=ride;Check(Resolve(ai.data(),0,2,0)==0,"Unknown identical AI remains native");
    Check(TrackRideCopy(ride.data(),ai.data())&&Resolve(ai.data(),0,2,0)==-2.5f,
        "observed menu/loader RideInfo copy inherits stance before opening customization");
    std::vector<DWORD> unknown=ride;
    Check(!TrackRideCopy(unknown.data(),ai.data())&&Resolve(ai.data(),0,2,0)==0,
        "menu allocation reuse with an unknown source drops old ownership");
    auto customization=reinterpret_cast<BYTE*>(stable)+0xFA0+0x198;
    customization[0x194]=1;
    Check(BindRaceCustomization(ai.data(),customization,0)&&Resolve(ai.data(),0,2,0)==-2.5f,
        "local-player render load inherits the exact native profile customization record");
    Check(!BindRaceCustomization(ai.data(),customization,2)&&Resolve(ai.data(),0,2,0)==0,
        "AI usage and reused RideInfo allocation discard owned stance");
    std::array<BYTE,0x198> copied{};std::memcpy(copied.data(),customization,copied.size());
    Check(!BindRaceCustomization(ai.data(),copied.data(),0),"copied equipment or handle alone cannot prove ownership");
    OriginalCustomizationCopy=reinterpret_cast<void*>(&NativeCustomizationCopyFixture);
    auto destination=copied.data();
    __asm {
        push esi
        push edi
        mov esi,customization
        mov eax,destination
        call CustomizationCopyCave
        pop edi
        pop esi
    }
    Check(std::memcmp(customization,copied.data(),copied.size())==0,"copy cave preserves native source/destination registers and 0x198-byte deep copy");
    Check(BindRaceCustomization(ai.data(),copied.data(),0),"observed native deep copy preserves exact owned lineage");
    copied[0]^=1;
    Check(!BindRaceCustomization(ai.data(),copied.data(),0),"changed/reused customization copy loses old lineage");
    stable[5]=8;stable[8]=2;reinterpret_cast<BYTE*>(stable+5)[16]=1;
    Check(!BindRaceCustomization(ai.data(),customization,0),"ambiguous customization ownership fails closed");
    stable[5]=0xffffffff;
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
    const BYTE importedCall[]={0xFF,0x15,0xEC,0x02,0x89,0};
    BYTE redirectedCall[]={0xE8,0,0,0,0,0x90};
    const int32_t offset=-37;std::memcpy(redirectedCall+1,&offset,sizeof(offset));
    Check(FolderCallTarget(importedCall,0x12345678)==0x12345678 &&
        FolderCallTarget(redirectedCall,0)==reinterpret_cast<uintptr_t>(redirectedCall)+5+offset,
        "Native and redirected folder calls select the actual game path provider");
    redirectedCall[5]=0xCC;
    Check(!FolderCallTarget(redirectedCall,0x12345678),"Unknown directory patches disable sidecar I/O");
    Check(CompletedProfileName("C:\\Games\\SAVE\\Alpha\\Alpha")=="Alpha"&&CompletedProfileName("Alpha")=="Alpha", "PC completion filenames normalize without using incoming directories");
    std::cout<<"Save lifecycle, isolation, CRC and native-byte preservation passed\n";
    return 0;
}
