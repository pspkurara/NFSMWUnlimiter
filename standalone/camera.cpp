// Camera-only product: native PC 1.3 hooks and an ordinary ASI loader suffice.
// No Unlimiter initialization, car-part tables, editor companion or owned saves.
#include "../CustomizationCamera.h"

BOOL APIENTRY DllMain(HMODULE module,DWORD reason,LPVOID) {
    if(reason!=DLL_PROCESS_ATTACH)return TRUE;
    DisableThreadLibraryCalls(module);
    const uintptr_t base=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    const auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE||base!=0x400000)return FALSE;
    const auto nt=reinterpret_cast<const IMAGE_NT_HEADERS*>(base+dos->e_lfanew);
    if(nt->Signature!=IMAGE_NT_SIGNATURE||nt->OptionalHeader.AddressOfEntryPoint!=0x3C4040)return FALSE;
    const bool lifecycle=CustomizationProfileHost::Install();
    if(!CustomizationControls::Install(lifecycle)) {
        CustomizationCamera::Report("Standalone camera controls unavailable; native camera retained.");return TRUE;
    }
    CustomizationCamera::Install();
    // After publishing hooks the module must remain loaded even on partial feature
    // rejection. Individual guarded features retain native behavior when unavailable.
    return TRUE;
}
