# Random stance lifecycle

Status: implemented in the host/optional companion ABI; fixture-tested. Live
Quick Race, rival, restart and replay verification is still pending.

Random stance must follow random part selection, never a render frame, camera
change, or settings-file reload. The existing Unlimiter random-parts adapter is
called from racer creation (5E8FE0) and Quick Race random selection (7B4907).
Selection belongs after that adapter finishes, so body and wheel identity are final.
An early return, including HPC paint-only compatibility, must still finish or clear
the stance selection exactly once.

Configuration offers disabled, bounded random values, or a named preset pool.
Defaults remain disabled. Automatic ranges are separate from the manual editor's
ranges: widening a manual range to allow a grounded body must not also produce
extreme AI cars. Presets outside automatic bounds must not enter the candidate pool.
Missing channels inherit native/preset baselines. A selected preset is one grouped
front/rear choice, not an independent mixture of unrelated presets per channel.

## Carry the selection through native copies

Quick Race constructs a temporary RideInfo, randomizes it, and gives it to CarViewer.
On accept, the native frontend writes that ride to the temporary race customization
record. Therefore a map attached only to the temporary RideInfo is insufficient.
The verified PC transitions are:

1. Random parts complete: generate one transient selection for that ride generation.
2. CarViewer::SetRideInfo (7A9DB0) copies to static RideInfo 9BA088 before frontend
   construction. Carry that exact source copy, then the loaded/frontend copies.
3. FECustomizationRecord::WriteRideIntoRecord (56F2F0): carry it into that record.
4. Native customization deep copy (689361): carry it into the race copy.
5. WriteRecordIntoRide (56F2B0) and race render-connection load: bind the selection
   back to the resulting ride instance.

Generation identity and part fingerprints must guard every transition. Rebuilding
or rerolling a vehicle replaces its transient selection; profile/flow teardown and
allocation reuse clear stale associations. Equal parts on another car are not an
ownership or generation match. Reopening a menu or rendering smoke must not reroll.

Transient AI/Quick Race selections must never be written to the owned-car sidecar
or replace a saved owned-car record. The existing explicit owned-car save lifecycle
continues independently. Optional host/companion ABI extensions should keep the
sampler in the stance companion and native lifecycle adapters in the Unlimiter host.
The feature must fail closed if a required copy hook has an unknown owner/layout.

Implementation: `CustomizationRandom.h` observes the completed random-parts
adapter, the shared frontend/loaded/deep-copy callbacks, and both native record
writers. Separate transient tables validate part fingerprints and preserve a
generation with each observed copy. Ride initialization/stock rebuild and record
construction/default/preset conversion erase the destination before reuse;
profile load/rollback resets both tables. Owned native records are excluded.
All lifecycle hooks must pass signature checks before sampling is enabled.
The sampler uses its own seed, without consuming the game's random stream.

Configure the companion's writable user INI, not the seeding INI in `scripts`,
after its first launch. See its README and distributed `[RandomStance]` examples.
Manual editor ranges do not widen automatic selection bounds. Default disabled.
Random-parts integration must be enabled in Unlimiter. Selections are transient,
so restarting the game does not persist them; the native random-selection flow
chooses new values as it generates new cars. Owned edits continue to save normally.

Required checks include reroll, accept, restart/replay without reroll, AI copies,
identical cars, reused allocations, profile changes, missing companion, disabled
random-parts integration, and preservation of owned save data.
