# iCSM: Building, signing, and importing game data

[简体中文](BUILD.md) | English

This source kit targets iPhone and iPad. The native Source game modules, SDL, and Metal backend are compiled locally from source. It does not include precompiled game frameworks, the author's signing credentials, account settings, or device configuration. Third-party static dependencies such as V8 and font/video libraries, along with generated protocol headers, are provided. Maps, materials, skin previews, and offline shader libraries are supplied separately in `iCSM-v2.zip`.

## Windows / browser-based builds

Use the [GitHub Actions cloud build guide](CLOUD_BUILD.en.md) to build from Windows without a local Mac, then sign and install the IPA with your own account. The local Xcode build below requires macOS.

For **both IPA and matching data from GitHub**, use [Build iCSM IPA + Data](DATA_BUILD.md). It rebuilds Metal libraries from the included MSL and packages GitHub-hosted resource inputs. Use its paired Release ZIP instead of the frozen iCloud package described below.

## 1. Build and install on a Mac

Requirements: macOS, Xcode with the iPhoneOS 27.x SDK, Python 3, and an internet connection for the first installation of pinned CMake and Ninja versions. The minimum device OS is iOS / iPadOS 27.0. Windows can transfer game data; it cannot run the Xcode build.

Sign in to your own Apple account in **Xcode → Settings → Accounts**. Connect and unlock the device, and enable Developer Mode. Open a terminal at the repository root, where the `icsm` script is located:

```sh
./icsm doctor
./icsm setup --team YOUR_TEAM_ID --bundle-id com.yourname.icsm --device YOUR_DEVICE_ID
./icsm build
./icsm install
```

Replace the placeholders with your own Team ID, a unique Bundle ID, and your device ID. `./icsm doctor` lists the connected devices. After installation, you can start the app with `./icsm launch`.

The build compiles the native engine, Metal/SDL modules, and app, then signs the app with your certificate and exports `Build/iCSM.ipa`. Use `./icsm modules` to rebuild the native modules or `./icsm app` to rebuild only the app. A full source build takes time and requires substantial disk space. For updates, keep the same Team ID and Bundle ID and install over the existing app. Uninstalling removes its game data, inventory, settings, and touch layout.

The app's signing intermediates are stored in `~/Library/Caches/iCSM-build/` to avoid Finder metadata added by iCloud-synced Documents/Desktop folders. Native module outputs and the final IPA remain in the kit's `Build/` directory.

The IPA contains ARM64 iOS modules and does not depend on Rosetta or runtime CPU translation. Free Apple account provisioning commonly expires after seven days and requires signing and installing again; follow the actual limits reported by Xcode and the device. If the device asks you to trust the developer certificate, complete that step in Settings.

## 2. Import data from macOS or Windows

**Direct import on iPhone / iPad (Build 149):** download the matching ZIP to Files, then tap **“选择数据包 ZIP” (Select Data ZIP)** at the top left of the initial screen. Choose the ZIP from On My iPhone / iPad or iCloud Drive. No extraction or manual move into the app directory is needed; renamed downloads are accepted, while the resource manifest is still strictly validated. Keep the app in the foreground while reading and extracting cloud files. The original ZIP is preserved and can be deleted by the user afterward.

**Computer file sharing:**

