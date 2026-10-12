# Build the IPA and data entirely from GitHub

English | [简体中文](#简体中文)

The repository contains mobile sources, data packaging scripts, **7,906 actual Metal shader sources** and resource hashes. Existing compiled maps/models/textures/audio are hosted as hashed volumes in [GitHub Releases](https://github.com/zktfjcksxk-spec/iCSM/releases/tag/data-inputs-v1-20261009). Original map/model authoring projects are not included. No iCloud input is required by this full workflow. It does not build the macOS game or standalone dedicated servers; shared source directories needed by iOS remain included.

## Run the full cloud build

1. Open **Actions → Build iCSM IPA + Data → Run workflow** in a repository where you can run workflows.
2. Keep the default Bundle ID and `2` native compiler jobs, or enter your Bundle ID. Leave `data_repository` as `zktfjcksxk-spec/iCSM` for this project's input release.
3. For a fork using private upstream inputs, add **Settings → Secrets and variables → Actions → `DATA_RELEASE_TOKEN`**: your own fine-grained GitHub token with **read-only Contents permission** for the input repository. The token owner must already have access. The original repository's own workflow uses its automatic token without this secret. No Apple credentials are uploaded.
4. Wait for **app** and **data** to succeed. The ARM64 Mac compiles MSL, creates the matching app manifest and builds the IPA; Linux streams resources into the data ZIP and publishes the paired export in **the repository running the workflow**.
5. Follow the release link in the run summary. Download `iCSM-unsigned.ipa`, `iCSM.entitlements`, `ipa-report.json`, **all** `iCSM-v2.zip.partNNN` files, `iCSM-v2.zip.parts.json` and `join_data.py`. SHA-256 records are included.

Forking does not copy Release assets. You may instead copy the **exact indexed input assets** into a release with the same tag in another accessible GitHub repository and enter it in `data_repository`. Every volume and file is checked against the versioned catalogs. Interrupted result uploads leave an unpublished draft. Workflow publishing requires Contents write permission; repository policies can restrict it. Private runner use consumes the owner's Actions allowance. The small intermediate app/shader artifact lasts one day; large data files use Releases rather than Actions artifact storage.

## Combine and install

Put every data volume, the parts JSON and `join_data.py` in the **same folder**. Install Python 3, open a terminal there and run:

```powershell
# Windows
py -3 join_data.py
```

```sh
# macOS / Linux
python3 join_data.py
```

The script validates every volume and the whole archive, producing **`iCSM-v2.zip`**. Allow room for the volumes plus the combined ZIP. These are transport segments of one ordinary ZIP, not separate archives. **Do not import or extract individual volumes.** Delete volumes only after successful reconstruction.

[Re-sign and install the IPA](CLOUD_BUILD.en.md#2-sign-and-install-on-windows) using your own Apple account, then import the complete ZIP through the app's initial **Select Data ZIP** button. Devices require **iOS / iPadOS 27.0+**; allow **35 GB** of device space for initial import.

**The IPA and ZIP must come from the same full-build release.** Compiler/SDK changes can change Metal hashes. The old iCloud package is a separate frozen distribution and may not match the newly generated manifest. **Build iCSM IPA** remains the app-only workflow for that original distribution.

## Optional local Mac commands

Requires Xcode with iPhoneOS 27.x, Python 3 and authenticated GitHub CLI (`gh`) with access to the input release. If the Metal compiler is missing, install it with `xcodebuild -downloadComponent metalToolchain`, as described by [Apple](https://developer.apple.com/documentation/xcode/downloading-and-installing-additional-xcode-components); the cloud workflow handles this automatically. Run at the repository root:

```sh
python3 scripts/build_data.py shaders --jobs 4
./icsm build --unsigned --bundle-id com.icsm.client --jobs 6
python3 scripts/verify_ipa.py Build/iCSM-unsigned.ipa --unsigned --data-manifest Build/data/distribution-assets.json --report Build/ipa-report.json
python3 scripts/build_data.py package
python3 scripts/join_data.py Build/release/iCSM-v2.zip.parts.json
```

Generate shaders **before** compiling the app; the first command updates its manifest. The final ZIP is in `Build/release/`. Signed local builds can use the existing setup/build flow after shader generation.

**Status:** this newly added full-data workflow has not been executed. Build200 now has a separately published, locally verified IPA/data pair; this does not certify a cloud run. Previous successful Build 149 runs apply only to the existing app-only workflow.

## 简体中文

1. 在有运行权限的仓库中打开 **Actions → Build iCSM IPA + Data → Run workflow**。
2. 保持默认 Bundle ID 和 `2` 个编译任务；资源仓库默认 `zktfjcksxk-spec/iCSM`。
3. 原仓库运行无需额外令牌。如果在 Fork 运行，且原资源仓库是私有的，需要在 Fork 的 Actions Secrets 添加 `DATA_RELEASE_TOKEN`：自己的 GitHub 细粒度令牌，仅授予资源仓库 **Contents 读取权限**，账户本身必须有访问权。Fork 不复制 Release 附件，也可把相同输入附件放到自己可访问仓库的同名 Release，再修改资源仓库输入。
4. 等待 **app** 和 **data** 两个任务完成。Mac 编译真实 MSL 和 IPA，Linux 下载资源并打包，将产物上传到**运行工作流的仓库 Releases**。
5. 下载 IPA、全部数据分卷、`iCSM-v2.zip.parts.json` 和 `join_data.py` 到电脑同一文件夹；Windows 运行 `py -3 join_data.py`，macOS 运行 `python3 join_data.py`，得到 **`iCSM-v2.zip`**。合并时会校验每个分卷和整体 SHA-256，需预留分卷加完整 ZIP 的空间。
6. 用自己的 Apple 账户重签名并安装 IPA，再从游戏初始页导入合并后的 ZIP。不要解压或导入单个分卷。设备要求 **iOS / iPadOS 27.0+**，首次导入建议预留 **35 GB**。

仓库已包含 **7,906 份真实 Metal 着色器源码**、校验清单和打包脚本，大资源分卷在 [GitHub Release](https://github.com/zktfjcksxk-spec/iCSM/releases/tag/data-inputs-v1-20261009)。地图／模型是现有编译后资源，没有原始制作工程。完整工作流不使用 iCloud，也不构建 macOS 游戏或独立服务端。

**IPA 与数据包必须来自同一次完整构建。** 新编译的着色器可能改变校验值，旧 iCloud 包不一定匹配。原 **Build iCSM IPA** 流程仍用于原冻结版本。

**验证状态：** 完整云流程尚未运行；Build200 另有本地导出并验证的配套 IPA／数据发布，不能据此宣称云流程通过。此前已通过的 Build 149 云编译只证明原 IPA 工作流；新流程的结果以实际 GitHub Actions 运行为准。
