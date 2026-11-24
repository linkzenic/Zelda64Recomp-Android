# Android clean-port workspace

This project is a staged Android port built directly on the upstream Zelda64 Recompiled development branch. Each stage must pass on physical devices before the next subsystem is introduced.

## Current stage: clean upstream gameplay foundation

The separate `game` application now cross-builds the complete upstream Zelda64 Recompiled runtime as an SDLActivity shared library. It includes the freshly generated game and RSP sources, RT64, RmlUi, audio, the upstream launcher, and isolated Linkzenic touch-control, Android mod-import, save-editor, Player Model Manager, Audio API, randomizer-runtime, clock-texture import, and custom Vulkan-driver slices. It does not include a bundled ROM, randomizer mod, or GPU driver; users may install compatible mods and select their own compatible driver archive.

The launcher and opening game sequence have been validated on physical Android hardware with stock Vulkan drivers. Android lifecycle recovery, immersive high-refresh presentation, system-document ROM and mod import, 48 kHz stereo audio, physical controller input, touch controls, native touch settings, motion sensors, the embedded Linkzenic save editor, the complete Player Model Manager dependency stack, Audio API, and an end-to-end local randomizer seed have passed on the Odin2 Portal. The same clean APK passes lifecycle recovery, touch input, persisted touch settings, sustained 48 kHz audio, the complete optional randomizer stack, local seed generation, file creation, and generated-seed gameplay on a Galaxy S26 running Android 16 with its stock Adreno 840 driver. Remaining Linkzenic features return in isolated groups so compatibility regressions remain attributable.

The randomizer Python sources come from the same pinned `MMRecompAPCppGlue` revision used by Linkzenic 0.6.9. Prepare the default source location before building:

```sh
git clone --recursive https://github.com/RecompRando/MMRecompAPCppGlue.git \
    build/rando/MMRecompAPCppGlue
git -C build/rando/MMRecompAPCppGlue checkout --detach \
    236359fc502eb2f52c47dae0c283fc80c395e971
git -C build/rando/MMRecompAPCppGlue submodule update --init --recursive
```

Set `ZELDA_RANDO_GLUE_DIR` to use a source tree in a different location.

Build it from the repository root with JDK 21 and the Android SDK configured:

```sh
ANDROID_HOME="$HOME/Library/Android/sdk" \
JAVA_HOME="/Applications/Android Studio.app/Contents/jbr/Contents/Home" \
gradle -p android :game:assembleDebug
```

The debug APK is written to `android/game/build/outputs/apk/debug/game-debug.apk`. Its package ID is `io.github.zelda64recomp.cleanboot`, so it can be installed alongside the existing Android port. The APK contains program assets and native libraries only; a user-owned ROM must be supplied separately.

## Production upgrade

Build the 1.0 upgrade with `gradle -p android :game:assembleRelease -PzeldaProductionUpgrade=true`.
The APK uses the published package ID `io.github.zelda64recomp` and version code
`100`. Configure the original APK signing key through `ANDROID_KEYSTORE_PATH`,
`ANDROID_KEYSTORE_PASSWORD`, `ANDROID_KEY_ALIAS`, and `ANDROID_KEY_PASSWORD`.
Without those values the output is unsigned. Verify that the signed APK's
certificate matches the published 0.6.9 and 0.6.10 APKs before distribution.

Android retains data already stored in the app during an in-place update. On
first launch after updating, users who stored data in the shared Zelda64 folder
can select it in the Android folder picker. The import verifies the copied files
and activates them the next time the app opens. Fresh installs start at the
normal launcher.

## Diagnostic probe

The `probe` application contains Android `NativeActivity` glue and builds focused sources directly from RT64 and N64ModernRuntime. It validates the recompiler ABI and scheduler queue, then creates an RT64 interface, device, command queue, swapchain, and framebuffer and presents one frame. It does not link the Zelda game, SDL, the full runtime, mods, audio, or a custom Vulkan driver.

Build it from the repository root:

```sh
gradle --no-daemon -p android :probe:assembleDebug
```

Install and launch it:

```sh
adb install -r android/probe/build/outputs/apk/debug/probe-debug.apk
adb shell am start -n io.github.zelda64recomp.probe/android.app.NativeActivity
adb logcat -s ZeldaAndroidProbe ZeldaRuntimeProbe ZeldaRt64Probe
```

A successful run logs `stage=0 native lifecycle probe started`, `stage=3 success` after the headless runtime checks, and `stage=2 success` after RT64 presents its test frame.

The earlier stock-Vulkan implementation remains in `vulkan_probe.cpp` as the Stage 1 diagnostic. Keeping the raw Vulkan and RT64 paths separate makes regressions attributable to either the platform/driver boundary or RT64 itself.

## Port gates

1. Native loading and lifecycle
2. Vulkan instance, device, surface, and swapchain
3. RT64 test frame without the game runtime
4. N64ModernRuntime generated-function and scheduler tests
5. Minimal unmodified Zelda64 Recompiled boot (passed on Odin2 Portal)
6. Audio and physical input (passed on Odin2 Portal)
7. Storage and ROM import (passed on Odin2 Portal)
8. Touch controls and mobile UI (overlay, virtual controller, native settings, and motion sensors passed)
9. Android-native mod import (system picker and upstream installer passed)
10. Linkzenic save editor (embedded package and configuration passed)
11. YAZMT API mods (Core and Global Objects passed)
12. Player Model Manager, KV, filesystem addon, and rebuilt 16 KiB-compatible native helpers (passed)
13. Audio API and 16 KiB-safe ARM64 mod patching (passed)
14. Randomizer Python runtime, Java bridge, local seed generation, session selection, and generated-seed game start (passed)
15. Samsung Android 16 stock-driver lifecycle, touch input, settings persistence, sustained audio, full randomizer generation, and generated-seed gameplay (passed on Galaxy S26)
16. Remaining Linkzenic-specific features

Samsung or model-specific workarounds are not accepted without a reduced test which fails before the change and passes afterward.