**Full GitHub builds:** [combine the data volumes](DATA_BUILD.md#combine-and-install) from the same Release as your IPA. **Original app-only builds:** download [iCSM-v1.zip (iCloud)](https://www.icloud.com/iclouddrive/039w_r3_mnpuCiw9Ecvmvr7zA).

You can open the installed app before importing data; it will show the import instructions. Keep the filename exactly `iCSM-v2.zip`. Do not extract the archive on your computer.

- **Mac**: Finder → connected device → Files → iCSM. Drag `iCSM-v2.zip` into iCSM.
- **Windows**: Apple Devices → connected device → Files → iCSM → Add File. Select `iCSM-v2.zip`.

Leave the app on its import screen while copying. **Wait for the computer to confirm that the transfer has finished**, then tap **“检查并导入” (Check and Import)** in the app. Do not replace game data during a match. Alternatively, use the iOS Files app to place the archive in **On My iPhone / iPad → iCSM**.

The extracted data occupies approximately **19.65 GB**. The initial import temporarily needs space for both the archive and extracted files, plus caches; allow at least **35 GB of free space** before transferring. The app checks the available space. It validates files using SHA-256, ZIP CRC, and path checks before committing them individually. If import fails or is interrupted, the app displays the reason and retains already verified files for the next attempt. Use the data pack that matches this release's resource manifest.

After a successful Check and Import from the app's file-sharing directory, the app deletes that ZIP automatically. Select Data ZIP preserves the selected source file. **Do not delete `game-assets` or `runtime-i5`.** The `runtime-i5` folder visible through file sharing holds personal configuration and saved data and does not need to be imported again.

Apple's file-sharing instructions: [Finder on Mac](https://support.apple.com/en-us/119585) and [Apple Devices on Windows](https://support.apple.com/en-us/120402). The separate app/data workflow and import recovery design reference [brolnickij/emu](https://github.com/brolnickij/emu).

## 3. First launch and settings

Startup proceeds through resource validation/import, initial Metal shader preparation, and loading the main menu. The data pack contains **7,906 offline-compiled iOS Metal shader libraries**. The app loads and validates them on the device during the initial preparation and reuses its persistent records afterward. If the system purges temporary caches, they can be restored from the imported data without downloading the entire pack again.

Shader libraries and render pipelines are different. New combinations of maps, materials, and skins may still create pipelines on first use. The app retains its device-specific pipeline caching and recording mechanisms. GPU pipeline binaries are not copied between different device models, and first-use compilation may still occur.

Build 149 defaults to the medium preset using the measured iPhone optimization profile: approximately 1.7 million scene pixels with aspect-ratio adaptation (1920×884 on iPhone 17 Pro Max, 1504×1128 on a 4:3 iPad), 100% scene scale, a 120 FPS limit, and frame interpolation and spatial upscaling disabled by default. Base map textures are preserved, distant shadow coverage is reduced to 65%, ordinary map bump lighting is simplified, and texture filtering uses 4×. Skin lighting and texture detail default to the highest setting. FPS, frame interpolation, motion blur, and weapon quality remain individually selectable; the remaining settings are managed by the very low / low / medium / high presets. Existing settings are preserved across updates and fullscreen is enforced.

The avatar uses the default CSGO icon and the initial nickname is **icsmer**. Tap the name bar at the bottom center of the main menu to open the system keyboard and save a new name. Names persist across restarts and updates; they must be nonempty and no longer than 127 UTF-8 bytes.

After startup resources and the main menu are ready, an announcement is shown for at least 20 foreground seconds before the lobby opens.

Tested devices are **iPhone 17 Pro Max and M5 iPad Pro**. Other models have not undergone full compatibility, memory-use, or frame-rate testing. 120 Hz output, frame interpolation support, and performance depend on the actual device.

## 4. Local files and privacy

`config.local.json`, `Build/`, and signed IPAs are local build products and should not be included in the public source kit. The app does not request your Apple account password or download game assets from the internet. The importer uses a fixed resource allowlist and does not overwrite inventory, touch layouts, or player names. Uninstalling removes the app's entire data container; back it up first.

Third-party copyright and license texts are retained. The resource pack contains the game assets and modifications used by this project; their use and distribution remain subject to the rights applicable to those resources.

Return to the [English project overview](https://github.com/zktfjcksxk-spec/iCSM#english).

## 5. Export error logs

On the initial loading/import page, **“导出报错日志” (Export Error Logs)** is always available at the top right, even before any data is imported. In the game, use Settings → Game Settings → Game → Error Logs → Export Error Logs. Save or share the ZIP with the system share sheet; the app also stores up to three exports in Documents/Diagnostics.

Exports include the current and two previous startup sessions, device/system/app versions, startup errors, renderer state, and import-diagnostics.json. Import diagnostics identify the failing stage and file, package size, available and required space, missing assets, ZIP errors, CRC/SHA-256 mismatches, file-provider errors, and underlying Metal shader errors. Each text log is bounded to its last 2 MiB. Configuration, saves, game assets and command mailboxes are excluded; common password commands are filtered. Logs can still contain player/server details, so inspect them before sharing.

After a crash, reopen the app and export promptly. System crash diagnostics supplied by MetricKit are included when delivered; a ZIP does not necessarily contain a complete crash stack.
