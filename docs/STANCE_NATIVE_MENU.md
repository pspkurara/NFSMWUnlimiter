# Body-shop stance menu implementation plan

Status: researched; not implemented or installed. Race propagation and the
blocking-popup hotfix take priority. Do not show an inert shopping item.

## Entry and assets

Add one `Stance adjustment` entry under the existing body/parts shop, including
career access. Do not add a top-level shop category or encode car-part entries
into the native save. Use the loaded game's default `CustomTuning` slider package
and its native widgets as the starting template; check the required objects at
runtime. Missing/replaced template objects must disable only this entry safely.
No Binary edit or whole frontend bundle replacement should be required.

The existing fork has native FE object cloning in FEPackage.h. CustomTuning uses
TuningSlider, UIWidgetMenu, cSlider and seven existing slider rows. Its native
Setup, StoreSettings and exit handling operate on physics tuning and Pause_Main;
they must never run against stance records. A stance session needs its own
setup, notifications, grouped apply/discard and return-to-body-shop path while
ordinary pause-menu tuning retains all native behavior. Object layouts and
vtable slots must be verified against PC disassembly, not PS2 headers.

## Session and purchase semantics

Use the existing owned editor ABI transaction: capture the owned-car key and
absolute values, preview while moving sliders, stage the entire adjustment on
accept, cancel all preview changes on discard or context loss. Native profile
save commits the sidecar later. Use front/rear/linked selection with six channels
per page and numeric absolute values/ranges from the companion. Preserve inherit
versus explicit override. Initial values come from the same native baseline used
by rendering; opening or closing the menu must not change them.

Default price is zero. A synthetic part purchase is unnecessary if a native
confirmation can commit the free grouped adjustment. Configurable paid purchase
must wait until native cash/cart rollback is implemented transactionally; do not
charge currency independently or make failed saves consume money.

## Required verification

Career body shop, My Cars, parts preview, apply, discard, leaving/switching
profiles, baseline vanilla assets, REDUX assets, keyboard/controller sliders,
front/rear/linked changes, native physics tuning after leaving stance, and
race/freeroam reflection of staged/saved values. No modal Windows popup or debug
hotkey is required for normal use.
