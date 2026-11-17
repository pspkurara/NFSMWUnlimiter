# Camera separation plan

The camera product should work without Unlimiter or ExtraOptions. Using the
same profile-adjacent save technique is not a semantic dependency on either mod.
This is the intended boundary, not the current distribution's dependency claim.

## Settings ownership

Use one common game-level baseline for camera behavior. Keep native key-config
lifecycle semantics for any active binding overrides: comparison, discard,
device defaults and actual native input deserialization remain authoritative.
Do not turn every profile creation into a new camera configuration when the game
itself retains its active key configuration. A global-only installation must not
need owned-car save records or an installed Unlimiter merely for persistence.
The current profile sidecar remains supported until a tested migration exists;
this plan does not silently switch existing users to a different storage mode.

## Product boundaries

| Product | Required integration |
| --- | --- |
| Driving orbit, mouse orbit and active camera bindings | Native PC 1.3 camera/input and ordinary ASI loader; no car-part mod |
| General garage camera input | Native garage input; reuse existing mouse path |
| Attachment navigation conflict and driver-part preview | Part-menu integration, kept with the matching Unlimiter fork where needed |
| Owned-car stance and visual geometry | Owned-car identity and rendering adapter in the matching fork |

Profile path resolution, name validation, CRC/native hash and current-profile
lookup are now shared source in `CustomizationProfileStorage.h`; this layer
includes no car records, editor ABI, MinHook or Unlimiter runtime. Controls use
this layer directly for file identity. Atomic file replacement remains owned by
each extension's writer. Do not add an extra mandatory ASI for these helpers. Owned-car identity tables must stay outside
camera storage. Native save/load hook ownership must be coordinated when the
camera and customization products coexist: use one versioned registration path,
validate the owner, and fail safely on unknown competing patches. Do not install
two independent unconditional patches over the same native entry point.

The controls header builds without `CustomizationSave`. Both controls and owned-car
persistence subscribe separately to `CustomizationProfileHost.h`, a bounded,
versioned C ABI. The first ready module owns the three native profile transactions;
later known modules reuse it. Begin/end callbacks use the same listener snapshot,
run outside the registry lock, and retain the native reader's temporary-load
semantics. Installation rolls back only its own hooks on failure. Callback modules
are pinned for the process lifetime; dynamic unloading is unsupported.

The fork still embeds driving orbit for existing installations. A standalone
`NFSMWExtendedCamera.asi` is now built from `standalone/CMakeLists.txt`, without
Unlimiter initialization, owned-car saves or the stance companion. Optional
versioned controls/camera runtime APIs coordinate ownership in either load order.
The second module reads the first module's active controls session, forwards input
provider/mirror capabilities and registers the stance menu's garage update with
the camera owner. It does not patch the native controls or camera sites again.
Unknown older hook owners fail the existing signature checks.

Automated fixtures load two separate DLL instances under the actual ASI names in
both initialization orders. They check shared edits/discard, one runtime owner,
mirror capabilities, idempotent garage observer registration and input-provider
forwarding. Their native patch/UI startup is stubbed. Live solo/combined camera,
native controls, save/restart, garage menus and driver previews still require QA
before this development product is called supported. Profile registry/native
transaction tests continue separately. No mandatory shared runtime ASI was added.

The standalone CMake entry point builds this product from these shared headers,
its bundled dependency sources and its own test fixtures. It contains no game
data or required companion binary and can be moved into a separate repository.

## Camera contexts

Default to the verified main-player Cubic driving camera. Investigate race-start
waiting and countdown first: these may already run this path, but that has not
been verified in-game. Do not claim that enabling orbit makes a context's native
input polling run. Expose additional contexts only after their view/mover and
update lifecycle are established.

Scripted ICE/NIS cameras use different paths. An opt-in context may be supported
only if the native animation, transitions and progression remain intact. Never
reuse a Cubic object layout on an unrelated mover or override every look-at call.
Unsupported scripted shots retain their native camera. A mover/context transition
must clear accumulated orbit and pending mouse input.

## Verification status

The user confirmed that enabling orbit permits rotation while driving on the
available Windows 11/Xbox 360 setup. They also confirmed the separate Controls
dialog and cancel restoration. Native look-back reset was missing; the new guarded
Cubic flag integration is built but awaits in-game confirmation. Durable sidecar
restoration, countdown and scripted-shot coverage remain unverified.
