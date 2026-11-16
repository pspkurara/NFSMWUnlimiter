#pragma once

// Opt-in, calibrated clipping of the native combined wheel mesh. This header owns
// call sites, never eView/queue function entries (which other plugins may own).
#include <windows.h>
#include <d3d9.h>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace CustomizationClip {
namespace Detail {
constexpr uintptr_t ItemBase = 0x93E878, QueueCount = 0x982CDC;
constexpr unsigned Capacity = 4096, ItemStride = 0x44;
struct Profile {
    std::string name;
    int kit;
    bool present[4] = {};
    float plane[4][4] = {};
};
struct Tag {
    uint32_t epoch = 0, frame = 0;
    void* view = nullptr;
    uintptr_t mesh = 0, matrix = 0;
    float worldPlane[4] = {};
};
static Tag tags[Capacity];
static std::vector<Profile> profiles;
static std::string iniPath;
static uint32_t epoch = 1;
static bool installed = false;
static const char* status = "Disabled";

inline uint32_t Read32(uintptr_t p) { return *reinterpret_cast<const uint32_t*>(p); }
inline bool Readable(const void* p, size_t size) {
    if (!p || !size) return false;
    MEMORY_BASIC_INFORMATION m = {};
    if (!VirtualQuery(p, &m, sizeof(m)) || m.State != MEM_COMMIT ||
        (m.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return false;
    const uintptr_t a = reinterpret_cast<uintptr_t>(p);
    const uintptr_t end = reinterpret_cast<uintptr_t>(m.BaseAddress) + m.RegionSize;
    return a <= end && size <= end - a;
}

// Row-vector matrices: p' = p*M, so plane coefficients satisfy M*q' = q.
// Solve directly instead of relying on a renderer-specific transpose convention.
inline bool TransformPlane(const float* matrix, const float* plane, float* result) {
    double a[4][5];
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            const float value = matrix[row * 4 + col];
            if (!std::isfinite(value)) return false;
            a[row][col] = value;
        }
        if (!std::isfinite(plane[row])) return false;
        a[row][4] = plane[row];
    }
    for (int col = 0; col < 4; ++col) {
        int pivot = col;
        for (int row = col + 1; row < 4; ++row)
            if (std::fabs(a[row][col]) > std::fabs(a[pivot][col])) pivot = row;
        if (std::fabs(a[pivot][col]) < 1e-12) return false;
        for (int j = col; j < 5; ++j) {
            const double value = a[col][j]; a[col][j] = a[pivot][j]; a[pivot][j] = value;
        }
        const double divisor = a[col][col];
        for (int j = col; j < 5; ++j) a[col][j] /= divisor;
        for (int row = 0; row < 4; ++row) if (row != col) {
            const double scale = a[row][col];
            for (int j = col; j < 5; ++j) a[row][j] -= scale * a[col][j];
        }
    }
    double length = 0;
    for (int i = 0; i < 4; ++i) length += a[i][4] * a[i][4];
    if (!std::isfinite(length) || length < 1e-24) return false;
    const double scale = 1.0 / std::sqrt(length);
    for (int i = 0; i < 4; ++i) result[i] = static_cast<float>(a[i][4] * scale);
    return true;
}
inline bool ParsePlane(const char* text, float* result) {
    const char* p = text;
    for (int i = 0; i < 4; ++i) {
        char* end = nullptr;
        result[i] = std::strtof(p, &end);
        if (end == p || !std::isfinite(result[i])) return false;
        while (*end == ' ' || *end == '\t') ++end;
        if (i != 3) { if (*end != ',') return false; p = end + 1; }
        else if (*end) return false;
    }
    const double n = double(result[0]) * result[0] + double(result[1]) * result[1] +
                     double(result[2]) * result[2];
    return n > 1e-16;
}
inline const Profile& GetProfile(const char* name, int kit) {
    for (const auto& p : profiles) if (p.kit == kit && p.name == name) return p;
    Profile p;
    p.name = name; p.kit = kit;
    char section[160], text[256];
    const int count = std::snprintf(section, sizeof(section), "CAR:%s:KIT:%d", name, kit);
    if (count > 0 && count < int(sizeof(section)) &&
        GetPrivateProfileIntA(section, "Enabled", 0, iniPath.c_str()) == 1) {
        const char* keys[] = {"FrontLeftPlane", "FrontRightPlane", "RearRightPlane", "RearLeftPlane"};
        for (int wheel = 0; wheel < 4; ++wheel) {
            GetPrivateProfileStringA(section, keys[wheel], "", text, sizeof(text), iniPath.c_str());
            p.present[wheel] = ParsePlane(text, p.plane[wheel]);
        }
    }
    profiles.push_back(p);
    return profiles.back();
}

// This is called once around the complete queue entry, including any existing
// VRM opaque/custom/transparent split. Never split or replay native items here.
static void __cdecl ExecuteQueue() {
    reinterpret_cast<void(__cdecl*)()>(0x6E0200)();
    ++epoch;
    if (!epoch) { std::memset(tags, 0, sizeof(tags)); epoch = 1; }
}

static HRESULT __stdcall DrawWheel(uintptr_t item, IDirect3DDevice9* device,
    D3DPRIMITIVETYPE type, INT baseVertex, UINT minVertex, UINT numVertices,
    UINT startIndex, UINT primitiveCount) {
    auto draw = [&]() { return device->DrawIndexedPrimitive(type, baseVertex, minVertex,
                                                           numVertices, startIndex, primitiveCount); };
    if (item < ItemBase || (item - ItemBase) % ItemStride) return draw();
    const uintptr_t index = (item - ItemBase) / ItemStride;
    if (index >= Capacity) return draw();
    const Tag& tag = tags[index];
    if (tag.epoch != epoch || tag.frame != Read32(0x982B78) ||
        tag.mesh != Read32(item) || tag.matrix != Read32(item + 0x1C)) return draw();

    // Clip planes are interpreted in shader output space. Fixed-function wheels,
    // exhausted clip slots and unsupported devices retain native rendering.
    IDirect3DVertexShader9* shader = nullptr;
    if (FAILED(device->GetVertexShader(&shader)) || !shader) return draw();
    shader->Release();
    DWORD enabled = 0;
    D3DCAPS9 caps = {};
    if (FAILED(device->GetRenderState(D3DRS_CLIPPLANEENABLE, &enabled)) ||
        FAILED(device->GetDeviceCaps(&caps))) return draw();
    DWORD slot = 0;
    while (slot < caps.MaxUserClipPlanes && slot < 6 && (enabled & (1u << slot))) ++slot;
    if (slot >= caps.MaxUserClipPlanes || slot >= 6) return draw();
    if (!Readable(tag.view, sizeof(void*))) return draw();
    const auto* platform = *reinterpret_cast<const unsigned char* const*>(tag.view);
    const uintptr_t context = Read32(0x982CB4);
    if (!Readable(reinterpret_cast<void*>(context), 0x51)) return draw();
    const size_t offset = *reinterpret_cast<const unsigned char*>(context + 0x50) ? 0x100 : 0x80;
    if (!Readable(platform, offset + 64)) return draw();
    float plane[4], oldPlane[4];
    if (!TransformPlane(reinterpret_cast<const float*>(platform + offset), tag.worldPlane, plane) ||
        FAILED(device->GetClipPlane(slot, oldPlane))) return draw();
    if (FAILED(device->SetClipPlane(slot, plane))) return draw();
    if (FAILED(device->SetRenderState(D3DRS_CLIPPLANEENABLE, enabled | (1u << slot)))) {
        device->SetClipPlane(slot, oldPlane); return draw();
    }
    const HRESULT result = draw();
    device->SetRenderState(D3DRS_CLIPPLANEENABLE, enabled);
    device->SetClipPlane(slot, oldPlane);
    return result;
}

// Native call already pushed the seven COM arguments. Insert only the current
// item pointer before them, preserving the caller's return and stdcall cleanup.
__declspec(naked) static void DrawBridge() {
    __asm {
        pop ecx
        push esi
        push ecx
        jmp DrawWheel
    }
}
} // namespace Detail

