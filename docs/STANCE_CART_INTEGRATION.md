# Native stance shopping-cart integration

## Required behavior and current defect

The user confirmed on 2026-11-17 that Fiat lowering now works visually. They
also reported that reopening the stance screen after A/Enter resets displayed
values although geometry remains changed. A/Enter currently calls
CustomizationSave::Edit(action=1) and saves the global Enabled flag immediately.
That implementation violates the required shop transaction: accepting this
screen must only add/update one stance item in the native shopping cart.
Purchase in the shop is the sole commit point. Removing/discarding the item
restores the previously purchased stance; editing a queued item starts from its
queued absolute values. Limits must never be rebased around modified geometry.

Do not ship an intermediate adapter that has an invisible separate cart or
commits on leaving this screen. Price defaults to zero. A configurable price
must participate in native affordability, checkout totals and career charging.

## State and ownership

Maintain three distinct owned-car layers: purchased values (existing Pending),
native-cart values and the currently open editor preview. Resolution in the
frontend is preview > cart > purchased > inherited/native. Race resolution and
native disk-save snapshots use purchased only. Existing Pending is staging for
the native profile save, not the shop cart. Reopening must read cart > purchased
for initial slider values; the inherited baseline remains unmodified.

Cancel from the editor removes only its preview and restores any prior cart
entry. Accept replaces the cart entry, then closes the editor lease. Removing
the native cart item removes that layer. Failed/aborted purchase leaves it
queued. Successful purchase promotes it once, after native checkout succeeds.
Profile/car changes, shop relinquish and native memory backup/restore must
follow the native cart lifetime without spilling values to another car. Enabled
needs the same preview/cart/purchased lifecycle; the companion must not write
its persistent General.Enabled when merely accepting the stance screen.

## Verified PC call paths

Inspected local speed.exe disassembly, cross-referenced against the local
SPEED_EXE_1_3 symbol map and decompiled manager source:

| Function / field | PC address or offset | Notes |
| --- | --- | --- |
| global CarCustomizeManager | 9B9EC0 | object, not pointer-to-object |
| manager selected record | +04 | checkout reads customization/career indices |
| manager cart list | +1B4 / +1B8 | next/previous embedded list nodes |
| manager cart count | +1BC | incremented by AddToCart |
| SelectablePart copy ctor | 7A9E60 | vtable 8B7518, size 2C |
| SelectablePart value ctor | 7A9EA0 | eight stack args, ret 20h |
| RemoveFromCart | 7AEB60 | deletes item, decrements count, ret 4 |
| IsPartTypeInCart | 7AEB90 | part slot or performance type identity |
| AddToCart | 7B5870 | CanTradeIn/native part lookups precede allocation |
| EmptyCart | 7BAB40 | deletes list then tail-calls ResetPreview |
| Checkout | 7BAB60 | career charging, installs each active native part |
| ResetPreview | 7B5A50 | shared by checkout and discard; not a commit signal |
| RelinquishControl | 7AEB10 | clears customization UI ownership |
| cart UI AddItem | 7B09F0 | widget allocation and native string objects |
| cart UI Setup | 7B0C30 | builds rows from manager cart |
| cart UI NotificationMessage | 7BB240 | purchase/remove navigation |

PC ShoppingCartItem has vtable +0, linked-list node +4/+8, buying part +C,
trade-in +10, active byte +14, allocation size18. The decompiled cross-platform
header omits the PC virtual pointer in its annotated offsets: do not copy those
offsets blindly. PC SelectablePart has actual CarPart +C, slot +10, upgrade +14,
physics type +18, performance flag +1C, state +20, price +24, junkman flag +28.

Native Checkout installs nonperformance slots into the actual customization
record at 7BACB5 before updating preview/heat. A synthetic out-of-range slot
must NEVER reach that installation. Likewise AddToCart can ask for a trade-in
before allocating an item; a fabricated slot cannot safely be passed into that
path without handling it explicitly. UI descriptions, activity, totals, removal,
marker/backroom handling and checkout all need a coherent tagged-item adapter.
Do not assume that overriding InstallPart alone protects the direct record call.

## Implementation and remaining live checks

CustomizationStanceShop.h now implements the platform-independent state machine
with owner-bound absolute snapshots, separate purchased/cart/editor layers,
revision-tagged replacement/removal and a frozen checkout snapshot. Tests cover
reopen/replace/cancel/remove/buy, failed buy, owner changes, repeated checkout,
invalid input, purchased-state exclusion and eight accept/reopen adjustments
that must stop at the same absolute lower bound.

The owned-car host now owns this Shop instance. BeginShop/PreviewShop/CancelShop/
AcceptShop/RemoveShop and the two checkout callbacks validate the current token
and keep cart acceptance out of Pending. NativeMenuOwner reads queued absolute
values, frontend resolution sees the shop snapshot and race resolution still
uses purchased values only. Profile load, native memory restore and native car
record reuse clear transient shop state. The production save fixture exercises
these paths and proves native save snapshots exclude cart values, including
reopening a queued item and cancelling its later edit. All 12 root tests pass.
CustomizationStanceCart.h connects the native menu to one zero-cost tagged
ShoppingCartItem allocated with native part/item constructors. Accept queues it,
replacing an earlier stance entry. Native row formatting supplies its name and
retains price/check widgets. Native deletion discards its preview. Checkout
freezes its revision, detaches only that node before the native installer runs,
then promotes the absolute values and destroys the detached item once. A guard
also skips the synthetic slot during native ResetPreview's PreviewPart calls.
Ordinary items retain their native checkout path. Existing ExtraOptions/VRM
renderer hooks are not replaced by this adapter.

