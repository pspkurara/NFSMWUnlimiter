# Body-shop stance menu

Status: implemented in source; native entry/slider/return behavior still requires
runtime validation. Runtime tests exposed a missing pause-package crash, followed by an optional
numeric-label rejection and invalid return-selection crash. The current source
registers a separate template, accepts title-only slider rows and restores a
verified menu ordinal. These fixes still require a successful end-to-end test.
Do not treat successful builds or unit tests as in-game compatibility evidence.

## Entry and assets

One `Stance adjustment` entry is added under the existing body/parts shop for
owned career/My Cars records. No top-level shop or synthetic car part is added.
`CustomizationStanceAssets.h` reads the installed game's InGameB bundle (raw or
outer JDLZ), extracts its stock Pause_Performance_Tuning package and creates one
independent `MWCL_Stance_Editor.fng` in memory. The asset is never written to disk
or distributed. Binary-installed source assets remain authoritative.

The clone is registered as an uncompressed native package with stable storage
for process lifetime. It has its own factory dispatch and cannot be freed by the
ordinary race bundle unloader. Missing/unsupported templates suppress the entry.
Native pause tuning keeps its original package and factory. The stance adapter
never calls native physics Setup, StoreSettings or tuning notifications.

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
both-axle mode asks the user to select a source first. Button 4 restores inheritance.
The controller's Button 4 mapping still needs live confirmation.

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

Accept stages a free grouped owned-car adjustment. Back discards the preview.
Native profile save commits the isolated owned-car sidecar later. The global
feature flag is a separate user INI setting; preview/cancel applies to it too.
Failed INI saves restore the prior staged values and keep the editor open.
Context/owner changes invalidate the lease and discard uncommitted preview.
Opening, changing axle pages or closing without changes does not alter values.
UI range changes never rescale saved absolute values.

No currency is charged and no shopping-cart item is fabricated. Paid purchase
support is deferred until native cash/cart rollback can be transactional.
Bonus/stock cars without a durable owned record currently use INI presets and
the opt-in debug editor; the native owned-car menu is intentionally unavailable.

## Verification

Unit tests cover asset bounds/rename/malformed compression, grouped session
values, inheritance, linked deltas, lease isolation and enable preview/cancel.
The inspected REDUX InGameB.bun and InGameB.lzc both produce the same 78,452-byte
independent wrapper. Runtime checks still needed: career/My Cars entry and
labels, keyboard/controller sliders, linked/front/rear, apply/discard/return,
profile switching, native pause tuning, race/freeroam appearance and tire FX.