struct Ticket {
    uint32_t epoch = 0, frame = 0, first = 0;
    void* view = nullptr;
    float worldPlane[4] = {};
};
inline const char* Status() { return Detail::status; }

// Call once during plugin initialization, before rendering starts. The caller's
// existing EXE fingerprint/version gate remains mandatory.
inline bool Install(const char* absoluteIniPath) {
    using namespace Detail;
    if (installed) return true;
    if (!absoluteIniPath || !*absoluteIniPath) return false;
    iniPath = absoluteIniPath;
    if (GetPrivateProfileIntA("WheelClipping", "Enabled", 0, iniPath.c_str()) != 1) return false;
    struct Patch { uintptr_t address; unsigned length; void* target; unsigned char old[6]; };
    Patch patches[] = {
        {0x6E05FC, 6, reinterpret_cast<void*>(&DrawBridge)},
        {0x6E0E20, 5, reinterpret_cast<void*>(&ExecuteQueue)},
        {0x6E2F3B, 5, reinterpret_cast<void*>(&ExecuteQueue)},
        {0x6E2F7B, 5, reinterpret_cast<void*>(&ExecuteQueue)},
        {0x6E3036, 5, reinterpret_cast<void*>(&ExecuteQueue)},
        {0x6E5147, 5, reinterpret_cast<void*>(&ExecuteQueue)},
        {0x6E51B7, 5, reinterpret_cast<void*>(&ExecuteQueue)},
        {0x6E529E, 5, reinterpret_cast<void*>(&ExecuteQueue)},
        {0x6E5323, 5, reinterpret_cast<void*>(&ExecuteQueue)},
        {0x6E533C, 5, reinterpret_cast<void*>(&ExecuteQueue)}
    };
    const unsigned char expectedDraw[] = {0xFF, 0x92, 0x48, 0x01, 0, 0};
    for (auto& p : patches) {
        auto* code = reinterpret_cast<unsigned char*>(p.address);
        if (!Readable(code, p.length)) { status = "Unavailable call site"; return false; }
        std::memcpy(p.old, code, p.length);
        if (p.length == 6 ? std::memcmp(code, expectedDraw, 6) != 0 :
            code[0] != 0xE8 || uintptr_t(p.address + 5 + *reinterpret_cast<int32_t*>(code + 1)) != 0x6E0200) {
            status = "Another mod changed a required call site"; return false;
        }
    }
    struct Page { uintptr_t address; DWORD protect; };
    std::vector<Page> pages;
    SYSTEM_INFO system = {}; GetSystemInfo(&system);
    for (auto& p : patches) {
        const uintptr_t address = p.address - p.address % system.dwPageSize;
        bool duplicate = false;
        for (const auto& page : pages) if (page.address == address) duplicate = true;
        if (duplicate) continue;
        Page page = {address, 0};
        if (!VirtualProtect(reinterpret_cast<void*>(address), system.dwPageSize, PAGE_EXECUTE_READWRITE, &page.protect)) {
            for (const auto& previous : pages) { DWORD ignored;
                VirtualProtect(reinterpret_cast<void*>(previous.address), system.dwPageSize, previous.protect, &ignored); }
            status = "Cannot write call sites"; return false;
        }
        pages.push_back(page);
    }
    for (auto& p : patches) {
        unsigned char code[6] = {0xE8, 0, 0, 0, 0, 0x90};
        const int32_t relative = int32_t(reinterpret_cast<uintptr_t>(p.target) - (p.address + 5));
        std::memcpy(code + 1, &relative, sizeof(relative));
        std::memcpy(reinterpret_cast<void*>(p.address), code, p.length);
    }
    FlushInstructionCache(GetCurrentProcess(), nullptr, 0);
    for (const auto& page : pages) { DWORD ignored;
        VirtualProtect(reinterpret_cast<void*>(page.address), system.dwPageSize, page.protect, &ignored); }
    installed = true; status = "Enabled: calibrated combined-wheel planes";
    return true;
}

