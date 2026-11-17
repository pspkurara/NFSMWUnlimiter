# Garage camera focus recovery

Reported reproduction: lose foreground focus in Showcase, return to very fast
rotation; stick movement arrests the spin but becomes weak/snaps right; returning
to a fixed customization camera briefly carries that fast rotation across.

Static inspection found two related paths in GarageJoyEvents:

- ReadStick rejects background input. That rejection previously entered the
  fallback native joy handler with the competing camera branch enabled again.
- Mouse handoff clears GarageControlled. Later StopGarageSpeeds consequently
  cannot stop a residual native mouse velocity, even on focus loss.

The adapter now separates camera eligibility from input availability. Eligible
garage screens drain native joy queues with the competing camera branch masked
even when a pad is unavailable or focus is lost. Background frames and the first
foreground frame stop only an already-manual mover; they do not create manual
mode or alter native automatic/cinematic modes. Mouse ownership is accepted only
after that focus boundary. Other screens and disabled remapping retain native
handling.

This narrows concrete stale-input/ownership paths. It does not prove that a
native mouse delta, camera transition animation or another mod cannot produce
the remaining symptom. The exact reported focus/Showcase/back sequence still
requires live verification; build and existing regression tests are indirect.
