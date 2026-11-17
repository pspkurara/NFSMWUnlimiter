#pragma once

#include <windows.h>
#include <Xinput.h>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include "includes\injector\injector.hpp"
#include "CustomizationControls.h"
#include "CustomizationOrbitMath.h"
#include "CustomizationMouseOrbit.h"

// Optional, independently guarded hooks for the PC 1.3 executable. No preview
// state is written to RideInfo, the profile, or a game save.
namespace CustomizationCamera
{
    struct Options
    {
        bool garage = true;
        bool partsOnly = false;
        bool inheritInversion = true, inheritDeadzone = true;
        bool driverPreview = true;
        bool resetOnLookBack = true;
        bool resetOnViewChange = true;
        unsigned fixedEyePovMask = 3;
        bool leftStick = false;
        bool invertX = false;
        bool invertY = false;
        DWORD controller = 0;
        float deadzone = 0.22f;
        float garageSpeed = 1.0f;
    };

    static Options Config;
    using GarageObserver=void(__cdecl*)();
    static std::atomic<GarageObserver> GarageUpdateObserver{nullptr};
    static DWORD* PartsScreen = nullptr;
    static bool DriverPreviewActive = false;
    static bool GarageControlled = false;
    static void* ControlledGarageMover=nullptr;
    static bool GarageHadFocus=true;
    static bool Installed = false;
    static bool NativeLookBackVerified = false;
    static bool NativeViewVerified = false;
    static bool NativeMouseVerified = false;
    static bool NativeHeadingVerified = false;
    static DWORD(__stdcall* GetPadState)(DWORD, XINPUT_STATE*) = nullptr;

    // A separate ASI may supply normalized, already mapped stick input through
    // this exported versioned API. Passing nullptr restores the XInput backend.
    typedef BOOL(__cdecl* InputProviderV1)(BOOL leftStick, float* x, float* y, BOOL* reset);
    static std::atomic<InputProviderV1> InputProvider{nullptr};
    struct RuntimeApi {
        uint32_t size=sizeof(RuntimeApi),version=1;
        uint32_t(__cdecl* garageObserver)(void(__cdecl*)())=nullptr;
        uint32_t(__cdecl* inputProvider)(InputProviderV1)=nullptr;
    };
    static_assert(sizeof(RuntimeApi)==16,"Camera runtime ABI 1");
    static const RuntimeApi* Peer=nullptr;
    static std::atomic<const RuntimeApi*> PublishedRuntime{nullptr};
    static uint32_t __cdecl RegisterGarageObserver(void(__cdecl* callback)()) {
        if(Peer)return Peer->garageObserver(callback);
        if(!callback||!CustomizationRuntimePeer::Pin(reinterpret_cast<const void*>(callback)))return 0;
        GarageObserver expected=nullptr;
        return GarageUpdateObserver.compare_exchange_strong(expected,callback)||expected==callback?1u:0u;
    }
    static uint32_t __cdecl SetInputProvider(InputProviderV1 callback) {
        if(Peer)return Peer->inputProvider(callback);
        if(callback&&!CustomizationRuntimePeer::Pin(reinterpret_cast<const void*>(callback)))return 0;
        InputProvider=callback;return 1;
    }
    static const RuntimeApi LocalRuntime={sizeof(RuntimeApi),1,&RegisterGarageObserver,&SetInputProvider};
    static bool PublishRuntime() {
        if(!CustomizationRuntimePeer::Pin(reinterpret_cast<const void*>(&RegisterGarageObserver)))return false;
        PublishedRuntime.store(&LocalRuntime,std::memory_order_release);return true;
    }

