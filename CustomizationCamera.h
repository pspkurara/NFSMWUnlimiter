#pragma once

#include <windows.h>
#include <Xinput.h>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include "includes\injector\injector.hpp"

// Optional, independently guarded hooks for the PC 1.3 executable. No preview
// state is written to RideInfo, the profile, or a game save.
namespace CustomizationCamera
{
    struct Options
    {
        bool garage = true;
        bool driverPreview = true;
        bool raceOrbit = false;
        bool leftStick = false;
        bool invertX = false;
        bool invertY = false;
        DWORD controller = 0;
        float deadzone = 0.22f;
        float garageSpeed = 1.0f;
        float raceDegreesPerSecond = 100.0f;
        float pitchLimit = 60.0f;
        float returnDelay = -1.0f;
        float returnSpeed = 120.0f;
    };

    static Options Config;
    static DWORD* PartsScreen = nullptr;
    static bool DriverPreviewActive = false;
    static bool GarageControlled = false;
    static bool Installed = false;
    static DWORD(__stdcall* GetPadState)(DWORD, XINPUT_STATE*) = nullptr;

    // A separate ASI may supply normalized, already mapped stick input through
    // this exported versioned API. Passing nullptr restores the XInput backend.
    typedef BOOL(__cdecl* InputProviderV1)(BOOL leftStick, float* x, float* y, BOOL* reset);
    static InputProviderV1 InputProvider = nullptr;

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
        if (InputProvider)
        {
            BOOL requestReset = FALSE;
            if (!InputProvider(Config.leftStick, &x, &y, &requestReset) ||
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
            if (length <= Config.deadzone) x = y = 0;
            else
            {
                float scale = Clamp((length - Config.deadzone) / (1 - Config.deadzone), 0, 1) / length;
                x *= scale;
                y *= scale;
            }
        }
        if (Config.invertX) x = -x;
        if (Config.invertY) y = -y;
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

    static void __fastcall GarageJoyEvents(DWORD* garage, void*)
    {
        DriverPreviewActive = Config.driverPreview && PartsScreen && garage[0x88 / 4] == 0x308;
        float x = 0, y = 0;
        bool reset = false;
        bool control = Config.garage && PartsScreen && PartsScreen[0x148 / 4] != 0x307 &&
                       GarageMover(garage) && ReadStick(x, y, reset);
        if (!control)
        {
            if (GarageControlled) SetGarageSpeeds(garage, 0, 0);
            GarageControlled = false;
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
        SetGarageSpeeds(garage, x, y);
        GarageControlled = true;
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
            push 0x74EC5F
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
        float x = 0, y = 0;
        bool reset = false;
        if (*reinterpret_cast<int*>(0x925E90) != 6 || !mainView || !ReadStick(x, y, reset))
        {
            Yaw = Pitch = 0;
            LastOrbitTick = 0;
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
        else if (x != 0 || y != 0)
        {
            Yaw = std::remainder(Yaw + x * Config.raceDegreesPerSecond * dt, 360.0f);
            Pitch = Clamp(Pitch + y * Config.raceDegreesPerSecond * dt, -Config.pitchLimit, Config.pitchLimit);
            LastOrbitInput = now;
        }
        else if (Config.returnDelay >= 0 && now - LastOrbitInput >= ULONGLONG(Config.returnDelay * 1000))
        {
            Yaw = ApproachZero(Yaw, Config.returnSpeed * dt);
            Pitch = ApproachZero(Pitch, Config.returnSpeed * dt);
        }
        if (Yaw == 0 && Pitch == 0)
        {
            original(matrix, eye, look, up);
            return;
        }
        // eCreateLookAtMatrix uses render-space coordinates (Z is up). The stock
        // target and distance are retained; the stock camera resumes each frame.
        float dx = eye->x - look->x, dy = eye->y - look->y, dz = eye->z - look->z;
        float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (!std::isfinite(distance) || distance < 0.05f)
        {
            original(matrix, eye, look, up);
            return;
        }
        constexpr float radians = 0.01745329251994329577f;
        float azimuth = std::atan2(dy, dx) + Yaw * radians;
        float elevation = Clamp(std::atan2(dz, std::sqrt(dx * dx + dy * dy)) + Pitch * radians, -1.4f, 1.4f);
        float horizontal = distance * std::cos(elevation);
        Vec orbitEye = {look->x + horizontal * std::cos(azimuth), look->y + horizontal * std::sin(azimuth),
                        look->z + distance * std::sin(elevation), 0};
        original(matrix, &orbitEye, look, up);
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
        Installed = true;
        Settings settings;
        Config.garage = settings.ReadInteger("Garage", "Enabled", 1) != 0;
        Config.driverPreview = settings.ReadInteger("Garage", "PreviewDriver", 1) != 0;
        Config.raceOrbit = settings.ReadInteger("RaceOrbit", "Enabled", 0) != 0;
        Config.leftStick = settings.ReadInteger("Input", "CameraStick", 1) == 0;
        Config.invertX = settings.ReadInteger("Input", "InvertX", 0) != 0;
        Config.invertY = settings.ReadInteger("Input", "InvertY", 0) != 0;
        Config.controller = DWORD(Clamp(float(settings.ReadInteger("Input", "ControllerIndex", 0)), 0, 3));
        Config.deadzone = Number(settings, "Input", "Deadzone", 0.22f, 0, 0.95f);
        Config.garageSpeed = Number(settings, "Garage", "RotationSpeed", 1, 0.01f, 10);
        Config.raceDegreesPerSecond = Number(settings, "RaceOrbit", "DegreesPerSecond", 100, 1, 720);
        Config.pitchLimit = Number(settings, "RaceOrbit", "PitchLimitDegrees", 60, 0, 80);
        Config.returnDelay = Number(settings, "RaceOrbit", "ReturnDelaySeconds", -1, -1, 60);
        Config.returnSpeed = Number(settings, "RaceOrbit", "ReturnDegreesPerSecond", 120, 1, 720);

        // No XInput import or replacement DLL: use the system runtime if available.
        HMODULE xinput = LoadLibraryExA("xinput1_4.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!xinput) xinput = LoadLibraryExA("xinput9_1_0.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (xinput) GetPadState = reinterpret_cast<decltype(GetPadState)>(GetProcAddress(xinput, "XInputGetState"));

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
        if ((Config.garage || Config.driverPreview) && Matches(0x7B9F23, joyCall) && Matches(0x7A2380, joyBranch) &&
            Matches(0x476E70, rotateFunction) && Matches(0x476F30, rotateFunction))
            injector::MakeCALL(0x7B9F23, GarageJoyEvents, true);
        else if (Config.garage || Config.driverPreview) Report("Garage camera signature mismatch; input hook not installed.");

        const unsigned char driver[] = {0x8B, 0x0D, 0x90, 0x5E, 0x92, 0, 0x83, 0xE0, 1, 0x83, 0xF9, 3, 0x75, 2, 0x33, 0xC0};
        if (Config.driverPreview && Matches(0x74EC4F, driver))
        {
            injector::MakeJMP(0x74EC4F, DriverVisibilityCave, true);
            injector::MakeNOP(0x74EC54, 1, true);
        }
        else if (Config.driverPreview) Report("Driver visibility signature mismatch; render hook not installed.");

        const unsigned char orbitCall[] = {0xE8, 0xDF, 0x13, 0x25, 0};
        const unsigned char moverView[] = {0x8B, 0x4E, 0x10};
        if (Config.raceOrbit && Matches(0x47DCBC, orbitCall) && Matches(0x47DCC1, moverView))
            injector::MakeCALL(0x47DCBC, OrbitLookAtCave, true);
        else if (Config.raceOrbit) Report("Race orbit signature mismatch; orbit hook not installed.");
    }
}

extern "C" __declspec(dllexport) BOOL __cdecl NFSCustomizationCameraSetInputV1(
    unsigned int version, CustomizationCamera::InputProviderV1 provider)
{
    if (version != 1) return FALSE;
    CustomizationCamera::InputProvider = provider;
    return TRUE;
}