The companion's optional MWCL_ControlNativeCartV1 keeps Enabled transient while
queued, preserves it when reopening/cancelling an editor and writes its user
INI only at checkout. The host smoke tests load the actual ASI and compare INI
contents before/after staging/cancelling. Native adapter fixtures execute its
actual insertion, replacement, checkbox, failure, deletion and checkout code
against isolated manager/native-call substitutes, including mixed ordinary and
stance rows. These prove host behavior, not the game's live calling conventions
or visual row layout. Constructor/prolog/call-site guards protect installation;
live UI validation remains mandatory.

This first adapter targets regular career shops and My Cars. The junkman
backroom uses marker indices rather than ordinary purchases: the stance entry
is hidden there, and entering its cart discards an unpurchased stance item before
native marker setup. It is not yet a supported stance purchase surface. Price
is currently fixed at zero; configurable charging is not implemented.

Live validation requires a zero-cost stance-only cart and a mixed native-part
cart in both My Cars and career shop: preview, reopen unchanged, cancel editor,
remove item, discard all, purchase once,
reopen absolute values, then save/reload and race rendering. Test copies only
until those checks pass. Live cart insertion and correct current-value display
after reopening were confirmed with the post-initialization redraw. Event
diagnostics record initial, queued and purchased snapshots and profile/shop resets.

Race ownership must be established before VehicleRenderConn::SetupLoading
(PC 75D1A0), not just when Load returns: a committed load can synchronously copy
RideInfo into the loader. A scoped load context binds the exact owned native
customization before that copy. A regression fixture resolves purchased values
from the first synchronous loader copy; actual race rendering still needs QA.

Quick Race first writes CarViewer's selected RideInfo into its reserved debug
customization record (uiQRBrief exit). The profile host now owns WriteRideIntoRecord
(56F2F0), records that observed owned-car lineage, and lets the optional random
adapter observe the same completed conversion. An unowned replacement invalidates
the alias; matching equipment alone cannot inherit another car's saved stance.
Native BuildRideForPlayer clears old bindings before reusing a RideInfo, including
identical unowned cars. Conversion fingerprints include the preset key while
excluding subsequent physics/tuning changes.

Native IsCarStableDirty (57FC90) compares only the native car-stable backup. A
stance-only purchase leaves those bytes unchanged, so the host also compares
purchased extension values against BackupCarStable (57FC50). It combines the
results while always allowing native checkpoint cleanup. Previews and queued
items cannot trigger this save request; no dummy part or native data mutation is
used. The usual save flow on leaving My Cars / career shops can therefore include
stance-only purchases. The latest save and Quick Race changes still require live QA.

The live My Cars Quick Race diagnosis found an earlier missing copy: the player's
408-byte PVehicle customization exactly matched the owned stable record, but its
ownership provenance was absent. QuickGame::CreateCars first copies that record
to a stack buffer at PC 6F4A16; player setup can also copy it into an embedded
buffer at 741725. Observing only the later PVehicle allocation (689361) is too late.
Both earlier copies now use the same checked copy observer, preserving their
ESI/EDI calling convention and all native bytes. No equipment-match fallback is
used. The shared observer also forwards transient random selections independently.
Regression fixtures cover both intermediate copies, register preservation, the
owned -> intermediate -> PVehicle chain, and an identical unowned replacement.

The reconstructed CareerGame derives from QuickGame without overriding car
creation. This supports a shared career/player path, but is not live PC coverage
evidence. Career races/free roam, Quick Race, and randomized opponents still need
separate runtime checks; passing a synthetic copy fixture cannot establish them.
The latest live stance-only purchase did request the normal save flow and commit
the owned-car sidecar after native SaveDone. Reload persistence still needs QA.

Tagged stance sliders redraw their two native meters directly rather than calling
the original TuningSlider::Draw, which force-restarts every title's UNHIGHLIGHT
and slider group's INIT scripts. Normal focus navigation retains its native
scripts; refreshing the absolute values no longer restarts every row's animation.
This redraw change is built and fixture-tested, with visual confirmation pending.

Footer prompts walk the complete native package, including all duplicate
HELP_BUTTON states. Every matching state receives Copy, X: Reset and, when
available, R3: Showcase prompts. Native package ownership handles cloned text
objects on unload. The x86 fixture executes the real ForAllChildren/ForAllObjects
instructions against duplicate and nested groups, verifying callback slot 1.
Showcase also accepts PAD_BUTTON0, the mapped action used by native customization
screens; direct R3/Space input remains available. Display and transitions need
live confirmation after installation.

The body-shop entry uses the installed VISUAL_PART_RIMS texture. Its accept cue
uses the native generic customization sound (50), as suspension/brake tuning
does, through the tagged screen's sound callback. Other screens and dialogs keep
the original callback. This optional sound hook does not gate the editor.

Live original-installation inspection on 2026-11-18 found a visible PC Help
group (D9E63061) outside the named HELP_BUTTON groups. Its FEString still read
Help; the existing Copy/Reset/Showcase clones were in the hidden console/help
states. Footer discovery now also matches direct strings with the native Help
language hash 61D8F34F. Extra PC prompts use a separate row to avoid Accept/Cancel.
The native traversal fixture covers this library-expanded, differently named
group. Installation and visible confirmation of this correction remain pending.

The user corrected the initial Space result: Space does enter Showcase in the
original session. The extra SCREEN_TICK polling proposed from the initial report
was removed; the existing frame observer works. Controller activation and return
with preview retention still require confirmation. Showcase now receives the
active FECarRecord argument, as native customization does.

Deferred presentation request: keep the header to Stance | Both/Front/Rear,
put units beside each channel, and add right-aligned limit labels on each row.
