# Customization camera and driver preview

Include `CustomizationCamera.h` once from `UnlimiterStuff.h`, and call
`CustomizationCamera::Install()` after the normal Unlimiter initialization.
The implementation targets the v3.3.1.1338 fork and does not change its save format.
Every optional hook checks the original PC instruction bytes or vtable pointer
before writing. A competing hook disables the affected feature and reports through
`OutputDebugStringA`; it is never overwritten blindly.

Place `NFSMWCustomizationCamera.ini` beside `NFSMWUnlimiter.asi`:

```ini
[Input]
; Xbox controller index 0..3. 0 = left stick, 1 = right stick.
ControllerIndex = 0
CameraStick = 1
Deadzone = 0.22
InvertX = 0
InvertY = 0

[Garage]
Enabled = 1
PreviewDriver = 1
RotationSpeed = 1.0

```

The garage camera uses the chosen XInput stick in parts selection (excluding
custom gauges) and, with `[Garage] PartsOnly=0` (default), other garage screens
where native rotation is permitted. Set PartsOnly=1 to limit the remap to parts. The default right stick therefore no longer
shares native camera rotation with the left stick's part navigation. The original
game still drains its event queues. Its native camera permission flags are scoped
to that synchronous call and immediately restored. Mouse/keyboard-only operation
and disconnected controllers retain native behavior. Input is ignored while the
game is not foreground. A third party mapper that deliberately maps the right
stick to menu directions must remove that duplicate mapping; the game's final
PAD_LEFT/PAD_RIGHT notifications do not preserve reliable device provenance.

Driver preview bypasses only the front-end driver hide condition while the
driver selection screen (menu ID `0x308`) is active. It does not turn on a missing
mesh, enable a disabled part, change NIS driver visibility, or touch a saved car.
Screen destruction clears preview state. Ordinary menus and racing keep the
existing visibility rules.

Driving controls are edited with F7 while the native controller options screen
is open. The separate extension dialog participates in that screen's change
comparison, cancellation and device defaults. Driving bindings and tuning are
saved beside the native profile through the controls extension; the garage INI
does not override them. See [controls lifecycle](docs/CONTROLS_RESEARCH.md).

Experimental race orbit operates on the main player's Cubic camera look-at
construction, after its normal camera calculations. Horizontal rotation covers
360 degrees; vertical rotation is bounded; an optional reset binding resets
both axes. Reset ships unbound to avoid the native right-stick-click camera-view
action. In Rate mode, with `[RaceOrbit] ResetOnLookBack=1` (default), the native remapped
look-back action clears the orbit and keeps the stock view while held, including
when the orbit stick is moved simultaneously. Releasing look-back leaves the
orbit centered. This uses the verified Cubic action flag, not an Xbox button
number; set the option to 0 to retain the independent orbit offset. The flag's
setter, getter and Update byte signatures must all match or only this automatic
reset is disabled. This change still needs in-game verification.
The orbit resets on mover change, controller disconnect, focus loss,
and long update gaps. Mirror and split-screen views are excluded. Other camera
movers, scripted ICE cameras and debug cameras are untouched.

**Limitations:** orbit happens after the stock camera collision calculation, so
its rotated destination does not have independent wall collision avoidance.
Only the main-player Cubic camera path is implemented. No gameplay test or visual
validation is implied by passing the byte checks. Camera replacements that patch
the same call site are detected; replacements elsewhere may need separate
compatibility settings. The native part-camera orientation and speed limits still
apply in the garage. No model-specific camera offsets are assumed.

## Separate ASI garage input integration

`NFSCustomizationCameraSetInputV1` is a C export (cdecl; x86 export names may have
a leading underscore). A separate input ASI may resolve it with `GetProcAddress`:

```cpp
using Input = BOOL(__cdecl*)(BOOL leftStick, float* x, float* y, BOOL* reset);
using SetInput = BOOL(__cdecl*)(unsigned version, Input provider);
// SetInput(1, provider);  SetInput(1, nullptr) returns to system XInput.
```

