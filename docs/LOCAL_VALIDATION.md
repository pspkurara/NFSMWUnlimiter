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
