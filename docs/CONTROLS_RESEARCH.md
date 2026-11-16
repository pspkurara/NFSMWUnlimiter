# Active key-configuration lifecycle: implementation and evidence

The user requires driving orbit bindings to appear with native key configuration
and to follow its persistence owner, not simply the currently selected profile.
New-profile creation must preserve or reset the extension exactly as native
active bindings do. Driving controls now use a separate extension of the active native key
configuration. The garage camera continues to use its own INI.

Inspected PC 1.3 evidence:

- UIOptionsPCController constructor `0x545EB0`, deleting destructor `0x545F50`,
  NotificationMessage `0x545F70`, vtable `0x89D234`.
- `0x628CB0` returns `0x91F41C + actionIndex*0x34`. Native configuration entries
  start with the action ID at `0x91F418`, followed by four 12-byte bindings.
- `0x51D4B0` compares the screen's saved binding snapshot to current bindings.
  `0x51D590` restores that snapshot and the old profile option byte at `+0x84`.
  The notification branches at `0x54603F` and `0x5461CB` use these operations.
- `0x51D6E0` restores defaults through `0x6284D0` or `0x63CD80` depending on
  selected device. It marks the profile option byte and refreshes the screen.
- Input action evaluation uses the larger global table through `0x920388`,
  while the visible remapping screen exposes only its subset. Reusing an arbitrary
  debug-look action would overwrite an existing action's meaning.

An `input.bin` exists next to profiles in the observed Documents directory, but
its filename is absent from the inspected executable. Existence alone does not
prove this binary or installed input MOD owns it. Do not implement persistence
by watching that file without identifying its reader/writer and active instance.

Serialization evidence from the inspected binary:

- `0x56E180` writes device ID plus 19 native action records of 48 bytes each;
  `0x58FE4A` calls it from UserProfile::SaveToBuffer.
- `0x56E200` reads the same 916-byte block; `0x5ACD30` calls it from
  UserProfile::LoadFromBuffer before checking commit and later car validation.
  The reader mutates global input bindings even in a temporary profile load.
- A saved controller device-ID mismatch with a nonzero current device calls
  native controller defaults at `0x63CD80`. The extension mirrors this reset.

`CustomizationControls.h` observes actual writer/reader execution, extends native
screen comparison/restore/defaults, and provides an F7 dialog from PC Controls.
The camera consumes this active mapping. Saving captures the state at native
input serialization, then writes its own profile-adjacent sidecar after native
SaveDone success. Native backup buffers carry matched extension snapshots with
input-block CRCs; unknown buffers never borrow an unrelated disk configuration.
No-reader profile creation preserves active settings. See
[controls behavior](../CustomizationControls.md).

MSVC builds and standalone state/sidecar lifecycle tests pass. These tests do not
execute the real native FNG UI or establish live hook correctness. Accept/cancel,
device defaults, load/create/restart and controller input need in-game testing.

## Native camera ownership evidence

The inspected PC executable identifies Cubic look-back at byte `+0xA1` through
setter `0x4741A0`, getter `0x4741D0` and the Update read at `0x47D7ED`.
Cubic `SetPovType` at `0x46B050` compares the requested POV against current
`+0x9C` and writes desired `+0x98` at `0x46B0A9`. Constructor `0x481BD0`
initializes both fields and stores its CameraAnchor argument at `+0x80`
(`0x481C1D`). `SetDisableLag` writes `+0xA0` at `0x4741B9`.
The runtime guards match these specific instructions before using the fields.
This detects state changes after native remapping; it does not guess physical
controller button assignments or overwrite the native action dispatch.

`CameraInfo.hpp` names bumper/hood POV values 0/1 and external values 2..6;
the PC setter's grouping also treats 2..6 as the external group. A MOD's added
cockpit replacement is not automatically identifiable from that naming alone.
Fixed-eye selection therefore has an explicit POV bitmask override.

## Native mouse input evidence

Global mouse object `0x987F50` updates in the frame loop via `0x6E7667` to
`0x6C9C80`. That calls cursor update `0x6C9B60`, computes normalized position
deltas at object +0x14/+0x18, copies wheel from `0x628D00` to +0x1C, and stores
left/right button states at +0x20/+0x23. Native frame counter `0x982B78` increments
at `0x6E7662`, immediately before that update. DirectInput mouse data +8 is copied
to the wheel value at `0x634A20`; the input reset clears it at `0x63F892`.
The camera reads the completed buffer on the following camera update, not a new
OS event queue.

Native garage HandleMouse `0x7A2010` reads the same delta, wheel and left-button
fields. Its call at `0x7BA179` precedes garage update `0x7BA185`, which dispatches
HandleJoyEvents at `0x7B9F23`. Consequently an unconditional zero stick speed in
the joy hook overwrites native mouse rotation. Preserving mouse speeds requires
both an idle stick and the native mouse permission flags, not just a held button.

Native pause wrapper `0x468390` tail-calls GameFlowManager::IsPaused `0x64B680`.
Its exact instruction signature is required alongside the native mouse layout;
added orbit ignores input during native pause. Countdown and scripted camera
context support remains a separate, unverified task.
