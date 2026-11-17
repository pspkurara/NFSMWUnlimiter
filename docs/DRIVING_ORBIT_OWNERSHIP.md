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
native mouse-wheel zoom, processed earlier, still worked. Seven signature-checked
rotation setter call sites now skip only rotation during that synchronous drain.
Native zoom events and permissions survive; old native rotation fields are
restored. Focus gaps still suppress all manual events. The callback fixture checks
zoom preservation, rotation/permission restoration and focus gating. Native
controller zoom and camera focus behavior still need runtime confirmation.
Two zoom call sites additionally preserve automatic/cinematic modes for zero
input: native SetZoomSpeed(0) also claims manual control. Actual nonzero zoom may
claim that mode, and a subsequent zero release stops it normally. The final
native horizontal-stop call is suppressed together with the six directional
rotation/reset calls so an idle controller cannot claim a manual orbit.

[Garage] ControllerZoom independently disables mapped controller zoom. Mouse
wheel calls remain native. OverrideZoomButtons replaces only controller zoom
with physical XInput ZoomInButton/ZoomOutButton masks and ZoomSpeed; LT/RT masks
retain analog pressure. These settings do not require enabling stick rotation.
The replacement stops on release and across focus/update gaps without turning
an automatic mover into manual on neutral input. This revision has regression
coverage for binding masks/trigger pressure, but live zoom direction, button
conflicts and settings combinations are still unverified. The stance-only host
does not yet implement its own scoped controller-zoom suppression.
