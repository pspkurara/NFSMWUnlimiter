# Customization Lab development branch

This is an experimental fork of **Unlimiter v3.3.1.1338**, the version found in
the inspected REDUX 3.04 installation. It is not a migration to Unlimiter 4.x.
The companion project is NFSMWCustomizationLab, a separate Win32 ASI with a C ABI.
This branch is unfinished; building it does not establish game compatibility.

## Implemented paths

- `CustomizationCamera.h`: configurable Xbox stick selection in parts menus,
  scoped driver preview and optional Cubic driving-camera orbit. See the camera
  document for input-mapper and camera-collision limitations.
- `CustomizationBridge.h`: absolute front/rear width, radius, half-track,
  camber and brake-marker adjustments scoped to native rendering. Body height
  and rake change body parts, mirrors, plate, spoiler and roof submissions.
  Physics, collision and native save structures are not changed. Native light
  emitters and shadow geometry still need height/rake validation.
- `CustomizationSave.h`: profile-adjacent owned-car data staged by the companion
  editor and committed after the native SaveDone callback. See
  [save details](CUSTOMIZATION_SAVE.md).
- `CustomizationControls.h`: additional F7 camera controls dialog attached to
  native PC Controls transactions and native input serialization. Bindings use
  a profile-adjacent controls sidecar. See the controls document for ownership
  and the remaining native UI/runtime checks.
- `CustomizationClip.h`: opt-in calibrated combined-wheel clipping in the main
  view, including deferred-queue tagging and D3D state restoration. It does not
  discover fenders. No unmeasured real-car plane is enabled by default.

Install the companion ASI and its INI alongside this fork. The native body-shop
adapter adds an owned-car stance entry using the game's installed slider assets;
see [native menu details and validation status](STANCE_NATIVE_MENU.md).
The separate Windows editor is debug-only and disabled by default. Opt in with
`[General] DebugEditorHotkey=1` to use `EditorKey` (default F6); F5 reloads the
user INI. Existing installations may assign different keys. The feature itself
ships disabled and can be enabled from the stance UI. Owned-car values are
staged for the next native save; unowned car-type presets remain in the INI.

Ranges and steps live in the companion INI. Width and radius are positive
dimensions; their +/- controls change those dimensions in both directions.
Height, camber and brake inset are signed. Offset is absolute half-track in
metres. Changing editing ranges does not rescale existing saved values.
Rim and tire are one native wheel model in this backend: independent stretch
geometry is not implemented. Brake inset is manual, with no guessed universal
spoke-clearance detection.

## Executable and hook ownership

The inspected Windows 11 installation uses PC 1.3 speed.exe SHA256
`0c5675a08cd71fd6d31ca87e992a915054bd8b80d268bff0561d7ecc2067e342`.
The original Unlimiter ASI SHA256 was
`d1f7516bbf67bc78563e938515a10cf7c0e1c1654406ef1b6eab9ef3c7ba54c9`.
Every new patch checks its expected native bytes or original CALL target.
Unknown competing sites disable the affected extension. Original upstream
Unlimiter patches retain their original compatibility policy.

The body adapter calls the existing RenderPart entry so the installed VRM hook
remains on the path. Wheel clipping wraps queue CALL sites and the indexed draw
inside the executor, rather than replacing the VRM queue entry. This preserves
the intended chain, but is not proof of semantic compatibility in every mod stack.

Other executables, Unlimiter 4.x, OS versions, camera replacements and custom
render backends are not validated. Unknown cars retain unconfigured clip behavior.
Do not describe this branch as universally compatible.

## Verification recorded so far

- MSVC Win32 Release build of the fork and companion.
- CTest: body-transform math; owned-save preview/stage/native-success lifecycle;
  profile isolation; native-byte preservation; CRC/revision mismatch rejection;
  same-equipment AI exclusion; controls serialization/backup/input-reader lifecycle;
  companion range/override/loader/API tests.
- D3D9 HAL fixture: a calibrated plane changed coverage from 1301 to 940 pixels;
  stdcall adapter, stale tags and clip-state restoration checked. This fixture
  used software vertex processing. Native-game and HWVP checks remain open.
- With the original game running, the isolated copy failed at `speed.exe+0x2e6d5f`
  reading a null D3D device. The unmodified Unlimiter baseline failed at the same
  address. After the user saved and exited the original, the baseline reached
  its rendered loading screen. The modified fork/editor, with VRM disabled only
  in the isolated copy, reached the main menu and rendered cars; debugger output
  confirmed save and controls hooks installed. Feature-level checks remain open.
- One test with VRM enabled stalled before a window appeared. Its main thread
  was suspended and another VRM thread waited in loader synchronization; this
  is evidence for a startup coexistence investigation, not a proven root cause.
  The original installation and VRM source were not modified.

## Required work before a supported release

Driving orbit bindings now use the active native key-config extension, including
comparison, cancel, defaults, serialization and native input-reader ownership.
This implementation still needs live accept/cancel/reset, load/create and restart
verification; see [input research](CONTROLS_RESEARCH.md).

In-game testing is also required for simultaneous sticks, driver menu exit,
save/reload of two owned cars of the same type, garage-to-race RideInfo lineage,
manual brake sign/clearance, body lighting/shadows, device reset, and configured
clip profiles at different stance extremes. Unknown RideInfo copies are skipped
instead of guessing ownership. Fender-specific boundaries and stretched-tire
geometry require additional vehicle-aware work.

## Race radius calibration

The connection's native render/contact radius can differ from the scaled wheel
mesh radius. Radius adjustment therefore applies the mesh change as a delta:
`connectionRadius + (newMeshRadius - originalMeshRadius)`. For example, a native
0.31 m connection radius with a 0.30 m mesh becomes 0.36 m for a requested 0.35 m
mesh. Replacing it with 0.35 m would discard the vehicle's existing calibration
and introduce a discontinuity at the default. No-change values remain exact.
Tests cover both sides of that default and reject invalid/negative results.

Native UpdateTires uses this radius for wheel roll and compression before deriving
world contact/FX points. The adapter restores its temporary connection fields
after the update; it does not edit physics radius, collision or saved native data.
This math evidence does not replace live smoke/skid/suspension validation.

## Build

Use CMake with an installed Visual Studio C++ toolchain and Windows SDK:

```powershell
cmake -S . -B build-customization -A Win32
cmake --build build-customization --config Release
ctest --test-dir build-customization -C Release --output-on-failure
```

No legacy DirectX helper SDK is needed. MinHook is statically linked under its
included BSD license. Original Unlimiter source and notices remain upstream's.
Do not ship proprietary game executables, user profiles, dumps or research logs.
