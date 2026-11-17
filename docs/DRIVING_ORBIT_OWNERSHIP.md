# Driving orbit ownership

Vehicle-relative lock is a property of the additional camera, not a replacement
for the normal driving camera. Enabling the option at neutral leaves the native
pose and up vector untouched and discards the previous vehicle-local reference.
A held forward direction still owns a custom pose even with zero azimuth.

Mouse ownership and retained rate-mode ownership keep their local reference when
the axes stop moving, including a zero-angle retained view. Direction mode releases
at neutral. Explicit reset, native look-back/view reset policies, settings changes,
context loss and completed automatic return release the rate owner; mouse ownership
continues to use the existing takeover rules. All ownership is runtime-only.

Regression cases cover enabled-but-neutral, held forward, reset while held,
retained ownership, retained angles and explicit zoom. Visual/runtime confirmation
is still required. No sidecar schema or saved camera option changes.

Garage zoom follow-up (2026-11-18): the rotation override disabled both native
camera permission flags while draining HandleJoyEvents. PC 7A2380 gates rotation
and mapped zoom (actions 2B/2C) together, so controller zoom was dropped while
native mouse-wheel zoom, processed earlier, still worked. Six signature-checked
rotation setter call sites now skip only rotation during that synchronous drain.
Native zoom events and permissions survive; old native rotation fields are
restored. Focus gaps still suppress all manual events. The callback fixture checks
zoom preservation, rotation/permission restoration and focus gating. Native
controller zoom and camera focus behavior still need runtime confirmation.
