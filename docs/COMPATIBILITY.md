# Installation and dependency policy

Use the normal PC 1.3 ASI-loader installation path. REDUX is a mod stack, not
a required API. Common vanilla-based executables used for modding are evaluated
by their executable layout and original instruction signatures, rather than by
requiring one whole-file hash. No new ASI-loader or installation-hook framework
is part of this project. Unknown or already-owned patch sites are rejected.

The currently built integration is a fork of installed Unlimiter 3.3.1.1338 plus
the independent customization editor ASI. On vanilla, first install upstream
Unlimiter 3.3.1.1338 and its data/configuration normally, then replace only the
files described by the distribution instructions. Do not use this fork as a
drop-in replacement for Unlimiter 4.x. Existing game/mod configurations remain
the owner's files, not files supplied by this extension.

This packaging dependency is not a claim that Unlimiter intrinsically owns all
features. The attachment/driver menu extension comes from Unlimiter. The owned
save extension and controls lifecycle observe native profile functions; their
file formats do not depend on ExtraOptions. Stance and orbit hooks also target
native game functions. Keep these adapters separate from the editor API, and
avoid adding an Unlimiter or ExtraOptions dependency to an otherwise independent
feature merely for convenience. This product may require its matching Unlimiter
fork; it does not require the separately distributed VRM plugin. VRM integration
is optional coexistence, not an installation dependency. No separate ASI per
individual camera/stance feature is required by this policy.

Only the inspected Windows 11 PC 1.3 executable has been exercised. The recorded
SHA-256 identifies the test binary; it is not the extension's acceptance rule.
Byte signatures passing on another executable are necessary but do not prove
visual or save compatibility. Other OS versions and hook-heavy mod combinations
require their own checks before being called supported.
