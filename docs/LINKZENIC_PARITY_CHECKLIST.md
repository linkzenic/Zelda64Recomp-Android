# Linkzenic Android 1.0 parity checklist

This checklist separates platform compatibility from release parity. A successful
boot on a device is necessary, but it is not sufficient to replace the existing
Linkzenic Android release.

## Release-candidate foundation

- [x] Non-debuggable `1.0.0-rc1` APK builds successfully.
- [x] APK signature, 16 KiB ZIP alignment, and all packaged ARM64 ELF load
  alignments validate.
- [x] Side-by-side package upgrades the clean test application without clearing
  its private data.
- [x] Production `1.0` uses package ID `io.github.zelda64recomp`, version code
  `100`, and the same signing certificate as the published `0.6.9` and `0.6.10` APKs.
- [x] The signed production APK updates both published APKs in place on an
  Android 16 emulator. The rehearsal preserves a private test file and imports
  shared test save and mod settings files through the folder picker.

## Runtime and presentation

- [x] Launcher and Clock Town backdrop render on Odin2 Portal and Galaxy S26.
- [x] Stock Adreno Vulkan paths work on Adreno 740 and Adreno 840.
- [x] Home/resume recreates the SDL surface and Vulkan swapchain.
- [x] Touch overlay, dynamic left stick, face buttons, and eye toggle work.
- [x] Linkzenic Ship N64, GameCube, and Modern integrated touch presets are present, with Classic retained as a fallback.
- [x] Modern and GameCube receive a live RC visual/input pass; style changes apply immediately and Modern is restored as the persisted default.
- [x] The per-preset drag/resize/hide editor opens in the RC and Cancel leaves the saved layout unchanged.
- [ ] The N64 integrated preset receives a dedicated gameplay mapping pass.
- [x] Physical controller and motion-sensor paths initialize.
- [ ] Extended physical-controller gameplay pass covers menus, movement, combat,
  targeting, items, pause screens, and name entry.
- [ ] Continuous gameplay soak test runs for at least two hours without a crash,
  device loss, audio failure, or save corruption.

## Audio, saves, and settings

- [x] Audio API initializes its 48 kHz path on Odin2 Portal and Galaxy S26.
- [x] Samsung sustained-audio test recorded an active track with no track
  underruns.
- [x] Clean-app settings and randomizer session data survive a non-debug RC
  upgrade.
- [x] Generated randomizer save and KV database reopen in RC gameplay.
- [ ] Normal save creation, reload, backup, and recovery receive an RC pass.
- [ ] Every launcher and in-game settings page is compared against `0.6.9`.

## Linkzenic features

- [x] Linkzenic launcher styling and touch-oriented settings UI are present.
- [x] N64 Mode is present.
- [x] Embedded save editor is present and has passed its configuration gate.
- [x] Player Model Manager, filesystem addon, KV, Core, and Global Objects load.
- [x] Android-native mod import through the system document picker has passed.
- [x] Custom Vulkan-driver selection and clock-texture import implementations are
  present.
- [x] Custom Vulkan-driver import, selection, restart, fallback, and removal pass
  in the non-debug RC.
- [x] Imported `.o2r` clock pack copies into private storage, reloads at runtime,
  survives restart, and reports the imported filename in the non-debug RC.
- [ ] Original and bundled 3DS clock styles each receive a final gameplay pass in
  the non-debug RC.
- [ ] Representative player-model import and selection pass in the non-debug RC.

## Randomizer

- [x] Embedded Python and Archipelago runtime initialize on Android.
- [x] Solo generation creates an archive, session metadata, KV database, and save.
- [x] A generated solo session enters gameplay on Odin2 Portal and Galaxy S26.
- [x] An existing clean-app session survives upgrade and enters RC gameplay.
- [x] The non-debug RC opens the Multiworld connection form with server,
  slot-name, password, connect, and back controls.
- [ ] Multiworld creation, connection, reconnect, item delivery, and save resume
  pass on the RC.
- [ ] The release packaging decision for the optional randomizer mod stack is
  finalized and documented.

## Migration and compatibility

- [x] A read-only migration inventory covers the existing `/sdcard/Zelda64`
  ROM, saves, configs, mods, model data, randomizer sessions, clock pack, and
  custom driver selection.
- [x] Migration copies data into private staging without modifying or
  deleting the source tree.
- [x] Migration is resumable, verifies copied files, records failures, and can be
  safely retried.
- [x] Verified staging activates transactionally in a rehearsal package, backs up
  conflicts, preserves rebuilt Android dependencies, and enters gameplay with the
  migrated ROM, save, configuration, mods, and player-model data.
- [x] The rehearsal package opens the migrated clock pack, discovers and resumes
  the migrated solo randomizer session into gameplay, and audits the reported
  player-model warnings.
- [x] The migrated clock pack retains its source SHA-256 and loads when `Import`
  is selected. The migrated `Original` clock setting is restored after the test;
  the pack's two missing optional hour textures fall back without blocking load.
- [x] Player-model warnings are confined to legacy entries with duplicate internal
  names or 16/17/19 display lists where 18 are expected. Verified migration did
  not alter those files, the selected migrated model renders, and gameplay
  continues.
- [x] A representative migrated compatibility set loads 52 mods together and
  enters gameplay, including Player Model Manager, Audio API, Interface Helper,
  controller/HUD mods, texture packs, and the optional randomizer.
- [ ] Known desktop-only native-plugin samples are rejected without affecting
  compatible mods.

The existing `0.6.9` application must not be replaced until every unchecked item
required for the public 1.0 scope is either passed or explicitly removed from that
scope with release notes.
