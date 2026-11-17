# Active input configuration: extended orbit bindings

PC 1.3 stores native keyboard/controller bindings inside the UserProfile buffer.
The input writer at `0x56E180` is called from UserProfile::SaveToBuffer; the
reader at `0x56E200` is called from LoadFromBuffer. This implementation tracks
those specific calls. The presence of an old `input.bin` in Documents is not
used as evidence of the active configuration.

Open the game's **PC Controls** screen, then press **F7** for the additional
camera bindings window. Change horizontal/vertical stick axes, controller port,
pad reset combination, keyboard left/right/up/down/reset, inversion, deadzone,
speed, pitch and return behavior. Keyboard entries accept A..Z or Windows virtual
key numbers/hex; zero is unbound. Bindings share the keyboard/controller with
driving actions, so choose unused inputs. This extension cannot detect conflicts
with an arbitrary mod's private action maps. This is an additional Windows
dialog invoked from the native controls screen, not additional native FNG rows.

**Apply to native session** changes the active controls configuration, then
returns to the game. The native comparison hook includes extension changes, so
editing only the camera settings still triggers the native controls confirmation.
The native discard/restore path restores the extension snapshot. Its device
defaults action resets the matching extension device bindings. The dialog's
Cancel does not apply its local fields. If the native screen is destroyed or
recreated while the dialog is open, applying the stale dialog is refused.

The camera ships disabled and keyboard actions unbound. Default pad axes are
right X/Y; reset is unbound because right-thumb click commonly changes the
native camera view. Remap that native action first if assigning right-thumb
reset here. This input mapping affects race orbit; garage
part camera input remains separately configured. Runtime uses the active state,
not a global INI. The camera hook must query `Runtime()` and `ReadOrbit()`.
If any guarded native lifecycle hook conflicts or differs, the controls extension
does not install and `ReadOrbit()` returns false.

Saving a UserProfile that actually serialized native controls captures the
extension state alongside that exact buffer. A successful SaveDone writes
`NFSMWCustomizationControls.bin` beside the profile's native save, using a
temporary file, CRC, native SHA-256 revision binding, and atomic replacement.
The original profile bytes are untouched. Changes are not durable until the
native save succeeds. Saving a native backup buffer captures an in-memory
extension backup, and loading that buffer restores the matching state.

A load switches extension state **only if the native input reader actually ran**.
The reader changes the global native bindings before the caller checks its
commit flag or later validation result, so the extension follows that change
even for temporary loads. A new profile or other path which retains
the current native input configuration also retains its extension settings.
Missing, malformed, or native-revision-mismatched sidecar data selects safe
defaults when a different native input configuration is loaded. As with the
stance sidecar, keep the native save and its extension files together when
backing up or transferring a profile.

Native backup snapshots are also bound to the serialized input block's CRC.
An unknown or reused temporary buffer selects defaults rather than borrowing
the on-disk profile's unrelated settings. Native device-ID mismatch resets the
extension's controller bindings along with the native controller defaults.

The host connects five lifecycle callbacks after `CustomizationControls::Install`
receives a successful native-lifecycle status: BeginSave, EndSave, Done,
BeginLoad and EndLoad. The controls implementation itself does not include
CustomizationSave or owned-car tables. The fork and standalone camera can each
supply the host; the later module subscribes to the already published owner.
They must run around the actual original native SaveToBuffer/LoadFromBuffer,
and Done must run after confirmed native disk-save success. Rendering and the
dialog share a mutex for active state; no sidecar I/O runs under this mutex.
Tests cover camera-only dirty detection, discard, device reset, actual sidecar
serialization, exact save snapshots, temporary-buffer identity, no-reader profile
creation, partial input streams, CRC and native revision rejection. Live input and native UI behavior
still require testing in the game on the available Windows 11 installation.

The optional `MWCL_GetControlsRuntimeV1` and `MWCL_GetCameraRuntimeV1` services
allow the standalone ASI and matching fork to share a single active controls
session and camera/input hook owner. The shared State ABI is size/version checked.
Only the controls owner starts the dialog and subscribes its controls observer;
later modules query that state, preserving cancel/load behavior and one sidecar
writer. Callback code is pinned for process lifetime; dynamic unload is unsupported.

## Orbit modes and sidecar migration