// wheel order: front left, front right, rear right, rear left, matching native MW.
// carToWorld must be the unspun CAR root, never the rotating wheel matrix.
inline Ticket BeginWheel(const char* carName, int kit, unsigned wheel,
                          const float* carToWorld, void* view) {
    using namespace Detail;
    Ticket ticket;
    // Reflection/mirror/shadow cameras need separate calibration and validation.
    // The main native eView also serves the supported frontend car renderer.
    if (!installed || !carName || wheel >= 4 || view != reinterpret_cast<void*>(0x919650) ||
        !Readable(carToWorld, 64)) return ticket;
    const auto& profile = GetProfile(carName, kit);
    if (!profile.present[wheel] || !TransformPlane(carToWorld, profile.plane[wheel], ticket.worldPlane)) return ticket;
    ticket.first = Read32(QueueCount);
    if (ticket.first >= Capacity) return Ticket{};
    ticket.epoch = epoch; ticket.frame = Read32(0x982B78); ticket.view = view;
    return ticket;
}
inline void EndWheel(const Ticket& ticket) {
    using namespace Detail;
    if (!ticket.epoch || ticket.epoch != epoch || ticket.frame != Read32(0x982B78)) return;
    const unsigned end = Read32(QueueCount);
    if (end < ticket.first || end > Capacity) return;
    for (unsigned i = ticket.first; i < end; ++i) {
        Tag& tag = tags[i];
        tag.epoch = ticket.epoch; tag.frame = ticket.frame; tag.view = ticket.view;
        tag.mesh = Read32(ItemBase + i * ItemStride);
        tag.matrix = Read32(ItemBase + i * ItemStride + 0x1C);
        std::memcpy(tag.worldPlane, ticket.worldPlane, sizeof(tag.worldPlane));
    }
}
} // namespace CustomizationClip
