# Body-shop stance menu

Status: native entry, height preview, cart insertion and reopening absolute values
were confirmed in the test copy. Autosave and race transfer are awaiting the
latest fixes' live checks. Reset/help confirmation is implemented in source and
has not yet been visually checked. The footer/inspection refinements below also
await a live check; passing tests do not verify their layout.
Do not treat successful builds or unit tests as in-game compatibility evidence.

## Entry and assets

With `[StanceMenu] OrbitCamera=1` (default), the editor reuses the installed
Showcase camera metadata through the native screen-info lookup. It keeps the
same editor package, car instance and preview, allowing native mouse camera
input and the configured garage camera stick without a separate Showcase screen.
Garage remapping must be enabled with `[Garage] Enabled=1, PartsOnly=0`; the
existing deadzone/inversion settings remain in charge. Other packages retain
their original metadata. An unknown/occupied camera hook disables this optional
view only. Actual orbit/mouse behavior still requires live validation.

R3 (right-stick click) or Space toggles an in-place Showcase inspection view:
slider groups and the heading are hidden while the same preview/session remains
active. R3/Space, B, A or Y returns to adjustment without queuing or purchasing
anything. This does not switch to the stock Showcase package or rebuild the car.
The footer advertises the shortcut. Inputs are polled only while this editor is
active and the game has foreground focus; a reset dialog blocks the shortcut.

The visible Y help-group strings are relabelled **Copy**, including their
shadow. An adjacent **X: Reset** hint is cloned from those installed strings,
owned by their native parent list, and never added to the stock tuning package.
X opens a native three-button dialog: **Reset selected**, **Reset all**, and
**Cancel** (initial selection). Selected reset affects the selected axle scope;
reset all restores inherited values for all six channels on both axles. Neither
changes global Enabled or bypasses preview/cart/checkout persistence.

One `Stance adjustment` entry is added under the existing body/parts shop for
owned career/My Cars records. No top-level shop or part-database asset is added.
`CustomizationStanceAssets.h` reads the installed game's InGameB bundle (raw or
outer JDLZ), extracts its stock Pause_Performance_Tuning package and creates one
independent `MWCL_Stance_Editor.fng` in memory. The asset is never written to disk
or distributed. Binary-installed source assets remain authoritative.

The clone is registered as an uncompressed native package with stable storage
for process lifetime. It has its own factory dispatch and cannot be freed by the
ordinary race bundle unloader. Missing/unsupported templates suppress the entry.
Native pause tuning keeps its original package and factory. The stance adapter
never calls native physics Setup, StoreSettings or tuning notifications.

Text writes use native FESetString (PC 515CC0), which grows FEWideString safely;
FEPrintf alone truncates text to the source asset's existing capacity. The header
group's HEADER_TEXT_MAIN/SHADOW children are edited rather than the group itself.

Only verified widget navigation/redraw notifications are forwarded. Native text
editing and ResetScreen notifications are blocked: they assume other widget
layouts or would delete the stance sliders. Modified FEString labels are copied
and restored before the native screen destructor; the shared title is untouched.

## Session and confirmation

Native templates without OPTION_DATA_n show absolute values inside each row
title. Returning to Parts uses the actual appended one-based option position;
the extension category ID is never interpreted as a native selection ordinal.

Seven native slider rows expose height, half-track, camber, complete wheel width,
tire radius, brake inset and the global enabled flag. Left/right adjusts the
selected row; the stock left/right trigger messages cycle both/front/rear axle.
The header identifies both/front/rear. Button 5 (the stock Help/Y action)
copies the selected channel from the active individual axle to the opposite axle;
both-axle mode asks the user to select a source first. Button 4 (X) opens a native
reset/help confirmation for the selected channel and axle selection. The dialog
describes editing controls and cart semantics. No/cancel leaves the preview
unchanged; Yes restores inheritance in the preview only. Purchase remains a
separate shop checkout. The default response is No.
The guarded PC DialogInterface helper uses the installed Dialog.fng asset,
without a Windows modal or replacement UI resource. If its entry is unknown,
reset is unavailable without changing values. Live dialog QA remains required.

Switching axle pages never changes values. In both-axle mode, editing a channel
synchronizes only that channel, using the last selected individual axle as its
reference (front on first entry). Other channels retain their individual settings.
Copy uses the displayed absolute source, including inherited values, without
rescaling it to newly narrowed editing ranges.

Normal adjustment uses five channel steps; RB/Shift uses one and LB/Ctrl uses
twenty. Fine takes precedence when both modifiers are held. These multipliers
are configurable under `[StanceMenu]` in `NFSMWCustomizationCamera.ini`.
An explicit geometry change or copy enables the live preview; Cancel restores
the previous global flag and owned values. The separate enabled row can still
turn the preview off. New controls, labels and preview behavior need runtime QA.

Accept adds/replaces one free grouped adjustment in the native shopping cart.
Back discards only the current editor preview, retaining an earlier queued item.
Native cart removal discards queued values. Shop checkout alone purchases them;
native profile save then commits the isolated owned-car sidecar. The global
feature flag is a separate user INI setting with the same preview/cart lifecycle.
Failed INI persistence keeps the item queued without purchasing its values.
Context/owner changes invalidate the lease and discard uncommitted preview.
Opening, changing axle pages or closing without changes does not alter values.
UI range changes never rescale saved absolute values.

No currency is charged. A tagged native cart row is allocated with the native
constructors and removed before the native installer can see its synthetic slot.
See [cart integration and remaining QA](STANCE_CART_INTEGRATION.md).
Paid purchase support remains deferred.
Bonus/stock cars without a durable owned record currently use INI presets and
the opt-in debug editor; the native owned-car menu is intentionally unavailable.

## Verification

Unit tests cover asset bounds/rename/malformed compression, grouped session
values, inheritance, linked deltas, lease isolation and enable preview/cancel.
The inspected REDUX InGameB.bun and InGameB.lzc both produce the same 78,452-byte
independent wrapper. Runtime checks still needed: career/My Cars entry and
labels, keyboard/controller sliders, linked/front/rear, apply/discard/return,
profile switching, native pause tuning, race/freeroam appearance and tire FX.
