NFSMW Customization Lab - DEVELOPMENT SNAPSHOT

This is unfinished experimental work, not a supported universal release.
Driving-camera key-config integration is implemented; real-game verification remains open.
See docs/CUSTOMIZATION_LAB.md before attempting a test installation.

The payload contains a fork of Unlimiter 3.3.1.1338 and an independent ASI.
It targets the inspected PC 1.3 executable, not Unlimiter 4.x or arbitrary EXEs.
No game files, save profiles, registry keys or user configuration are bundled.

For a later test installation:
1. Exit the target game normally. Back up its scripts/NFSMWUnlimiter.asi and
   the entire selected profile folder, including any extension sidecars.
2. Use an installation already configured for Unlimiter 3.3.1.1338. Copy only
   the five payload files in scripts/ into its scripts/ directory.
   Do not replace the existing NFSMWUnlimiterSettings.ini or UnlimiterData.
3. Keep stance, race orbit and clipping disabled until the relevant checks
   in the documentation are completed. Garage camera/driver code is enabled
   in its supplied INI and remains experimentally unverified.
4. F6 opens the stance editor. Owned cars stage changes for the next normal
   game save; unowned cars explicitly use car-type INI presets.
5. F7 from the native PC Controls screen opens driving-camera bindings.
   Apply returns changes to the native controls confirmation transaction.
   Preserve NFSMWCustomizationControls.bin alongside its matching profile.

Rollback: exit the game, restore the backed-up original Unlimiter ASI, and
rename NFSMWCustomizationLab.asi to NFSMWCustomizationLab.asi.disabled.
Keep profile extension files with their matching native profile backup.
Do not delete or rewrite the native save to uninstall this snapshot.

Source archives and SHA256 payload hashes are included. The manifest records
the exact source commits and explicitly marks native runtime QA incomplete.

Optional installer: install_customization.ps1 -Snapshot <unpacked snapshot>
-GamePath <game directory> -BackupRoot <your writable backup directory>.
Normally exit the game first. Administrator rights may be needed for Program
Files. The installer verifies the inspected executable and payload hashes, keeps
existing INI settings, backs up replaced files, and writes a receipt. It changes
only the five listed scripts files and does not touch saves, VRM or ExtraOptions.
This is still an unfinished development snapshot. Stance and clipping ship off.
