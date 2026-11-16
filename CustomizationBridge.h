#pragma once
#include "CustomizationMath.h"
#include "CustomizationClip.h"
#include <MinHook.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <windows.h>

// Optional C ABI. No dependency on the companion's C++ runtime or ASI load order.
namespace CustomizationBridge {
using Matrix=CustomizationMath::Matrix;
using SetContextFn=void(__cdecl*)(const char*,unsigned,unsigned,unsigned,int);
using ResolveFn=float(__cdecl*)(const char*,unsigned,unsigned,int,int,float);
using EnabledFn=int(__cdecl*)();
SetContextFn setContext=nullptr;
ResolveFn resolve=nullptr;
EnabledFn enabled=nullptr;
DWORD nextLookup=0;

void Log(const char* text) {
    OutputDebugStringA(text);
    char root[MAX_PATH]{};
    if(!GetEnvironmentVariableA("LOCALAPPDATA",root,MAX_PATH)) return;
    std::string directory=std::string(root)+"\\NFSMWCustomizationLab";
    CreateDirectoryA(directory.c_str(),nullptr);
    FILE* f=nullptr;
    if(fopen_s(&f,(directory+"\\bridge.log").c_str(),"a")==0&&f) {
        fprintf(f,"%s\n",text);fclose(f);
    }
}
bool Bytes(uintptr_t address,const unsigned char* expected,size_t size) {
    return std::memcmp(reinterpret_cast<void*>(address),expected,size)==0;
}
bool CallTarget(uintptr_t site,uintptr_t target) {
    auto p=reinterpret_cast<const unsigned char*>(site);
    return p[0]==0xe8 && site+5+*reinterpret_cast<const int*>(p+1)==target;
}
bool Ready() {
    if(resolve) return true;
    DWORD now=GetTickCount();
    if(static_cast<LONG>(now-nextLookup)<0) return false;
    nextLookup=now+1000;
    auto module=GetModuleHandleA("NFSMWCustomizationLab.asi");
    if(!module) return false;
    auto version=reinterpret_cast<unsigned(__cdecl*)()>(GetProcAddress(module,"MWCL_GetApiVersion"));
    if(!version||version()!=1) return false;
    auto context=reinterpret_cast<SetContextFn>(GetProcAddress(module,"MWCL_SetAxleContext"));
    auto value=reinterpret_cast<ResolveFn>(GetProcAddress(module,"MWCL_ResolveValueEx"));
    auto active=reinterpret_cast<EnabledFn>(GetProcAddress(module,"MWCL_IsEnabled"));
    if(!context||!value||!active) return false;
    setContext=context;resolve=value;enabled=active;
    Log("Customization companion ABI 1 connected.");
    return true;
}
struct Context {
    DWORD* car{};
    const char* name{};
    unsigned body{},frontWheel{},rearWheel{};
    int kit{};
    Matrix bodyWorld{},heightDelta{},drawRoot{};
    bool heightChanged{},rootCaptured{};
};
thread_local Context* current=nullptr;
unsigned PartHash(DWORD* ride,int slot) {
    DWORD* part=RideInfo_GetPart(ride,slot);
    return part?part[0]:0;
}
float Value(const Context& c,int axle,int channel,float original) {
    float value=resolve(c.name,c.body,axle?c.rearWheel:c.frontWheel,axle,channel,original);
    if(enabled&&enabled()) value=CustomizationSave::Resolve(reinterpret_cast<DWORD*>(c.car[33]),axle,channel,value);
    // Defense at the process boundary even when another companion implements the ABI.
    return std::isfinite(value)?value:original;
}
using RenderFn=bool(__thiscall*)(DWORD*,void*,const float*,const Matrix*,Matrix*,Matrix*,Matrix*,unsigned,int,int,float,int,int);
RenderFn originalRender=nullptr;
bool __fastcall Render(DWORD* car,void*,void* view,const float* position,const Matrix* body,
    Matrix* tires,Matrix* brakes,Matrix* spinners,unsigned flags,int light,int reflection,float shadow,int tireLod,int carLod) {
    if(!Ready()||!car||!car[33]||!car[34]||!body||!position)
        return originalRender(car,view,position,body,tires,brakes,spinners,flags,light,reflection,shadow,tireLod,carLod);
    Context context{};
    context.car=car;
    DWORD* ride=reinterpret_cast<DWORD*>(car[33]);
    context.name=GetCarTypeName(ride[0]);
    context.body=PartHash(ride,23);
    if(auto part=RideInfo_GetPart(ride,23))
        context.kit=CarPart_GetAppliedAttributeIParam(part,bStringHash("KITNUMBER"),0);
    context.frontWheel=PartHash(ride,66);
    context.rearWheel=PartHash(ride,67);
    if(!context.rearWheel) context.rearWheel=context.frontWheel;
    const bool frontend=*reinterpret_cast<int*>(0x925e90)==3;
    if(frontend&&!reflection&&view==reinterpret_cast<void*>(0x919650)) {
        setContext(context.name,context.body,context.frontWheel,context.rearWheel,1);
        CustomizationSave::EditorContext(ride);
    } else if(!frontend) {
        setContext(nullptr,0,0,0,0);
        CustomizationSave::EditorContext(nullptr);
    }

    // Snapshot/restore: render-only values must never accumulate or enter a native save.
    float saved[18];
    std::memcpy(saved,car+68,sizeof(saved));
    float* offsets=reinterpret_cast<float*>(car+68);
    float* widths=reinterpret_cast<float*>(car+72);
    float* radii=reinterpret_cast<float*>(car+74);
    float* widthScale=reinterpret_cast<float*>(car+76);
    float* radiusScale=reinterpret_cast<float*>(car+80);
    float* brakeMarker=reinterpret_cast<float*>(car+84);
    for(int wheel=0;wheel<4;++wheel) {
        const int axle=wheel>1;
        float offset=Value(context,axle,1,std::fabs(offsets[wheel]));
        if(offset>=0.f&&offset<=5.f) offsets[wheel]=std::copysign(offset,offsets[wheel]);
        const float width=Value(context,axle,3,widths[axle]*widthScale[wheel]);
        const float radius=Value(context,axle,4,radii[axle]*radiusScale[wheel]);
        if(width>=.005f&&width<=5.f&&widths[axle]>.005f) widthScale[wheel]=width/widths[axle];
        if(radius>=.005f&&radius<=5.f&&radii[axle]>.005f) radiusScale[wheel]=radius/radii[axle];
    }
    for(int axle=0;axle<2;++axle) {
        float inset=Value(context,axle,5,0.f);
        // Native wheel-local +Y points inward; its brake marker is scaled with wheel width.
        float scale=widthScale[axle?2:0];
        if(std::fabs(inset)<=2.f&&scale>.001f) brakeMarker[axle]+=inset/scale;
    }
    context.bodyWorld=*body;
    for(int j=0;j<3;++j) context.bodyWorld.m[3][j]=position[j];
    float front=Value(context,0,0,0.f),rear=Value(context,1,0,0.f);
    if(std::fabs(front)<=2.f&&std::fabs(rear)<=2.f&&car[1517]) {
        auto attributes=reinterpret_cast<const float*>(car[1517]);
        // PC ecar layout: TireOffsets array is at +0x10, stride 16, x is longitudinal.
        context.heightDelta=CustomizationMath::HeightTransform(attributes[4],attributes[12],front,rear);
        context.heightChanged=front!=0.f||rear!=0.f;
    }
    Context* previous=current;current=&context;
    const bool result=originalRender(car,view,position,body,tires,brakes,spinners,flags,light,reflection,shadow,tireLod,carLod);
    current=previous;
    std::memcpy(car+68,saved,sizeof(saved));
    return result;
}
using RenderPartFn=void(__thiscall*)(DWORD*,void*,void*,Matrix*,void*,unsigned);
auto originalPart=reinterpret_cast<RenderPartFn>(0x7387e0);
void __fastcall BodyPart(DWORD* car,void*,void* view,void* part,Matrix* matrix,void* lighting,unsigned flags) {
    Matrix* transformed=matrix;
    if(current&&current->car==car&&current->heightChanged&&matrix) {
        // Native render queues keep the matrix pointer. Allocate from the game's frame pool.
        auto allocate=reinterpret_cast<Matrix*(__cdecl*)(int)>(0x405ed0);
        if(auto copy=allocate(1)) {
            *copy=CustomizationMath::MoveBodyPart(*matrix,current->bodyWorld,current->heightDelta);
            transformed=copy;
        }
    }
    // BASE and BODY use the unanimated root. Its native scale and frontend bias
    // are already present; wheel clip planes must follow this exact fender root.
    if(current&&current->car==car&&transformed) {
        const uintptr_t relative=reinterpret_cast<uintptr_t>(part)-reinterpret_cast<uintptr_t>(car);
        if((relative>=0xb84&&relative<0xb84+20)||
           (relative>=0xb84+23*20&&relative<0xb84+24*20)) {
            current->drawRoot=*transformed;current->rootCaptured=true;
        }
    }
    // Calling the normal entry preserves the existing VRM RenderPart detour.
    originalPart(car,view,part,transformed,lighting,flags);
}
using ViewRenderFn=void(__thiscall*)(void*,void*,Matrix*,void*,unsigned,void*);
void __fastcall DirectBody(void* view,void*,void* model,Matrix* matrix,void* lighting,unsigned flags,void* material) {
    Matrix* transformed=matrix;
    if(current&&current->heightChanged&&matrix) {
        auto allocate=reinterpret_cast<Matrix*(__cdecl*)(int)>(0x405ed0);
        if(auto copy=allocate(1)) {
            *copy=CustomizationMath::MoveBodyPart(*matrix,current->bodyWorld,current->heightDelta);
            transformed=copy;
        }
    }
    reinterpret_cast<ViewRenderFn>(0x6da9b0)(view,model,transformed,lighting,flags,material);
}
template<unsigned Wheel>
void __fastcall WheelPart(DWORD* car,void*,void* view,void* part,Matrix* matrix,void* lighting,unsigned flags) {
    CustomizationClip::Ticket ticket;
    if(current&&current->car==car&&current->rootCaptured) {
        ticket=CustomizationClip::BeginWheel(current->name,current->kit,Wheel,&current->drawRoot.m[0][0],view);
    }
    originalPart(car,view,part,matrix,lighting,flags);
    CustomizationClip::EndWheel(ticket);
}
float __stdcall Camber(DWORD* car,float original,int rear) {
    if(!current||current->car!=car) return original;
    const float degrees=Value(*current,rear,2,original*7.f);
    return std::fabs(degrees)<=45.f?degrees/7.f:original;
}
void __declspec(naked) FrontCamber() {
    __asm {
        pushad
        push 0
        push dword ptr [edi+0xf0]
        push esi
        call Camber
        popad
        push 0x74efd5
        ret
    }
}
void __declspec(naked) RearCamber() {
    __asm {
        pushad
        push 1
        push dword ptr [edi+0xf8]
        push esi
        call Camber
        popad
        push 0x74efee
        ret
    }
}
bool Install() {
    const unsigned char entry[]={0x55,0x8b,0xec,0x83,0xe4,0xf0};
    const unsigned char front[]={0xd9,0x87,0xf0,0,0,0};
    const unsigned char rear[]={0xd9,0x87,0xf8,0,0,0};
    if(GetModuleHandleA(nullptr)!=reinterpret_cast<HMODULE>(0x400000)||
       !Bytes(0x74e160,entry,sizeof(entry))||!Bytes(0x74efcf,front,sizeof(front))||
       !Bytes(0x74efe8,rear,sizeof(rear))||!CallTarget(0x74ec9b,0x7387e0)||
       !CallTarget(0x74ed6b,0x6da9b0)||!CallTarget(0x74ee81,0x6da9b0)||!CallTarget(0x74ef45,0x6da9b0)) {
        Log("Stance hooks disabled: executable signature or hook ownership differs.");return false;
    }
    if(MH_Initialize()!=MH_OK||MH_CreateHook(reinterpret_cast<void*>(0x74e160),&Render,
        reinterpret_cast<void**>(&originalRender))!=MH_OK) {
        Log("Stance hooks disabled: trampoline creation failed.");return false;
    }
    if(MH_EnableHook(reinterpret_cast<void*>(0x74e160))!=MH_OK) {
        MH_RemoveHook(reinterpret_cast<void*>(0x74e160));
        Log("Stance hooks disabled: trampoline activation failed.");return false;
    }
    injector::MakeCALL(0x74ec9b,BodyPart,true);
    injector::MakeCALL(0x74ed6b,DirectBody,true); // Mirrors and license plate
    injector::MakeCALL(0x74ee81,DirectBody,true); // Spoiler, including marker variant
    injector::MakeCALL(0x74ef45,DirectBody,true); // Roof scoop, including marker variant
    injector::MakeJMP(0x74efcf,FrontCamber,true);
    injector::MakeJMP(0x74efe8,RearCamber,true);
    // Capture only verified wheel submission call sites; preserve RenderPart entry hooks.
    const uintptr_t wheelSites[]={0x74f660,0x74f807,0x74f9f8,0x74fa49,0x74fc53,0x74fca2};
    bool wheelsSafe=true;
    for(auto site:wheelSites) wheelsSafe=wheelsSafe&&CallTarget(site,0x7387e0);
    if(wheelsSafe) {
        injector::MakeCALL(wheelSites[0],&WheelPart<0>,true);
        injector::MakeCALL(wheelSites[1],&WheelPart<1>,true);
        injector::MakeCALL(wheelSites[2],&WheelPart<2>,true);
        injector::MakeCALL(wheelSites[3],&WheelPart<2>,true);
        injector::MakeCALL(wheelSites[4],&WheelPart<3>,true);
        injector::MakeCALL(wheelSites[5],&WheelPart<3>,true);
        HMODULE self=nullptr;char path[MAX_PATH]{};
        if(GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(&Install),&self)&&GetModuleFileNameA(self,path,MAX_PATH)) {
            char* slash=strrchr(path,'\\');
            if(slash) {
                std::string ini(path,slash+1);
                ini+="NFSMWCustomizationClip.ini";
                CustomizationClip::Install(ini.c_str());
                Log(CustomizationClip::Status());
            }
        }
    } else Log("Wheel capture disabled: another mod owns a submission call site.");
    Log("Stance bridge installed for PC 1.3, Unlimiter 3.3.1.1338.");return true;
}
} // namespace CustomizationBridge
