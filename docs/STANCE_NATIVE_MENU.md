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

R3 (right-stick click) or Space switches to the actual native **Showcase.fng**
screen. Its return package is our reserved **MWCL_Stance_Editor.fng**, with the
same reserved category/arguments. The stance screen is destroyed/reconstructed,
but its edit lease, absolute values, axle scope and selected row survive this
trip. Native A/B returns without queuing or purchasing anything. The footer
advertises the shortcut. Inputs are polled only while this editor is active and
the game has foreground focus; a reset dialog blocks the shortcut. A guarded
observer of the native Showcase return notification arms reconstruction; other
Showcase callers keep their normal path. Inspection itself has no time limit.
If the owner/profile changes, return is redirected to the parent shop and the
old edit is discarded. An unavailable return hook disables the shortcut only.
Actual transition, retained preview and A/B return still require live checks.

### Footer insertion crash, 2026-11-18

Opening stance with build a2a9c25 crashed before slider setup completed. WER dump
`speed.exe.6188.dmp` identifies access violation 5B217B inside the native list
inserter: the new footer string and predecessor were reversed. PC 5B2160 reads
the new node from stack+4 and predecessor from stack+8, contrary to the console
declaration's parameter order. `CustomizationFrontendList::AppendChild` now owns
that PC adapter. Its regression test executes the inspected relocation-free
x86 function bytes with empty/populated lists and ABADCAFE unlinked sentinels;
it checks count, head, tail and both links after each insertion. This verifies
the ABI fix, while actual footer layout still requires a game check.

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

On 2026-11-18 the user confirmed fresh My Cars stance and the renamed Copy
prompt in the original install. Live inspection found both extra footer labels
in the visible PC group, with correct text and position but scale (0,0,0).
Native FEObject copying allocates a zeroed current draw-data block (5C323B..
5C325A). New labels now copy the source's first 44h bytes of current appearance
into their separate allocation before positioning; no source pointer is shared.
The header now contains only Stance and axle selection. Rows show metres or
degrees and a right-justified cloned label for the signed configured range.
Successful cart submission explicitly plays native cue 50, the same cue returned
for suspension/brakes by PC CustomizePerformance::NotifySoundMessage (7A7130).
The cloned tuning screen's generic select cue is suppressed. Visible layout,
sound and Showcase focus recovery still require verification of this revision.

The axle selector also adds two mouseable FEImage prompts in the header's right
area. They reuse the installed PC prompt image, its complete 54h current data
(including UVs), and FEngSetButtonTexture at 515BB0 with L1/R1 hashes 5BC/682,
matching native CustomizeRims::Setup. Native mouse-down messages identify the
actual cloned object and change the axle once. The native mouse-state list is
rebuilt on the garage tick, never inside its click callback. This does not yet
implement arbitrary button reassignment or controller-zoom policy; configurable
assignment, independent zoom enablement, separate mouse zoom and native-image
prompts following the effective mapping are remaining input work. Live image
appearance, click hitboxes and interaction with the user's texture replacement
must be verified before release.

The next input revision assigns held precision to X, fast adjustment to L3, and
the reset dialog to Start, leaving native LB/RB axle navigation available. The
footer text is mouseable: precision toggles on click; any actual pad activity
clears that latch and returns to hold/release behavior. Reset and Showcase footer
labels are clickable too. Modifier/reset XInput masks and keyboard VKs are read
from [StanceMenu] in NFSMWCustomizationCamera.ini even without the camera ASI.
FineButton/FastButton/ResetButton and FineKey/FastKey/ResetKey are independent.
The footer's fine/reset names follow those masks, but arbitrary remapped native
images, axle-key reassignment and remaining hardcoded help text are unfinished.
Normal/fast movement rounds to an absolute zero-origin step grid on adjustment;
fine movement remains relative. Loading/selecting rows never quantizes a value.
The session fixture exercises 10,000 opposite coarse moves without float drift.
