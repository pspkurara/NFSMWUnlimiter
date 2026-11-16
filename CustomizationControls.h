#pragma once
#include "CustomizationControlState.h"
#include "CustomizationProfileStorage.h"
#include "CustomizationProfileHost.h"
#include <MinHook.h>
#include <mutex>
#include <map>
#include <vector>
#include <Xinput.h>
#include <atomic>
#include <sstream>
#include <cwchar>
#include <cstdlib>

// PC 1.3 input extensions. The native profile owns the input configuration;
// switching a profile only switches this state when its native input reader ran.
namespace CustomizationControls {
static std::mutex ControlMutex;
static std::mutex FileMutex;
static Session ControlSession;
static DWORD* Screen=nullptr;
static uint64_t ScreenGeneration=0,DialogGeneration=0;
static bool Installed=false,AllowLiveEditing=false,DialogLive=false;
static unsigned OpenKey=VK_F7;
static std::string DialogProfile;
static thread_local bool WroteInput=false,ReadInput=false;
static thread_local State WrittenState{};
static thread_local bool ResetLoadedController=false;
static thread_local uint32_t WrittenInputCrc=0,ReadInputCrc=0;
static State Saving{};
static bool SaveCaptured=false;
static std::string SavingProfile;
struct Backup {std::string profile;State state;uint32_t inputCrc;};
static std::map<void*,Backup> Backups;
static std::atomic<HWND> Dialog{nullptr};
static HWND Message=nullptr;
static HWND EnabledBox=nullptr,InvertXBox=nullptr,InvertYBox=nullptr,YawBox=nullptr,PitchBox=nullptr;
static HWND ControllerBox=nullptr,ResetBox=nullptr,ModeBox=nullptr,MouseBox=nullptr,MouseSpeedBox=nullptr,MouseZoomBox=nullptr,HeadingBox=nullptr,MirrorBox=nullptr;
static std::array<HWND,5> Keys{},Tuning{};
static DWORD(WINAPI* PadState)(DWORD,XINPUT_STATE*)=nullptr;

static State Runtime() {std::lock_guard<std::mutex> lock(ControlMutex);return ControlSession.active;}
static bool Foreground() {DWORD pid=0;GetWindowThreadProcessId(GetForegroundWindow(),&pid);return pid==GetCurrentProcessId();}
static float Axis(const XINPUT_GAMEPAD& p,unsigned axis) {
    switch(axis){case 1:return p.sThumbLX/32768.f;case 2:return p.sThumbLY/32768.f;
    case 3:return p.sThumbRX/32768.f;case 4:return p.sThumbRY/32768.f;default:return 0;}
}
static bool ReadOrbit(const State& s,float& x,float& y,bool& reset) {
    x=y=0;reset=false;
    if(!Installed||!s.enabled||Dialog||!Foreground())return false;
    XINPUT_STATE pad{};
    const bool connected=PadState&&PadState(s.controller,&pad)==ERROR_SUCCESS;
    static bool wasConnected=false;
    const bool disconnected=wasConnected&&!connected;wasConnected=connected;
    if(disconnected)return false; // Reset accumulated orbit once on device loss.
    if(connected){
        x=Axis(pad.Gamepad,s.yawAxis);y=Axis(pad.Gamepad,s.pitchAxis);
        float length=std::sqrt(x*x+y*y);
        if(length<=s.deadzone)x=y=0;
        else {float gain=std::min(1.f,(length-s.deadzone)/(1-s.deadzone))/length;x*=gain;y*=gain;}
        reset=s.resetButtons&&(pad.Gamepad.wButtons&s.resetButtons)==s.resetButtons;
    }
    if(!s.mouseEnabled&&!connected&&std::all_of(std::begin(s.keys),std::end(s.keys),[](uint32_t k){return k==0;}))return false;
    auto down=[&](unsigned key){return key&&(GetAsyncKeyState(int(key))&0x8000)!=0;};
    x=std::clamp(x+float(down(s.keys[1]))-float(down(s.keys[0])),-1.f,1.f);
    y=std::clamp(y+float(down(s.keys[2]))-float(down(s.keys[3])),-1.f,1.f);
    reset=reset||down(s.keys[4]);if(s.invertX)x=-x;if(s.invertY)y=-y;
    return true;
}
static bool ReadOrbit(float& x,float& y,bool& reset) {return ReadOrbit(Runtime(),x,y,reset);}

struct FileHeader {
    char magic[8]={'M','W','C','L','I','N','1',0};
    uint32_t version=1,size=sizeof(State),crc=0,reserved=0;
    uint8_t digest[32]{};
};
static_assert(sizeof(FileHeader)==56,"Controls file version 1");
static State LoadState(const std::string& name) {
    State state{};
    try {
        auto directory=CustomizationProfileStorage::ProfileDirectory(name);if(directory.empty())return state;
        std::ifstream stream(directory/L"NFSMWCustomizationControls.bin",std::ios::binary);
        FileHeader header{};State candidate{},decoded{};
        stream.read(reinterpret_cast<char*>(&header),sizeof(header));
        if(!stream||std::memcmp(header.magic,"MWCLIN1",8)||header.version!=1||header.reserved||
            (header.size!=sizeof(State)&&header.size!=Version3StateBytes&&header.size!=Version2StateBytes&&header.size!=LegacyStateBytes))return state;
        stream.read(reinterpret_cast<char*>(&candidate),header.size);
        std::array<uint8_t,32> digest{};
        if(stream&&stream.peek()==EOF&&header.crc==CustomizationProfileStorage::Crc(&candidate,header.size)&&
            DecodeState(&candidate,header.size,decoded)&&
            CustomizationProfileStorage::Digest(directory/name,digest)&&!std::memcmp(header.digest,digest.data(),32))state=decoded;
    }catch(...){CustomizationProfileStorage::Log("Input sidecar read failed; safe defaults selected.");}
    return state;
}
static void OnBeginSave(void* buffer,bool native) {
    WroteInput=false;
    std::lock_guard<std::mutex> lock(ControlMutex);
    if(native)SaveCaptured=false;else Backups.erase(buffer);
}
static void OnEndSave(void* buffer,bool native) {
    if(!WroteInput){CustomizationProfileStorage::Log("Input writer not completed in this profile snapshot.");return;}
    auto profile=CustomizationProfileStorage::CurrentProfile();if(!profile)return;
    std::string name=CustomizationProfileStorage::Name(reinterpret_cast<const char*>(profile));
    std::lock_guard<std::mutex> lock(ControlMutex);
    if(native){Saving=WrittenState;SavingProfile=name;SaveCaptured=true;CustomizationProfileStorage::Log("Native disk-save input snapshot captured.");}
    else {if(Backups.size()>8)Backups.clear();Backups[buffer]={name,WrittenState,WrittenInputCrc};}
}
static bool WriteState(const std::string& name,const State& snapshot) {
    if(!Valid(snapshot)||!CustomizationProfileStorage::ValidName(name))return false;
    std::lock_guard<std::mutex> fileLock(FileMutex);
    try {
        auto directory=CustomizationProfileStorage::ProfileDirectory(name);if(directory.empty())return false;
        FileHeader header;std::array<uint8_t,32> digest{};
        if(!CustomizationProfileStorage::Digest(directory/name,digest)){CustomizationProfileStorage::Log("Controls sidecar: completed native file not found at resolved directory.");return false;}
        std::memcpy(header.digest,digest.data(),32);header.crc=CustomizationProfileStorage::Crc(&snapshot,sizeof(snapshot));
        auto target=directory/L"NFSMWCustomizationControls.bin",temp=directory/L"NFSMWCustomizationControls.bin.tmp";
        HANDLE file=CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(file==INVALID_HANDLE_VALUE)return false;
        DWORD bytes=0;
        bool ok=WriteFile(file,&header,sizeof(header),&bytes,nullptr)&&bytes==sizeof(header)&&
            WriteFile(file,&snapshot,sizeof(snapshot),&bytes,nullptr)&&bytes==sizeof(snapshot)&&FlushFileBuffers(file);
        CloseHandle(file);
        std::array<uint8_t,32> currentDigest{};
        ok=ok&&CustomizationProfileStorage::Digest(directory/name,currentDigest)&&currentDigest==digest;
        if(ok&&std::filesystem::exists(target))CopyFileW(target.c_str(),(directory/L"NFSMWCustomizationControls.bin.bak").c_str(),FALSE);
        ok=ok&&MoveFileExW(temp.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);
        if(!ok)DeleteFileW(temp.c_str());
        else CustomizationProfileStorage::Log("Controls sidecar committed beside completed native save.");
        return ok;
    }catch(...){CustomizationProfileStorage::Log("Input sidecar commit failed; native save untouched.");return false;}
}
static void OnSaveDone(const char* profile) {
    State snapshot;std::string name=CustomizationProfileStorage::Name(profile);
    {std::lock_guard<std::mutex> lock(ControlMutex);if(!SaveCaptured||name!=SavingProfile)return;snapshot=Saving;SaveCaptured=false;}
    WriteState(name,snapshot);
}
static std::string CurrentName() {
    auto profile=CustomizationProfileStorage::CurrentProfile();
    return profile&&reinterpret_cast<const BYTE*>(profile)[32]?CustomizationProfileStorage::Name(reinterpret_cast<const char*>(profile)):std::string();
}
static void OnBeginLoad() {ReadInput=false;ResetLoadedController=false;}
static void OnEndLoad(const char* profile,void* buffer,bool native,bool,bool,int) {
    // The native reader changes GLOBAL bindings before LoadFromBuffer checks
    // its commit flag or later car validation. Follow the actual reader, even
    // for a temporary profile load, rather than leaving the two maps divergent.
    if(!ReadInput)return;
    const std::string name=CustomizationProfileStorage::Name(profile);
    State loaded;bool backupFound=false;
    {std::lock_guard<std::mutex> lock(ControlMutex);auto found=Backups.find(buffer);
        if(!native&&found!=Backups.end()&&found->second.profile==name&&found->second.inputCrc==ReadInputCrc){loaded=found->second.state;backupFound=true;}}
    // An unknown/reused temporary buffer is not evidence for the on-disk
    // profile's input ownership. Never borrow its unrelated sidecar.
    if(!backupFound&&native)loaded=LoadState(name);
    if(ResetLoadedController)ResetDevice(loaded,true);
    std::lock_guard<std::mutex> lock(ControlMutex);ControlSession.active=loaded;
    ++ScreenGeneration; // Invalidate any dialog from the previous input owner.
    if(Screen)ControlSession.Begin();
    if(!backupFound)Backups.clear();
}

static const CustomizationProfileHost::Observer ControlsObserver={sizeof(CustomizationProfileHost::Observer),1,0x4354524C,nullptr,
    [](void*,void* buffer,uint32_t native){OnBeginSave(buffer,native!=0);},
    [](void*,void* buffer,uint32_t native){OnEndSave(buffer,native!=0);},
    [](void*,const char* name){OnSaveDone(name);},
    [](void*){OnBeginLoad();},
    [](void*,const char* name,void* buffer,uint32_t native,uint32_t success,uint32_t commit,int32_t player){
        OnEndLoad(name,buffer,native!=0,success!=0,commit!=0,player);
    }};

static std::wstring Text(HWND h){std::wstring s(GetWindowTextLengthW(h)+1,L'\0');GetWindowTextW(h,s.data(),int(s.size()));s.resize(wcslen(s.c_str()));return s;}
static std::wstring Number(float n){wchar_t b[48];swprintf_s(b,L"%.6g",n);return b;}
static HWND Child(const wchar_t* type,const std::wstring& text,DWORD style,int x,int y,int w,int h,int id=0){
    HWND result=CreateWindowExW(0,type,text.c_str(),WS_CHILD|WS_VISIBLE|style,x,y,w,h,Dialog,reinterpret_cast<HMENU>(INT_PTR(id)),GetModuleHandleW(nullptr),nullptr);
    SendMessageW(result,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),TRUE);return result;
}
static void Populate(const State& s){
    SendMessageW(HeadingBox,BM_SETCHECK,s.vehicleHeading?BST_CHECKED:BST_UNCHECKED,0);
    SendMessageW(MirrorBox,BM_SETCHECK,s.hideMirror?BST_CHECKED:BST_UNCHECKED,0);
    SendMessageW(MouseBox,BM_SETCHECK,s.mouseEnabled?BST_CHECKED:BST_UNCHECKED,0);
    SetWindowTextW(MouseSpeedBox,Number(s.mouseSensitivity).c_str());SetWindowTextW(MouseZoomBox,Number(s.mouseZoomStep).c_str());
    SendMessageW(ModeBox,CB_SETCURSEL,s.orbitMode,0);
    SendMessageW(EnabledBox,BM_SETCHECK,s.enabled?BST_CHECKED:BST_UNCHECKED,0);
    SendMessageW(InvertXBox,BM_SETCHECK,s.invertX?BST_CHECKED:BST_UNCHECKED,0);
    SendMessageW(InvertYBox,BM_SETCHECK,s.invertY?BST_CHECKED:BST_UNCHECKED,0);
    SetWindowTextW(ControllerBox,std::to_wstring(s.controller).c_str());SetWindowTextW(ResetBox,std::to_wstring(s.resetButtons).c_str());
    SendMessageW(YawBox,CB_SETCURSEL,s.yawAxis,0);SendMessageW(PitchBox,CB_SETCURSEL,s.pitchAxis,0);
    for(int i=0;i<5;++i)SetWindowTextW(Keys[i],std::to_wstring(s.keys[i]).c_str());
    const float numbers[]={s.deadzone,s.speed,s.pitchLimit,s.returnDelay,s.returnSpeed};
    for(int i=0;i<5;++i)SetWindowTextW(Tuning[i],Number(numbers[i]).c_str());
}
static bool Integer(HWND box,uint32_t& out,bool key=false){
    auto text=Text(box);if(text.empty())return false;
    if(key&&text.size()==1&&((text[0]>=L'A'&&text[0]<=L'Z')||(text[0]>=L'a'&&text[0]<=L'z'))){out=uint32_t(towupper(text[0]));return true;}
    if(text[0]==L'-')return false;wchar_t* end=nullptr;
    bool hex=text.size()>2&&text[0]==L'0'&&(text[1]==L'x'||text[1]==L'X');
    auto result=wcstoul(text.c_str(),&end,hex?16:10);
    if(end==text.c_str()||*end)return false;out=result;return true;
}
static bool ApplyDialog(bool saveNow=false){
    State next;next.orbitMode=uint32_t(SendMessageW(ModeBox,CB_GETCURSEL,0,0));next.enabled=SendMessageW(EnabledBox,BM_GETCHECK,0,0)==BST_CHECKED;
    next.invertX=SendMessageW(InvertXBox,BM_GETCHECK,0,0)==BST_CHECKED;next.invertY=SendMessageW(InvertYBox,BM_GETCHECK,0,0)==BST_CHECKED;
    next.yawAxis=uint32_t(SendMessageW(YawBox,CB_GETCURSEL,0,0));next.pitchAxis=uint32_t(SendMessageW(PitchBox,CB_GETCURSEL,0,0));
    if(!Integer(ControllerBox,next.controller)||!Integer(ResetBox,next.resetButtons))return false;
    for(int i=0;i<5;++i)if(!Integer(Keys[i],next.keys[i],true))return false;
    float* numbers[]={&next.deadzone,&next.speed,&next.pitchLimit,&next.returnDelay,&next.returnSpeed};
    for(int i=0;i<5;++i){auto text=Text(Tuning[i]);wchar_t* end=nullptr;*numbers[i]=wcstof(text.c_str(),&end);if(end==text.c_str()||*end)return false;}
    next.vehicleHeading=SendMessageW(HeadingBox,BM_GETCHECK,0,0)==BST_CHECKED;
    next.hideMirror=SendMessageW(MirrorBox,BM_GETCHECK,0,0)==BST_CHECKED;
    next.mouseEnabled=SendMessageW(MouseBox,BM_GETCHECK,0,0)==BST_CHECKED;
    HWND mouseBoxes[]={MouseSpeedBox,MouseZoomBox};float* mouseValues[]={&next.mouseSensitivity,&next.mouseZoomStep};
    for(int i=0;i<2;++i){auto text=Text(mouseBoxes[i]);wchar_t* end=nullptr;*mouseValues[i]=wcstof(text.c_str(),&end);if(end==text.c_str()||*end)return false;}
    if(!Valid(next))return false;
    std::unique_lock<std::mutex> lock(ControlMutex);
    if(ScreenGeneration!=DialogGeneration||(!Screen&&!DialogLive)||CurrentName()!=DialogProfile)return false;
    if(saveNow) {
        if(!DialogLive)return false;
        auto profile=DialogProfile;
        lock.unlock();
        if(!WriteState(profile,next))return false;
        lock.lock();
        if(ScreenGeneration!=DialogGeneration||CurrentName()!=profile)return false;
    }
    return ControlSession.Apply(next);
}
static LRESULT CALLBACK WindowProc(HWND h,UINT msg,WPARAM wp,LPARAM lp){
    if(msg==WM_COMMAND){switch(LOWORD(wp)){
        case 1:if(ApplyDialog())DestroyWindow(h);else SetWindowTextW(Message,L"Invalid value or native controls screen closed. Check ranges and reopen if needed.");return 0;
        case 4:if(ApplyDialog(true))DestroyWindow(h);else SetWindowTextW(Message,L"Cannot save yet. Save the profile in the game first, then reopen camera settings.");return 0;
        case 2:DestroyWindow(h);return 0;
        case 3:Populate(State{});return 0;
    }}
    if(msg==WM_CLOSE){DestroyWindow(h);return 0;}
    if(msg==WM_DESTROY){Dialog=nullptr;return 0;}
    return DefWindowProcW(h,msg,wp,lp);
}
static void OpenDialog(){
    if(Dialog){SetForegroundWindow(Dialog);return;}
    State snapshot;
    {std::lock_guard<std::mutex> lock(ControlMutex);if(!Screen&&!AllowLiveEditing)return;
        DialogProfile=CurrentName();if(!CustomizationProfileStorage::ValidName(DialogProfile))return;
        DialogLive=!Screen;snapshot=ControlSession.active;DialogGeneration=ScreenGeneration;}
    Dialog=CreateWindowExW(WS_EX_TOOLWINDOW,L"MWCL_CONTROLS",L"Driving camera settings",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,CW_USEDEFAULT,CW_USEDEFAULT,820,690,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    EnabledBox=Child(L"BUTTON",L"Enable race 360 degree camera",BS_AUTOCHECKBOX,16,16,400,25);
    Child(L"STATIC",DialogLive?L"Apply changes now, or use Save and apply to keep them with this profile.":L"Return to the game and confirm your control changes after Apply.",0,16,48,770,25);
    Child(L"STATIC",L"Controller index (0..3)",0,16,85,180,24);ControllerBox=Child(L"EDIT",L"0",WS_BORDER|WS_TABSTOP,210,82,100,25);
    const wchar_t* axes[]={L"Unbound",L"Left stick X",L"Left stick Y",L"Right stick X",L"Right stick Y"};
    Child(L"STATIC",L"Orbit horizontal axis",0,16,123,180,24);YawBox=Child(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP,210,120,180,180);
    Child(L"STATIC",L"Orbit vertical axis",0,16,161,180,24);PitchBox=Child(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP,210,158,180,180);
    for(auto axis:axes){SendMessageW(YawBox,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(axis));SendMessageW(PitchBox,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(axis));}
    Child(L"STATIC",L"Reset pad button mask",0,16,199,180,24);ResetBox=Child(L"EDIT",L"0",WS_BORDER|WS_TABSTOP,210,196,180,25);
    InvertXBox=Child(L"BUTTON",L"Invert horizontal",BS_AUTOCHECKBOX,16,234,185,25);InvertYBox=Child(L"BUTTON",L"Invert vertical",BS_AUTOCHECKBOX,210,234,180,25);
    const wchar_t* keyNames[]={L"Keyboard orbit left",L"Keyboard orbit right",L"Keyboard orbit up",L"Keyboard orbit down",L"Keyboard orbit reset"};
    const wchar_t* tuningNames[]={L"Deadzone (0..0.95)",L"Degrees/sec (1..720)",L"Pitch limit (0..80)",L"Return delay (-1..60)",L"Return speed (1..720)"};
    for(int i=0;i<5;++i){int y=85+i*38;Child(L"STATIC",keyNames[i],0,420,y,200,24);Keys[i]=Child(L"EDIT",L"0",WS_BORDER|WS_TABSTOP,640,y-3,140,25);
        int ty=285+i*33;Child(L"STATIC",tuningNames[i],0,16,ty,220,24);Tuning[i]=Child(L"EDIT",L"",WS_BORDER|WS_TABSTOP,250,ty-3,140,25);}
    MouseBox=Child(L"BUTTON",L"Enable mouse drag and wheel zoom",BS_AUTOCHECKBOX,420,280,360,25);
    Child(L"STATIC",L"Mouse sensitivity (0.01..10)",0,420,315,230,25);MouseSpeedBox=Child(L"EDIT",L"",WS_BORDER|WS_TABSTOP,660,312,120,25);
    Child(L"STATIC",L"Zoom step (0.01..1)",0,420,350,230,25);MouseZoomBox=Child(L"EDIT",L"",WS_BORDER|WS_TABSTOP,660,347,120,25);
    Child(L"STATIC",L"Stick mode",0,420,405,120,25);
    ModeBox=Child(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP,540,402,240,130);
    SendMessageW(ModeBox,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Rate orbit (UG2 style)"));
    SendMessageW(ModeBox,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Direction: release to center"));
    HeadingBox=Child(L"BUTTON",L"Keep orbit relative to vehicle heading",BS_AUTOCHECKBOX,16,458,375,25);
    MirrorBox=Child(L"BUTTON",L"Mirror auto-hide (integration pending)",BS_AUTOCHECKBOX,420,458,375,25);
    EnableWindow(MirrorBox,FALSE);
    Message=Child(L"STATIC",L"Direction mode uses both axes for horizontal direction; native look-back can run alongside it.",0,16,500,770,40);
    if(DialogLive)Child(L"BUTTON",L"Save and apply",WS_TABSTOP,16,595,230,30,4);
    Child(L"BUTTON",L"Apply",WS_TABSTOP,330,595,180,30,1);Child(L"BUTTON",L"Cancel",WS_TABSTOP,520,595,120,30,2);Child(L"BUTTON",L"Defaults",WS_TABSTOP,650,595,120,30,3);
    Populate(snapshot);ShowWindow(Dialog,SW_SHOW);SetForegroundWindow(Dialog);
}
static DWORD WINAPI RunUI(void*){
    WNDCLASSW wc{};wc.lpfnWndProc=WindowProc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"MWCL_CONTROLS";
    wc.hCursor=LoadCursorW(nullptr,MAKEINTRESOURCEW(32512));wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_BTNFACE+1);RegisterClassW(&wc);
    bool previous=false;
    for(;;){MSG msg;while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE))if(!Dialog||!IsDialogMessageW(Dialog,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}
        bool key=(GetAsyncKeyState(OpenKey)&0x8000)!=0;if(key&&!previous&&Foreground())OpenDialog();previous=key;Sleep(20);}
}

using StreamFn=char*(__cdecl*)(void*,void*);
using ConstructorFn=DWORD*(__thiscall*)(DWORD*,void*);
using DestructorFn=void*(__thiscall*)(DWORD*,unsigned);
using CompareFn=bool(__thiscall*)(DWORD*);
using ScreenFn=void(__thiscall*)(DWORD*);
static StreamFn OriginalWriter=nullptr,OriginalReader=nullptr;
static ConstructorFn OriginalConstructor=nullptr;static DestructorFn OriginalDestructor=nullptr;
static CompareFn OriginalCompare=nullptr;static ScreenFn OriginalRestore=nullptr,OriginalDefaults=nullptr;
static constexpr size_t NativeInputBytes=4+19*48;
static bool CompleteStream(void* begin,void* end,char* result) {
    return uintptr_t(end)>=uintptr_t(begin)&&uintptr_t(end)-uintptr_t(begin)>=NativeInputBytes&&
        uintptr_t(result)==uintptr_t(begin)+NativeInputBytes;
}
static char* __cdecl Writer(void* begin,void* end){
    WrittenState=Runtime();char* p=OriginalWriter(begin,end);
    WroteInput=CompleteStream(begin,end,p);
    if(WroteInput)WrittenInputCrc=CustomizationProfileStorage::Crc(begin,NativeInputBytes);
    return p;
}
static char* __cdecl Reader(void* begin,void* end){
    uint32_t savedDevice=0;
    if(uintptr_t(end)>=uintptr_t(begin)&&uintptr_t(end)-uintptr_t(begin)>=4)std::memcpy(&savedDevice,begin,4);
    char* p=OriginalReader(begin,end);ReadInput=CompleteStream(begin,end,p);
    if(ReadInput) {
        ReadInputCrc=CustomizationProfileStorage::Crc(begin,NativeInputBytes);
        auto manager=reinterpret_cast<DWORD*(__cdecl*)()>(0x634370)();
        auto currentDevice=reinterpret_cast<uint32_t(__thiscall*)(DWORD*,int)>(0x634DA0)(reinterpret_cast<DWORD*>(manager[2]),3);
        ResetLoadedController=currentDevice&&currentDevice!=savedDevice;
    }
    return p;
}
static DWORD* __fastcall Constructor(DWORD* screen,void*,void* data){auto result=OriginalConstructor(screen,data);std::lock_guard<std::mutex> lock(ControlMutex);Screen=result;++ScreenGeneration;ControlSession.Begin();return result;}
static void* __fastcall Destructor(DWORD* screen,void*,unsigned flags){{std::lock_guard<std::mutex> lock(ControlMutex);if(Screen==screen){Screen=nullptr;++ScreenGeneration;ControlSession.End();}}return OriginalDestructor(screen,flags);}
static bool __fastcall Compare(DWORD* screen,void*){bool native=OriginalCompare(screen);std::lock_guard<std::mutex> lock(ControlMutex);return native&&!(screen==Screen&&ControlSession.Dirty());}
static void __fastcall Restore(DWORD* screen,void*){OriginalRestore(screen);std::lock_guard<std::mutex> lock(ControlMutex);if(screen==Screen)ControlSession.Restore();}
static void __fastcall Defaults(DWORD* screen,void*){
    auto manager=reinterpret_cast<DWORD*(__cdecl*)()>(0x634370)();
    auto input=reinterpret_cast<DWORD*>(manager[2]);
    const bool controller=screen[0xE4/4]==3&&reinterpret_cast<bool(__thiscall*)(DWORD*)>(0x628C50)(input);
    OriginalDefaults(screen);std::lock_guard<std::mutex> lock(ControlMutex);
    if(screen==Screen)ResetDevice(ControlSession.active,controller);
}
static bool Install(bool nativeLifecycleReady){
    if(Installed)return true;
    if(!nativeLifecycleReady)return false;
    char path[MAX_PATH]{};HMODULE module=nullptr;
    if(GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCSTR>(&Install),&module)&&GetModuleFileNameA(module,path,MAX_PATH)) {
        if(char* slash=std::strrchr(path,'\\')) {
            strcpy_s(slash+1,MAX_PATH-size_t(slash+1-path),"NFSMWCustomizationCamera.ini");
            AllowLiveEditing=GetPrivateProfileIntA("ControlsEditor","AllowOutsideControls",0,path)!=0;
            unsigned key=GetPrivateProfileIntA("ControlsEditor","Hotkey",VK_F7,path);
            if(key>=1&&key<=255)OpenKey=key;
        }
    }
    struct Hook{uintptr_t at;void* replacement;void** original;std::vector<unsigned char> signature;};
    std::vector<Hook> hooks={
        {0x56E180,&Writer,reinterpret_cast<void**>(&OriginalWriter),{0x53,0x55,0x56,0x57,0xE8,0xE7,0x61,0x0C,0}},
        {0x56E200,&Reader,reinterpret_cast<void**>(&OriginalReader),{0x51,0x55,0x56,0xE8,0x68,0x61,0x0C,0}},
        {0x545EB0,&Constructor,reinterpret_cast<void**>(&OriginalConstructor),{0x6A,0xFF,0x68,0x98,0xFF,0x86,0}},
        {0x545F50,&Destructor,reinterpret_cast<void**>(&OriginalDestructor),{0x56,0x8B,0xF1,0xE8,0x28,0x48,0xFE,0xFF}},
        {0x51D4B0,&Compare,reinterpret_cast<void**>(&OriginalCompare),{0x83,0xEC,0x0C,0x53,0x55,0x56,0x8B,0xE9}},
        {0x51D590,&Restore,reinterpret_cast<void**>(&OriginalRestore),{0x83,0xEC,8,0x53,0x55,0x8B,0xE9}},
        {0x51D6E0,&Defaults,reinterpret_cast<void**>(&OriginalDefaults),{0x56,0x57,0x8B,0xF9,0xE8,0x87,0x6C,0x11,0}}
    };
    for(const auto& hook:hooks)if(std::memcmp(reinterpret_cast<void*>(hook.at),hook.signature.data(),hook.signature.size())){CustomizationProfileStorage::Log("Controls extension disabled: executable/hook signature mismatch.");return false;}
    auto initialized=MH_Initialize();if(initialized!=MH_OK&&initialized!=MH_ERROR_ALREADY_INITIALIZED)return false;
    size_t created=0;for(auto& hook:hooks){if(MH_CreateHook(reinterpret_cast<void*>(hook.at),hook.replacement,hook.original)!=MH_OK)break;++created;}
    if(created==hooks.size()){
        bool ok=true;for(auto& hook:hooks)if(MH_EnableHook(reinterpret_cast<void*>(hook.at))!=MH_OK){ok=false;break;}
        if(ok){
            HMODULE xinput=LoadLibraryExW(L"xinput1_4.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);if(!xinput)xinput=LoadLibraryExW(L"xinput9_1_0.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
            if(xinput)PadState=reinterpret_cast<decltype(PadState)>(GetProcAddress(xinput,"XInputGetState"));
            if(CustomizationProfileHost::Register(ControlsObserver)) {
                Installed=true;if(auto thread=CreateThread(nullptr,0,RunUI,nullptr,0,nullptr))CloseHandle(thread);
                CustomizationProfileStorage::Log("Native controls lifecycle extension registered with profile host.");return true;
            }
        }
    }
    for(size_t i=0;i<created;++i){MH_DisableHook(reinterpret_cast<void*>(hooks[i].at));MH_RemoveHook(reinterpret_cast<void*>(hooks[i].at));}return false;
}
}