    class Settings
    {
        char path[MAX_PATH] = {};
    public:
        Settings()
        {
            HMODULE owner = nullptr;
            GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                              reinterpret_cast<LPCSTR>(&Config), &owner);
            GetModuleFileNameA(owner, path, MAX_PATH);
            char* file = std::strrchr(path, '\\');
            if (file) strcpy_s(file + 1, MAX_PATH - size_t(file + 1 - path), "NFSMWCustomizationCamera.ini");
            else strcpy_s(path, "NFSMWCustomizationCamera.ini");
        }
        int ReadInteger(const char* section, const char* key, int fallback)
        {
            return int(GetPrivateProfileIntA(section, key, fallback, path));
        }
        float ReadFloat(const char* section, const char* key, float fallback)
        {
            char text[64] = {};
            GetPrivateProfileStringA(section, key, "", text, sizeof(text), path);
            char* end = nullptr;
            float value = std::strtof(text, &end);
            return end != text ? value : fallback;
        }
    };

    static float Clamp(float value, float low, float high)
    {
        return value < low ? low : value > high ? high : value;
    }

    static float Number(Settings& ini, const char* section, const char* key,
                        float fallback, float low, float high)
    {
        float value = ini.ReadFloat(section, key, fallback);
        return std::isfinite(value) ? Clamp(value, low, high) : fallback;
    }

    static bool Matches(DWORD address, const unsigned char* bytes, size_t count)
    {
        MEMORY_BASIC_INFORMATION memory = {};
        if (!VirtualQuery(reinterpret_cast<void*>(address), &memory, sizeof(memory)) ||
            memory.State != MEM_COMMIT || (memory.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
            return false;
        return std::memcmp(reinterpret_cast<void*>(address), bytes, count) == 0;
    }

    template<size_t N>
    static bool Matches(DWORD address, const unsigned char (&bytes)[N])
    {
        return Matches(address, bytes, N);
    }

    static bool PointerMatches(DWORD address, DWORD expected)
    {
        return Matches(address, reinterpret_cast<const unsigned char*>(&expected), sizeof(expected));
    }

    static void Report(const char* message)
    {
        OutputDebugStringA("[NFSMW CustomizationCamera] ");
        OutputDebugStringA(message);
        OutputDebugStringA("\n");
    }

    static bool Foreground()
    {
        DWORD process = 0;
        GetWindowThreadProcessId(GetForegroundWindow(), &process);
        return process == GetCurrentProcessId();
    }

    static bool ReadStick(float& x, float& y, bool& reset)
    {
        x = y = 0;
        reset = false;
        if (!Foreground()) return false;
        const auto tuning=CustomizationControls::GarageTuning(CustomizationControls::Runtime(),
            Config.inheritInversion,Config.inheritDeadzone,{Config.deadzone,Config.invertX,Config.invertY});
        const auto provider=InputProvider.load();
        if (provider)
        {
            BOOL requestReset = FALSE;
            if (!provider(Config.leftStick, &x, &y, &requestReset) ||
                !std::isfinite(x) || !std::isfinite(y)) return false;
            x = Clamp(x, -1, 1);
            y = Clamp(y, -1, 1);
            reset = requestReset != FALSE;
        }
        else
        {
            XINPUT_STATE pad = {};
            if (!GetPadState || GetPadState(Config.controller, &pad) != ERROR_SUCCESS) return false;
            x = (Config.leftStick ? pad.Gamepad.sThumbLX : pad.Gamepad.sThumbRX) / 32768.0f;
            y = (Config.leftStick ? pad.Gamepad.sThumbLY : pad.Gamepad.sThumbRY) / 32768.0f;
            reset = (pad.Gamepad.wButtons & (Config.leftStick ? XINPUT_GAMEPAD_LEFT_THUMB : XINPUT_GAMEPAD_RIGHT_THUMB)) != 0;
            // Radial deadzone preserves diagonals and rescales the live range.
            float length = std::sqrt(x * x + y * y);
            if (length <= tuning.deadzone) x = y = 0;
            else
            {
                float scale = Clamp((length - tuning.deadzone) / (1 - tuning.deadzone), 0, 1) / length;
                x *= scale;
                y *= scale;
            }
        }
        if (tuning.invertX) x = -x;
        if (tuning.invertY) y = -y;
        return true;
    }

    static void __fastcall SetupParts(DWORD* screen, void*)
    {
        reinterpret_cast<void(__thiscall*)(DWORD*)>(0x7BD060)(screen);
        PartsScreen = screen;
        DriverPreviewActive = Config.driverPreview && screen[0x148 / 4] == 0x308;
    }

    static void* __fastcall DeleteParts(DWORD* screen, void*, unsigned int flags)
    {
        if (PartsScreen == screen)
        {
            PartsScreen = nullptr;
            DriverPreviewActive = false;
        }
        return reinterpret_cast<void*(__thiscall*)(DWORD*, unsigned int)>(0x7C01D0)(screen, flags);
    }

    static void* GarageMover(DWORD* garage)
    {
        DWORD* mover = reinterpret_cast<DWORD*>(garage[0x4C / 4]);
        // The PC constructor at 0x47687F assigns this vtable. Do not apply the
        // stock SelectCar layout to a camera class supplied by another mod.
        return mover && mover[0] == 0x894E90 ? mover : nullptr;
    }

    static void SetGarageSpeeds(DWORD* garage, float x, float y)
    {
        // Native setters convert automatic modes to manual even for zero speed.
        if(x==0&&y==0)return;
        void* mover = GarageMover(garage);
        if (!mover) return;
        // PC layout verified in SetV/HRotateSpeed and SelectCarCameraMover::Update.
        // ControlMode 0 disables manual rotation; changing it on actual input
        // keeps stock part-camera animations until the user moves the stick.
        int& mode = *reinterpret_cast<int*>(reinterpret_cast<BYTE*>(mover) + 0x80);
        if ((x != 0 || y != 0) && mode == 0) mode = 1;
        reinterpret_cast<void(__thiscall*)(void*, float)>(0x476E70)(mover, -y * Config.garageSpeed);
        reinterpret_cast<void(__thiscall*)(void*, float)>(0x476F30)(mover, x * Config.garageSpeed);
        garage[0x7C / 4] = 0;
        garage[0x80 / 4] = 0;
    }

    static void StopGarageSpeeds(DWORD* garage) {
        auto mover=GarageMover(garage);
        if(GarageControlled&&mover&&mover==ControlledGarageMover&&
            *reinterpret_cast<int*>(static_cast<BYTE*>(mover)+0x80)==1) {
            reinterpret_cast<void(__thiscall*)(void*,float)>(0x476E70)(mover,0);
            reinterpret_cast<void(__thiscall*)(void*,float)>(0x476F30)(mover,0);
        }
        GarageControlled=false;ControlledGarageMover=nullptr;
    }
    static void ApplyGarageInput(DWORD* garage,float x,float y,bool nativeMouse) {
        if(x==0&&y==0) {
            if(nativeMouse){GarageControlled=false;ControlledGarageMover=nullptr;}
            else StopGarageSpeeds(garage);
            return;
        }
        SetGarageSpeeds(garage,x,y);
        GarageControlled=true;ControlledGarageMover=GarageMover(garage);
    }

    static void __fastcall GarageJoyEvents(DWORD* garage, void*)
    {
        if(auto observer=GarageUpdateObserver.load())observer();
        DriverPreviewActive = Config.driverPreview && PartsScreen && garage[0x88 / 4] == 0x308;
        float x = 0, y = 0;
        bool reset = false;
        const bool nativePermission=*(reinterpret_cast<const BYTE*>(garage)+0x78)||*reinterpret_cast<const BYTE*>(0x926134);
        const bool foreground=Foreground();
        const bool inputReady=foreground&&GarageHadFocus;
        GarageHadFocus=foreground;
        const bool nativeMouse=inputReady&&NativeMouseVerified&&nativePermission&&*reinterpret_cast<const BYTE*>(0x987F70);
        const bool screenAllows=PartsScreen?PartsScreen[0x148/4]!=0x307:
            !Config.partsOnly&&(*(reinterpret_cast<const BYTE*>(garage)+0x78)||*reinterpret_cast<const BYTE*>(0x926134));
        const bool eligible=Config.garage&&screenAllows&&GarageMover(garage);
        if (!eligible)
        {
            if(!nativeMouse)StopGarageSpeeds(garage);
            else {GarageControlled=false;ControlledGarageMover=nullptr;}
            reinterpret_cast<void(__thiscall*)(DWORD*)>(0x7A2310)(garage);
            return;
        }

        // Let the original drain its event queues, with its competing camera
        // branch disabled only for this synchronous call. Menu input is untouched.
        BYTE& userRotate = *(reinterpret_cast<BYTE*>(garage) + 0x78);
        BYTE& showcase = *reinterpret_cast<BYTE*>(0x926134);
        BYTE savedRotate = userRotate, savedShowcase = showcase;
        userRotate = showcase = 0;
        reinterpret_cast<void(__thiscall*)(DWORD*)>(0x7A2310)(garage);
        userRotate = savedRotate;
        showcase = savedShowcase;
        if(!inputReady) {
            // Native mouse handling precedes this call. Drop residual manual
            // velocity during focus loss and the first returning frame, even
            // after mouse ownership cleared our stick lease. Do not create a
            // manual mode or change native automatic/cinematic modes.
            auto mover=GarageMover(garage);
            if(mover&&*reinterpret_cast<int*>(static_cast<BYTE*>(mover)+0x80)==1) {
                reinterpret_cast<void(__thiscall*)(void*,float)>(0x476E70)(mover,0);
                reinterpret_cast<void(__thiscall*)(void*,float)>(0x476F30)(mover,0);
            }
            GarageControlled=false;ControlledGarageMover=nullptr;return;
        }
        if(!ReadStick(x,y,reset)) {
            if(!nativeMouse)StopGarageSpeeds(garage);
            else {GarageControlled=false;ControlledGarageMover=nullptr;}
            return;
        }
        // Native mouse handling runs before this joy call and already set speeds.
        // An idle connected pad must not replace those speeds with zero.
        ApplyGarageInput(garage,x,y,nativeMouse);
    }

    static void __declspec(naked) DriverVisibilityCave()
    {
        __asm
        {
            mov ecx, dword ptr ds : [0x925E90]
            cmp ecx, 3
            jne original
            cmp byte ptr [DriverPreviewActive], 0
            je original
            and eax, 1
            // Preview the enabled selected part even when the separate NIS
            // driver visibility flag is off. No global flag is modified.
            push 0x74EA1E
            ret
        original:
            push 0x74EC55
            ret
        }
    }

    struct Vec { float x, y, z, pad; };
    struct Matrix { float m[16]; };
    static float Yaw = 0, Pitch = 0;
    static ULONGLONG LastOrbitTick = 0, LastOrbitInput = 0;
    static void* LastMover = nullptr;
    static bool WaitForNeutral = false;
    struct ViewStamp { uintptr_t anchor=0; int desired=0,current=0; BYTE lag=0; };
    static ViewStamp LastView;
    static CustomizationControls::MouseOrbit Mouse;
    static CustomizationControls::MirrorPose Mirror;
    static CustomizationOrbit::VehicleOrbit VehicleOrbit;
    static bool NativeMirrorVisible(int viewId) {
        return reinterpret_cast<bool(__cdecl*)(int)>(0x595D40)(viewId);
    }
    static bool __cdecl MirrorVisible(int viewId) {
        const auto controls=CustomizationControls::Runtime();
        if(Mirror.Hide(controls,viewId,*reinterpret_cast<const int*>(0x925E90)==6,
            CustomizationControls::Foreground(),GetTickCount64()))return false;
        return NativeMirrorVisible(viewId);
    }
    static void(__thiscall* OriginalRadarUpdate)(void*,void*)=nullptr;
    static void __fastcall RadarUpdate(void* radar,void*,void* player) {
        OriginalRadarUpdate(radar,player); // Detection, sound and LED logic remain native.
        if(!Mirror.HideRadar(CustomizationControls::Runtime(),NativeMirrorVisible(1),
            *reinterpret_cast<const int*>(0x925E90)==6,CustomizationControls::Foreground(),GetTickCount64()))return;
        // Use native scripts, without keeping FE-object pointers or visibility
        // flags across HUD lifetimes. Native Update restores its own scripts on release.
        for(unsigned offset:{0x38u,0x4Cu,0x50u}) {
            void* object=*reinterpret_cast<void**>(static_cast<BYTE*>(radar)+offset);
            if(!reinterpret_cast<bool(__cdecl*)(void*,uint32_t)>(0x514DA0)(object,0x0016A259))
                reinterpret_cast<void(__cdecl*)(void*,uint32_t,bool)>(0x514D10)(object,0x0016A259,true);
        }
    }
    static bool InstallRadarHook() {
        const unsigned char update[]={0x83,0xEC,0x1C,0x56,0x8B,0xF1,0xE8,0xB5,0x93,0x15,0};
        const unsigned char group[]={0x89,0x46,0x38};
        const unsigned char backing[]={0x89,0x6E,0x4C};
        const unsigned char mirrorBacking[]={0x89,0x6E,0x50};
        const unsigned char script[]={0x57,0x8B,0x7C,0x24,8,0x85,0xFF,0x74,0x2D};
        const unsigned char isScript[]={0x8B,0x44,0x24,4,0x85,0xC0,0x74,0x13};
        if(!Matches(0x566170,update)||!Matches(0x59CCE2,group)||
            !Matches(0x59CD60,backing)||!Matches(0x59CD91,mirrorBacking)||
            !Matches(0x514D10,script)||!Matches(0x514DA0,isScript))return false;
        auto status=MH_Initialize();if(status!=MH_OK&&status!=MH_ERROR_ALREADY_INITIALIZED)return false;
        if(MH_CreateHook(reinterpret_cast<void*>(0x566170),RadarUpdate,
            reinterpret_cast<void**>(&OriginalRadarUpdate))!=MH_OK)return false;
        if(MH_EnableHook(reinterpret_cast<void*>(0x566170))==MH_OK)return true;
        MH_RemoveHook(reinterpret_cast<void*>(0x566170));OriginalRadarUpdate=nullptr;return false;
    }
    static CustomizationControls::MouseSample MouseInput() {
        CustomizationControls::MouseSample sample;
        if(NativeMouseVerified) {
            sample.frame=*reinterpret_cast<const uint32_t*>(0x982B78);
            sample.held=*reinterpret_cast<const BYTE*>(0x987F70)!=0;
            sample.dx=*reinterpret_cast<const int*>(0x987F64);
            sample.dy=*reinterpret_cast<const int*>(0x987F68);
            sample.wheel=*reinterpret_cast<const int*>(0x987F6C);
        }
        return sample;
    }
    static bool SameView(const ViewStamp& a,const ViewStamp& b) {
        return a.anchor==b.anchor&&a.desired==b.desired&&a.current==b.current&&a.lag==b.lag;
    }

    static float ApproachZero(float value, float step)
    {
        return value > 0 ? (value > step ? value - step : 0) : (value < -step ? value + step : 0);
    }

    static void __cdecl OrbitLookAt(void* mover, Matrix* matrix, const Vec* eye, const Vec* look, const Vec* up)
    {
        const auto original = reinterpret_cast<void(__cdecl*)(Matrix*, const Vec*, const Vec*, const Vec*)>(0x6CF0A0);
        ULONGLONG now = GetTickCount64();
        // Main player view only; mirror, split screen and other movers retain stock behavior.
        bool mainView = *reinterpret_cast<int*>(static_cast<BYTE*>(mover) + 0x10) == 1;
        if(mainView)Mirror.Clear();
        float x = 0, y = 0;
        bool reset = false;
        const auto controls = CustomizationControls::Runtime();
        static CustomizationControls::State lastControls;
        if(!CustomizationControls::Equal(lastControls,controls)) {
            lastControls=controls;LastOrbitTick=0;Yaw=Pitch=0;WaitForNeutral=false;Mouse.Reset();VehicleOrbit.Clear();
        }
        if (!controls.enabled || *reinterpret_cast<int*>(0x925E90) != 6 || !mainView ||
            (NativeMouseVerified && reinterpret_cast<bool(__cdecl*)()>(0x468390)()) ||
            !CustomizationControls::ReadOrbit(controls, x, y, reset))
        {
            Yaw = Pitch = 0;
            LastOrbitTick = 0;
            WaitForNeutral = false;
            Mouse.Reset();
            VehicleOrbit.Clear();
            original(matrix, eye, look, up);
            return;
        }
        const bool directionMode=controls.orbitMode==1;
        ViewStamp view;
        if(NativeViewVerified) {
            const auto bytes=static_cast<const BYTE*>(mover);
            view.anchor=*reinterpret_cast<const uintptr_t*>(bytes+0x80);
            view.desired=*reinterpret_cast<const int*>(bytes+0x98);
            view.current=*reinterpret_cast<const int*>(bytes+0x9C);
            view.lag=bytes[0xA0];
        }
        const bool viewChanged=LastOrbitTick&&
            (LastMover!=mover||(NativeViewVerified&&!SameView(view,LastView)));
        LastView=view;
        if(viewChanged||LastMover!=mover||!LastOrbitTick||now-LastOrbitTick>250)VehicleOrbit.Clear();
        const bool nativeBack=NativeLookBackVerified&&*(static_cast<const BYTE*>(mover)+0xA1);
        const bool takeover=reset||x!=0||y!=0||nativeBack||viewChanged;
        const auto mouseSample=MouseInput();
        // Context/mover gaps discard old cursor deltas and a retained mouse pose.
        if(LastMover!=mover||!LastOrbitTick||now-LastOrbitTick>250)Mouse.Reset();
        const bool mouseActivity=NativeMouseVerified&&Mouse.Update(mouseSample,controls,takeover,Yaw,Pitch);

        if(!directionMode&&Config.resetOnViewChange&&viewChanged)WaitForNeutral=true;
        if(WaitForNeutral&&!directionMode) {
            Yaw=Pitch=0;LastMover=mover;LastOrbitTick=LastOrbitInput=now;
            if(x==0&&y==0)WaitForNeutral=false;
            original(matrix,eye,look,up);return;
        }
        // Read the action after native key mapping, not a fixed controller button.
        // While held, stock look-back owns the view even if the orbit stick moves.
        if (!directionMode && Config.resetOnLookBack && NativeLookBackVerified &&
            nativeBack)
        {
            Yaw = Pitch = 0;
            LastMover = mover;
            LastOrbitTick = LastOrbitInput = now;
            original(matrix, eye, look, up);
            return;
        }
        if (LastMover != mover || !LastOrbitTick || now - LastOrbitTick > 250)
        {
            Yaw = Pitch = 0;
            LastOrbitInput = now;
            LastMover = mover;
        }
        float dt = LastOrbitTick ? Clamp(float(now - LastOrbitTick) * 0.001f, 0, 0.1f) : 0;
        LastOrbitTick = now;
        if (reset) Yaw = Pitch = 0;
        else if (!Mouse.owns && (directionMode || x != 0 || y != 0))
        {
            CustomizationControls::ApplyAxes(controls,x,y,dt,Yaw,Pitch);
            LastOrbitInput = now;
        }
        else if (!Mouse.owns && controls.returnDelay >= 0 && now - LastOrbitInput >= ULONGLONG(controls.returnDelay * 1000))
        {
            Yaw = ApproachZero(Yaw, controls.returnSpeed * dt);
            Pitch = ApproachZero(Pitch, controls.returnSpeed * dt);
        }
        if(mouseActivity)LastOrbitInput=now;
        if (Yaw == 0 && Pitch == 0 && Mouse.zoom == 1 && !controls.vehicleHeading)
        {
            original(matrix, eye, look, up);
            return;
        }
        const bool fixedEye=NativeViewVerified&&
            ((view.desired>=0&&view.desired<32&&(Config.fixedEyePovMask&(1u<<view.desired)))||
             (view.current>=0&&view.current<32&&(Config.fixedEyePovMask&(1u<<view.current))));
        CustomizationOrbit::Pose native={{eye->x,eye->y,eye->z},{look->x,look->y,look->z}};
        CustomizationOrbit::Pose rotated;
        Vec lockedUp{};const Vec* orbitUp=up;
        bool composed=false;
        if(controls.vehicleHeading&&NativeHeadingVerified&&view.anchor) {
            const auto position=reinterpret_cast<const Vec*>(view.anchor+0x20);
            const auto forward=reinterpret_cast<const Vec*>(view.anchor+0x50);
            const auto vehicleUp=reinterpret_cast<const Vec*>(view.anchor+0x70);
            CustomizationOrbit::Frame frame;
            if(CustomizationOrbit::VehicleFrame({position->x,position->y,position->z},
                {forward->x,forward->y,forward->z},{vehicleUp->x,vehicleUp->y,vehicleUp->z},frame)) {
                composed=VehicleOrbit.Compose(native,frame,fixedEye,nativeBack,Yaw,Pitch,Mouse.zoom,rotated);
                if(composed){lockedUp={frame.up.x,frame.up.y,frame.up.z,0};orbitUp=&lockedUp;}
            }
        }
        if(!composed)composed=CustomizationOrbit::Rotate(native,Yaw,Pitch,fixedEye,rotated,Mouse.zoom);
        if(!composed) {
            original(matrix,eye,look,up);return;
        }
        Mirror.Observe(now,Yaw,Pitch);
        const Vec orbitEye={rotated.eye.x,rotated.eye.y,rotated.eye.z,0};
        const Vec orbitLook={rotated.look.x,rotated.look.y,rotated.look.z,0};
        original(matrix,&orbitEye,&orbitLook,orbitUp);
    }

    static void __declspec(naked) OrbitLookAtCave()
    {
        __asm
        {
            // Copy the four existing cdecl arguments, adding the verified ESI mover.
            push [esp + 16]
            push [esp + 16]
            push [esp + 16]
            push [esp + 16]
            push esi
            call OrbitLookAt
            add esp, 20
            ret
        }
    }

    static void Install()
    {
        if (Installed) return;
        CustomizationRuntimePeer::Gate gate(L"CameraRuntime");if(!gate)return;
        // Each consumer may use the system pad reader for its own UI modifiers.
        HMODULE xinput = LoadLibraryExA("xinput1_4.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!xinput) xinput = LoadLibraryExA("xinput9_1_0.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (xinput) GetPadState = reinterpret_cast<decltype(GetPadState)>(GetProcAddress(xinput, "XInputGetState"));
        Peer=CustomizationRuntimePeer::Find<RuntimeApi>("MWCL_GetCameraRuntimeV1",[](const RuntimeApi& api) {
            return api.size==sizeof(RuntimeApi)&&api.version==1&&
                CustomizationRuntimePeer::Pin(reinterpret_cast<const void*>(api.garageObserver))&&
                CustomizationRuntimePeer::Pin(reinterpret_cast<const void*>(api.inputProvider));
        });
        if(Peer) {Installed=true;PublishedRuntime.store(Peer,std::memory_order_release);return;}
#ifdef MWCL_CAMERA_RUNTIME_FIXTURE
        Installed=PublishRuntime();return;
#endif
        Installed = true;
        Settings settings;
        Config.garage = settings.ReadInteger("Garage", "Enabled", 1) != 0;
        Config.partsOnly = settings.ReadInteger("Garage", "PartsOnly", 0) != 0;
        Config.driverPreview = settings.ReadInteger("Garage", "PreviewDriver", 1) != 0;
        Config.resetOnLookBack = settings.ReadInteger("RaceOrbit", "ResetOnLookBack", 1) != 0;
        Config.resetOnViewChange = settings.ReadInteger("RaceOrbit", "ResetOnViewChange", 1) != 0;
        Config.fixedEyePovMask = unsigned(settings.ReadInteger("RaceOrbit", "FixedEyePovMask", 3));
        Config.leftStick = settings.ReadInteger("Input", "CameraStick", 1) == 0;
        Config.inheritInversion = settings.ReadInteger("Garage", "UseDrivingInversion", 1) != 0;
        Config.inheritDeadzone = settings.ReadInteger("Garage", "UseDrivingDeadzone", 1) != 0;
        Config.invertX = settings.ReadInteger("Input", "InvertX", 0) != 0;
        Config.invertY = settings.ReadInteger("Input", "InvertY", 0) != 0;
        Config.controller = DWORD(Clamp(float(settings.ReadInteger("Input", "ControllerIndex", 0)), 0, 3));
        Config.deadzone = Number(settings, "Input", "Deadzone", 0.22f, 0, 0.95f);
        Config.garageSpeed = Number(settings, "Garage", "RotationSpeed", 1, 0.01f, 10);

        const unsigned char mouseDelta[]={0x89,0x4E,0x14,0x89,0x56,0x18};
        const unsigned char mouseHeld[]={0xA0,0x70,0x7F,0x98,0};
        const unsigned char mouseWheel[]={0xDB,0x05,0x6C,0x7F,0x98,0};
        const unsigned char mouseUpdate[]={0xB9,0x50,0x7F,0x98,0};
        const unsigned char mouseFrame[]={0xA3,0x78,0x2B,0x98,0};
        const unsigned char pauseCheck[]={0xB9,0x70,0x5E,0x92,0,0xE9,0xE6,0x32,0x1E,0};
        NativeMouseVerified=Matches(0x468390,pauseCheck)&&Matches(0x6C9C6D,mouseDelta)&&Matches(0x7A2052,mouseHeld)&&
            Matches(0x515990,mouseWheel)&&Matches(0x6E7651,mouseUpdate)&&Matches(0x6E7662,mouseFrame);
        if(!NativeMouseVerified)Report("Native mouse layout not verified; added mouse orbit disabled.");

        const unsigned char ctorSetup[] = {0xE8, 0xAB, 0xCE, 0xFF, 0xFF};
        bool lifecycle = Matches(0x7C01B0, ctorSetup) && PointerMatches(0x8B7EA4, 0x7C01D0) &&
                         PointerMatches(0x8B7EB4, 0x7BD060);
        if ((Config.garage || Config.driverPreview) && lifecycle)
        {
            injector::MakeCALL(0x7C01B0, SetupParts, true);
            injector::WriteMemory(0x8B7EB4, &SetupParts, true);
            injector::WriteMemory(0x8B7EA4, &DeleteParts, true);
        }
        else if (Config.garage || Config.driverPreview)
        {
            Config.garage = Config.driverPreview = false;
            Report("CustomizeParts lifecycle signature mismatch; garage camera and driver preview disabled.");
        }

        const unsigned char joyCall[] = {0xE8, 0xE8, 0x83, 0xFE, 0xFF};
        const unsigned char joyBranch[] = {0x8A, 0x46, 0x78, 0x84, 0xC0, 0x75, 0x0D, 0xA0, 0x34, 0x61, 0x92, 0};
        const unsigned char rotateFunction[] = {0x8B, 0x81, 0x80, 0, 0, 0, 0x33, 0xD2};
        // Also supplies native-menu updates when both optional garage features are off.
        if (Matches(0x7B9F23, joyCall) && Matches(0x7A2380, joyBranch) &&
            Matches(0x476E70, rotateFunction) && Matches(0x476F30, rotateFunction))
            injector::MakeCALL(0x7B9F23, GarageJoyEvents, true);
        else if (Config.garage || Config.driverPreview) Report("Garage camera signature mismatch; input hook not installed.");

        const unsigned char driver[] = {0x8B, 0x0D, 0x90, 0x5E, 0x92, 0, 0x83, 0xE0, 1, 0x83, 0xF9, 3, 0x75, 2, 0x33, 0xC0};
        const unsigned char driverFlags[]={0x83,0x3A,0x33,0x75,0x0B,0x23,0x05,0x20,0x09,0x9B,0,
            0xE9,0xAF,0xFD,0xFF,0xFF,0x23,0x05,0x0C,0x33,0x90,0,0xE9,0xA4,0xFD,0xFF,0xFF};
        const unsigned char driverDraw[]={0x85,0xC0,0x0F,0x84,0x7A,0x02,0,0};
        if (Config.driverPreview && Matches(0x74EC4F, driver)&&Matches(0x74EC5F,driverFlags)&&Matches(0x74EA1E,driverDraw))
        {
            injector::MakeJMP(0x74EC4F, DriverVisibilityCave, true);
            injector::MakeNOP(0x74EC54, 1, true);
        }
        else if (Config.driverPreview) Report("Driver visibility signature mismatch; render hook not installed.");

        // PC Cubic setter, getter and Update independently identify the same flag.
        const unsigned char lookBackSetter[] = {0x8A,0x44,0x24,0x04,0x88,0x81,0xA1,0,0,0,0xC2,0x04,0};
        const unsigned char lookBackGetter[] = {0x8A,0x81,0xA1,0,0,0,0xF6,0xD8,0x1B,0xC0,0x25,0,0x80,0,0,0xC3};
        const unsigned char lookBackUpdate[] = {0x8A,0x86,0xA1,0,0,0,0x84,0xC0};
        const unsigned char anchorMatrix[]={0x8D,0x46,0x50,0x57,0x50,0xE8,0x14,0xC0,0xFD,0xFF};
        const unsigned char anchorRead[]={0x8B,0x8E,0x80,0,0,0};
        const unsigned char anchorPosition[]={0xD9,0x40,0x20};
        const unsigned char anchorUp[]={0xD9,0x40,0x74,0x8B,0x50,0x70,0xD9,0x40,0x78};
        NativeHeadingVerified=Matches(0x468FC2,anchorMatrix)&&Matches(0x47D41C,anchorRead)&&
            Matches(0x476738,anchorPosition)&&Matches(0x47D835,anchorUp);
        if(!NativeHeadingVerified)Report("Vehicle heading layout not verified; vehicle-relative orbit disabled.");
        NativeLookBackVerified = Matches(0x4741A0,lookBackSetter) &&
            Matches(0x4741D0,lookBackGetter) && Matches(0x47D7ED,lookBackUpdate);
        if (Config.resetOnLookBack && !NativeLookBackVerified)
            Report("Native look-back layout not verified; automatic orbit reset disabled.");

        const unsigned char povRead[]={0x8B,0x81,0x9C,0,0,0,0x8B,0x54,0x24,4};
        const unsigned char povWrite[]={0x89,0x91,0x98,0,0,0};
        const unsigned char anchorWrite[]={0x89,0x8E,0x80,0,0,0};
        const unsigned char lagWrite[]={0x88,0x81,0xA0,0,0,0};
        NativeViewVerified=Matches(0x46B050,povRead)&&Matches(0x46B0A9,povWrite)&&
            Matches(0x481C1D,anchorWrite)&&Matches(0x4741B9,lagWrite);
        if(!NativeViewVerified)
            Report("Native POV layout not verified; view-change reset and fixed-eye selection disabled.");

        const unsigned char orbitCall[] = {0xE8, 0xDF, 0x13, 0x25, 0};
        const unsigned char moverView[] = {0x8B, 0x4E, 0x10};
        if (Matches(0x47DCBC, orbitCall) && Matches(0x47DCC1, moverView))
        {
            injector::MakeCALL(0x47DCBC, OrbitLookAtCave, true);
            // Both native decisions feed one view-mode path: it switches off the
            // mirror scene and its mask together, without changing saved options.
            const unsigned char mirrorEntry[]={0xA1,0x84,0xD8,0x92,0,0x53,0x8B,0x5C,0x24,0x08};
            const unsigned char mirrorEnter[]={0xE8,0xC6,0x61,0xEC,0xFF};
            const unsigned char mirrorLeave[]={0xE8,0x73,0x61,0xEC,0xFF};
            if(Matches(0x595D40,mirrorEntry)&&Matches(0x6CFB75,mirrorEnter)&&Matches(0x6CFBC8,mirrorLeave)) {
                injector::MakeCALL(0x6CFB75,MirrorVisible,true);
                injector::MakeCALL(0x6CFBC8,MirrorVisible,true);
                const bool radar=InstallRadarHook();
                CustomizationControls::SetMirrorAvailable(true,radar);
                if(!radar)Report("Radar HUD layout differs or is already hooked; orbit radar suppression disabled.");
            } else Report("Native mirror visibility layout differs; orbit auto-hide disabled.");
        }
        else Report("Race orbit signature mismatch; orbit hook not installed.");
        if(!PublishRuntime())Report("Camera runtime service could not be pinned.");
    }
}

extern "C" __declspec(dllexport) BOOL __cdecl NFSCustomizationCameraSetInputV1(
    unsigned int version, CustomizationCamera::InputProviderV1 provider)
{
    if (version != 1) return FALSE;
    return CustomizationCamera::SetInputProvider(provider)?TRUE:FALSE;
}
extern "C" __declspec(dllexport) const CustomizationCamera::RuntimeApi* __cdecl MWCL_GetCameraRuntimeV1() {
    return CustomizationCamera::PublishedRuntime.load(std::memory_order_acquire);
}
