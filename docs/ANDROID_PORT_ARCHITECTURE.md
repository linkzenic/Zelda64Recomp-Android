# Android port architecture

## Principles

- Upstream Zelda64 Recompiled remains the source of truth.
- Android support is introduced below the game wherever possible: platform shell, renderer, and runtime.
- Every native binary is ARM64, 16 KiB-page compatible, and built with consistent branch protection.
- Device behavior is discovered through capability checks, not manufacturer names.
- Stock system Vulkan is the baseline. Custom drivers are a later optional feature.
- A compatibility change must include a focused reproduction or probe.

## Layer boundaries

### Platform shell

Owns Android lifecycle, native window availability, logging, file selection, storage integration, and app foreground state. It does not alter emulated game behavior.

### Renderer

Owns Vulkan capability discovery, surface and swapchain recreation, presentation, and RT64 integration. Device selection and optional features must be capability-driven.

### Recompiler runtime

Owns generated-code dispatch, emulated memory, scheduling, message queues, DMA semantics, cache visibility, and native mod loading. Android correctness belongs here rather than in Zelda game patches.

### Game application

Owns Zelda-specific configuration and presentation. It should not contain Samsung-specific DMA, allocator, audio, or pointer-repair logic.

## Initial exclusions

The first playable milestone intentionally excludes bundled mods, the randomizer, save-editor additions, custom GPU drivers, touch controls, and Linkzenic UI changes. Those features return only after the upstream game reaches stable gameplay through the clean runtime path.

## Validated gates

### Stage 0: native lifecycle

The native shell loads on physical Android 13 devices, receives a 1920×1080 `ANativeWindow`, and survives the different pause/window-destruction behavior observed across the test devices.

### Stage 1: stock Vulkan

The system Vulkan loader creates an instance, Android surface, graphics/present device, and swapchain without RT64 or a custom driver.

### Stage 2: upstream RT64 RHI

The untouched upstream RT64 Vulkan translation unit builds with NDK 28 and presents a cleared frame through its own interface, device, queue, framebuffer, and swapchain abstractions. The Android target bypasses RT64's desktop-oriented monolithic CMake without modifying the RT64 submodule.

Validated on the Odin2 Portal's Adreno 740 using the stock Vulkan driver. RT64 reported descriptor indexing and scalar block layout support and presented a 1920×1080 four-image swapchain.

### Stage 3: N64ModernRuntime ABI

Focused sources from the untouched upstream runtime build and execute natively on Android ARM64. The probe validates the generated-function calling convention, 32-bit sign extension, 64-bit N64 load/store behavior, byte ordering, 128-bit multiplication helpers, and scheduler priority-queue ordering against an 8 MiB RDRAM allocation.

The combined native library retains 16 KiB ELF load-segment alignment and GNU branch-protection properties after RT64 and runtime code are linked.

### Stage 4: complete upstream Android build

The complete upstream application now cross-compiles and links as `libmain.so` behind a minimal SDLActivity shell. Desktop-only build assumptions were replaced with explicit host-tool selection, an Android SDL window path, a pre-API-28 aligned allocator fallback, and cancel-only native file-dialog stubs.

The resulting debug APK uses the isolated package ID `io.github.zelda64recomp.cleanboot`, targets Android 16 with a minimum API level of 24, and passes Android's 16 KiB zip-alignment check. Its ARM64 load segments are aligned to `0x4000`. The APK contains no ROM and no Linkzenic-specific features.

The upstream launcher and the opening Majora's Mask sequence render on the Odin2 Portal's stock Adreno Vulkan driver. Two desktop assumptions had to be removed from RT64's full application path:

- Android uses an `R8G8B8A8_UNORM` swapchain and matching UI render target instead of the desktop `B8G8R8A8_UNORM` default.
- Variable descriptor pools reserve the descriptor layout's declared capacity. This prevents `VK_ERROR_OUT_OF_POOL_MEMORY` on implementations that account pool usage against the maximum variable binding size.

These changes are platform- and capability-based. They do not inspect the device manufacturer, model, or GPU name.

### Stage 5: Android lifecycle and ROM import

