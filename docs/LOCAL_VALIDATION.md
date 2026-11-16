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

The body-shop submenu and native sliders now exist in source with free grouped
stage/discard, an isolated runtime clone of the installed tuning asset and no
new top-level shop. Runtime testing exposed a missing package, then a title-only
row rejection and invalid return ordinal; fixes are built but a successful full
open/edit/discard/apply cycle is still required. Paid purchase is deferred.
The temporary Windows editor remains debug-only and disabled until opted in.
Vehicle-heading camera basis has legacy-state migration. Mirror auto-hide now
uses guarded native view-mode decisions and leaves the saved Rearview setting
unchanged; its scene/mask/reset behavior still needs runtime validation.

Frontend ReInit (also used by its constructor) and LoadedRideInfo's native copy
now propagate known RideInfo ownership. Allocation reuse drops stale associations;
unknown equal-equipment cars cannot borrow the selected global frontend record
in the value resolver. Cached menus read current owned values every render rather
than capturing stance at instance creation. Copy provenance is covered by fixtures;
real menu transitions still require in-game confirmation.

### Camera-only original-install update, 2026-11-17 06:49 JST

The original REDUX installation now uses a selective build: baseline `18894c9`
plus camera commits `ed59cb5`, `7d19940`, and `9ff541d`. The baseline's stance,
native save lifecycle, and companion ASI are retained. Experimental native stance
menu and race ownership changes are excluded. `tools/package_camera_update.py`
exports this source combination, checks patch application and builds/tests it;
all four baseline test suites passed, including legacy camera sidecar migration.

Installed ASI SHA-256:
`24463a2bf4580f7dfa3f99b35307480c06f8a21a9d910e61a66db6988abbb919`.
Existing INIs and saves were untouched. Rollback receipt is in the workspace's
`research/customization-original-backups/camera-20261117-064949/receipt.json`.
Both new options remain opt-in. Build/fixture success does not verify heading or
mirror behavior during a race; those live checks remain outstanding.

### Stance controls, 2026-11-17

The isolated test reached the native stance editor and logged all seven sliders
ready; the user reported that it appeared to work. Subsequent screen captures
confirmed compact numeric labels and the enabled flag. This does not yet establish
a complete edit/discard/apply/native-save/reload/race cycle.

Added configurable normal/fine/fast multipliers, per-channel opposite-axle copy,
and synchronization of only the channel explicitly edited in both-axle mode.
Page switches preserve all values and the last individual axle supplies the linked
reference. Geometry edits enable the preview; cancel retains the existing rollback.
Pure session tests cover copy isolation, inherited sources, saved absolute values
outside new UI ranges, linked synchronization and invalid multipliers.

The stock HEADERTEXT is a group, not an FEString; its real string children were
identified in the installed tuning asset. Runtime inspection also exposed title
truncation. Read-only PC disassembly confirmed FEPrintf's existing-capacity limit
(515D20) and FESetString's native grow/copy path (515CC0 to 5BCD20). Labels now use
that grow/copy API. This final text fix still needs live verification.

Repeated test exits followed by a separately launched original-install process
interrupted end-to-end checks. The latest stance binaries remain confined to the
development package/test copy; the original retains the camera-only update.

### Height adjustment through garage scale/bias

The user reported that a test Fiat lowered only its chassis. Source inspection
found a rigid-only inverse in both height-root capture and part transformation;
the native garage applies model scale and a Z-sort bias before those paths. Valid
non-rigid roots could therefore disable the complete body-height transformation.
The fix uses a checked affine inverse and caches the conjugated height transform
once per car/view. Singular, ill-conditioned, projective and non-finite matrices
still retain native geometry. This is a verified defect in the transform path;
the Fiat symptom still needs a live reproduction check and is not declared fixed.

Math fixtures cover rotated/transformed parts under nonuniform scale and shear,
deep uniform lowering, and rejection of invalid roots. The editor now shows the
selected channel's configured bounds in the heading, rather than adding another
limiter switch. Saved absolute values retain their existing validation bounds.

The user identified Claude as the source of concurrent original-game launches.
Do not repeatedly close its game to make room for our tests. Keep the latest
stance build in the private copy until coordinated runtime QA can proceed.
Automatic stance selection is now implemented with separate bounded ranges and
grouped presets; its native random-part and copy lifecycle is documented in
STANCE_RANDOMIZATION.md. Fixture coverage includes the extra static CarViewer
copy used between Quick Race reroll and accept. Live runtime QA remains pending.
