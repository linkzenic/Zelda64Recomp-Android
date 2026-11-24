# Built-in Mods

This folder contains mods that are built into the Zelda 64: Recompiled project. Built-in mods behave like normal mods but are present in a clean installation of the project. They can be updated or downgraded by placing the .nrm file for the mod in the appdata mods folder in the same way a normal mod would be, which overrides the built-in version with the manually installed version.

The list of built-in mods is as follows:
* Majora's Mask: Recompiled D-Pad Mod - https://github.com/Zelda64Recomp/MMRecompDpadMod
* Linkzenic Save Editor
* YAZMT Core
* YAZMT Global Objects

The Android package also seeds the native dependencies used by the Linkzenic build:
* Audio API (`magemods_audio_api.nrm` + `magemods_audio_api.so`)
* ProxyMM KV (`ProxyMM_KV.nrm` + `ProxyRecomp_KV005.so`)
* Player Model Manager and FS Models (`yazmt_mm_playermodelmanager*.nrm` + `yazmt_mm_playermodelmanager_fsmodels_extlib.so`)