The SDL activity now pauses RT64 presentation before Android tears down its surface. On return, the presentation thread recreates the Vulkan surface and swapchain before accepting another frame. A home-and-return test preserved the process and resumed the launcher without Vulkan errors.

ROM selection uses Android's Storage Access Framework. The activity copies the selected document to private cache, native code validates and installs it through the upstream ROM path, and the temporary cache copy is removed after validation. A physical-device import produced the expected canonical ROM SHA-1 while keeping ROM data out of the APK.

### Stage 6: audio and physical input

The Odin controller is detected through SDL as both a gamepad and joystick. An event from the physical controller device activated the upstream launcher and started the game.

Android audio runs as 48 kHz stereo PCM float through the media stream and built-in speaker. SDL's desktop-sized 256-frame transfer repeatedly starved Android's 2,886-frame AudioTrack buffer. The Android build now requests 4,096-frame transfers; a sustained physical-device sample recorded an active track with zero underruns and no stop/restart churn.

### Stage 7: Linkzenic feature reintroduction

The Android input layer exposes an SDL virtual game controller without importing the former port's renderer, DMA, allocator, audio patch, or manufacturer-specific workarounds. A Classic XML overlay remains available as a fallback. The primary N64, GameCube, and Modern presets use the normalized canvas controller from Linkzenic's Ship of Harkinian Android port, adapted at the input boundary to Zelda64 Recompiled's SDL mappings. The integrated controller retains Ship's colored geometry, floating sticks, D-pad, shoulder and trigger controls, Settings/Start row, visibility toggle, per-preset saved placement, and transactional drag/resize/hide editor.

The upstream General menu owns the integrated style, size (75%-150%), opacity (15%-100%), and editor entry. The Controls menu can disable touch controls, while the existing Android camera-sensitivity bridge remains independent. Android persists these values and applies style, size, opacity, visibility, and layout changes immediately. On the Odin2 Portal RC, Modern and GameCube switched live, touch activation worked through the virtual controller, the editor opened and cancelled without mutating the saved layout, and Modern persisted as the final selected style.

On the Odin2 Portal, SDL opened the virtual controller alongside the physical Odin controller, an on-screen south-button press launched the game, all settings changed the live overlay and survived persistence, and home/resume preserved the process and rendered game. The audio stream remained free of steady-state underruns.

Android accelerometer and gyroscope samples are rotated into the current landscape display orientation and submitted through that virtual controller's SDL sensor events. Sensor registration follows the activity lifecycle, and the shared motion-processing path rejects first or stale gyro intervals to prevent camera jumps after startup or resume.

The upstream Mods screen can install one or more user-selected mod packages through Android's system document picker. Documents are copied to private cache with their original sanitized names, passed through the upstream synchronous mod installer, and removed after the installer finishes. Android omits the desktop-only Open Mods Folder action.

The Linkzenic save editor returns as a self-contained embedded recompiled mod. It uses the upstream embedded-mod and configuration-schema paths and adds no native shared libraries, renderer hooks, or device-specific compatibility code. Its Mods entry, version, enable state, and configuration metadata load on the Odin2 Portal. Custom patch-dependent UI and optional driver management remain separate future slices.

The first dependency slice embeds YAZMT Core 0.1.1 and Global Objects 0.1.1. Both are architecture-independent recompiled packages with no external native libraries, so they preserve the clean runtime boundary while establishing the API foundation needed by Player Model Manager.

The complete Player Model Manager stack is seeded as matched package/library pairs in the private mods directory. It includes Player Model Manager 0.5.0, ProxyMM KV 0.0.5, and the filesystem addon 0.3.3. The legacy 4 KiB-aligned helper binaries are not used.

The Android build reproducibly cross-compiles the KV helper from `ProxyRecomp_KV` commit `33a49c3aed01dc64237d36669774057051b7e338` and the filesystem helper from `PlayerModelManager_FSModels` tag `v0.3.3` at commit `f1940fdf54f708c5c0d6bccf698fdc4abb11b4d4`. Vendored CC0 sources and license files live under `android/native_mods`. Both rebuilt ARM64 libraries retain the declared export sets, use only Android system-library dependencies, and have `0x4000` ELF load-segment alignment.