The callback supplies finite normalized axes in `[-1,1]` and a reset request. It
must apply its own deadzone. The camera applies the configured axis inversion.
Register/unregister on the game thread, and unregister before unloading the
provider DLL. No pointer to provider-owned input state is retained. This API
permits a standalone garage input plugin without another write to the game camera
code. Driving input uses the active key-config extension instead.

## Binary evidence and validation

Read-only comparison confirmed that `research/speed.exe` and the installed
REDUX 3.04 `speed.exe` both had SHA-256
`0c5675a08cd71fd6d31ca87e992a915054bd8b80d268bff0561d7ecc2067e342`.
The local disassembly confirms:

| Site | Verified role |
| --- | --- |
| `0x7C01B0` / `0x8B7EB4` | constructor and virtual calls to CustomizeParts::Setup |
| `0x8B7EA4` | deleting destructor, clears screen-lifetime preview state |
| `0x7B9F23` | GarageMainScreen::HandleJoyEvents call |
| `0x7A2380` | native camera permission branch |
| `0x476E70`, `0x476F30` | existing vertical/horizontal rotation setters |
| `0x74EC4F` | driver rendering branch that hides the driver when game flow is 3 |
| `0x47DCBC` | Cubic Update calls eCreateLookAtMatrix at `0x6CF0A0` |

The PC mover view ID at `+0x10` is read at `0x47DCC1`; main player ID 1 is also
named by the decompilation's `EVIEW_PLAYER1`. The live PC rotation setters and
Update, rather than the console structure layout, establish the control-mode
field at `+0x80`.

Required manual checks: attachments with left-stick navigation and right-stick
rotation; simultaneous stick use; camera disconnect/focus loss; driver selection,
cancel and purchase; leaving the shop; pause/cutscene transitions; race orbit
reset/recentering; both stock and add-on cars; other installed camera/input mods.

## View changes and first-person geometry

Rate mode also centers on verified native POV, anchor or lag-basis changes when
`[RaceOrbit] ResetOnViewChange=1` (default), waiting for neutral axes before
resuming. Direction mode composes with native look-back and view changes without
these automatic resets. Mode changes, focus loss and mover inactivity still
clear runtime state. See [mode settings](CustomizationControls.md).

The PC Cubic POV indices identify bumper as 0 and hood as 1. For these native
first-person paths the camera eye stays fixed while the look direction rotates;
external views orbit their native target. During a transition involving either
first-person POV, fixed-eye rotation remains selected. `FixedEyePovMask=3`
selects these two bits. A cockpit MOD replacing another POV can select its bit;
a generic cockpit marker is not inferred from car meshes. If the native POV
layout signatures do not match, automatic POV handling is disabled. These paths
and combinations still require game-level visual confirmation.

The native garage mouse handler runs before HandleJoyEvents. With the remap
active, an idle stick preserves the rotation speeds already set by native mouse
dragging; actual stick movement takes priority. The mouse handler's permission
flags remain authoritative, and its existing wheel zoom path is retained.
Added driving mouse controls are described in [controls](CustomizationControls.md).

## Shared inversion and threshold

Driving orbit already applies its active Controls editor inversion to sticks,
keyboard axes and mouse drag. The PC frontend/input database examined here has
no identified shared garage/driving camera inversion option to read safely; do
not interpret a steering-axis sign as a camera preference.

Garage stick rotation now defaults to the same active-profile inversion and
Deadzone, even when driving orbit is disabled. The existing controls transaction
and sidecar therefore also save/cancel these inherited preferences. The selected
garage stick/controller and garage rotation speed retain their separate roles.
Use `[Garage] UseDrivingInversion=0` or `UseDrivingDeadzone=0` independently to use
that property from `[Input] InvertX/InvertY` or `Deadzone`. Existing INIs without
the new keys inherit by default. A normalized external input provider retains
its existing deadzone contract. The game's native garage mouse rotation path
is unchanged; these garage switches concern the remapped stick.
