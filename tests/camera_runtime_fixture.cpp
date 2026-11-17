#include "../CustomizationCamera.h"
static uint32_t ticks=0;
static void __cdecl Tick() {++ticks;}
static BOOL __cdecl Input(BOOL,float* x,float* y,BOOL* reset) {*x=.25f;*y=.5f;*reset=FALSE;return TRUE;}
extern "C" __declspec(dllexport) uint32_t __cdecl FixtureStart() {
    if(!CustomizationControls::Install(true))return 0;
    CustomizationCamera::Install();
    return CustomizationCamera::PublishedRuntime.load()?1u:0u;
}
extern "C" __declspec(dllexport) uint32_t __cdecl FixtureOwners() {
    return (CustomizationControls::Peer?0u:1u)|(CustomizationCamera::Peer?0u:2u);
}
extern "C" __declspec(dllexport) void __cdecl FixtureSet(float speed) {
    std::lock_guard<std::mutex> lock(CustomizationControls::ControlMutex);
    CustomizationControls::ControlSession.active.speed=speed;
}
extern "C" __declspec(dllexport) float __cdecl FixtureGet() {return CustomizationControls::Runtime().speed;}
extern "C" __declspec(dllexport) void __cdecl FixtureBegin() {CustomizationControls::ControlSession.Begin();}
extern "C" __declspec(dllexport) void __cdecl FixtureRestore() {CustomizationControls::ControlSession.Restore();}
extern "C" __declspec(dllexport) void __cdecl FixtureMirror(uint32_t available) {CustomizationControls::SetMirrorAvailable((available&1)!=0,(available&2)!=0);}
extern "C" __declspec(dllexport) uint32_t __cdecl FixtureMirrorState() {
    return (CustomizationControls::MirrorAvailable.load()?1u:0u)|(CustomizationControls::RadarAvailable.load()?2u:0u);
}
extern "C" __declspec(dllexport) uint32_t __cdecl FixtureRegisterTick() {return CustomizationCamera::RegisterGarageObserver(&Tick);}
extern "C" __declspec(dllexport) void __cdecl FixtureTick() {if(auto observer=CustomizationCamera::GarageUpdateObserver.load())observer();}
extern "C" __declspec(dllexport) uint32_t __cdecl FixtureTicks() {return ticks;}
extern "C" __declspec(dllexport) uint32_t __cdecl FixtureInput() {return NFSCustomizationCameraSetInputV1(1,&Input);}
extern "C" __declspec(dllexport) uint32_t __cdecl FixtureInputState() {
    float x=0,y=0;BOOL reset=TRUE;
    auto provider=CustomizationCamera::InputProvider.load();
    return provider&&provider(FALSE,&x,&y,&reset)&&x==.25f&&y==.5f&&!reset;
}
extern "C" __declspec(dllexport) uint32_t __cdecl FixtureGarageIdle() {
    DWORD mover[0x140/4]{},garage[0x90/4]{};
    mover[0]=0x894E90;mover[0x80/4]=2;
    garage[0x4C/4]=reinterpret_cast<DWORD>(mover);
    CustomizationCamera::GarageControlled=false;CustomizationCamera::ControlledGarageMover=nullptr;
    // Reproduces the original trigger without executing native addresses. Any
    // unwanted setter call would hit unmapped native code in this fixture.
    CustomizationCamera::ApplyGarageInput(garage,0,0,false);
    if(mover[0x80/4]!=2||CustomizationCamera::GarageControlled)return 0;
    // Releasing input must also leave a newly started automatic animation alone.
    CustomizationCamera::GarageControlled=true;CustomizationCamera::ControlledGarageMover=mover;
    CustomizationCamera::ApplyGarageInput(garage,0,0,false);
    return mover[0x80/4]==2&&!CustomizationCamera::GarageControlled;
}