On the Odin2 Portal, Android loaded both helpers from private app storage, the KV database initialized, the filesystem addon created its model directory, and gameplay continued without native-loader or symbol errors.

The Audio API 0.8.5 package and native library are restored as an exact matched pair from Linkzenic 0.6.9. ARM64 function replacement now uses a BTI-compatible 24-byte trampoline, flushes the instruction cache, and changes protection across the entire patch span when it crosses a page boundary. This removed the 16 KiB-page `memcpy` fault seen during Audio API dependency resolution.

Android 16 also rejects `execmod`: after a page from an installed shared library has been made writable, the process cannot add execute permission back to that modified file-backed mapping. Android function replacement therefore copies each affected code page into a private anonymous mapping at the same virtual address before applying the patch. The page transitions from RW to RX and is never writable and executable simultaneously. Audio API initialized its 48 kHz path and gameplay continued on both the Odin2 Portal and Galaxy S26.

The Android randomizer runtime is packaged separately from the optional MMRecompRando mod. Chaquopy 17 provides Python 3.11 and the pinned minipelago source set from `MMRecompAPCppGlue` commit `236359fc502eb2f52c47dae0c283fc80c395e971`; a small Java bridge exposes local seed generation to the mod's native APCpp glue. The APK and every packaged Python native extension retain 16 KiB alignment.

The clean runtime also provides the randomizer's setup-init callback and continuation exports. The continuation preserves the upstream `ConsoleLogo` boot path; it does not restore the former port's device-specific synchronous-DMA workaround.

The complete local randomizer flow passes on the Odin2 Portal. The Android randomizer and its dependency packages load, the Solo settings screen invokes the embedded generator, Archipelago 0.6.1 writes a seed archive and session metadata to private app storage, and the selected session starts the game with its own initialized KV database. The validation seed entered the Clock Town opening sequence without a loader, Python, or native runtime error.

### Stage 8: cross-driver Vulkan audit

A fresh comparison against upstream RT64 found several desktop assumptions in the full renderer path that the standalone Vulkan probe did not make:

- Queue selection required `VK_QUEUE_TRANSFER_BIT` to be explicitly reported. Vulkan guarantees transfer operations on graphics and compute queues even when that bit is absent, so those queues now receive the guaranteed effective capability during selection. Compile-time checks cover graphics-plus-compute and compute-only families that omit the transfer bit, along with a family that genuinely has no transfer support.
- Descriptor indexing and scalar block layout were rejected when a Vulkan 1.2-or-newer driver exposed the promoted core features without advertising the legacy extension names. The renderer now accepts the core path and enables only extension names the driver actually advertises.
- Swapchain creation forced identity transform, transfer-destination usage, the raw window extent, and only opaque or inherited alpha. It now selects from the surface's advertised transforms, image usage, current or clamped extent, and all four Vulkan composite-alpha modes.
- Surface-format and present-mode enumeration, surface-capability queries, queue-family selection, and swapchain creation now report explicit failures. Successful startup logs the selected GPU, queue families, and final swapchain configuration without recording account, user, or filesystem identity.

The resulting full APK builds with NDK 28 and starts twice on the Odin2 Portal's stock Adreno 740 driver. The observed swapchain was `1920x1025`, used supported color-attachment and transfer-destination flags, identity transform, and inherited alpha. No Vulkan creation, device-loss, or presentation error appeared in either cold-start log.

The same APK was then validated through Samsung Remote Test Lab on a Galaxy S26 (`SM-S942U`) running Android 16. RT64 selected the stock Adreno 840 Vulkan 1.4.295 driver, created a `2340x1080` swapchain, and continuously rendered the Linkzenic launcher. After installing the verified ROM, the complete built-in mod stack loaded, Audio API initialized its 48 kHz path, the KV store initialized, and the Clock Town opening rendered without a Vulkan, loader, or native crash. A second 30-minute regression kept the same process alive across Home and resume, destroyed and recreated the SDL surface and Vulkan swapchain, accepted touch input, and preserved the touch-overlay setting across a force-stop. AudioFlinger recorded an active 48 kHz track for more than 100 seconds with no track underruns. The complete optional randomizer stack then loaded, generated a local Archipelago seed and save, and entered generated-seed gameplay. The Odin2 Portal regression run continued to load the same larger randomizer stack after the Android 16 code-page change.

