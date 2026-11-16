# Owned-car extension save

The native profile binary is read, not extended or rewritten. The extension
file is `NFSMWCustomizationLab.bin` inside the same profile directory. The game's
SHGetFolderPathA IAT is used so compatible save-directory redirection remains
in effect. The game title subdirectory comes from its native title pointer.

An editor stages twelve absolute floats and a presence mask, two axles per
channel. Preview is separate and disappears on cancel or editor-context change.
The host snapshots pending values when UserProfile::SaveToBuffer writes the
MemoryCard save buffer. MemcardCallbacks::SaveDone commits that snapshot with
flush, atomic replacement and a backup of the previous sidecar. Internal backup
buffers have their own snapshots and a restored native buffer restores its
matching pending extension state. Disk failures do not modify the native file.

The sidecar has a version, record size, bounded count, CRC32 and SHA256 of the
associated native profile revision. A mismatching revision, invalid value or
ambiguous schema is ignored. This prevents silently applying an older slot's
settings after the game was saved without the extension. Keep the native profile
and extension together when copying backups. A stale file is retained for manual
recovery; it is not silently reinterpreted against a new native revision.

Keys contain native owned-car handle, frontend key, vehicle key, customization
handle and career handle. Profile changes clear session tokens, pending state,
previews and render bindings. New native car records invalidate reused handles.
BuildRideForPlayer establishes known owned RideInfo bindings. Unknown pointers
are not matched merely by equipment: an AI car can have identical parts.
Copied RideInfo lineage and all game save/load flows still require real-game
verification. The extension is not claimed to work for every vehicle path.

The standalone fixture relocates game globals into its own process. It exercises
actual sidecar serialization, atomic commit, CRC and hash checks, native-byte
preservation, profile changes, preview/staging, stale token and identical AI
rejection. It does not execute native game hooks or establish their runtime ABI
beyond the inspected PC symbol signatures. Real-game confirmation is mandatory.
