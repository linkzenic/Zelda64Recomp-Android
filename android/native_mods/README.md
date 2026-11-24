# Android native mod helpers

These sources build the ARM64 helper libraries required by the bundled KV and Player Model Manager filesystem mods. Gradle cross-compiles them with the project's pinned NDK and copies the results beside their `.nrm` packages in the APK assets. The activity then seeds the matched package/library pairs into the private mods directory.

The vendored sources are pinned to the releases used by the bundled packages:

- `ProxyRecomp_KV005.so`: [garrettjoecox/ProxyRecomp_KV](https://github.com/garrettjoecox/ProxyRecomp_KV), commit `33a49c3aed01dc64237d36669774057051b7e338`
- `yazmt_mm_playermodelmanager_fsmodels_extlib.so`: [YAZ64MT/PlayerModelManager_FSModels](https://github.com/YAZ64MT/PlayerModelManager_FSModels), tag `v0.3.3`, commit `f1940fdf54f708c5c0d6bccf698fdc4abb11b4d4`

Both upstream projects use the CC0 1.0 license; their license files are retained with the source. Android builds must keep `0x4000` ELF load-segment alignment and the same exported function set declared by the corresponding mod manifests.