### Stage 9: read-only legacy-data inventory

The General menu can open Android's system folder picker directly at an existing
`Zelda64` directory and build a private, tab-separated migration inventory. The
inventory records every child file and directory with its relative path, size,
modification time, and MIME type. It requests read access only and does not copy,
move, rename, or delete source data.

On the Odin2 Portal, the non-debug RC inventoried 5,117 files, 584 child
directories, and 8,759,484,918 bytes. Independent before-and-after checks retained
the same source file and directory counts, and a sentinel clock pack retained its
size, modification time, and SHA-256 digest. Copying and activation remain a
separate future stage so they can be resumable, verified, and rehearsed without
overwriting a working installation.

### Stage 10: verified and resumable private staging

Android can copy a selected legacy tree into a private migration staging area
without changing live app data. Each file is copied through a temporary file,
hashed with SHA-256 on both sides, synchronized to storage, and atomically renamed
only after the digests match. A durable journal records the source metadata and
verified digest after every file. Retries verify existing staged files and skip
them, incomplete temporary files are replaced, unsafe relative paths are rejected,
and a staging area cannot be reused with a different selected source tree.

A separate debuggable rehearsal application ID isolates migration testing from
both the original release and the clean RC. On the Odin2 Portal, that package
staged all 5,117 files and 584 child directories from `/sdcard/Zelda64`, totaling
8,759,484,918 bytes, with zero failures. The staged tree independently retained
the same counts and sentinel SHA-256 digest as the source. A complete second pass
reported zero copies and 5,117 verified resumptions. Activation into the live data
layout remains a separate stage so conflicts and runtime-owned files can be
handled before the app starts.

### Stage 11: transactional rehearsal activation

Verified staging can be scheduled for activation on the next cold launch. The
activation runs before SDL and the native runtime start, moves conflicting live
files into a private backup tree, and records each atomic staged-to-live rename in
a durable journal. An interrupted activation resumes from that journal. The seven
rebuilt Android native-mod packages and libraries are explicitly preserved rather
than replaced by legacy copies, and activation cannot begin without a successful
staging-completion marker.

The full Odin2 Portal rehearsal activated 5,110 legacy files, preserved all seven
clean runtime dependencies, and backed up five pre-existing rehearsal files. The
preserved KV helper matched the clean bundled SHA-256 digest. The migrated ROM was
recognized without another import, the legacy mod set loaded alongside the clean
helpers, Audio API initialized at 48 kHz, the KV database opened, and the migrated
save entered rendered Clock Town gameplay. The source tree retained its original
counts and sentinel digest.

The migrated clock archive retained its source SHA-256 digest and loaded from
private storage when the rehearsal clock style was temporarily changed from its
migrated `Original` value to `Import`. Two optional sun/moon-hour textures are not
present in that legacy archive; the clock loader reports them and continues with
fallbacks. The original migrated setting was restored after the test.

With the optional randomizer mod temporarily re-enabled, the rehearsal UI found
the prior solo session for seed `14641797208563979056`, resumed it, opened the
matching migrated KV database, and entered rendered gameplay. The rehearsal mod
configuration was restored after the test. The player-model warning audit found
legacy entries with duplicate internal names and models containing 16, 17, or 19
display lists where the manager expects 18. Verified migration did not alter those
files, the selected migrated model rendered, and gameplay continued. A broader
representative-model compatibility pass remains a separate release gate. The same
run loaded 52 migrated mods together and reached gameplay without a loader error;
explicit rejection testing with known desktop-only native-plugin samples remains
a separate release gate.

## Private generated inputs

The upstream game and RSP sources are regenerated locally from the user's verified NTSC-U ROM. The compressed input matches SHA-1 `d6133ace5afaa0882cf214cf88daba39e266c078`; the decompressed build input matches the expected MD5 `f46493eaa0628827dbd6ad3ecd8d65d6`.

ROMs, decompressed ROMs, and ROM-derived generated sources remain excluded by the upstream `.gitignore`. They must never be committed, included in an APK as files, or uploaded to a public build service.
