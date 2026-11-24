# Zelda64 Recompiled Android 1.0 source

This snapshot accompanies the signed Linkzenic Android 1.0 APK. See `COPYING`
for the project's license. The APK's SHA-256 is
`9ad1ac5b16fc50f89434520a66d4611db75601b025c2305ca5dbcd86b55e5b78`.

## Build

Use a JDK 21 and the Android SDK with Android platform 36, Build Tools 36,
NDK 28.2.13676358, and CMake 3.30.3. Install Gradle, Ninja, LLVM Clang, and
LLD on the host. Run these steps from the archive root.

1. Run `bash android/fetch-source-dependencies.sh` to retrieve the pinned
   dependencies and apply the local RT64 and N64ModernRuntime changes.
2. Follow **Decompressing the target ROM** in `BUILDING.md` using your own
   NTSC-U Majora's Mask ROM.
3. Build the host tools, then regenerate the game code and game patches.

   ```sh
   cmake -S lib/N64ModernRuntime/N64Recomp -B build-n64recomp-gcc \
     -DCMAKE_BUILD_TYPE=Release
   cmake --build build-n64recomp-gcc --target N64RecompCLI RSPRecomp
   c++ -std=c++17 -O2 lib/rt64/src/tools/file_to_c/file_to_c.cpp \
     -o build-n64recomp-gcc/file_to_c

   mkdir -p RecompiledFuncs RecompiledPatches rsp
   ./build-n64recomp-gcc/N64Recomp us.rev1.toml
   ./build-n64recomp-gcc/RSPRecomp aspMain.us.rev1.toml
   ./build-n64recomp-gcc/RSPRecomp njpgdspMain.us.rev1.toml
   make -C patches CC=clang LD=ld.lld ZELDA_ANDROID_BUILTIN_DPAD=1
   ./build-n64recomp-gcc/N64Recomp patches.toml
   ./build-n64recomp-gcc/file_to_c patches/patches.bin mm_patches_bin \
     RecompiledPatches/patches_bin.c RecompiledPatches/patches_bin.h
   ```

4. Set `ANDROID_HOME` to your SDK. Run `bash android/setup-native-dependencies.sh`
   to build SDL 2.32.10 and FreeType 2.13.3 from their published sources.
5. Follow `android/README.md` to fetch the pinned randomizer sources. Build
   the Android package with
   `gradle -p android :game:assembleRelease -PzeldaProductionUpgrade=true`.
   A build without a configured keystore produces an unsigned APK.

## Bundled mod projects

- [D-Pad Special Items](https://github.com/Zelda64Recomp/MMRecompDpadMod)
- [Save Editor](https://github.com/linkzenic/mm-recomp-save-editor)
- [Audio API](https://github.com/B3n-17/mm-audio-api)
- [YAZMT mod sources](https://github.com/YAZ64MT)
- [KV](https://github.com/garrettjoecox/ProxyRecomp_KV)

The matching Android native helper sources and their licenses are under
`android/native_mods`.
