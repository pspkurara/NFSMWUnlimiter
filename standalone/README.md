# NFSMW Extended Camera (development)

An independent Win32 ASI for the normal PC 1.3 mod-loader setup. Driving orbit,
mouse drag/wheel zoom, native-controls session integration and garage camera
input do not require Unlimiter, ExtraOptions, VRM or the stance companion.
Attachment menus and driver parts must already exist to use their corresponding
camera/preview behavior; this ASI does not add those parts or menus.

Copy `NFSMWExtendedCamera.asi` and `NFSMWCustomizationCamera.ini` into `scripts`.
Keep an existing camera INI when upgrading. Open PC Controls and press F7 for the
camera dialog, unless your INI changes that hotkey. Driving orbit starts disabled.
The common configuration format and profile-adjacent controls sidecar are shared
with the fork, so existing settings require no filename migration. Back up the
native save and its extension files together. See `CustomizationControls.md` and
`CustomizationCamera.md` for input modes, save/cancel and garage behavior.

When installed alongside the matching updated Unlimiter fork, the first initialized
ASI owns camera/input hooks. The other uses its versioned runtime API rather than
patching those addresses again. Both use one controls session and profile host;
the stance menu can register its garage update with either camera owner. Older
fork builds without this runtime API cannot provide the same coinstallation
contract. Unknown competing native hooks retain their existing owner and disable
the conflicting extension. Do not install this development build over a running
game or treat signature acceptance as proof of compatibility with every mod.

Build this product alone:

```powershell
cmake -S standalone -B build-camera -A Win32
cmake --build build-camera --config Release
ctest --test-dir build-camera -C Release --output-on-failure
```

This CMake entry point does not compile or link Unlimiter, owned-car saves or the
stance editor. Tests exercise sidecar transactions and two genuinely separate
DLL instances in both initialization orders. Native patch/UI startup is stubbed
in those cross-DLL fixtures; live solo/combined camera, input and persistence QA
is still required. The original Windows 11 installation retains its previously
verified camera integration while these standalone changes are under test.

New camera/shared-runtime source is MIT licensed; see `LICENSE`. The bundled
MinHook and injector notices remain with their original source. No game binaries,
assets, user profiles or research dumps are distributed.
