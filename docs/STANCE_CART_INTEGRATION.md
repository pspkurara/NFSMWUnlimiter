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

## Next implementation and checks

CustomizationStanceShop.h now implements the platform-independent state machine
with owner-bound absolute snapshots, separate purchased/cart/editor layers,
revision-tagged replacement/removal and a frozen checkout snapshot. Tests cover
reopen/replace/cancel/remove/buy, failed buy, owner changes, repeated checkout,
invalid input, purchased-state exclusion and eight accept/reopen adjustments
that must stop at the same absolute lower bound. This code is not yet connected
to the running adapter and does not itself fix the live shop.

Connect it to a guarded PC
adapter using a tagged native cart item, retaining native allocation/destruction
and list ownership. Check the actual native UI row formatting before introducing
a synthetic part. The adapter must handle native install bypass and labels for
its tagged entry while all ordinary items retain their existing code paths.

Live validation requires a zero-cost stance-only cart and a mixed native-part
cart in both My Cars and career shop: preview, reopen unchanged, cancel editor,
remove item, discard all, insufficient funds (configured price), purchase once,
reopen absolute values, then save/reload and race rendering. Test copies only
until those checks pass. The currently running test still uses direct-commit
5b5f760 and must not be described as having this integration.