The F7 dialog offers Rate orbit (the existing incremental yaw/pitch behavior) and
Direction mode. Direction mode treats the chosen X/Y pair as horizontal azimuth:
forward is the native view, right is +90 degrees and back is 180 degrees. Holding
the stick does not accumulate rotation; releasing it returns to the native view.
Both axes therefore describe direction rather than vertical pitch in this mode;
the optional keyboard axis bindings use the same selected interpretation.
Native look-back and POV/basis changes compose with direction mode. Rate mode
centers on native look-back or POV/anchor/lag changes by default. A view change
waits for neutral input before another rate orbit so a held stick does not undo
that change immediately.

The current state version 5 adds independent Rate, Direction and mouse inversion,
plus the radar policy during camera mirror auto-hide. Legacy payloads (72-byte
version 1, 76-byte version 2, 88-byte version 3, 96-byte version 4) verify
the original length, CRC and native revision before migration. Existing mode,
bindings and tuning are preserved; legacy inversion initializes all three input
sources to the previous behavior. The next successful save writes the 116-byte
state without changing native save bytes.

## Mouse orbit

The F7 dialog includes a mouse enable switch, sensitivity and zoom step. With
orbit enabled, hold the native left mouse button and move to rotate. Scroll
zooms external camera distance in the range 0.25x..4x of its native distance.
The native mouse buffer is read once per game input frame and is never cleared,
modified or consumed from other users. No replacement Raw Input registration or
window subclass is installed. Delta units use the game's normalized cursor
coordinates; wheel steps use its DirectInput 120-unit notch convention.

Mouse retains its view after release. Any active camera stick/key/reset input,
native look-back, or POV/basis change returns ownership and zoom to the native
camera/selected stick mode. If the mouse button was held, release and drag anew
before reclaiming control. Controller takeover wins when both inputs occur in
the same frame. Idle or absent controllers do not block mouse-only operation.
Focus loss, native pause, settings changes, disconnect and context gaps discard
runtime camera ownership. Cursor motion without a drag does not rotate the view.

This deliberately reuses the native cursor path, so movement stops at the native
cursor bounds; unlimited captured/recentered dragging is not implemented.
First-person eye position stays fixed and wheel distance zoom has no visual
effect there; changing first-person FOV needs a separate verified integration.
Mouse rotation and zoom share the existing post-collision orbit limitation and
still need in-game validation, including menus, pause, controllers and remappers.

## Vehicle heading and mirror policies

These are independent switches, separate from Rate/Direction mode and mouse input.
`Keep orbit relative to vehicle heading` captures the native camera in the full
vehicle-local frame, including elevation and target. Native yaw/elevation animation
cannot change that reference until a POV/mover/settings change or context reset.
Native collision pull-in can shorten the external radius. This does not add new
collision avoidance after orbit rotation. First-person position is captured in
the same vehicle frame; standard look-back reverses the bearing without changing
the locked elevation.

`Deadzone (0..0.95)` already sets the stick response threshold (default 0.22).
The radial live range is rescaled after the deadzone. Rate mode with return delay
-1 holds the chosen angle; Direction mode returns to neutral when released.

Native PC look-back uses the current POV's camera parameters, not a verified
shared rear-only camera preset. A universal rear-view position/height/distance
option is therefore not supplied.

`Hide mirror while camera is rotated` suppresses the main-player mirror while a
successfully composed orbit has nonzero yaw or pitch. Neutral Direction input,
reset and completed recentering release it; zoom or vehicle-heading alignment
alone do not hide it. Focus/flow loss and a 250 ms stale camera sample release
the override. The game still decides visibility using HUD, camera, pause and the
saved Rearview setting; this feature never forces a normally hidden mirror on.

Both verified PC calls to the native mirror predicate (0x6CFB75/0x6CFBC8,
callee 0x595D40) use the same native view-mode path for scene and mask. Unknown
code ownership leaves the feature disabled in the editor. Native visual validation
remains required, including a mirror-capable POV and transitions to/from pause.

`Show radar during camera mirror auto-hide` is independent and defaults OFF.
While the camera temporarily hides a normally visible mirror, the guarded radar
Update hook runs native detection/audio first, then selects the native HIDE script
for the detector group and both backings. Native Update restores its own scripts
when orbit returns to neutral. No FE pointers survive an update. A mirror disabled
in the game's saved options keeps the normal native no-mirror detector display;
this switch does not suppress that display. An unavailable radar hook disables
only this checkbox, preserving native HUD behavior.

Rate, Direction and mouse X/Y inversion are independently editable and saved.
Garage input is a rate camera and can inherit Rate inversion through the garage
INI. A connected neutral stick does not call native rotation setters: those setters
would otherwise change the automatic garage camera to manual even at zero speed.
