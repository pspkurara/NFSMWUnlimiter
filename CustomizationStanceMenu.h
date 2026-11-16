#pragma once
#include "CustomizationSave.h"
#include "CustomizationStanceSession.h"
#include "CustomizationStanceAssets.h"
#include <array>
#include <string>
#include <cstdio>

// Native body-shop adapter. Only tagged instances reuse the stock tuning widgets;
// the original physics Setup/Notification/StoreSettings never run for these.
namespace CustomizationStanceMenu {
using CustomizationStance::MenuInfo;
using CustomizationStance::Session;
using CustomizationStance::Values;
constexpr unsigned Category=0x8F1;
constexpr const char* Package=CustomizationStanceAssets::Name;
constexpr unsigned Pressed=0x0C407210,Accept=0x406415E3,Back=0x911AB364;
constexpr unsigned Left=0x9120409E,Right=0xB5971BF1,PreviousAxle=0x5073EF13,NextAxle=0xD9FEEC59;
constexpr unsigned Inherit=0xC519BFC4,ExitComplete=0xE1FDE1D1;
static bool Installed=false,Constructing=false,BuildRequested=false,Opening=false,Closing=false,Finished=false;
static DWORD* Screen=nullptr;
static unsigned NameHash=0,PackageHash=0,OpeningArgs=0;
static void* OriginalDispatch=nullptr;
static void(__cdecl* OriginalLanguage)(DWORD*,unsigned)=nullptr;
static std::vector<uint8_t> TemplateChunk;
static bool AssetAttempted=false;
static ULONGLONG OpenedAt=0;
static uint64_t Owner=0;
static std::string Parent;
static Session EditSession;
static std::array<DWORD*,7> Sliders{};
struct SavedText {DWORD* object;unsigned name,language,flags;std::wstring text;};
static std::vector<SavedText> SavedStrings;
static void Remember(DWORD* object) {
    if(!object||object[0]!=0x89B05C)return;
    for(const auto& saved:SavedStrings)if(saved.object==object)return;
    const auto text=reinterpret_cast<const wchar_t*>(object[0x64/4]);
    const auto length=text?wcsnlen_s(text,2048):0;
    if(length==2048)return; // Replaced assets must not cause an unbounded copy.
    SavedStrings.push_back({object,object[0x10/4],object[0x60/4],object[0x1C/4],text?std::wstring(text,length):L""});
}
static void RestoreText() {
    for(const auto& saved:SavedStrings) {
        auto object=FEngFindObject(Package,saved.name);
        if(object!=saved.object||object[0]!=0x89B05C)continue;
        reinterpret_cast<void(__cdecl*)(DWORD*,const wchar_t*)>(0x515CC0)(object,saved.text.c_str());
        reinterpret_cast<void(__cdecl*)(DWORD*,unsigned)>(0x515C00)(object,saved.language);
        object[0x1C/4]=(object[0x1C/4]&~2u)|(saved.flags&2u);
    }
    SavedStrings.clear();
}
using BeginFn=int(__cdecl*)(uint64_t,MenuInfo*);
using ControlFn=int(__cdecl*)(uint64_t,int,int);
static BeginFn Begin=nullptr;
static ControlFn Control=nullptr;
using SetupFn=void(__thiscall*)(DWORD*);
using NotifyFn=void(__thiscall*)(DWORD*,unsigned,void*,unsigned,unsigned);
using FactoryFn=DWORD*(__cdecl*)(DWORD*);
using DrawFn=void(__thiscall*)(DWORD*);
using ActFn=void(__thiscall*)(DWORD*,const char*,unsigned);
using ReactFn=void(__thiscall*)(DWORD*,const char*,unsigned,void*,unsigned,unsigned);
static SetupFn OriginalSetup=nullptr,OriginalDestructor=nullptr;
static NotifyFn OriginalNotify=nullptr;
static FactoryFn OriginalFactory=nullptr;
static DrawFn OriginalDraw=nullptr;
static ActFn OriginalAct=nullptr;
static ReactFn OriginalReact=nullptr;

static const char* Labels[]={"Ride height (m)","Half-track / offset (m)","Camber (degrees)","Wheel / tire width (m)","Tire radius (m)","Brake inset (m)"};
static bool Core() {
    if(Begin&&Control)return true;
    auto module=GetModuleHandleA("NFSMWCustomizationLab.asi");if(!module)return false;
    Begin=reinterpret_cast<BeginFn>(GetProcAddress(module,"MWCL_BeginNativeMenuV1"));
    Control=reinterpret_cast<ControlFn>(GetProcAddress(module,"MWCL_ControlNativeMenuV1"));
    return Begin&&Control;
}
static bool Asset() {
    auto manager=*reinterpret_cast<void**>(0x91D21C);if(!manager)return false;
    auto find=reinterpret_cast<DWORD*(__thiscall*)(void*,const char*)>(0x585820);
    if(auto existing=find(manager,Package))return !TemplateChunk.empty()&&existing[0x18/4]==reinterpret_cast<DWORD>(TemplateChunk.data());
    if(TemplateChunk.empty()&&!AssetAttempted) {
        AssetAttempted=true;
        wchar_t executable[MAX_PATH]{};
        if(!GetModuleFileNameW(nullptr,executable,MAX_PATH))return false;
        const auto root=std::filesystem::path(executable).parent_path();
        for(const auto name:{L"GLOBAL/InGameB.bun",L"GLOBAL/InGameB.lzc",L"GLOBAL/INGAMEC.BUN"}) {
            std::ifstream file(root/name,std::ios::binary|std::ios::ate);if(!file)continue;
            const auto length=file.tellg();if(length<=0||length>CustomizationStanceAssets::MaxBundle)continue;
            std::vector<uint8_t> bytes(static_cast<size_t>(length));file.seekg(0);
            if(file.read(reinterpret_cast<char*>(bytes.data()),length)&&CustomizationStanceAssets::Extract(bytes,TemplateChunk))break;
        }
        if(TemplateChunk.empty()){CustomizationSave::Log("Stance template unavailable or unsupported; entry disabled.");return false;}
    }
    if(TemplateChunk.empty())return false;
    // Register a separate uncompressed package. Native race bundle unloaders can
    // never release this clone, and its storage stays stable for process lifetime.
    reinterpret_cast<int(__cdecl*)(void*)>(0x585AA0)(TemplateChunk.data());
    auto registered=find(manager,Package);
    CustomizationSave::Log(registered?"Independent stance template registered.":"Independent stance template registration failed.");
    return registered&&registered[0x18/4]==reinterpret_cast<DWORD>(TemplateChunk.data());
}
static bool Current() {
    return Owner&&CustomizationSave::NativeMenuOwnerCurrent(Owner)&&Control&&Control(EditSession.info.token,3,0)!=0;
}
static int Host(const Values* values,int action) {
    static_assert(sizeof(Values)==sizeof(CustomizationSave::Values),"Owned ABI");
    return CustomizationSave::Edit(Owner,reinterpret_cast<const CustomizationSave::Values*>(values),action);
}
static void Cancel() {
    if(Owner&&!Finished)Host(nullptr,2);
    if(Control&&EditSession.info.token)Control(EditSession.info.token,2,0);
    Finished=true;
}
static void Leave() {
    if(!Screen||Closing)return;
    Closing=true;
    cFEng_QueuePackageMessage(*reinterpret_cast<void**>(0x91CADC),0x587C018B,Package,0);
}
static void Tick() {
    if(Opening&&!Screen&&GetTickCount64()-OpenedAt>30000) {
        Cancel();Opening=false;CustomizationSave::Log("Stance menu open timed out; lease released.");
    }
    if(Screen&&!Closing&&!Current()){Cancel();Leave();}
}
static int SliderIndex(DWORD* slider) {
    for(int i=0;i<7;++i)if(Sliders[i]==slider)return i;
    return -1;
}
static void Text(unsigned hash,const char* text) {
    if(auto object=FEngFindObject(Package,hash);object&&object[0]==0x89B05C){Remember(object);FEPrintf_obj(object,"%s",text);}
}
static void Heading(const char* error=nullptr) {
    const char* modes[]={"Both axles (relative changes)","Front axle","Rear axle"};
    // Avoid changing the shared package title; the native setting label is redrawn on entry.
    Text(0x05CDDED4,error?error:modes[EditSession.axle]);
}
static void __fastcall Draw(DWORD* slider,void*) {
    const int index=SliderIndex(slider);
    if(Screen&&index>=0) {
        Remember(reinterpret_cast<DWORD*>(slider[0x2C/4]));
        Remember(reinterpret_cast<DWORD*>(slider[0x30/4]));
    }
    OriginalDraw(slider);
    if(!Screen||index<0)return;
    auto title=reinterpret_cast<DWORD*>(slider[0x2C/4]);
    auto data=reinterpret_cast<DWORD*>(slider[0x30/4]);
    if(!title||!data||title[0]!=0x89B05C||data[0]!=0x89B05C)return;
    if(index==6) {
        FEPrintf_obj(title,"%s","Enable stance overrides");FEPrintf_obj(data,"%s",EditSession.enabled?"ON":"OFF");return;
    }
    FEPrintf_obj(title,"%s",Labels[index]);
    unsigned axle=EditSession.axle==2?1:0,i=unsigned(index)*2+axle;
    if(!EditSession.axle&&EditSession.Value(index,0)!=EditSession.Value(index,1))
        FEPrintf_obj(data,"%.4g / %.4g",double(EditSession.Value(index,0)),double(EditSession.Value(index,1)));
    else FEPrintf_obj(data,"%.4g%s",double(EditSession.Value(index,axle)),(EditSession.active.mask&(1u<<i))?"":" (inherit)");
}
static void Refresh() {
    for(unsigned i=0;i<7;++i)if(auto slider=Sliders[i]) {
        float minimum=0,maximum=1,step=1,value=EditSession.enabled?1.f:0.f;
        if(i<6) {
            minimum=EditSession.info.minimum[i];maximum=EditSession.info.maximum[i];step=EditSession.info.step[i];
            value=EditSession.Value(i,EditSession.axle==2?1:0);
        }
        // Clamp only the slider's visual cursor; stored absolute values stay untouched until input.
        reinterpret_cast<void(__thiscall*)(DWORD*,float,float,float,float)>(0x575EC0)(slider,minimum,maximum,step,std::clamp(value,minimum,maximum));
        Draw(slider,nullptr);
    }
    Heading();
}
static bool Preview() {
    if(!Current()||!Control(EditSession.info.token,0,EditSession.enabled)||!Host(&EditSession.active,0)) {
        Cancel();Leave();return false;
    }
    return true;
}
static void __fastcall Act(DWORD* slider,void*,const char* package,unsigned message) {
    const int index=SliderIndex(slider);
    if(!Screen||index<0){OriginalAct(slider,package,message);return;}
    if(Closing||!Current()){Cancel();Leave();return;}
    if(message!=Left&&message!=Right)return;
    if(index==6)EditSession.enabled=message==Right;
    else EditSession.Step(unsigned(index),message==Right?1:-1);
    if(Preview())Refresh();
}
static bool Commit() {
    if(!Current())return false;
    if(!EditSession.Dirty()){Cancel();return true;}
    if(!Host(&EditSession.active,1))return false;
    if(!Control(EditSession.info.token,1,EditSession.enabled)) {
        // The INI writer failed: restore the previous staged values and keep previewing.
        Host(&EditSession.before,1);Host(&EditSession.active,0);return false;
    }
    Finished=true;return true;
}
static void __fastcall Notify(DWORD* screen,void*,unsigned message,void* object,unsigned p1,unsigned p2) {
    if(screen!=Screen){OriginalNotify(screen,message,object,p1,p2);return;}
    if(message==ExitComplete) {
        if(!Finished)Cancel();
        auto queue=reinterpret_cast<void(__thiscall*)(void*,const char*,unsigned,unsigned,bool)>(0x525940);
        queue(*reinterpret_cast<void**>(0x91CADC),Parent.empty()?*reinterpret_cast<const char**>(g_pCustomizeSubPkg):Parent.c_str(),1|(Category<<16),0,false);
        return;
    }
    if(Closing)return;
    if(!Current()){Cancel();Leave();return;}
    if(message==Back){Cancel();Leave();return;}
    if(message==Accept) {
        if(Commit())Leave();else Heading("Unable to apply. Check active car/profile and writable settings.");
        return;
    }
    if(message==PreviousAxle||message==NextAxle) {
        EditSession.axle=(EditSession.axle+(message==NextAxle?1:2))%3;Refresh();return;
    }
    if(message==Inherit) {
        auto selected=reinterpret_cast<DWORD*>(screen[0x34/4]);const int index=SliderIndex(selected);
        if(index>=0&&index<6){EditSession.Inherit(unsigned(index));if(Preview())Refresh();}return;
    }
    // UIWidgetMenu also handles text-input widgets and destructive screen resets.
    // TuningSlider has neither layout: forward only verified navigation/draw paths.
    switch(message) {
    case 0x35F8620B: // INITCOMPLETE
    case 0x92B703B5: // REFRESH_WIDGETS
    case 0x72619778: // PAD_UP
    case 0x911C0A4B: // PAD_DOWN
    case Left: case Right:
    case 0x9803F6E2: // native mouse navigation
        reinterpret_cast<NotifyFn>(0x5A96C0)(screen,message,object,p1,p2);
        break;
    default: break;
    }
}
static void __fastcall Setup(DWORD* screen,void*) {
    if(!Constructing){OriginalSetup(screen);return;}
    Screen=screen;Opening=false;Closing=false;Sliders.fill(nullptr);
    CustomizationSave::Log("Stance screen constructing native sliders.");
    *reinterpret_cast<BYTE*>(reinterpret_cast<BYTE*>(screen)+0x20)=1; // MenuScreen::IsGarageScreen (PC ctor 5862C6).
    if(!BuildRequested||!Current()){Cancel();return;}
    auto record=*reinterpret_cast<DWORD**>(_FECarRecord);
    auto stable=CustomizationSave::Stable();
    if(!record||!stable){Cancel();return;}
    auto add=reinterpret_cast<unsigned(__thiscall*)(DWORD*,DWORD*,DWORD*,int,bool)>(0x594090);
    for(int i=0;i<7;++i) {
        add(screen,stable,record,i,false);
        // PC UIWidgetMenu::Options tail is +30; bTNode is embedded at widget+4.
        auto node=reinterpret_cast<DWORD*>(screen[0x30/4]);
        auto slider=node?node-1:nullptr;
        if(!slider||slider[0]!=0x8A1548||!slider[0x2C/4]||!slider[0x30/4]||!slider[0x5C/4]) {
            CustomizationSave::Log("Stance menu template missing required slider objects.");Cancel();return;
        }
        *reinterpret_cast<BYTE*>(reinterpret_cast<BYTE*>(slider)+0xF0)=1;
        Sliders[i]=slider;
    }
    Refresh();reinterpret_cast<void(__thiscall*)(DWORD*,int)>(0x5A5EA0)(screen,1);
    Heading();CustomizationSave::Log("Stance screen sliders ready.");
}
static DWORD* __cdecl Factory(DWORD* data) {
    const bool tagged=data&&((data[2]&0xFFFF)==Category);
    const bool previous=Constructing,previousRequest=BuildRequested;
    BuildRequested=tagged&&Opening&&data[2]==OpeningArgs&&Current();Constructing=tagged;
    DWORD* result=OriginalFactory(data);Constructing=previous;BuildRequested=previousRequest;
    if(tagged&&!result){Cancel();Opening=false;}
    return result;
}
static void __fastcall Destructor(DWORD* screen,void*) {
    if(screen==Screen) {
        if(!Finished)Cancel();RestoreText();Screen=nullptr;Sliders.fill(nullptr);Owner=0;Opening=false;Closing=false;
    }
    OriginalDestructor(screen);
}
static void __fastcall React(DWORD* option,void*,const char* parent,unsigned message,void* object,unsigned p1,unsigned p2) {
    if((option[0x50/4]&0xFFFF)!=Category){OriginalReact(option,parent,message,object,p1,p2);return;}
    if(message!=Pressed||Screen||Opening||!Core()||!Asset())return;
    uint64_t owner=0;CustomizationSave::Values values;
    if(!CustomizationSave::NativeMenuOwner(owner,values))return;
    MenuInfo info;
    if(!Begin(owner,&info))return;
    Values initial;std::memcpy(&initial,&values,sizeof(initial));
    if(!EditSession.Begin(info,initial)){Control(info.token,2,0);return;}
    Owner=owner;Finished=false;Closing=false;Opening=true;OpenedAt=GetTickCount64();
    OpeningArgs=option[0x50/4];Parent=parent?parent:"";
    OriginalReact(option,parent,message,object,p1,p2);
}
static void __cdecl Language(DWORD* object,unsigned hash) {
    OriginalLanguage(object,hash);
    if(object&&hash==NameHash)FEPrintf_obj(object,"%s","Stance adjustment");
}
static DWORD* __cdecl CreateScreen(DWORD* package,unsigned args) {
    CustomizationSave::Log("Independent stance screen factory selected.");
    DWORD data[]={reinterpret_cast<DWORD>(Package),reinterpret_cast<DWORD>(package),args};
    return Factory(data);
}
// Native ScreenFactory takes its hash in EDI and two cdecl stack arguments.
// Dispatch only our reserved package; all stock constructors retain their path.
static void __declspec(naked) Dispatch() {
    __asm {
        cmp edi,PackageHash
        jne stock
        push dword ptr [esp+8]
        push dword ptr [esp+8]
        call CreateScreen
        add esp,8
        ret
    stock:
        jmp dword ptr [OriginalDispatch]
    }
}
void AddPartsOption(DWORD* screen) {
    if(!Installed||!CustomizationSave::HooksReady||!Core()||!Asset())return;
    // The native-save implementation currently supports owned career/My Cars records.
    auto record=*reinterpret_cast<DWORD**>(_FECarRecord);CustomizationSave::Key key;
    {
        std::lock_guard<std::mutex> lock(CustomizationSave::Mutex);
        CustomizationSave::EnsureProfileLocked();
        auto stable=CustomizationSave::Stable();const auto at=reinterpret_cast<uintptr_t>(record),first=reinterpret_cast<uintptr_t>(stable);
        if(!stable||at<first||at>=first+200*20||(at-first)%20||!CustomizationSave::ReadKey(record,key))return;
    }
    // Exactly the PC native CustomizeMainOption construction, without interpreting
    // our extension category as a native unlockable part or charging the cart.
    auto option=reinterpret_cast<DWORD*>(j_malloc(0x58));if(!option)return;
    IconOption_Create(option,bStringHash("VISUAL_PART_BODY"),NameHash,0);
    option[0]=0x8B7558;option[0x4C/4]=reinterpret_cast<DWORD>(Package);
    option[0x50/4]=Category|(screen[0x150/4]<<16);option[0x54/4]=1;
    IconScrollerMenu_AddOption(screen,option);
}
static bool Install() {
    if(Installed)return true;
    struct Hook {uintptr_t address;void* replacement;void** original;std::vector<BYTE> bytes;};
    const std::vector<Hook> hooks={
        {0x5A6060,&Setup,reinterpret_cast<void**>(&OriginalSetup),{0x6A,0xFF,0x68,0x78,0x37,0x87,0}},
        {0x5A9600,&Factory,reinterpret_cast<void**>(&OriginalFactory),{0x6A,0xFF,0x68,0x34,0x3C,0x87,0}},
        {0x5A9910,&Notify,reinterpret_cast<void**>(&OriginalNotify),{0x64,0xA1,0,0,0,0}},
        {0x5A90D0,&Destructor,reinterpret_cast<void**>(&OriginalDestructor),{0x56,0x8B,0xF1,0x8B,0x86,0x28,1,0,0}},
        {0x575CC0,&Draw,reinterpret_cast<void**>(&OriginalDraw),{0x56,0x8B,0xF1,0x8B,0x86,0xD8,0,0,0}},
        {0x575B60,&Act,reinterpret_cast<void**>(&OriginalAct),{0x56,0x8B,0xF1,0x8A,0x86,0xF0,0,0,0}},
        {0x7AA020,&React,reinterpret_cast<void**>(&OriginalReact),{0x81,0x7C,0x24,0x08,0x10,0x72,0x40,0x0C}},
        {0x515C00,&Language,reinterpret_cast<void**>(&OriginalLanguage),{0x56,0x8B,0x74,0x24,0x08,0x85,0xF6}},
        {0x571D60,&Dispatch,&OriginalDispatch,{0x83,0xEC,0x0C,0x56,0x33,0xF6}}};
    for(const auto& hook:hooks)if(std::memcmp(reinterpret_cast<void*>(hook.address),hook.bytes.data(),hook.bytes.size())) {
        CustomizationSave::Log("Native stance menu disabled: executable layout or hook owner differs.");return false;
    }
    // Corroborate the PC object offsets used above, rather than trusting console layouts.
    if(*reinterpret_cast<const DWORD*>(0x7BB608)!=0x8B7558||*reinterpret_cast<const DWORD*>(0x5A9016)!=0x8A2A74)return false;
    if(std::memcmp(reinterpret_cast<void*>(0x585AA0),"\x57\x8B\x7C\x24\x08\x8B\x07",7)||
       std::memcmp(reinterpret_cast<void*>(0x585820),"\x53\x56\x8B\x74\x24\x0C",6))return false;
    const auto status=MH_Initialize();if(status!=MH_OK&&status!=MH_ERROR_ALREADY_INITIALIZED)return false;
    NameHash=bStringHash("MWCL_STANCE_ADJUSTMENT");PackageHash=0xFFFFFFFF;
    for(const unsigned char* c=reinterpret_cast<const unsigned char*>(Package);*c;++c)PackageHash=PackageHash*33+std::toupper(*c);
    size_t created=0;bool ok=true;
    for(const auto& hook:hooks){if(MH_CreateHook(reinterpret_cast<void*>(hook.address),hook.replacement,hook.original)!=MH_OK){ok=false;break;}++created;}
    if(ok)for(const auto& hook:hooks)if(MH_EnableHook(reinterpret_cast<void*>(hook.address))!=MH_OK){ok=false;break;}
    if(!ok){for(size_t i=0;i<created;++i){MH_DisableHook(reinterpret_cast<void*>(hooks[i].address));MH_RemoveHook(reinterpret_cast<void*>(hooks[i].address));}return false;}
    Installed=true;CustomizationCamera::GarageUpdateObserver=&Tick;
    CustomizationSave::Log("Native body-shop stance menu installed (free grouped adjustment).");return true;
}
}
