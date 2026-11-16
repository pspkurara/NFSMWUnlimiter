# Local validation record

2026-11-16, Windows 11, inspected PC 1.3 executable SHA-256
`0c5675a08cd71fd6d31ca87e992a915054bd8b80d268bff0561d7ecc2067e342`.
This records the scope of evidence, not universal compatibility.

## Installed development snapshot

Snapshot commit `18894c9` was installed into the user's REDUX 3.04 game after
normal save/exit. Existing Unlimiter 3.3.1.1338 was backed up with a receipt;
existing settings, ExtraOptions, VRM and native saves were retained. Five payload
hashes were verified. Local key overrides use F9 for camera settings, F10 for
stance editor and F11 for stance reload to avoid the observed ExtraOptions keys.
Stance and wheel clipping remain disabled. The installer fixture independently
verified original-file backup, receipt contents, payload hashes and user-INI
preservation before the real installation.

A subsequent original-game process loaded Unlimiter, CustomizationLab,
ExtraOptions and VRM from that installation. Read-only memory checks confirmed
installed detours at native save/load, input writer, car rendering, garage input
and Cubic orbit call sites. VRM's own log continued to record gameplay events.
This establishes startup coexistence and actual hook installation; it does not
prove camera behavior, stance geometry or all VRM interactions.

## User-observed behavior

The user confirmed that the earlier dialog opens in Controls, cancellation
restores the prior extension state, and enabling orbit allows stick rotation
while driving with an Xbox 360 controller. The reported steering-stick rotation
was in garage/customization. New directional, mouse, reset and first-person
geometry paths still require visual game-level checks. Durable profile-sidecar
restoration after normal save/restart is not yet established by user testing.

## Automated coverage

Fork tests cover geometry transforms, owned-car save isolation and validation,
controls transactions and mode behavior, actual controls sidecar files and legacy
migration, mouse frame deduplication and input ownership, and zoom/eye invariants.
The companion has separate core and enabled/disabled host tests. Passing these
checks does not substitute for native game testing. The extracted controls
sidecar fixture compiles without the owned-car save header.

## Reported quick-race issue

The user reported Civic and CLK GTR becoming/retaining a GTO police vehicle,
with the VRM model penetrating the roof and the issue retained after returning
to menus. The inspected live session had the ExtraOptions Freeze Car branch
inactive (`0x74` at `0x6F48E7`). This does not prove whether that feature had been
used earlier. VRM logged COPGTO driver/body data, but those records alone do not
identify the selected player's record or prove which component caused the switch.

An 8-bit `IsCop` helper in this fork has no call sites; it is not evidence of this
symptom's cause. The installation contains 168 CARS directories, which is not an
authoritative count of loaded car-type records. No speculative source fix was
applied for the reported vehicle switch. Reproduction and selected-record versus
spawned-vehicle identity tracing remain open.

## Remaining product work

Complete native geometry/save QA, driver-menu lifetime checks, clipping coverage
including fenderless cars, and race owned-car lineage. A standalone camera host
and coordinated save-hook ownership remain necessary for independent camera
distribution. Countdown/scripted-camera contexts remain unverified. The installed
development snapshot must not be described as the completed requested product.

## Follow-up: race stance and emergency editor safety

The companion commit 789c4c3 disables the temporary editor hotkey by default,
including older INIs without DebugEditorHotkey. An invalid context no longer
opens a modal error popup while holding the mutex used by rendering. This
companion-only hotfix was backed up and installed in the original game.

The fork now binds racing RideInfo at VehicleRenderConn::Load, using local-player
usage 0 and the exact customization-record address within the active profile.
The PVehicle 0x198-byte deep-copy site propagates observed source ownership to
the allocated destination, guarded by part/handle fingerprint and active-profile
identity. No equipment match or copied handle alone proves ownership. Unknown copies,
AI usage and ambiguous records remain native. Fixtures cover these exclusions.

A guarded UpdateTires adapter changes temporary lateral positions and render
radii before native wheel matrices/contact points/FX are generated. A scoped
DoSkids adapter scales native skid width by the selected visual-width ratio;
stock calibration remains unchanged when there is no override. Position/radius
inputs are restored after the native update. This does not change handling or
physics suspension. Native PC offsets were checked against disassembly. Math
and lifecycle fixtures pass, but actual race rendering, skid/smoke alignment,
freeroam and durable game save/reload require live validation. Do not call this
race integration verified merely because the build passes.

The planned body-shop submenu, native sliders, grouped purchase/discard and
configurable price (default zero) are not implemented yet. The temporary editor
is debug-only and disabled until deliberately opted in. Vehicle-heading camera
basis is implemented with legacy-state migration; mirror auto-hide remains
pending and its new checkbox is disabled rather than pretending it works.

Frontend ReInit (also used by its constructor) and LoadedRideInfo's native copy
now propagate known RideInfo ownership. Allocation reuse drops stale associations;
unknown equal-equipment cars cannot borrow the selected global frontend record
in the value resolver. Cached menus read current owned values every render rather
than capturing stance at instance creation. Copy provenance is covered by fixtures;
real menu transitions still require in-game confirmation.
