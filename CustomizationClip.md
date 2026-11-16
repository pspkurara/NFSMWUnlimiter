# Calibrated native wheel clipping

`CustomizationClip.h` implements an **experimental, explicitly opted-in wheel
clip plane** for the verified MW 1.3 x86 renderer. It does not discover a fender
surface, recognize a fenderless car, or claim that every wheel can be clipped
without author calibration. Default is disabled, with no implicit car or kit
fallback. A car with no matching boundary remains entirely native.

The target is a small upper/outer penetration into a known fender. The native
`FRONT_WHEEL`/`REAR_WHEEL` model contains both tire and rim materials; consequently
the calibrated plane clips **both**. A single half-space cannot express the
exact nonconvex condition “above this height AND outside this fender” while
preserving every other point. A slanted plane can approximate that boundary for
a particular car, kit and stance range; a bad plane can visibly cut a rim or
expose the open tire interior. Do not enable it globally.

## Integration

Call `CustomizationClip::Install(absoluteIniPath)` once during initialization,
after the host plugin's verified executable fingerprint/version gate and before
rendering begins. Log `CustomizationClip::Status()`. Disabled or conflicting
sites return false without changing any call site. The optional code uses SDK
`d3d9.h` COM declarations; it adds no DirectX helper DLL dependency.

Around each of the six native wheel `RenderPart` submissions in
`CarRenderInfo::Render`, the host adapter supplies:

```cpp
auto ticket = CustomizationClip::BeginWheel(carName, kitNumber, wheelIndex,
                                           bodyCarToWorld, view);
OriginalWheelRenderPart(/* original arguments, unchanged */);
CustomizationClip::EndWheel(ticket);
```

The native wheel order is FL=0, FR=1, RR=2, RL=3. The duplicated rear-wheel calls
use the corresponding RR/RL index. `bodyCarToWorld` is a 16-float row-vector
matrix for the rendered **car body**, including any body ride-height adjustment.
It must not contain rotating wheel spin, wheel steering, width scale or camber.
The boundary stays with the fender as the wheel moves relative to it. Existing
host adapters remain responsible for the correct car identity and selected kit.

Only main `eView* == 0x919650` is accepted. Mirror, reflection and shadow views
remain native. Any separate native shadow renderer is unchanged. Consistency
between a clipped main-view wheel and its native shadow/reflection is therefore
not yet implemented.

## Configuration

Place `NFSMWCustomizationClip.ini` beside the host ASI and pass its absolute path.
Settings and per-car profiles are cached until process restart. Each wheel can be
enabled independently by providing its own valid plane. All coefficients must
be finite, comma separated, with a nonzero XYZ normal.

```ini
[WheelClipping]
Enabled = 0

; Example SECTION/KEY syntax only: supply measured coefficients for this asset.
; No ready-to-use boundary is provided for an unknown car or kit.
[CAR:YOUR_CAR_NAME:KIT:0]
Enabled = 0
; FrontLeftPlane = nx, ny, nz, d
; FrontRightPlane = nx, ny, nz, d
; RearRightPlane = nx, ny, nz, d
; RearLeftPlane = nx, ny, nz, d
```

`nx*x + ny*y + nz*z + d >= 0` retains a point; the negative side is clipped. The
plane is expressed in the car-body coordinate system in game units (normally
meters), not in a rotating wheel's local coordinates. The stock car coordinate
convention is longitudinal X, lateral Y, vertical Z, but imported assets and
their transforms must be checked. Start from a measured tangent plane inside the
fender and test the full supported suspension, steering and stance range.

Unknown car, changed kit, missing plane, invalid numbers, singular matrix,
exhausted hardware clip slots, unsupported device, and fixed-function wheel
shaders all retain native drawing. Front/rear profiles deliberately have no
fallback to an unrelated kit or generic bounding box. A bounding box cannot
identify the actual outer fender opening or wheel spoke surface.

## Deferred-rendering implementation

Changing D3D state around `eView::Render` is insufficient: that function queues
meshes and drawing happens later. This implementation captures newly appended
queue indices around the original wheel submission, copies the car-space plane
into a world-space plane, and associates it with the queued mesh/matrix identity,
frame number, and queue execution epoch. It never stores a stack wheel matrix.

