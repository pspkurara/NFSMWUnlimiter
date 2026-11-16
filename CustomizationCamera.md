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

[RaceOrbit]
; Experimental: requires an in-game visual check before enabling by default.
Enabled = 0
DegreesPerSecond = 100
PitchLimitDegrees = 60
; -1 retains orientation when released; >=0 recenters after this delay.
ReturnDelaySeconds = -1
ReturnDegreesPerSecond = 120
```

The garage camera uses the chosen XInput stick while a parts selection screen is
alive, excluding custom gauges. The default right stick therefore no longer
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

Experimental race orbit operates on the main player's Cubic camera look-at
construction, after its normal camera calculations. Horizontal rotation covers
360 degrees; vertical rotation is bounded; clicking the selected stick resets
both axes. The orbit resets on mover change, controller disconnect, focus loss,
and long update gaps. Mirror and split-screen views are excluded. Other camera
movers, scripted ICE cameras and debug cameras are untouched.

**Limitations:** orbit happens after the stock camera collision calculation, so
its rotated destination does not have independent wall collision avoidance.
Only the main-player Cubic camera path is implemented. No gameplay test or visual
validation is implied by passing the byte checks. Camera replacements that patch
the same call site are detected; replacements elsewhere may need separate
compatibility settings. The native part-camera orientation and speed limits still
apply in the garage. No model-specific camera offsets are assumed.

## Separate ASI input integration

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
permits a standalone input plugin without another write to the game camera code.

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
