# Active key-configuration lifecycle: unfinished integration

The user requires driving orbit bindings to appear with native key configuration
and to follow its persistence owner, not simply the currently selected profile.
New-profile creation must preserve or reset the extension exactly as native
active bindings do. The camera INI is currently only a bootstrap/configuration
path and does not fulfill this requirement.

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

Next steps: trace native configuration serialization and load/reset/copy owners;
add a transactional extension schema, bind editor and screen integration; wire
the camera provider to those active bindings; test accept/cancel/defaults,
device switch, profile load/create and restart. No production hook was added
based on the unproven file ownership hypothesis.