The native queue executor at `0x6E0200` uses ESI as its current render-item pointer.
After native material setup and effect `CommitChanges`, its indexed draw at
`0x6E05FC` is `FF 92 48 01 00 00`, the D3D9 `DrawIndexedPrimitive` COM slot. A
signature-checked CALL/NOP replacement inserts the current item pointer into a
stdcall adapter; the adapter calls the **same native draw exactly once**, with
the same index/vertex range and shaders. It only adds an unused hardware clip
plane while that wheel item is drawn, then restores its coefficients and enable
mask. No vertex/index buffers, native materials or LOD selections are replaced.

Nine original CALL sites targeting queue executor `0x6E0200` are wrapped at
`0x6E0E20`, `0x6E2F3B`, `0x6E2F7B`, `0x6E3036`, `0x6E5147`, `0x6E51B7`,
`0x6E529E`, `0x6E5323`, `0x6E533C`. Each wrapper calls the existing function
entry once, then invalidates captured items. It does not patch the executor
entry or split/replay the native queue. Thus an already installed VRM queue
entry hook retains its opaque/custom/transparent ordering and only runs once
per original native execution. Multi-pass native wheel shading remains intact.
If an overflow flush occurs inside wheel submission, that ticket's epoch no
longer matches and clipping is skipped rather than tagging an unrelated item.

All ten original call-site signatures are validated before the first mutation.
Code pages are unlocked once per unique page, patched, instruction cache flushed,
and their original protection restored. A conflicting call-site patch disables
this optional backend. The supported executable's image base must remain the
original `0x400000`; this header is not a pattern scanner for other releases.

Shader clip planes must be in homogeneous clip space. For row-vector transforms
`p' = p*M`, coefficients satisfy `M*q' = q`; the implementation solves this
system with pivoting. It uses the captured view's native platform VP matrix at
`+0x80`, or `+0x100` when the renderer's alternative-projection selector is active.
No square root, matrix inversion or INI parsing occurs for untagged geometry.

## Validation and outstanding work

Performed on the current Windows 11 machine:

- x86 MSVC C++17 `/W4` compilation of the full header and real naked/stdcall
  bridge succeeded without warnings.
- All ten original patch-site signatures matched the installed `speed.exe`.
- A hidden D3D9 HAL fixture with programmable `vs_2_0`/`ps_2_0` shaders rendered
  an indexed triangle through the actual clipping adapter: **1301 native pixels
  versus 940 clipped pixels**. The bridge was also actually invoked with native
  COM argument order and verified stdcall stack cleanup.
- The fixture checked translated-plane math, singular matrix rejection, invalid
  plane parsing, stale frame/queue-epoch fallback, restoration of the original
  clip mask, and preservation of an already occupied clip slot and unused-slot
  coefficients.

The successful fixture used HAL with **software vertex processing**; rasterization
and clipping ran through D3D9. Hardware vertex processing device creation returned
`D3DERR_DEVICELOST` in the current desktop session and remains unverified. The
fixture's synthetic queue/global addresses were relocated to unused memory in
its standalone process; the production header and installed game were unchanged.
The fixture lives in the development workspace under `research/clip-check` and is
not a distributed game plugin.

These checks do **not** establish a visually correct fender boundary in the game,
native MW shader-output compatibility, a correct host capture transform, or
coexistence under every REDUX graphics wrapper. Before enabling any real profile,
verify main-view customization and driving, LOD transitions and blur, left/right
mirroring, alternative projection, VRM glass ordering, device reset/Alt-Tab, and
the extremes of all exposed stance values. Keep this opt-in feature experimental
until those checks pass for the declared vehicle/kit and graphics stack.

Automatic brake-to-rim clearance remains separate: a minimum distance from the
wheel bounding box cannot distinguish a spoke's solid face from a hole. A
reliable automatic implementation requires explicit inner-face/clearance markers
or a verified native wheel material/triangle surface representation. Do not
replace this with guessed global brake offsets.

## Sources

- Microsoft [IDirect3DDevice9::SetClipPlane](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-setclipplane)
  specifies the retained half-space, programmable clip-space coefficients, and
  the separate `D3DRS_CLIPPLANEENABLE` mask.
- The workspace's `research/speed-disasm.txt` supplies the original PC queue,
  draw call, frame counter and view/matrix paths.
- `research/nfsmw-decomp/src/Speed/Indep/Src/World/CarRender.cpp`, around
  lines 3411–3605, shows combined wheel models, rim-material replacement, wheel
  LOD selection and the six wheel submissions. Shared decomp layouts are not
  blindly reused: its PC `eSolidPlat.hpp` itself warns that its platform layout
  was copied from a different platform/game and may be inaccurate.
- Existing VRM integration notes establish its deferred queue hook and the
  platform VP offsets. No VRM source file is modified by this backend.
