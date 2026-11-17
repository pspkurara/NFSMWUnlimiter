#pragma once
#include "CustomizationSave.h"

// PC adapter for one tagged native cart item. Native allocators, destructors,
// list nodes and price widgets remain in use; a synthetic slot is never installed.
namespace CustomizationStanceCart {
using Values=CustomizationSave::Values;
using CartControl=int(__cdecl*)(uint64_t,int,int);
static CartControl CoreControl=nullptr;
static DWORD* Item=nullptr;
static uint64_t Owner=0,Revision=0;
static bool Installed=false;
static std::optional<bool> AppliedEnabled;
static uintptr_t Address(uintptr_t original) {
#ifdef MWCL_CART_TEST_ADDRESS
    return MWCL_CART_TEST_ADDRESS(original);
#else
    return original;
#endif
}
using CheckoutFn=void(__thiscall*)(DWORD*);
using DeleteFn=void*(__thiscall*)(DWORD*,unsigned);
using NameFn=void(__thiscall*)(DWORD*);
static CheckoutFn OriginalCheckout=nullptr,OriginalRelinquish=nullptr;
static CheckoutFn OriginalCartSetup=nullptr;
static DeleteFn OriginalDelete=nullptr;
static NameFn OriginalName=nullptr;
using PreviewPartFn=void(__thiscall*)(DWORD*,int,void*);
static PreviewPartFn OriginalPreviewPart=nullptr;
static DWORD* Manager(){return reinterpret_cast<DWORD*>(Address(0x9B9EC0));}
static bool Core() {
    if(!CoreControl)if(auto module=GetModuleHandleA("NFSMWCustomizationLab.asi"))
        CoreControl=reinterpret_cast<CartControl>(GetProcAddress(module,"MWCL_ControlNativeCartV1"));
    return CoreControl!=nullptr;
}
static void Clear() {
    CustomizationSave::Log("Stance state reset: shop relinquished control.");
    if(Core()&&Owner)CoreControl(Owner,2,0);
    std::lock_guard<std::mutex> lock(CustomizationSave::Mutex);
    CustomizationSave::Shop.Clear();AppliedEnabled.reset();
}
static void Tick() {
    if(!Item||!Owner)return;
    std::optional<bool> enabled;
    {
        std::lock_guard<std::mutex> lock(CustomizationSave::Mutex);
        auto& shop=CustomizationSave::Shop;
        if(auto cart=shop.Cart(Owner)) {
            shop.SetActive(Owner,Revision,*reinterpret_cast<const BYTE*>(reinterpret_cast<const BYTE*>(Item)+0x14)!=0);
            if(auto visible=shop.Visible(Owner))enabled=visible->enabled;
        }
    }
    if(!enabled){if(Core())CoreControl(Owner,2,0);AppliedEnabled.reset();return;}
    if(enabled!=AppliedEnabled&&Core()&&CoreControl(Owner,0,*enabled))AppliedEnabled=enabled;
}
static void __fastcall Name(DWORD* widget,void*) {
    if(Item&&widget[0x54/4]==reinterpret_cast<DWORD>(Item)) {
        if(auto text=reinterpret_cast<DWORD*>(widget[0x2C/4]))
            reinterpret_cast<void(__cdecl*)(DWORD*,const wchar_t*)>(Address(0x515CC0))(text,L"Stance adjustment");
        return;
    }
    OriginalName(widget);
}
static void* __fastcall Delete(DWORD* item,void*,unsigned flags) {
    if(item==Item) {
        bool checkout=false;
        {
            std::lock_guard<std::mutex> lock(CustomizationSave::Mutex);
            checkout=CustomizationSave::Shop.CheckingOut(Owner);
        }
        if(!checkout) {
            CustomizationSave::CancelShop(Owner);
            CustomizationSave::RemoveShop(Owner,Revision);
            if(Core())CoreControl(Owner,2,0);
        }
        Item=nullptr;AppliedEnabled.reset();
    }
    return OriginalDelete(item,flags);
}
static void Detach(DWORD* item) {
    auto next=reinterpret_cast<DWORD*>(item[1]),previous=reinterpret_cast<DWORD*>(item[2]);
    previous[0]=reinterpret_cast<DWORD>(next);next[1]=reinterpret_cast<DWORD>(previous);
    --Manager()[0x1BC/4];
}
static void __fastcall Checkout(DWORD* manager,void*) {
    if(!Item||manager!=Manager()){OriginalCheckout(manager);return;}
    const bool active=*reinterpret_cast<const BYTE*>(reinterpret_cast<const BYTE*>(Item)+0x14)!=0;
    if(!active) {OriginalCheckout(manager);return;}
    if(!CustomizationSave::BeginShopCheckout(Owner,Revision)) {
        // An expired owner/profile must never send our synthetic slot to native installation.
        *reinterpret_cast<BYTE*>(reinterpret_cast<BYTE*>(Item)+0x14)=0;
        OriginalCheckout(manager);return;
    }
    if(!Core()||!CoreControl(Owner,1,0)) {
        CustomizationSave::EndShopCheckout(Owner,false);
        CustomizationSave::Log("Stance checkout retained: enabled settings could not be saved.");return;
    }
    auto item=Item;
    // Native UI has already checked affordability. Keep the tagged item out of
    // both direct record installation and native backroom-marker consumption.
    // Current item price is zero, so removing it cannot alter the approved total.
    Detach(item);
    OriginalCheckout(manager);
    if(!CustomizationSave::EndShopCheckout(Owner,true))
        CustomizationSave::Log("Stance checkout owner changed unexpectedly; owned values were not written.");
    Item=nullptr;AppliedEnabled.reset();
    OriginalDelete(item,1);
}
static void __fastcall Relinquish(DWORD* manager,void*) {
    if(manager==Manager())Clear();
    OriginalRelinquish(manager);
}
static void __fastcall PreviewPart(DWORD* manager,void*,int slot,void* part) {
    if(Item&&manager==Manager()&&slot==0x8F1&&!part)return;
    OriginalPreviewPart(manager,slot,part);
}
static void __fastcall CartSetup(DWORD* screen,void*) {
    // A stance item has no junkman marker. Remove its unpurchased preview before
    // native backroom setup can use the synthetic slot as a marker-array index.
    if(Item&&reinterpret_cast<bool(__cdecl*)()>(Address(0x575B00))())
        reinterpret_cast<bool(__thiscall*)(DWORD*,DWORD*)>(Address(0x7AEB60))(Manager(),Item);
    OriginalCartSetup(screen);
}
static bool Accept(uint64_t owner) {
    if(!Installed||!Core()||!owner)return false;
    // Native backroom rows map to junkman markers. This item has no such marker;
    // ordinary career shops and My Cars use the normal zero-cost cart path.
    if(reinterpret_cast<bool(__cdecl*)()>(Address(0x575B00))())return false;
    bool enabled=false;
    {
        std::lock_guard<std::mutex> lock(CustomizationSave::Mutex);
        auto visible=CustomizationSave::Shop.Visible(owner);if(!visible)return false;
        enabled=visible->enabled;
    }
    auto part=reinterpret_cast<DWORD*>(reinterpret_cast<void*(__cdecl*)(int)>(Address(0x652AD0))(0x2C));
    auto item=reinterpret_cast<DWORD*>(reinterpret_cast<void*(__cdecl*)(int)>(Address(0x652AD0))(0x18));
    if(!part||!item) {
        if(part)reinterpret_cast<void(__cdecl*)(void*)>(Address(0x652B00))(part);
        if(item)reinterpret_cast<void(__cdecl*)(void*)>(Address(0x652B00))(item);
        return false;
    }
    std::memset(part,0,0x2C);std::memset(item,0,0x18);
    reinterpret_cast<DWORD*(__thiscall*)(DWORD*,void*,int,unsigned,int,bool,int,int,bool)>(Address(0x7A9EA0))
        (part,nullptr,0x8F1,0,7,false,0,0,false);
    reinterpret_cast<DWORD*(__thiscall*)(DWORD*,DWORD*,void*)>(Address(0x7A9F20))(item,part,nullptr);
    if(!CoreControl(owner,0,enabled)) {OriginalDelete(item,1);return false;}
    const auto revision=CustomizationSave::AcceptShop(owner);
    if(!revision){OriginalDelete(item,1);Tick();return false;}
    if(Item) {
        auto old=Item;Item=nullptr;
        reinterpret_cast<bool(__thiscall*)(DWORD*,DWORD*)>(Address(0x7AEB60))(Manager(),old);
    }
    auto sentinel=reinterpret_cast<DWORD*>(reinterpret_cast<BYTE*>(Manager())+0x1B4);
    auto node=item+1,tail=reinterpret_cast<DWORD*>(sentinel[1]);
    tail[0]=reinterpret_cast<DWORD>(node);sentinel[1]=reinterpret_cast<DWORD>(node);
    node[0]=reinterpret_cast<DWORD>(sentinel);node[1]=reinterpret_cast<DWORD>(tail);
    ++Manager()[0x1BC/4];
    Item=item;Owner=owner;Revision=revision;AppliedEnabled=enabled;return true;
}
static bool Install(uintptr_t compatibleNameTarget=0) {
    if(Installed)return true;
    struct Hook {uintptr_t address;void* target;void** original;std::vector<BYTE> bytes;};
    const Hook hooks[]={
        {0x7BAB60,&Checkout,reinterpret_cast<void**>(&OriginalCheckout),{0x83,0xEC,0x08,0xA1,0x90,0xCF,0x91,0}},
        {0x7A9F40,&Delete,reinterpret_cast<void**>(&OriginalDelete),{0x56,0x8B,0xF1,0xE8,0x18,0,0,0}},
        {0x7AEB10,&Relinquish,reinterpret_cast<void**>(&OriginalRelinquish),{0xA1,0x90,0xCF,0x91,0,0x53,0x56,0x57}},
        {0x7AF110,&PreviewPart,reinterpret_cast<void**>(&OriginalPreviewPart),{0x8B,0x44,0x24,0x08,0x81,0xEC,0x10,0x03,0,0}},
        {0x7B0C30,&CartSetup,reinterpret_cast<void**>(&OriginalCartSetup),{0x51,0xA1,0xC4,0x9E,0x9B,0}}
    };
    for(const auto& hook:hooks)if(std::memcmp(reinterpret_cast<void*>(hook.address),hook.bytes.data(),hook.bytes.size()))return false;
    const BYTE ctor[]={0x8B,0x54,0x24,0x08,0x8B,0xC1,0x8B,0x4C,0x24,0x04};
    if(std::memcmp(reinterpret_cast<void*>(0x7A9EA0),ctor,sizeof(ctor))||
       std::memcmp(reinterpret_cast<void*>(0x7A9F20),ctor,sizeof(ctor))||
       *reinterpret_cast<const DWORD*>(0x8B751C)!=0x7A9F40)return false;
    if(*reinterpret_cast<const BYTE*>(0x7BB17B)!=0xE8)return false;
    const auto nameTarget=uintptr_t(0x7BB180+*reinterpret_cast<const int32_t*>(0x7BB17C));
    if(nameTarget!=0x7B63A0&&nameTarget!=compatibleNameTarget)return false;
    OriginalName=reinterpret_cast<NameFn>(nameTarget);
    size_t created=0;bool ok=true;
    for(const auto& hook:hooks){if(MH_CreateHook(reinterpret_cast<void*>(hook.address),hook.target,hook.original)!=MH_OK){ok=false;break;}++created;}
    if(ok)for(const auto& hook:hooks)if(MH_EnableHook(reinterpret_cast<void*>(hook.address))!=MH_OK){ok=false;break;}
    if(!ok){for(size_t i=0;i<created;++i){MH_DisableHook(reinterpret_cast<void*>(hooks[i].address));MH_RemoveHook(reinterpret_cast<void*>(hooks[i].address));}return false;}
    injector::MakeCALL(0x7BB17B,&Name,true);
    Installed=true;return true;
}
}
