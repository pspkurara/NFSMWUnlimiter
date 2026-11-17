#pragma once
#include "CustomizationMath.h"
#include "CustomizationRaceMath.h"
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
    Matrix heightWorld{},heightDelta{},drawRoot{};
    bool heightChanged{},rootCaptured{},traceRootSeen{};
    unsigned trace=0,traceParts=0;
};
thread_local Context* current=nullptr;
static bool TraceBodyMatrices=false;
// Opt-in, bounded snapshots: render hooks must not produce a log every frame.
struct TraceKey {
    uintptr_t car,view;float front,rear;int reflection;
};
static unsigned TraceCount=0;
static TraceKey TraceKeys[64]{};
static unsigned BeginBodyTrace(DWORD* car,void* view,float front,float rear,int reflection) {
    if(!TraceBodyMatrices||TraceCount==64||(!front&&!rear))return 0;
    const TraceKey key={reinterpret_cast<uintptr_t>(car),reinterpret_cast<uintptr_t>(view),front,rear,reflection};
    for(unsigned i=0;i<TraceCount;++i) {
        const auto& old=TraceKeys[i];
        if(old.car==key.car&&old.view==key.view&&old.front==front&&old.rear==rear&&old.reflection==reflection)return 0;
    }
    TraceKeys[TraceCount]=key;return ++TraceCount;
}
static void TraceMatrix(const Context& context,const char* stage,const Matrix* matrix,const void* part=nullptr) {
    if(!context.trace||!matrix)return;
    char line[768]{};
    const auto& m=matrix->m;
    const double determinant=double(m[0][0])*(double(m[1][1])*m[2][2]-double(m[1][2])*m[2][1])-
        double(m[0][1])*(double(m[1][0])*m[2][2]-double(m[1][2])*m[2][0])+
        double(m[0][2])*(double(m[1][0])*m[2][1]-double(m[1][1])*m[2][0]);
    sprintf_s(line,"BodyTrace %u %s part=%p determinant=%.9g matrix="
        "[%.9g %.9g %.9g %.9g; %.9g %.9g %.9g %.9g; %.9g %.9g %.9g %.9g; %.9g %.9g %.9g %.9g]",
        context.trace,stage,part,determinant,m[0][0],m[0][1],m[0][2],m[0][3],
        m[1][0],m[1][1],m[1][2],m[1][3],m[2][0],m[2][1],m[2][2],m[2][3],m[3][0],m[3][1],m[3][2],m[3][3]);
    Log(line);
}
unsigned PartHash(DWORD* ride,int slot) {
    DWORD* part=RideInfo_GetPart(ride,slot);
    return part?part[0]:0;
}
float Value(const Context& c,int axle,int channel,float original) {
    float value=resolve(c.name,c.body,axle?c.rearWheel:c.frontWheel,axle,channel,original);
    if(enabled&&enabled()) {
        auto ride=reinterpret_cast<DWORD*>(c.car[33]);
        value=CustomizationRandom::Resolve(ride,axle,channel,value);
        value=CustomizationSave::Resolve(ride,axle,channel,value);
    }
    // Defense at the process boundary even when another companion implements the ABI.
    return std::isfinite(value)?value:original;
}
static Context CarContext(DWORD* car) {
    Context context{};context.car=car;
    auto ride=reinterpret_cast<DWORD*>(car[33]);
    context.name=GetCarTypeName(ride[0]);context.body=PartHash(ride,23);
    if(auto part=RideInfo_GetPart(ride,23))context.kit=CarPart_GetAppliedAttributeIParam(part,bStringHash("KITNUMBER"),0);
    context.frontWheel=PartHash(ride,66);context.rearWheel=PartHash(ride,67);
    if(!context.rearWheel)context.rearWheel=context.frontWheel;
    return context;
}
struct TireEffects {DWORD* connection;float skidScale[4]{1,1,1,1};};
static thread_local TireEffects* activeEffects=nullptr;
using UpdateTiresFn=void(__thiscall*)(DWORD*,float,float,const void*);
using SkidsFn=void(__thiscall*)(void*,float,const void*,const void*,const void*,float);
static UpdateTiresFn originalUpdateTires=nullptr;
static SkidsFn originalSkids=nullptr;
static void __fastcall Skids(void* state,void*,float intensity,const void* delta,const void* tire,const void* body,float width) {
    if(activeEffects)for(unsigned i=0;i<4;++i)
        if(reinterpret_cast<void*>(activeEffects->connection[0x74/4+i])==state) {width*=activeEffects->skidScale[i];break;}
    originalSkids(state,intensity,delta,tire,body,width);
}
static void __fastcall UpdateTires(DWORD* connection,void*,float dt,float speed,const void* data) {
    auto car=reinterpret_cast<DWORD*>(connection[0x44/4]);
    if(!Ready()||!enabled()||!car||!car[33]) {originalUpdateTires(connection,dt,speed,data);return;}
    const auto context=CarContext(car);
    // PC layout verified independently: positions +90, radii +D0, TireState* +74.
    float* positions=reinterpret_cast<float*>(connection+0x90/4);
    float* radii=reinterpret_cast<float*>(connection+0xD0/4);
    float snapshot[20];std::memcpy(snapshot,positions,sizeof(snapshot));
    const auto offsets=reinterpret_cast<const float*>(car+68),widths=reinterpret_cast<const float*>(car+72);
    const auto modelRadii=reinterpret_cast<const float*>(car+74),widthScale=reinterpret_cast<const float*>(car+76);
    const auto radiusScale=reinterpret_cast<const float*>(car+80);
    TireEffects effects{connection};
    for(int i=0;i<4;++i) {
        const int axle=i/2;
        const CustomizationRace::Wheel before={offsets[i],widths[axle]*widthScale[i],modelRadii[axle]*radiusScale[i]};
        const CustomizationRace::Wheel after={std::copysign(Value(context,axle,1,std::fabs(before.offset)),before.offset),
            Value(context,axle,3,before.width),Value(context,axle,4,before.radius)};
        CustomizationRace::Adjustment adjustment;
        if(CustomizationRace::Adjust(positions[i*4+1],radii[i],before,after,adjustment)) {
            positions[i*4+1]=adjustment.positionY;radii[i]=adjustment.radius;effects.skidScale[i]=adjustment.skidScale;
        }
    }
    auto previous=activeEffects;activeEffects=&effects;
    // Native UpdateTires creates both wheel matrices and world contact/FX points.
    originalUpdateTires(connection,dt,speed,data);
    activeEffects=previous;std::memcpy(positions,snapshot,sizeof(snapshot));
}
static bool InstallTireEffects() {
    const unsigned char update[]={0x55,0x8B,0xEC,0x83,0xE4,0xF0,0x83,0xEC,0x64};
    const unsigned char skid[]={0x55,0x8B,0xEC,0x83,0xE4,0xF0,0x81,0xEC,0xB4,0,0,0};
    const unsigned char positionRead[]={0x8D,0x87,0x90,0,0,0};
    const unsigned char stateRead[]={0x8B,0x99,0x98,0xFC,0xFF,0xFF};
    if(!Bytes(0x746750,update,sizeof(update))||!Bytes(0x745580,skid,sizeof(skid))||
       !Bytes(0x746986,positionRead,sizeof(positionRead))||!Bytes(0x7469FF,stateRead,sizeof(stateRead)))return false;
    struct Hook {uintptr_t address;void* target;void** original;};
    const Hook hooks[]={{0x746750,&UpdateTires,reinterpret_cast<void**>(&originalUpdateTires)},
        {0x745580,&Skids,reinterpret_cast<void**>(&originalSkids)}};
    size_t created=0;bool ok=true;
    for(const auto& hook:hooks) {if(MH_CreateHook(reinterpret_cast<void*>(hook.address),hook.target,hook.original)!=MH_OK){ok=false;break;}++created;}
    if(ok)for(const auto& hook:hooks)if(MH_EnableHook(reinterpret_cast<void*>(hook.address))!=MH_OK){ok=false;break;}
    if(!ok)for(size_t i=0;i<created;++i){MH_DisableHook(reinterpret_cast<void*>(hooks[i].address));MH_RemoveHook(reinterpret_cast<void*>(hooks[i].address));}
    return ok;
}
using RenderFn=bool(__thiscall*)(DWORD*,void*,const float*,const Matrix*,Matrix*,Matrix*,Matrix*,unsigned,int,int,float,int,int);
RenderFn originalRender=nullptr;
bool __fastcall Render(DWORD* car,void*,void* view,const float* position,const Matrix* body,
    Matrix* tires,Matrix* brakes,Matrix* spinners,unsigned flags,int light,int reflection,float shadow,int tireLod,int carLod) {
    if(!Ready()||!car||!car[33]||!car[34]||!body||!position)
        return originalRender(car,view,position,body,tires,brakes,spinners,flags,light,reflection,shadow,tireLod,carLod);
    Context context=CarContext(car);
    DWORD* ride=reinterpret_cast<DWORD*>(car[33]);
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
    float front=Value(context,0,0,0.f),rear=Value(context,1,0,0.f);
    context.trace=BeginBodyTrace(car,view,front,rear,reflection);
    if(context.trace) {
        char line[384]{};
        sprintf_s(line,"BodyTrace %u car=%p type=%.64s view=%p reflection=%d damage=%p height=[%.9g %.9g] attributes=%p position=[%.9g %.9g %.9g]",
            context.trace,car,context.name?context.name:"?",view,reflection,reinterpret_cast<void*>(car[0x40/4]),
            front,rear,reinterpret_cast<void*>(car[1517]),position[0],position[1],position[2]);
        Log(line);TraceMatrix(context,"native-body",body);
    }
    if(std::fabs(front)<=2.f&&std::fabs(rear)<=2.f) {
        auto attributes=reinterpret_cast<const float*>(car[1517]);
        // PC ecar layout: TireOffsets array is at +0x10, stride 16, x is longitudinal.
        // A missing axle layout still permits uniform translation, never guessed rake.
        context.heightDelta=CustomizationMath::HeightTransform(attributes?attributes[4]:0.f,
            attributes?attributes[12]:0.f,front,rear);
        context.heightChanged=front!=0.f||rear!=0.f;
        TraceMatrix(context,"height-delta",&context.heightDelta);
    }
    Context* previous=current;current=&context;
    const bool result=originalRender(car,view,position,body,tires,brakes,spinners,flags,light,reflection,shadow,tireLod,carLod);
    current=previous;
    std::memcpy(car+68,saved,sizeof(saved));
    return result;
}
using RenderPartFn=void(__thiscall*)(DWORD*,void*,void*,Matrix*,void*,unsigned);
auto originalPart=reinterpret_cast<RenderPartFn>(0x7387e0);
using CopyMatrixFn=Matrix*(__thiscall*)(Matrix*,const Matrix*);
Matrix* __fastcall CaptureBodyRoot(Matrix* destination,void*,const Matrix* source) {
    // 0x74E91D copies biased_local_world before per-part damage/door animation.
    // A BASE/BODY draw can instead contain the damage actor's GetPartMatrix.
    Matrix* result=reinterpret_cast<CopyMatrixFn>(0x445090)(destination,source);
    if(current&&!current->rootCaptured&&source) {
        const bool traceRoot=current->trace&&!current->traceRootSeen;
        if(traceRoot)TraceMatrix(*current,"biased-root",source);
        current->traceRootSeen=true;
        Matrix inverse;
        if(CustomizationMath::AffineInverse(*source,inverse)) {
            // Cache the conjugated transform once per car/view. Every rigid or
            // animated body part then needs only one matrix multiplication.
            current->heightWorld=CustomizationMath::Multiply(CustomizationMath::Multiply(inverse,current->heightDelta),*source);
            current->drawRoot=current->heightChanged
                ?CustomizationMath::Multiply(current->heightDelta,*source):*source;
            current->rootCaptured=true;
            TraceMatrix(*current,"height-world",&current->heightWorld);
        } else {
            // Retain native geometry for singular, non-affine or invalid roots.
            current->heightChanged=false;
            if(traceRoot)Log("BodyTrace rejected root: affine inverse unavailable; native geometry retained.");
        }
    }
    return result;
}
void __fastcall BodyPart(DWORD* car,void*,void* view,void* part,Matrix* matrix,void* lighting,unsigned flags) {
    Matrix* transformed=matrix;
    if(current&&current->car==car&&current->rootCaptured&&current->heightChanged&&matrix) {
        // Native render queues keep the matrix pointer. Allocate from the game's frame pool.
        auto allocate=reinterpret_cast<Matrix*(__cdecl*)(int)>(0x405ed0);
        if(auto copy=allocate(1)) {
            *copy=CustomizationMath::Multiply(*matrix,current->heightWorld);
            transformed=copy;
        }
    }
    if(current&&current->car==car&&current->trace&&current->traceParts<8) {
        // Include the BODY slot even if earlier visible parts exhausted the sample.
        const auto address=reinterpret_cast<uintptr_t>(part),begin=reinterpret_cast<uintptr_t>(car)+0xB84;
        const bool bodySlot=address>=begin&&address-begin>=23*20&&address-begin<24*20;
        if(current->traceParts<4||bodySlot) {
            TraceMatrix(*current,"part-input",matrix,part);TraceMatrix(*current,"part-output",transformed,part);
            ++current->traceParts;
        }
    }
    // Calling the normal entry preserves the existing VRM RenderPart detour.
    originalPart(car,view,part,transformed,lighting,flags);
}
using ViewRenderFn=void(__thiscall*)(void*,void*,Matrix*,void*,unsigned,void*);
void __fastcall DirectBody(void* view,void*,void* model,Matrix* matrix,void* lighting,unsigned flags,void* material) {
    Matrix* transformed=matrix;
    if(current&&current->rootCaptured&&current->heightChanged&&matrix) {
        auto allocate=reinterpret_cast<Matrix*(__cdecl*)(int)>(0x405ed0);
        if(auto copy=allocate(1)) {
            *copy=CustomizationMath::Multiply(*matrix,current->heightWorld);
            transformed=copy;
        }
    }
    reinterpret_cast<ViewRenderFn>(0x6da9b0)(view,model,transformed,lighting,flags,material);
}
template<unsigned Wheel>
void __fastcall WheelPart(DWORD* car,void*,void* view,void* part,Matrix* matrix,void* lighting,unsigned flags) {
    CustomizationClip::Ticket ticket;
    // PC 0x74EA4E reads CarRenderInfo+0x40 as mDamageBehaviour. A fixed plane
    // cannot describe its independently posed or damaged fender panels.
    if(current&&current->car==car&&current->rootCaptured&&!car[0x40/4]) {
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
    char modulePath[MAX_PATH]{};HMODULE selfModule=nullptr;
    if(GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCSTR>(&Install),&selfModule)&&GetModuleFileNameA(selfModule,modulePath,MAX_PATH)) {
        if(char* slash=strrchr(modulePath,'\\')) {
            strcpy_s(slash+1,MAX_PATH-size_t(slash+1-modulePath),"NFSMWUnlimiterSettings.ini");
            TraceBodyMatrices=GetPrivateProfileIntA("Debug","StanceBodyMatrices",0,modulePath)!=0;
        }
    }
    const unsigned char entry[]={0x55,0x8b,0xec,0x83,0xe4,0xf0};
    const unsigned char front[]={0xd9,0x87,0xf0,0,0,0};
    const unsigned char rear[]={0xd9,0x87,0xf8,0,0,0};
    if(GetModuleHandleA(nullptr)!=reinterpret_cast<HMODULE>(0x400000)||
       !Bytes(0x74e160,entry,sizeof(entry))||!Bytes(0x74efcf,front,sizeof(front))||
       !Bytes(0x74efe8,rear,sizeof(rear))||!CallTarget(0x74ec9b,0x7387e0)||
       !CallTarget(0x74e91d,0x445090)||!CallTarget(0x74e9ad,0x6da9b0)||
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
    injector::MakeCALL(0x74e91d,CaptureBodyRoot,true); // Exact pre-animation native root
    injector::MakeCALL(0x74ec9b,BodyPart,true);
    injector::MakeCALL(0x74e9ad,DirectBody,true); // Left/right side mirrors
    injector::MakeCALL(0x74ed6b,DirectBody,true); // License plate
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
    Log(InstallTireEffects()?"Race tire matrices, contact FX and proportional skid width hooks installed.":
        "Race tire effects disabled: unknown layout or competing hooks.");
    Log("Stance bridge installed for PC 1.3, Unlimiter 3.3.1.1338.");return true;
}
} // namespace CustomizationBridge
