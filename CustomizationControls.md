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
CustomizationSave or owned-car tables. The fork currently supplies the host.
They must run around the actual original native SaveToBuffer/LoadFromBuffer,
and Done must run after confirmed native disk-save success. Rendering and the
dialog share a mutex for active state; no sidecar I/O runs under this mutex.
Tests cover camera-only dirty detection, discard, device reset, actual sidecar
serialization, exact save snapshots, temporary-buffer identity, no-reader profile
creation, partial input streams, CRC and native revision rejection. Live input and native UI behavior
still require testing in the game on the available Windows 11 installation.

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

The current state version 3 appends mouse settings to the version-2 mode field
and the 72-byte version-1 prefix. Loading either legacy size verifies its original
length, CRC and native revision before migration. Version 1 retains Rate mode;
version 2 retains its chosen mode. Existing bindings and tuning are preserved,
with defaults for new mouse fields. Unknown payload versions and sizes are
rejected. The next successful save writes the current 88-byte payload without
altering native save bytes.

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
